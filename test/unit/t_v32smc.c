/*
 * t_v32smc.c -- differential test of V.32's absolute and differential encoders.
 *
 * Nothing in the object builds a `struct v32_smc` or a `struct v32_symout`,
 * so the fixture builds both and hands the two implementations the same
 * bytes.  The layout is therefore part of what is under test: a wrong offset
 * sends the wrong value to the shift, the index or the ring and the two sides
 * part company on the first symbol.
 *
 * The ring is deliberately SHORTER than the run in most cases, so the wrap is
 * taken many times and the whole buffer is compared afterwards -- a wrap that
 * happened one symbol early would otherwise be invisible.  The unused tail of
 * each ring is filled with a sentinel and compared too, which is what catches
 * a write past `limit`.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32dec.h"	/* SMCv32_PMAP16, declared once, see D308 */
#include "dsplib/v32smc.h"

extern void ref_SMCv32_encoder_abs(void *smc, void *out, const short *in,
				   unsigned short count);
extern void ref_SMCv32_encoder_dif(void *smc, void *out, const short *in,
				   unsigned short count);
extern void ref_SMCv32_encoder_tcm(void *smc, void *out, short *in,
				   unsigned short count);
extern const unsigned short ref_SMCv32_PMAP16[4];
extern const unsigned short ref_SMCv32_PMAP_ABS16[4];
extern const short ref_TrellisEncodeDifTable[16];
extern const short ref_TrellisTransitionTable[32];
extern const unsigned short ref_SMCv32_MOD[8];

#define RING	64
#define RUN	200

static void
fill(short *buf, int n, int seed)
{
	int i;
	unsigned int x = (unsigned int)seed * 2654435761u + 11u;

	for (i = 0; i < n; i++) {
		x = x * 1103515245u + 12345u;
		buf[i] = (short)(x >> 11);
	}
}

static int
run(const char *label, int differential, short mode, short shift,
    short quad, short limit, const short *state)
{
	static short in[RUN];
	static short ring_a[RING], ring_b[RING];
	struct v32_smc sa, sb;
	struct v32_symout oa, ob;
	int fed;
	int chunk;
	int i;

	diff_begin(label);

	memset(&sa, 0, sizeof(sa));
	sa.mode = mode;
	sa.shift = shift;
	sa.quad = quad;
	for (i = 0; i < 3; i++)
		sa.state[i] = state[i];
	sb = sa;

	memset(ring_a, 0x5a, sizeof(ring_a));
	memset(ring_b, 0x5a, sizeof(ring_b));
	memset(&oa, 0, sizeof(oa));
	oa.buf = ring_a;
	oa.widx = 3;			/* start off the origin */
	oa.limit = limit;
	ob = oa;
	ob.buf = ring_b;

	fill(in, RUN, mode * 17 + shift);

	/* Ragged chunks: quad, state and widx are what carry across calls. */
	fed = 0;
	for (chunk = 1; fed + chunk <= RUN; chunk += 3) {
		if (differential) {
			ref_SMCv32_encoder_dif(&sa, &oa, in + fed,
					       (unsigned short)chunk);
			SMCv32_encoder_dif(&sb, &ob, in + fed,
					   (unsigned short)chunk);
		} else {
			ref_SMCv32_encoder_abs(&sa, &oa, in + fed,
					       (unsigned short)chunk);
			SMCv32_encoder_abs(&sb, &ob, in + fed,
					   (unsigned short)chunk);
		}
		diff_eq_int("quad after chunk at %ld", sb.quad, sa.quad, fed);
		diff_eq_int("widx after chunk at %ld", ob.widx, oa.widx, fed);
		for (i = 0; i < 3; i++)
			diff_eq_int("state[%ld]", sb.state[i], sa.state[i], i);
		fed += chunk;
	}

	/*
	 * The WHOLE ring, including the part beyond `limit`: a write past the
	 * end lands there and nothing else would report it.
	 */
	for (i = 0; i < RING; i++)
		diff_eq_int("ring[%ld]", ring_b[i], ring_a[i], i);

	/* Untouched fields must stay untouched on both sides. */
	diff_eq_int("mode unchanged (%ld)", sb.mode, sa.mode, 0);
	diff_eq_int("shift unchanged (%ld)", sb.shift, sa.shift, 0);
	diff_eq_int("limit unchanged (%ld)", ob.limit, oa.limit, 0);
	diff_eq_int("the ring wrapped (%ld)", RUN > limit, 1, limit);

	return diff_end();
}

/*
 * The trellis coder needs its own fixture: it writes back to the input
 * buffer, so each side gets its own copy of the input and both copies are
 * compared afterwards.  `trellis` must start in 0..3 and `prev` in 0..7 --
 * every table index is derived from them and the object bounds neither.
 */
static int
run_tcm(const char *label, short nbits, short trellis, short prev, short quad,
	short limit)
{
	static short in_a[RUN], in_b[RUN], seed[RUN];
	static short ring_a[RING], ring_b[RING];
	struct v32_smc sa, sb;
	struct v32_symout oa, ob;
	int fed;
	int chunk;
	int i;

	diff_begin(label);

	memset(&sa, 0, sizeof(sa));
	sa.mode = 1;
	sa.quad = quad;
	sa.trellis_diff_state = trellis;
	sa.trellis_state = prev;
	sa.uncoded_bits = (unsigned short)nbits;
	sb = sa;

	memset(ring_a, 0x5a, sizeof(ring_a));
	memset(ring_b, 0x5a, sizeof(ring_b));
	memset(&oa, 0, sizeof(oa));
	oa.buf = ring_a;
	oa.widx = 1;
	oa.limit = limit;
	ob = oa;
	ob.buf = ring_b;

	fill(seed, RUN, nbits * 13 + trellis);
	memcpy(in_a, seed, sizeof(seed));
	memcpy(in_b, seed, sizeof(seed));

	fed = 0;
	for (chunk = 1; fed + chunk <= RUN; chunk += 3) {
		ref_SMCv32_encoder_tcm(&sa, &oa, in_a + fed,
				       (unsigned short)chunk);
		SMCv32_encoder_tcm(&sb, &ob, in_b + fed,
				   (unsigned short)chunk);
		diff_eq_int("quad at %ld", sb.quad, sa.quad, fed);
		diff_eq_int("trellis at %ld", sb.trellis_diff_state, sa.trellis_diff_state, fed);
		diff_eq_int("prev at %ld", sb.trellis_state, sa.trellis_state, fed);
		diff_eq_int("widx at %ld", ob.widx, oa.widx, fed);
		fed += chunk;
	}

	/* The write-back: the caller's buffer is an output too. */
	for (i = 0; i < RUN; i++)
		diff_eq_int("input word %ld after masking", in_b[i], in_a[i],
			    i);
	for (i = 0; i < RING; i++)
		diff_eq_int("ring[%ld]", ring_b[i], ring_a[i], i);

	/* It must really have masked something, or the sweep proves little. */
	{
		int changed = 0;

		for (i = 0; i < RUN; i++)
			if (in_b[i] != seed[i])
				changed++;
		diff_eq_int("the input was masked in place (%ld)",
			    changed > 0, 1, changed);
	}
	diff_eq_int("nbits untouched (%ld)", sb.uncoded_bits, sa.uncoded_bits, 0);

	return diff_end();
}

int
main(void)
{
	static const short zero[3] = { 0, 0, 0 };
	static const short loaded[3] = { 9, 4, 15 };
	int rc = 0;
	int i;

	diff_begin("the two phase maps");
	for (i = 0; i < 4; i++) {
		diff_eq_int("SMCv32_PMAP16[%ld]", SMCv32_PMAP16[i],
			    ref_SMCv32_PMAP16[i], i);
		diff_eq_int("SMCv32_PMAP_ABS16[%ld]", SMCv32_PMAP_ABS16[i],
			    ref_SMCv32_PMAP_ABS16[i], i);
	}
	rc |= diff_end();

	/* Absolute: mode only reaches the tag, so all three are worth a run. */
	rc |= run("abs, mode 0", 0, 0, 0, 0, 40, zero);
	rc |= run("abs, mode 1", 0, 1, 0, 2, 40, zero);
	rc |= run("abs, mode 2, ring 7", 0, 2, 0, 3, 7, zero);
	rc |= run("abs, ring longer than the run", 0, 0, 0, 1, RING, zero);

	/*
	 * Differential: mode selects the arm AND the accumulator, so each of
	 * the three is a different path, and `shift` decides which two bits of
	 * the input drive the phase.
	 */
	rc |= run("dif, mode 0 (the literal-1 arm)", 1, 0, 0, 0, 40, zero);
	rc |= run("dif, mode 1", 1, 1, 0, 0, 40, zero);
	rc |= run("dif, mode 2, state preloaded", 1, 2, 0, 1, 40, loaded);
	rc |= run("dif, mode 0, shift 2", 1, 0, 2, 2, 40, loaded);
	rc |= run("dif, mode 1, shift 4", 1, 1, 4, 3, 13, loaded);
	rc |= run("dif, mode 1, shift 14 (sign reaches it)", 1, 1, 14, 0, 40,
		  zero);
	rc |= run("dif, mode 2, shift 8, ring 5", 1, 2, 8, 2, 5, loaded);

	diff_begin("the trellis coder's three tables");
	for (i = 0; i < 16; i++)
		diff_eq_int("TrellisEncodeDifTable[%ld]",
			    TrellisEncodeDifTable[i],
			    ref_TrellisEncodeDifTable[i], i);
	for (i = 0; i < 32; i++)
		diff_eq_int("TrellisTransitionTable[%ld]",
			    TrellisTransitionTable[i],
			    ref_TrellisTransitionTable[i], i);
	for (i = 0; i < 8; i++)
		diff_eq_int("SMCv32_MOD[%ld]", SMCv32_MOD[i],
			    ref_SMCv32_MOD[i], i);
	rc |= diff_end();

	/*
	 * The trellis coder.  `nbits` sets three masks at once, so the sweep
	 * is over it first; the `prev > 3` arm needs a prev above 3 to be
	 * taken at all, and the four trellis states are four different rows.
	 */
	rc |= run_tcm("tcm, nbits 4, from rest", 4, 0, 0, 0, 40);
	rc |= run_tcm("tcm, nbits 4, prev 5 (the +bit_hi arm)", 4, 0, 5, 0,
		      40);
	rc |= run_tcm("tcm, nbits 4, trellis 3, prev 7", 4, 3, 7, 3, 40);
	rc |= run_tcm("tcm, nbits 1", 1, 1, 2, 1, 40);
	rc |= run_tcm("tcm, nbits 2, ring 9", 2, 2, 6, 2, 9);
	rc |= run_tcm("tcm, nbits 3", 3, 1, 4, 3, 40);
	rc |= run_tcm("tcm, nbits 5", 5, 2, 1, 1, 40);
	rc |= run_tcm("tcm, nbits 6, ring 5", 6, 3, 3, 2, 5);

	return rc;
}
