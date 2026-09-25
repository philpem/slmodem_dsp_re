/*
 * V27t_prc.c -- split out of the merged v27.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/v27fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/faxfifo.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v27cfg.h"

/*
 * V27TX_modem .text 0x0a3330, 192 bytes.
 *
 * `V29TX_modem`'s own shape: fill the FIFO from `in` unless
 * `V27TXP_INT_0008` is non-zero (in which case `*count` is already queued
 * elsewhere), then run the installed handler in a do/while seeded with
 * `V27TX_FRMSIZE[rate]` budget -- V.29's own fixed `V29TX_MODEM_BUDGET`
 * literal, here the same per-rate table every `TxHdx*V27` handler already
 * reads.  `in` is re-passed unchanged to every call in the loop, never
 * advanced; only `out` advances, by what each call returns.
 */
int
V27TX_modem(void *modem, unsigned short *in, short *out,
	   unsigned short *count)
{
	struct v27_tx_source *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = ((struct v27_tx *)modem)->source;

	((struct v27_tx *)modem)->result.byte.flags &=
		(unsigned char)~V27TX_RESULT_B1_BIT1;

	if (prm->int_0008 == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					prm->fifo,
				in, *count);
	else
		taken = *count;

	budget = V27TX_FRMSIZE[prm->rate];
	total = 0;
	do {
		short got;

		prm = ((struct v27_tx *)modem)->source;
		got = prm->handler
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		((struct v27_tx *)modem)->result.byte.flags |= V27TX_RESULT_B1_BIT1;
		((struct v27_tx *)modem)->result.byte.status = V27TX_RESULT_BYTE_07;
	}

	*count = (unsigned short)total;

	return ((struct v27_tx *)modem)->result.word;
}

/*
 * GenEQTrnSequenceV27 .text 0x0a33f0, 134 bytes.
 *
 * `TxHdxEQCondV27`'s own fill/scramble/choose sequence, free-standing over a
 * caller's buffer and count instead of `*budget`.  No reconstructed caller
 * reaches it and, per the reverse-edge probe `docs/remaining.md` records,
 * neither does anything in the object -- exported API surface with no
 * internal caller, not a missing graph hop.
 */
void
GenEQTrnSequenceV27(void *modem, unsigned short *buf, unsigned short count)
{
	unsigned short i;

	for (i = 0; i < count; i++)
		buf[i] = 7;

	ScrambleDataV27(modem, buf, (short)count);

	if (count != 0) {
		struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
		short rate = prm->rate;

		for (i = 0; i < count; i++) {
			if (buf[i + 1] & 0x04)
				buf[i] = (unsigned short)
					V27TX_PATTERN_ALT[rate];
			else
				buf[i] = (unsigned short)
					V27TX_PATTERN_CARR[rate];
		}
	}
}

/*
 * TxNextStateV27 .text 0x0a3480, 1148 bytes.
 *
 * `jmp *table(,%eax,4)` on `V27TXP_STATE`, bounded `cmp $0xa` / `ja default`
 * -- eleven arms, not V.29's seven, and each arm's printed string names the
 * state being LEFT (the object's own debug strings at .rodata.str1.1
 * 0x4c21..0x4cdc, CLAUDE.md's evidence rank 1):
 *
 *   V27TX_STATE_START    installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_QUIET    SGD_control(gen={PATTERN_CARR[rate],1}), installs
 *                        TxHdxAltV27, budget FRMSIZE[rate]*10
 *   V27TX_STATE_CARR     installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_NOCARR   SGD_control(gen={PATTERN_ALT[rate],1}), installs
 *                        TxHdxAltV27, budget ALT_COUNT[TRAIN_LONG]
 *   V27TX_STATE_ALT      installs TxHdxEQCondV27, budget
 *                        EQCOND_COUNT[TRAIN_LONG]
 *   V27TX_STATE_EQCOND   SGD_control(gen={PATTERN_SCR1[rate],1}), installs
 *                        TxHdxSCR1V27, budget 8, SetScramblerV27(modem),
 *                        RETURNS (bypasses the shared tail)
 *   V27TX_STATE_SCR1     installs TxHdxDataV27, budget 1 (no table lookup),
 *                        RESULT_B1_BIT0 SET, RETURNS
 *   V27TX_STATE_DATA     SGD_control(gen={PATTERN_SCR1[rate],1}), installs
 *                        TxHdxSCR1V27, budget FRMSIZE[rate] -- reached only
 *                        from TxHdxDataV27's own underrun-bypass arm, which
 *                        nothing reconstructed drives (see TxHdxDataV27)
 *   V27TX_STATE_TURNOFF  installs TxHdxQuietV27, budget FRMSIZE[rate]
 *   V27TX_STATE_NOENG    installs TxHdxIdleV27, budget 0, RESULT_B2_BIT0 SET
 *   V27TX_STATE_IDLE     installs TxHdxStartV27, budget 0 -- wraps to START
 *
 * EVERY ARM CLEARS RESULT_B2_BIT0 on its way out except NOENG's, which SETS
 * it, and every arm clears RESULT_B1_BIT0 except EQCOND's (cleared
 * explicitly, matching the shared tail) and SCR1's (SET, and returned
 * before the tail can clear it) -- `TxNextStateV29`'s own SCR1 asymmetry,
 * one state index later in this machine's numbering.
 *
 * `V27TX_STATE_NOCARR`/`_ALT` INDEX BY `V27TXP_TRAIN_LONG`, NOT
 * `V27TXP_RATE` -- the one place this machine differs from `V27TX_FRMSIZE`'s
 * own rate indexing, and it is what the object's `movswl 0xe(%ecx)` reads
 * (field 0x0e, not 0xc) at both sites.
 *
 * `req.det` IS `SGD_CTL.det`, read back out of the global exactly as
 * `TxNextStateV29`'s own two sites do -- this machine's four, matching
 * `sgd.h`'s own tally of "7+4+2 = 13" SGD_control sites across V.17, V.27ter
 * and V.29.
 */
void
TxNextStateV27(void *modem)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short state = prm->state;

	switch (state) {
	case V27TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_START\n");
		prm->countdown =
			V27TX_FRMSIZE[prm->rate];
		prm->handler =
			TxHdxQuietV27;
		prm->state = V27TX_STATE_QUIET;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_QUIET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_QUIET\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_CARR[prm->rate];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = ((struct v27_tx *)modem)->source;
		prm->countdown = (short)
			(V27TX_FRMSIZE[prm->rate] * 10);
		prm->handler =
			TxHdxAltV27;
		prm->state = V27TX_STATE_CARR;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_CARR:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_CARR\n");
		prm->countdown =
			V27TX_FRMSIZE[prm->rate];
		prm->handler =
			TxHdxQuietV27;
		prm->state = V27TX_STATE_NOCARR;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_NOCARR:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_NOCARR\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_ALT[prm->rate];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = ((struct v27_tx *)modem)->source;
		prm->countdown =
			V27TX_ALT_COUNT[prm->train_long];
		prm->handler =
			TxHdxAltV27;
		prm->state = V27TX_STATE_ALT;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_ALT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_ALT\n");
		prm->countdown =
			V27TX_EQCOND_COUNT[prm->train_long];
		prm->handler =
			TxHdxEQCondV27;
		prm->state = V27TX_STATE_EQCOND;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_EQCOND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_EQCOND\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_SCR1[prm->rate];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = ((struct v27_tx *)modem)->source;
		prm->countdown = 8;
		prm->handler =
			TxHdxSCR1V27;
		prm->state = V27TX_STATE_SCR1;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		((struct v27_tx *)modem)->result.byte.flags &=
			(unsigned char)~V27TX_RESULT_B1_BIT0;
		SetScramblerV27(modem);
		return;

	case V27TX_STATE_SCR1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_SCR1\n");
		prm->countdown = 1;
		prm->handler =
			TxHdxDataV27;
		prm->state = V27TX_STATE_DATA;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		((struct v27_tx *)modem)->result.byte.flags |= V27TX_RESULT_B1_BIT0;
		return;

	case V27TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_DATA\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V27TX_PATTERN_SCR1[prm->rate];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = ((struct v27_tx *)modem)->source;
		prm->countdown =
			V27TX_FRMSIZE[prm->rate];
		prm->handler =
			TxHdxSCR1V27;
		prm->state = V27TX_STATE_TURNOFF;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_TURNOFF:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_TURNOFF\n");
		prm->countdown =
			V27TX_FRMSIZE[prm->rate];
		prm->handler =
			TxHdxQuietV27;
		prm->state = V27TX_STATE_NOENG;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	case V27TX_STATE_NOENG:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_NOENG\n");
		prm->countdown = 0;
		prm->handler =
			TxHdxIdleV27;
		prm->state = V27TX_STATE_IDLE;
		((struct v27_tx *)modem)->result.byte.flags2 |= V27TX_RESULT_B2_BIT0;
		((struct v27_tx *)modem)->result.byte.flags &=
			(unsigned char)~V27TX_RESULT_B1_BIT0;
		return;

	case V27TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_STATE_IDLE\n");
		prm->countdown = 0;
		prm->handler =
			TxHdxStartV27;
		prm->state = V27TX_STATE_START;
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27TX_DEFAULT, %d\n", state);
		((struct v27_tx *)modem)->result.byte.flags2 &=
			(unsigned char)~V27TX_RESULT_B2_BIT0;
		((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_DEFAULT;
		((struct v27_tx *)modem)->result.byte.flags = (unsigned char)
			((((struct v27_tx *)modem)->result.byte.flags
			  | V27TX_RESULT_B1_BIT1)
			 & (unsigned char)~V27TX_RESULT_B1_BIT0);
		break;
	}

	((struct v27_tx *)modem)->result.byte.flags &=
		(unsigned char)~V27TX_RESULT_B1_BIT0;
}

/*
 * TxHdxIdleV27 .text 0x0a3900, 116 bytes.
 *
 * `V27TX_STATUS_IDLE` is written UNCONDITIONALLY at entry, even on the
 * transition arm, which the object does not undo -- `TxHdxIdleV29`'s own
 * shape.  With the FIFO non-empty this does not modulate at all, just
 * transitions; with it empty this spends the WHOLE current `*budget` on
 * `TxNoCarrierV27` in one call.
 */
short
TxHdxIdleV27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	struct fax_fifo *fifo;
	short taken;
	short r;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_IDLE;

	fifo = (struct fax_fifo *)prm->fifo;
	if (fifo->count != 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = *budget;
	r = TxNoCarrierV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxDataV27 .text 0x0a3980, 314 bytes.
 *
 * Drain the FIFO through `FIFO_read`, scramble, modulate.
 *
 * THE ONE-TIME ENTRY STATUS.  `V27TXP_COUNTDOWN` is nonzero exactly once, on
 * the call right after the SCR1->DATA transition -- SCR1's own arm seeds it
 * to 1 and never overwrites it with a table lookup the way every other arm
 * does -- so this is the only handler that reads it as anything but a
 * countdown, and it clears the field immediately after.
 *
 * `FIFO_read` NEVER RETURNS MORE THAN IT IS ASKED FOR (`faxfifo.h`'s own
 * contract), and this function asks for exactly `*budget`, so the object's
 * `got > *budget` branch is UNREACHABLE from `V27TX_modem`'s own loop --
 * `TxHdxDataV29`'s own `V29TXP_INT_0008` arm, one modulation over, and
 * `t_v29txcreate.c`'s own idiom for reaching it (`TxHdxDataV27` called
 * directly with `V27TXP_INT_0008` poked non-zero) is the only way in.
 */
short
TxHdxDataV27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short taken;
	short got;
	short r;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_DATA;

	if (prm->countdown != 0) {
		short rate = prm->rate;

		prm->countdown = 0;

		if (rate == 0)
			((struct v27_tx *)modem)->result.byte.status =
				V27TX_STATUS_ENTER_DATA_2400;
		else if (rate == 1)
			((struct v27_tx *)modem)->result.byte.status =
				V27TX_STATUS_ENTER_DATA_4800;
		else
			((struct v27_tx *)modem)->result.byte.status =
				V27TX_STATUS_DEFAULT;
	}

	taken = *budget;
	got = (short)FIFO_read((struct fax_fifo *)
					prm->fifo,
			       in, (unsigned short)taken);

	if (*budget <= got) {
		ScrambleDataV27(modem, in, got);
		r = (short)ModDataV27(modem, in, out, (unsigned short)got);
		*budget = (short)(*budget - got);
		return r;
	}

	/* Underrun: FIFO_read returned fewer than asked for. */
	if (prm->int_0008 != 0) {
		short remaining = (short)(*budget - got);

		*budget = remaining;
		ScrambleDataV27(modem, in, got);
		r = (short)ModDataV27(modem, in, out, (unsigned short)got);
		TxNextStateV27(modem);
		return r;
	}

	((struct v27_tx *)modem)->result.byte.flags |= V27TX_RESULT_B1_BIT1;
	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_UNDERRUN;
	taken = *budget;
	ScrambleDataV27(modem, in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxSCR1V27 .text 0x0a3ac0, 206 bytes.
 *
 * `SGD_symbol_gen`, `ScrambleDataV27`, `ModDataV27` -- the scrambled-1s
 * training pattern.  `TxNextStateV27`'s EQCOND arm seeds
 * `V27TXP_COUNTDOWN` to 8 for this handler's first calls and its OWN arm
 * (reached when that countdown hits zero) reseeds it to 1 and installs
 * `TxHdxDataV27`, so this handler's own countdown-exhausted transition is
 * what carries the machine into DATA.
 */
short
TxHdxSCR1V27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short countdown;
	short taken;
	short r;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_TRAINING;

	countdown = prm->countdown;
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	prm->countdown = (short)(countdown - taken);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, taken);
	ScrambleDataV27(modem, in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);

	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxEQCondV27 .text 0x0a3b90, 264 bytes.
 *
 * Equaliser conditioning: fill `in[0..taken)` with the literal 7, scramble
 * the whole run, then walk it choosing `V27TX_PATTERN_ALT[rate]` or
 * `V27TX_PATTERN_CARR[rate]` per element from bit 2 of the FOLLOWING
 * scrambled element -- `in[taken]`, one element past what was filled, on the
 * loop's last iteration.  Reproduced as `in[i + 1] & 0x04`, a full-word test
 * rather than the object's byte test on `((unsigned char *)&in[i+1])[0]`;
 * x86 is little-endian, so the two are the same value for every `in[i+1]`.
 */
short
TxHdxEQCondV27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short countdown;
	short taken;
	short r;
	short i;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_TRAINING;

	countdown = prm->countdown;
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	prm->countdown = (short)(countdown - taken);

	for (i = 0; i < taken; i++)
		in[i] = 7;

	ScrambleDataV27(modem, in, taken);

	if (taken != 0) {
		short rate;

		prm = ((struct v27_tx *)modem)->source;
		rate = prm->rate;

		for (i = 0; i < taken; i++) {
			if (in[i + 1] & 0x04)
				in[i] = (unsigned short)V27TX_PATTERN_ALT[rate];
			else
				in[i] = (unsigned short)
					V27TX_PATTERN_CARR[rate];
		}
	}

	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxAltV27 .text 0x0a3ca0, 190 bytes.
 *
 * `SGD_symbol_gen` fills `in` with `taken` symbols using the pattern
 * `TxNextStateV27`'s NOCARR arm just installed into the SGD, then
 * `ModDataV27` modulates them -- `TxHdxQuietV27`'s shape with the source
 * swapped for a real pattern.
 */
short
TxHdxAltV27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short countdown;
	short taken;
	short r;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_TRAINING;

	countdown = prm->countdown;
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	prm->countdown = (short)(countdown - taken);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, taken);
	r = (short)ModDataV27(modem, in, out, (unsigned short)taken);

	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxQuietV27 .text 0x0a3d60, 160 bytes.
 *
 * Spend `min(V27TXP_COUNTDOWN, *budget)` on `TxNoCarrierV27`, or transition
 * once the countdown reaches zero.  `TxHdxAltV27` is the identical shape
 * over `SGD_symbol_gen`+`ModDataV27` instead.
 */
short
TxHdxQuietV27(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v27_tx_source *prm = ((struct v27_tx *)modem)->source;
	short countdown;
	short taken;
	short r;

	((struct v27_tx *)modem)->result.byte.status = V27TX_STATUS_TRAINING;

	countdown = prm->countdown;
	if (countdown <= 0) {
		TxNextStateV27(modem);
		return 0;
	}

	taken = (short)((countdown <= *budget) ? countdown : *budget);
	prm->countdown = (short)(countdown - taken);

	r = TxNoCarrierV27(modem, in, out, (unsigned short)taken);
	*budget = (short)(*budget - taken);
	return r;
}

/*
 * TxHdxStartV27 .text 0x0a3e10, 21 bytes.  Nothing but the transition.
 */
short
TxHdxStartV27(void *modem, unsigned short *in, short *out, short *budget)
{
	(void)in;
	(void)out;
	(void)budget;
	TxNextStateV27(modem);
	return 0;
}
