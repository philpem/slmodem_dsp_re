/*
 * V17t_prc.c -- split out of the merged v17.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies
 * moved verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/v17fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17cfg.h"
#include "dsplib/v17dec.h"
#include "dsplib/v32smc.h"
#include "dsplib/vtb.h"

#define RXROOT(modem)		((struct v17rx *)(modem))
#define TXROOT(modem)		((struct v17tx *)(modem))
#define RXCTL(modem)		(RXROOT(modem)->ctl)
#define RXSTATE(modem)		(RXROOT(modem)->state)
#define TXPRIV(modem)		(TXROOT(modem)->priv)
#define TXBLOCK(modem)		(TXROOT(modem)->fp)
#define CTL(modem)		RXCTL(modem)
#define RXS(modem)		RXSTATE(modem)
#define TXP(modem)		TXPRIV(modem)
#define TXFP(modem)		TXBLOCK(modem)

/*
 * V17TX_modem -- .text 0x0a0e40, 182 bytes.  See v17fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 */
int
V17TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	struct v17tx_priv *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = TXPRIV(modem);

	TXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17TX_RESULT_B1_BIT1;

	if (prm->r08 == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					prm->fifo,
				in, *count);
	else
		taken = *count;

	budget = V17TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = TXPRIV(modem);
		got = prm->process(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns, which is what the object encodes
		 * (`movb $0x9,0x20(%edi)`) and is why it cannot be written
		 * through `AT_I`.
		 */
		TXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_09;
	}

	*count = (unsigned short)total;

	return TXROOT(modem)->result.word;
}

/*
 * ---------------------------------------------------------------------------
 * TxNextStateV17 -- .text 0x0a0f00, 1,439 bytes.
 *
 * `jmp *table(,%eax,4)` on `V17TXP_STATE`, bounded `cmp $0xb`/`ja default`
 * -- a real jump table, twelve entries, `V17TX_STATE_START`..
 * `V17TX_STATE_IDLE`.  The case bodies and the state names are v17fax.h's
 * own derivation (rank 1, the twelve debug strings), reproduced here exactly
 * as `TxNextStateV29` reproduces its own seven.
 *
 * `TxHdxSCR1V17` IS INSTALLED BY TWO ARMS (`BRIDGE` and `DATA`) AND
 * `TxHdxSilenceV17` BY THREE (`START`, `TEP` and `SCR1_END`) -- neither
 * handler looks at which state led to it; both read only the SGD
 * configuration and the countdown this function seeds beside them.  See
 * v17fax.h's own state table for the full installs-what-next-STATE listing.
 *
 * THREE OF THE TWELVE ARMS RETURN DIRECTLY RATHER THAN FALLING TO THE SHARED
 * TAIL -- `SCR1`, `DATA` and `SCR1_END` clear `V17TX_OBJ_RESULT_B2`'s bit 0
 * and SET `V17TX_OBJ_RESULT_B1`'s bit 0 (`V17TX_RESULT_B1_BIT0`) with their
 * own inline code and `ret` directly; every other arm clears BOTH bits
 * through the tail every `break` reaches at the bottom of this function.
 * `QUIET_END` is the exception that still `break`s: it explicitly SETS
 * `V17TX_OBJ_RESULT_B2`'s bit and clears `V17TX_OBJ_RESULT_B1`'s -- which the
 * shared tail's own unconditional `&= ~V17TX_RESULT_B1_BIT0` reproduces for
 * free, so nothing extra is needed there.
 *
 * `V17TXP_INT_000C`'s TWO READERS ARE `ALT` (the training BUDGET: 0x26
 * against 0xba0) AND `EQCOND` (whether `BRIDGE` is installed on the way to
 * `SCR1`, or skipped) -- see the field's own comment in v17fax.h.
 *
 * `req.det` IS `SGD_CTL.det`, READ BACK OUT OF THE GLOBAL RATHER THAN
 * HARDCODED NULL, the shape F9700 established for this file's whole family
 * -- seven of the thirteen sites sgd.h's own comment counts.
 *
 * THE WRONG-COPY RITUAL (F134).  Swapping which handler `BRIDGE`'s own arm
 * installs (`TxHdxSCR1V17` for `TxHdxDataV17`) failed
 * `t_v17txcreate.c`'s `test_tx_cycle` immediately -- the DATA state was never
 * entered and the FIFO-fed cycle stalled in BRIDGE/SCR1.  Reverted; `make one
 * T=t_v17txcreate` green again.
 *
 * Finding F9912.
 */
void
TxNextStateV17(void *modem)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short state = prm->state;

	switch (state) {
	case V17TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_START\n");
		prm->countdown = 0x30;
		prm->process =
			TxHdxSilenceV17;
		prm->state = V17TX_STATE_SILENCE;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_SILENCE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SILENCE\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		SetEncoderV17(modem, 1, 0);
		prm = TXPRIV(modem);
		prm->countdown = 0x1e0;
		prm->process =
			TxHdxTEP_V17;
		prm->state = V17TX_STATE_TEP;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_TEP:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_TEP\n");
		prm->countdown = 0x30;
		prm->process =
			TxHdxSilenceV17;
		prm->state = V17TX_STATE_QUIET;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_QUIET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_QUIET\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0xe;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		SetEncoderV17(modem, 1, 0);
		prm = TXPRIV(modem);
		prm->countdown = 0x100;
		prm->process =
			TxHdxABV17;
		prm->state = V17TX_STATE_ALT;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_ALT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_ALT\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0xf;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = TXPRIV(modem);
		prm->countdown = (short)
			((prm->r0c != 0) ? 0x26 : 0xba0);
		prm->process =
			TxHdxEQCondV17;
		prm->state = V17TX_STATE_EQCOND;
		prm->r1c = 0;
		SeedScramblerV17(modem, 0x2ecdd5);
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_EQCOND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_EQCOND\n");
		if (prm->r0c != 0) {
			short mode = prm->mode;
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			SetTxModeV17(modem, mode);

			prm = TXPRIV(modem);
			mode = prm->mode;
			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);

			SetEncoderV17(modem, 2, 3);
			prm = TXPRIV(modem);
			prm->countdown = 0x30;
			prm->process = TxHdxSCR1V17;
			prm->state = V17TX_STATE_SCR1;
		} else {
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0x111;
			gen.word_syms = 8;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);

			SetEncoderV17(modem, 0, 3);
			prm = TXPRIV(modem);
			prm->countdown = 0x40;
			prm->process = TxHdxBridgeV17;
			prm->state = V17TX_STATE_BRIDGE;
		}
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_BRIDGE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_BRIDGE\n");
		{
			short mode = prm->mode;
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			SetTxModeV17(modem, mode);

			prm = TXPRIV(modem);
			mode = prm->mode;
			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		SetEncoderV17(modem, 2, 0);
		prm = TXPRIV(modem);
		prm->countdown = 0x30;
		prm->process =
			TxHdxSCR1V17;
		prm->state = V17TX_STATE_SCR1;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_SCR1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SCR1\n");
		prm->countdown = 1;
		prm->process =
			TxHdxDataV17;
		prm->state = V17TX_STATE_DATA;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_DATA\n");
		{
			short mode = prm->mode;
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word =
				(unsigned short)V17TX_PATTERN_SCR1[mode];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		prm = TXPRIV(modem);
		prm->countdown = 0x20;
		prm->process =
			TxHdxSCR1V17;
		prm->state = V17TX_STATE_SCR1_END;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_SCR1_END:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_SCR1_END\n");
		prm->countdown = 0x30;
		prm->process =
			TxHdxSilenceV17;
		prm->state = V17TX_STATE_QUIET_END;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT0;
		return;

	case V17TX_STATE_QUIET_END:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_QUIET_END\n");
		prm->countdown = 0;
		prm->process =
			TxHdxIdleV17;
		prm->state = V17TX_STATE_IDLE;
		prm->r08 = 1;
		TXROOT(modem)->result.byte.flags2 |= V17TX_RESULT_B2_BIT0;
		break;

	case V17TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_STATE_IDLE\n");
		prm->countdown = 0;
		prm->process =
			TxHdxStartV17;
		prm->state = V17TX_STATE_START;
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17TX_DEFAULT, %d\n", state);
		TXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17TX_RESULT_B2_BIT0;
		TXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_07;
		TXROOT(modem)->result.byte.flags = (unsigned char)
			((TXROOT(modem)->result.byte.flags
			  | V17TX_RESULT_B1_BIT1)
			 & ~V17TX_RESULT_B1_BIT0);
		break;
	}

	TXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17TX_RESULT_B1_BIT0;
}

/*
 * TxHdxIdleV17 -- .text 0x0a14a0, 116 bytes.  Exactly `TxHdxIdleV21`'s and
 * `TxHdxIdleV29`'s shape: report `V17TX_STATUS_IDLE` unconditionally, then
 * spend the whole call's budget on `TxNoCarrierV17` while the FIFO is empty,
 * or hand off to `TxNextStateV17` the moment it is not.
 */
short
TxHdxIdleV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	struct fax_fifo *fifo =
		(struct fax_fifo *)prm->fifo;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_IDLE;

	if (fifo->count == 0) {
		unsigned short b = (unsigned short)*budget;
		short nsamples = (short)TxNoCarrierV17(modem, in, out, b);

		*budget = (short)((unsigned short)*budget - b);
		return nsamples;
	}

	TxNextStateV17(modem);
	return 0;
}

/*
 * TxHdxDataV17 -- .text 0x0a1520, 349 bytes.  `TxHdxDataV21`'s and
 * `TxHdxDataV29`'s shape one modulation over: an optional one-shot rate
 * report the first call after `TxHdxSCR1V17` installs it (`V17TXP_SHORT_001A`
 * seeded to 1), then the three-arm FIFO read -- satisfied and
 * underrun-with-`V17TXP_INT_0008`-clear both scramble+modulate exactly what
 * was taken (the underrun arm takes the FULL REQUESTED BUDGET rather than
 * what the FIFO gave, raising `V17TX_RESULT_B1_BIT1` and reporting
 * `V17TX_STATUS_UNDERRUN`); underrun-with-`V17TXP_INT_0008`-set modulates
 * only what the FIFO gave, LEAVES the remainder in `*budget`, and calls
 * `TxNextStateV17` before returning.
 */
short
TxHdxDataV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	unsigned short req;
	unsigned short taken;
	short nsamples;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_DATA;

	if (prm->countdown != 0) {
		short mode = prm->mode;

		prm->countdown = 0;

		if (mode == 1)
			TXROOT(modem)->result.byte.status =
				V17TX_STATUS_DATA_RATE_9600;
		else if (mode == 0)
			TXROOT(modem)->result.byte.status =
				V17TX_STATUS_DATA_RATE_7200;
		else if (mode == 2)
			TXROOT(modem)->result.byte.status =
				V17TX_STATUS_DATA_RATE_12000;
		else if (mode == 3)
			TXROOT(modem)->result.byte.status =
				V17TX_STATUS_DATA_RATE_14400;
		else
			TXROOT(modem)->result.byte.status = V17TX_RESULT_BYTE_07;
	}

	req = (unsigned short)*budget;
	taken = (unsigned short)
		FIFO_read((struct fax_fifo *)prm->fifo,
			  in, req);

	if (req <= taken) {
		ScrambleDataV17(modem, in, taken);
		nsamples = (short)ModDataV17(modem, in, out, taken);
		*budget = (short)((unsigned short)*budget - taken);
		return nsamples;
	}

	if (prm->r08 != 0) {
		*budget = (short)(req - taken);
		ScrambleDataV17(modem, in, taken);
		nsamples = (short)ModDataV17(modem, in, out, taken);
		TxNextStateV17(modem);
		return nsamples;
	}

	TXROOT(modem)->result.byte.flags |= V17TX_RESULT_B1_BIT1;
	TXROOT(modem)->result.byte.status = V17TX_STATUS_UNDERRUN;
	ScrambleDataV17(modem, in, req);
	nsamples = (short)ModDataV17(modem, in, out, req);
	*budget = (short)((unsigned short)*budget - req);

	return nsamples;
}

/*
 * See `TxHdxEQCondV17`'s own comment: the same body, a different symbol --
 * installed both by `BRIDGE` (pre-data) and by `DATA` (post-data), which is
 * `V17TX_STATE_SCR1`'s own comment in v17fax.h.
 */
short
TxHdxSCR1V17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_TRAINING;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

short
TxHdxBridgeV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_TRAINING;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxEQCondV17 -- .text 0x0a1820, 206 bytes.
 * TxHdxBridgeV17 -- .text 0x0a1750, 206 bytes.
 * TxHdxSCR1V17   -- .text 0x0a1680, 206 bytes.
 *
 * THE THREE ARE ONE BODY, COMPILED THREE TIMES -- byte for byte the same
 * instruction sequence at all three addresses (`dis.py` over each range),
 * the same shape `RxHdxBridgeV17`/`RxHdxPrtcolV17` already carry on the
 * receive side of this file: `SGD_symbol_gen` then `ScrambleDataV17` then
 * `ModDataV17`, driven by whichever `SGD_control` request the installing
 * arm of `TxNextStateV17` built immediately before.  What differs between
 * the three transitions is upstream, in `TxNextStateV17` itself; the
 * handler code does not look at which state led to it.
 */
short
TxHdxEQCondV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_TRAINING;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	ScrambleDataV17(modem, in, n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxABV17 -- .text 0x0a18f0, 190 bytes.  Function name AB, debug string
 * ALT -- the mismatch `TxHdxABV29`/`V29TX_STATE_ALT` already carries, one
 * modulation over.  `SGD_symbol_gen` (word_syms=2, data_word=0xe, set by
 * `TxNextStateV17`'s QUIET arm) straight into `ModDataV17`, no
 * `ScrambleDataV17` -- unlike `TxHdxSCR1V17` below, V.17's alternating
 * training dibit is unscrambled.
 */
short
TxHdxABV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	TXROOT(modem)->result.byte.status = V17TX_STATUS_TRAINING;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxTEP_V17 -- .text 0x0a19b0, 179 bytes.  Installed by `SILENCE`;
 * `SGD_symbol_gen` (word_syms=2, data_word=0, set by `TxNextStateV17`'s
 * SILENCE arm) straight into `ModDataV17` -- no `ScrambleDataV17`, and see
 * `TxHdxSilenceV17`'s own comment for the missing status write.
 */
short
TxHdxTEP_V17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	nsamples = (short)ModDataV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxSilenceV17 -- .text 0x0a1a70, 158 bytes.  Installed by `START`, `TEP`
 * and `SCR1_END` alike (see `TxNextStateV17`'s own comment); every call
 * spends `min(remaining, *budget)` on `TxNoCarrierV17` and counts the
 * countdown down towards `TxNextStateV17`.
 *
 * IT DOES NOT WRITE `V17TX_OBJ_RESULT`, and neither does `TxHdxTEP_V17` --
 * MEASURED, not an omission: no `movb`/`orb`/`andb` touches the byte
 * anywhere in either function's disassembly, where every OTHER countdown
 * handler in this file (`TxHdxABV17`, `TxHdxEQCondV17`, `TxHdxBridgeV17`,
 * `TxHdxSCR1V17`) writes `V17TX_STATUS_TRAINING` on entry.  `TxHdxQuietV29`,
 * this file's closest V.29 analogue, writes its own status even on its
 * no-carrier arm -- so this is a genuine divergence from the sibling shape
 * and not a transcription slip.
 */
short
TxHdxSilenceV17(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v17tx_priv *prm = TXPRIV(modem);
	short remaining;
	unsigned short n;
	short nsamples;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV17(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	nsamples = (short)TxNoCarrierV17(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxStartV17 -- .text 0x0a1b10, 21 bytes.  Nothing but the transition.
 */
short
TxHdxStartV17(void *modem, unsigned short *in, short *out, short *budget)
{
	TxNextStateV17(modem);
	return 0;
}
