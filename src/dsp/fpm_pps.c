/*
 * fpm_pps.c -- Fixed Point Modem: the generic transmit pulse shaper.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_PPS_filter  .text 0x0a9590, 753 bytes
 *   FPM_PPS_init    .text 0x0a98c0, 259 bytes
 *   FPM_PPS_free    .text 0x0a9890,  35 bytes
 *
 * `taps` comes from init (`cfg.coeffs / cfg.phases`, an `idiv`) and so does
 * the two history buffers being `taps` entries rather than V.22's `2 * taps`
 * (`sysdep_malloc(2 * taps)` BYTES).  Both readings were made while writing
 * the filter and are now compiled and driven.
 *
 * See include/dsplib/fpm_pps.h for the block.  The arithmetic:
 *
 *   - THE TWO RAILS ARE FILTERED SEPARATELY AND EACH IS TRUNCATED TO 16 BITS
 *     BEFORE THE SUBTRACTION.  Two 32-bit accumulators, each `>> 15` and cast,
 *     and only then differenced; folding them into one accumulator changes the
 *     answer at the boundary.
 *   - THE DIFFERENCE IS TRUNCATED AGAIN before the output gain, which is a
 *     32-bit multiply and a further `>> 15`.
 *   - `count` AND THE RETURN ARE UNSIGNED, and both are forced: the count is
 *     decremented through `movzwl %ax` and the counter is incremented through
 *     it.  So a `count` of zero produces nothing rather than wrapping, and a
 *     produced count above 32767 comes back as itself.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/sysdep.h"

/*
 * FPM_PPS_CFG -- .rodata 0x00c4a0, 40 bytes.
 *
 * Bytes read straight from `.rodata`: `0a 00 03 00 01 00 00 00 ff 7f 00 00`
 * then four zero dwords (the four pointers `fpm_pps.h` already documents as
 * NULL, confirmed with no relocation at any of the four offsets) then
 * `78 00 00 00 00 00 00 00`.  10 phases, a nominal step of 3, mapped, a Q15
 * unity-ish gain of 32767, and 120 coefficients -- 12 taps at 10 phases.
 */

/*
 * One rail: `taps` terms, newest first, over the circular history against
 * every `phases`-th coefficient starting at `phase`.
 *
 * The same two-run shape as `fpm_sre.c`'s dot product, and for the same
 * reason -- the history wraps, the coefficient walk does not.
 */
static short
pps_rail(const short *hist, const short *coeff, short phases, short phase,
	 short taps, short widx)
{
	const short *h = hist + widx;
	const short *c = coeff + phase;
	int acc = 0;
	short i;

	for (i = widx; i >= 0; i--) {
		acc += *h * *c;
		h--;
		c += phases;
	}

	h = hist + taps - 1;
	for (i = (short)(taps - 1); i > widx; i--) {
		acc += *h * *c;
		h--;
		c += phases;
	}

	return (short)(acc >> 15);
}

unsigned short
FPM_PPS_filter(struct fpm_pps *state, struct fpm_smc_ring *src, short *out,
	       unsigned short count)
{
	short phases = state->cfg.phases;
	short step = state->cfg.step;
	short taps = state->taps;
	short need = state->need;
	short phase = state->phase;
	short widx = state->widx;
	short ridx = src->ridx;
	short len = src->len;
	unsigned short produced = 0;

	while (count != 0) {
		short si, sq, yi, yq;

		if (need != 0) {
			/*
			 * Two forms of symbol source.  The mapped one takes
			 * the ring entry's LOW BYTE as a constellation index;
			 * the direct one reads the ring's own rails.
			 */
			if (state->cfg.mapped != 0) {
				unsigned char k = (unsigned char)src->sym[ridx];

				si = state->cfg.imap[k];
				sq = state->cfg.qmap[k];
			} else {
				si = src->i[ridx];
				sq = src->q[ridx];
			}

			count = (unsigned short)(count - need);
			ridx = (short)(ridx + 1 < len ? ridx + 1 : 0);
			widx = (short)(widx + 1 < taps ? widx + 1 : 0);
			state->hist_i[widx] = si;
			state->hist_q[widx] = sq;
		}

		yi = pps_rail(state->hist_i, state->cfg.coeff_i, phases, phase,
			      taps, widx);
		yq = pps_rail(state->hist_q, state->cfg.coeff_q, phases, phase,
			      taps, widx);

		produced = (unsigned short)(produced + 1);
		*out++ = (short)(((short)(yi - yq) * state->cfg.scale) >> 15);

		/*
		 * Advance the phase.  A wrap is what consumes the next symbol,
		 * so the outputs per symbol are phases / (step + step_adj).
		 */
		need = 0;
		phase = (short)(phase + step + state->cfg.step_adj);
		if (phase >= phases) {
			phase = (short)(phase - phases);
			need = 1;
		}
	}

	state->need = need;
	state->widx = widx;
	state->phase = phase;
	src->ridx = ridx;
	return produced;
}

/*
 * Release both histories, `hist_q` first -- the same order init's realloc path
 * uses, and the reverse of the order they are allocated in.  Reproduced
 * because the object encodes it; no test can see it, since nothing allocates
 * afterwards.  The pointers are not cleared.
 */
void
FPM_PPS_free(struct fpm_pps *state)
{
	sysdep_free(state->hist_q);
	sysdep_free(state->hist_i);
}

/*
 * THE REUSE TEST GUARDS EXACTLY WHAT IT SIZES, WHICH IS WHERE THIS DIFFERS
 * FROM ITS SIBLING.  `FPM_SRE_init` decides on `cfg.coeffs` and then sizes a
 * fourth buffer on `cfg.rms_len`, which the test never looks at (deviation
 * D400).  Here the test is `state->taps < taps` and BOTH buffers are
 * `2 * taps` bytes, so a re-init cannot leave a buffer that is too small --
 * and a re-init that raises `cfg.coeffs` WITHOUT raising `coeffs / phases`
 * takes the reuse path, which an SRE-shaped test would not.  That case is
 * driven in `t_fpm_pps.c` and it is the only thing that separates the two
 * siblings' guards.
 *
 * AND THE BYTE COUNTS ARE NOT TRUNCATED TO 16 BITS, where `FPM_SRE_init`'s
 * are.  `lea (%esi,%esi,1)` and `add %esi,%esi` double the sign-extended
 * 32-bit tap count; there is no `cwtl` or 16-bit store in either size, so a
 * tap count above 16383 does not wrap here.  Measured, not carried over.
 */
void
FPM_PPS_init(struct fpm_pps *state, const struct fpm_pps_cfg *cfg, int fresh)
{
	short taps;
	short i;

	/*
	 * The configuration is copied FIRST and everything below reads the
	 * copy, not the caller's -- the object's ten dword moves are followed
	 * by loads from the state.  `FPM_SRE_init` is the other way round,
	 * because its reuse test needs the OLD `cfg.coeffs`.
	 */
	state->cfg = *cfg;

	/*
	 * `phase` is SEEDED FROM THE NOMINAL STEP, not from zero, which is the
	 * one initialisation here with no counterpart in V.22 -- there the
	 * step is the literal 3 and there is nothing to seed from.  Nothing
	 * reduces it modulo `phases`, so a configuration whose `step` is not
	 * below `phases` starts outside the range the filter maintains.
	 */
	state->need = 0;
	state->phase = state->cfg.step;
	state->widx = 0;

	/* Signed division, and it faults on a zeroed configuration. */
	taps = (short)(state->cfg.coeffs / state->cfg.phases);

	if (!fresh && state->taps < taps) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Reallocate FPM_PPS buffer");
		sysdep_free(state->hist_q);
		sysdep_free(state->hist_i);
		fresh = 1;
	}

	state->taps = taps;

	if (fresh) {
		state->hist_i = sysdep_malloc(2 * state->taps);
		state->hist_q = sysdep_malloc(2 * state->taps);
	}

	/*
	 * Cleared to the NEW tap count either way, which on the reuse path is
	 * at most the count the buffers were allocated with.
	 */
	for (i = 0; i < state->taps; i++) {
		state->hist_i[i] = 0;
		state->hist_q[i] = 0;
	}
}

/*
 * The state's layout is a byte count from a build where pointers are four
 * bytes, so these are compiled only under that ABI.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define PPS_ASSERT_OFF(field, off) \
	typedef char fpm_pps_off_##field[ \
		((int)__builtin_offsetof(struct fpm_pps, field) == (off)) \
			? 1 : -1]

PPS_ASSERT_OFF(cfg, 0x00);
PPS_ASSERT_OFF(need, 0x28);
PPS_ASSERT_OFF(phase, 0x2a);
PPS_ASSERT_OFF(widx, 0x2c);
PPS_ASSERT_OFF(taps, 0x2e);
PPS_ASSERT_OFF(hist_i, 0x30);
PPS_ASSERT_OFF(hist_q, 0x34);

#define PPS_ASSERT_CFG_OFF(field, off) \
	typedef char fpm_pps_cfg_off_##field[ \
		((int)__builtin_offsetof(struct fpm_pps_cfg, field) == (off)) \
			? 1 : -1]

PPS_ASSERT_CFG_OFF(phases, 0x00);
PPS_ASSERT_CFG_OFF(step, 0x02);
PPS_ASSERT_CFG_OFF(mapped, 0x04);
PPS_ASSERT_CFG_OFF(scale, 0x08);
PPS_ASSERT_CFG_OFF(step_adj, 0x0c);
PPS_ASSERT_CFG_OFF(imap, 0x10);
PPS_ASSERT_CFG_OFF(qmap, 0x14);
PPS_ASSERT_CFG_OFF(coeff_i, 0x18);
PPS_ASSERT_CFG_OFF(coeff_q, 0x1c);
PPS_ASSERT_CFG_OFF(coeffs, 0x20);
PPS_ASSERT_CFG_OFF(aux, 0x24);

typedef char fpm_pps_cfg_size[(sizeof(struct fpm_pps_cfg) == 0x28) ? 1 : -1];
typedef char fpm_pps_size[(sizeof(struct fpm_pps) == 0x38) ? 1 : -1];

#endif /* 32-bit */
