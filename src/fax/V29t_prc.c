/*
 * V29t_prc.c -- split out of the merged v29.c / v29data.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <stddef.h>
#include <string.h>

#include "dsplib/v29fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"

/*
 * ---------------------------------------------------------------------------
 * V29TX_modem -- .text 0x0a46b0, 182 bytes.  See v29fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 *
 * IT IS `V17TX_modem` AND `V21TX_modem` FOR A THIRD MODULATION, and the
 * object says so: all three are 182 bytes and the instruction sequences differ
 * in four immediates and nothing else -- the gate at params + 0x08, the
 * dispatch slot at + 0x10, the budget 0x30 and the status byte 7.  V.21's are
 * + 0x04, + 0x08, 6 and 4; V.17's are + 0x08, + 0x14, 0x30 and 9.  Finding
 * F9253.
 *
 * The parameter block is read ONCE before the gate and re-read at the top of
 * every loop iteration; the object hoists the first iteration's read out of
 * the loop (0x0a46bf and 0x0a475e reach the head at 0x0a46ea, and the back
 * edge at 0x0a46e7 reloads), which is loop rotation of exactly this source.
 */
int
V29TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	struct v29_tx_params *prm;
	int result;
	unsigned short taken;
	short budget;
	short total;

	prm = ((struct v29_tx_root *)modem)->params;

	((struct v29_tx_root *)modem)->result.byte.flags &=
		(unsigned char)~V29TX_RESULT_B1_BIT1;

	if (prm->int_0008 == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					prm->fifo,
				in, *count);
	else
		taken = *count;

	budget = V29TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = ((struct v29_tx_root *)modem)->params;
		got = prm->handler
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		((struct v29_tx_root *)modem)->result.byte.flags |=
			V29TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns -- `movb $0x7,0x1c(%edi)` at 0x0a4724 -- which is
		 * why it cannot be written through `FIELD_INT`.
		 */
		((struct v29_tx_root *)modem)->result.byte.status = V29TX_RESULT_BYTE_07;
	}

	*count = (unsigned short)total;
	/*
	 * The result is a raw int field in the wrapped object.  Copying its
	 * representation avoids a typed-alias read and keeps the count store
	 * ahead of the result load, as in the original object.
	 */
	memcpy(&result, &((struct v29_tx_root *)modem)->result.word, sizeof result);

	return result;
}

/*
 * GenEQTrnSequenceV29  .text 0x0a4770  99 bytes
 *
 * The V.29 equaliser-training generator: a 7-bit LFSR whose feedback is
 * bit0 XOR bit1, planted at bit 7 BEFORE the shift so it lands at bit 6
 * after -- the object computes `((sr << 6) & 0x80) ^ ((sr & 1) << 7)`, ORs
 * it in, shifts right once and masks to 7 bits.  A 1 bit answers
 * constellation index 0xb, a 0 bit answers 0.
 *
 * The register is a short in a block this tree has not modelled (see
 * V29TX_OBJ_SCRAM in v29data.h); it is read once, stepped n times and
 * written back once, exactly as the object does.
 */
void
GenEQTrnSequenceV29(void *modem, unsigned short *out, unsigned short n)
{
	void *blk = *(void **)(void *)((unsigned char *)modem
				       + V29TX_OBJ_SCRAM);
	short *srp = (short *)(void *)((unsigned char *)blk + V29SCRAM_SR);
	int sr = *srp;
	unsigned short i;

	for (i = n; i != 0; i--) {
		int lsb = sr & 1;
		int fb = ((sr << 6) & 0x80) ^ (lsb << 7);

		sr = (int)(((unsigned)(sr | fb) >> 1) & 0x7f);
		*out++ = (unsigned short)(lsb ? 0xb : 0);
	}
	*srp = (short)sr;
}

/*
 * ---------------------------------------------------------------------------
 * TxNextStateV29 -- .text 0x0a47e0, 698 bytes.
 *
 * `jmp *0xc370(,%eax,4)` on `V29TXP_STATE`, bounded `cmp $0x6` / `ja
 * default` -- a real jump table, not a compare chain, unlike `TxNextStateV21`.
 * The seven case bodies are read off `tools/dis.py`; the STATE NAMES are the
 * author's own, from seven debug strings at `.rodata.str1.1` 0x4d67..0x4de8
 * (`tools/relocscan.py --at` cannot pair a jump-table entry with its target,
 * so the printed name inside each arm is what settles which index is which
 * state -- CLAUDE.md's evidence rank 1), each naming the state being LEFT,
 * exactly as `TxNextStateV21`'s four do:
 *
 *   V29TX_STATE_START    installs TxHdxQuietV29, budget 0x30
 *   V29TX_STATE_QUIET    SGD_control(gen={0x4f,2 syms}), SetEncoderV29(1),
 *                        installs TxHdxABV29, budget 0x80
 *   V29TX_STATE_ALT      installs TxHdxEQCondV29, budget 0x180, reseeds
 *                        V29SCRAM_SR to 0x2a
 *   V29TX_STATE_EQCOND   SGD_control(gen={V29TX_PATTERN_SCR1[rate],1 sym}),
 *                        SeedScramblerV29(0), SetEncoderV29(0), installs
 *                        TxHdxSCR1V29, budget 0x30
 *   V29TX_STATE_SCR1     installs TxHdxDataV29, budget 1
 *   V29TX_STATE_DATA     installs TxHdxIdleV29, budget 0
 *   V29TX_STATE_IDLE     installs TxHdxStartV29, budget 0 -- wraps to START
 *
 * EVERY ARM'S PRINTED STRING NAMES THE INDEX IT RUNS UNDER, AND EVERY ONE
 * INSTALLS THE HANDLER FOR THE STATE IT ADVANCES TO -- the same
 * self-consistency check `V29RX_create`'s jump table used (finding F9320).
 *
 * THE FUNCTION NAME `TxHdxABV29` AND THE STATE NAME `V29TX_STATE_ALT` ARE
 * BOTH THE AUTHOR'S AND DISAGREE.  Not reconciled; see V29TX_STATE_ALT's own
 * comment.
 *
 * B1_BIT0/B2_BIT0's OWN VALUES PER ARM (V29TX_RESULT_B1_BIT0/_B2_BIT0,
 * neutral): every arm except SCR1 and DATA clears both; SCR1 sets B1_BIT0
 * (clears B2_BIT0); DATA sets B2_BIT0 (clears B1_BIT0).  The default arm
 * clears B2_BIT0, sets B1_BIT1 and clears B1_BIT0, and writes
 * V29TX_STATUS_DEFAULT -- the only arm that touches the status byte at all.
 *
 * THE SGD REQUESTS' `gen` HALF IS ONLY PARTLY INITIALISED, AND THAT IS THE
 * OBJECT'S: neither call site (QUIET's own transition or EQCOND's) writes
 * `gen.seq`/`seq_len`/`short_0006`/`seq_enable` -- only `data_word` and
 * `word_syms`, the two fields `SGD_symbol_gen` reads, which is all either
 * caller needs since neither drives `SGD_sequence_gen`.  Deviation D1291.
 *
 * `req.det` IS `SGD_CTL.det`, READ BACK OUT OF THE GLOBAL RATHER THAN
 * HARDCODED NULL -- the same shape F9700 established for V.17/V.27ter's
 * own sites, and this file's two of the thirteen.
 *
 * THE WRONG-COPY RITUAL (F134).  Swapping `TxHdxDataV29` and
 * `TxHdxIdleV29` in the SCR1 and DATA arms' installs (so SCR1 installs
 * Idle and DATA installs Data-again) failed `t_v29txcreate.c`'s
 * `run_tx_cycle()` immediately -- the DATA state's own probe never saw the
 * FIFO drained the way the reference build did, and the process-slot
 * comparison at the SCR1->DATA transition diverged first.  Reverted;
 * `make one T=t_v29txcreate` green again.  See the finding for the exact
 * failure text.  Finding F9701.
 */
void
TxNextStateV29(void *modem)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	short state = prm->state;

	switch (state) {
	case V29TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_START\n");
		prm->countdown = 0x30;
		prm->handler =
			TxHdxQuietV29;
		prm->state = V29TX_STATE_QUIET;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		break;

	case V29TX_STATE_QUIET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_QUIET\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = 0x4f;
			gen.word_syms = 2;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		SetEncoderV29(modem, 1);
		prm = ((struct v29_tx_root *)modem)->params;
		prm->countdown = 0x80;
		prm->handler =
			TxHdxABV29;
		prm->state = V29TX_STATE_ALT;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		break;

	case V29TX_STATE_ALT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_ALT\n");
		prm->countdown = 0x180;
		prm->handler =
			TxHdxEQCondV29;
		prm->state = V29TX_STATE_EQCOND;
		prm->scram_sr = 0x2a;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		break;

	case V29TX_STATE_EQCOND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_EQCOND\n");
		{
			struct sgd_gen_cfg gen;
			struct sgd_control_req req;

			gen.data_word = (unsigned short)
				V29TX_PATTERN_SCR1[prm->rate];
			gen.word_syms = 1;
			req.gen = &gen;
			req.det = SGD_CTL.det;
			SGD_control((struct sgd *)
					prm->sgd, &req);
		}
		SeedScramblerV29(modem, 0);
		SetEncoderV29(modem, 0);
		prm = ((struct v29_tx_root *)modem)->params;
		prm->countdown = 0x30;
		prm->handler =
			TxHdxSCR1V29;
		prm->state = V29TX_STATE_SCR1;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		break;

	case V29TX_STATE_SCR1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_SCR1\n");
		prm->countdown = 1;
		prm->handler =
			TxHdxDataV29;
		prm->state = V29TX_STATE_DATA;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		((struct v29_tx_root *)modem)->result.byte.flags |= V29TX_RESULT_B1_BIT0;
		return;

	case V29TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_DATA\n");
		prm->countdown = 0;
		prm->handler =
			TxHdxIdleV29;
		prm->state = V29TX_STATE_IDLE;
		((struct v29_tx_root *)modem)->result.byte.flags2 |= V29TX_RESULT_B2_BIT0;
		((struct v29_tx_root *)modem)->result.byte.flags &=
			(unsigned char)~V29TX_RESULT_B1_BIT0;
		return;

	case V29TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_STATE_IDLE\n");
		prm->countdown = 0;
		prm->handler =
			TxHdxStartV29;
		prm->state = V29TX_STATE_START;
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29TX_DEFAULT, %d\n", state);
		((struct v29_tx_root *)modem)->result.byte.flags2 &=
			(unsigned char)~V29TX_RESULT_B2_BIT0;
		((struct v29_tx_root *)modem)->result.byte.status = V29TX_STATUS_DEFAULT;
		((struct v29_tx_root *)modem)->result.byte.flags = (unsigned char)
			((((struct v29_tx_root *)modem)->result.byte.flags
			  | V29TX_RESULT_B1_BIT1)
			 & ~V29TX_RESULT_B1_BIT0);
		break;
	}

	((struct v29_tx_root *)modem)->result.byte.flags &=
		(unsigned char)~V29TX_RESULT_B1_BIT0;
}

/*
 * TxHdxIdleV29 -- .text 0x0a4aa0, 116 bytes.
 *
 * NO V29TXP_SHORT_0016 COUNTDOWN, unlike its four siblings below -- with the
 * FIFO empty it spends the WHOLE current budget on TxNoCarrierV29 in one
 * call; with the FIFO non-empty it does not modulate at all, just calls
 * TxNextStateV29.  Exactly TxHdxIdleV21's shape.
 *
 * V29TX_STATUS_IDLE (4) is written UNCONDITIONALLY at entry -- even on the
 * arm that immediately hands off to TxNextStateV29, which the object does
 * not override afterward.
 */
short
TxHdxIdleV29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	struct fax_fifo *fifo =
		(struct fax_fifo *)prm->fifo;

	((struct v29_tx_root *)modem)->result.byte.status = V29TX_STATUS_IDLE;

	if (fifo->count == 0) {
		unsigned short b = (unsigned short)*budget;
		short nsamples = (short)TxNoCarrierV29(modem, in, out, b);

		*budget = (short)((unsigned short)*budget - b);
		return nsamples;
	}

	TxNextStateV29(modem);
	return 0;
}

/*
 * TxHdxDataV29 -- .text 0x0a4b20, 314 bytes.
 *
 * ON THE ONE-SHOT CALL AFTER SCR1 INSTALLS IT (V29TXP_SHORT_0016 != 0,
 * which SCR1's own installer seeds to 1), reports a status keyed on
 * V29TXP_RATE and clears the field so no later call repeats it --
 * V29TX_STATUS_DATA_RATE_9600/_7200 when the field is 1 or 0, and
 * V29TX_STATUS_DEFAULT for anything else.
 *
 * THREE ARMS ON THE FIFO READ, TxHdxDataV21's shape one modulation over:
 * satisfied (the FIFO supplied the whole request) and underrun with
 * V29TXP_INT_0008 clear both scramble+modulate exactly what was taken (the
 * underrun arm takes the FULL REQUESTED BUDGET rather than what the FIFO
 * gave, raising V29TX_RESULT_B1_BIT1 and reporting V29TX_STATUS_UNDERRUN);
 * underrun with V29TXP_INT_0008 set modulates only what the FIFO gave,
 * LEAVES the remainder in `*budget`, and calls TxNextStateV29 before
 * returning.
 */
short
TxHdxDataV29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	unsigned short req;
	unsigned short taken;
	short nsamples;

	((struct v29_tx_root *)modem)->result.byte.status = 0;

	if (prm->countdown != 0) {
		short which = prm->rate;

		prm->countdown = 0;

		if (which == V29_RATE_7200)
			((struct v29_tx_root *)modem)->result.byte.status =
				V29TX_STATUS_DATA_RATE_7200;
		else if (which == V29_RATE_9600)
			((struct v29_tx_root *)modem)->result.byte.status =
				V29TX_STATUS_DATA_RATE_9600;
		else
			((struct v29_tx_root *)modem)->result.byte.status =
				V29TX_STATUS_DEFAULT;
	}

	req = (unsigned short)*budget;
	taken = (unsigned short)
		FIFO_read((struct fax_fifo *)prm->fifo,
			  in, req);

	if (req <= taken) {
		ScrambleDataV29(modem, in, taken);
		nsamples = (short)ModDataV29(modem, in, out, taken);
		*budget = (short)((unsigned short)*budget - taken);
		return nsamples;
	}

	if (prm->int_0008 != 0) {
		*budget = (short)(req - taken);
		ScrambleDataV29(modem, in, taken);
		nsamples = (short)ModDataV29(modem, in, out, taken);
		TxNextStateV29(modem);
		return nsamples;
	}

	((struct v29_tx_root *)modem)->result.byte.flags |= V29TX_RESULT_B1_BIT1;
	((struct v29_tx_root *)modem)->result.byte.status = V29TX_STATUS_UNDERRUN;
	ScrambleDataV29(modem, in, req);
	nsamples = (short)ModDataV29(modem, in, out, req);
	*budget = (short)((unsigned short)*budget - req);

	return nsamples;
}

/*
 * TxHdxSCR1V29 -- .text 0x0a4c60, 206 bytes.
 *
 * The first state to scramble: SGD_symbol_gen's data-word form
 * (word_syms=1, data_word=V29TX_PATTERN_SCR1[rate], set by
 * TxNextStateV29's EQCOND arm) THEN ScrambleDataV29, in place, before
 * ModDataV29.
 */
short
TxHdxSCR1V29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	short remaining;
	unsigned short n;
	short nsamples;

	((struct v29_tx_root *)modem)->result.byte.status = 1;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV29(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	ScrambleDataV29(modem, in, n);
	nsamples = (short)ModDataV29(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxEQCondV29 -- .text 0x0a4d30, 237 bytes.
 *
 * THE LFSR RECURRENCE IS GenEQTrnSequenceV29's OWN (v29data.h), WRITTEN OUT
 * AGAIN RATHER THAN CALLED -- no `call` in this whole loop, confirmed with
 * `dis.py`.  GenEQTrnSequenceV29 has no internal referrer anywhere in the
 * object (CLAUDE.md's fax-scope note), so the two are independent copies of
 * one algorithm and not caller and callee.  The register this copy reaches
 * through is V29SCRAM_SR at the SAME offset GenEQTrnSequenceV29's own
 * accessor uses -- V29TX_OBJ_SCRAM and V29TX_OBJ_PARAMS are one block, both
 * 0x20, confirmed by this function reaching V29SCRAM_SR (0x18) through the
 * SAME pointer TxNextStateV29 reaches V29TXP_STATE (0x14) through.
 *
 * UNLIKE THE OTHER THREE V29TXP_SHORT_0016 HANDLERS, this one always emits
 * V29_TRAIN_POINT_16 (11) or 0 -- never V29_TRAIN_POINT_8 -- so it is the
 * sixteen-point half of the training alternation only.
 */
short
TxHdxEQCondV29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	short remaining;
	unsigned short n;
	short reg;
	unsigned short i;
	short nsamples;

	((struct v29_tx_root *)modem)->result.byte.status = 1;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV29(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	reg = prm->scram_sr;
	for (i = 0; i < n; i++) {
		unsigned short x = (unsigned short)reg;
		int bit0 = x & 1;

		x = (unsigned short)((((x << 6) & 0x80) ^ (bit0 << 7)) | x);
		reg = (short)((x >> 1) & V29_TRAIN_LFSR_MASK);

		in[i] = (unsigned short)(bit0 ? V29_TRAIN_POINT_16 : 0);
	}
	prm->scram_sr = reg;

	nsamples = (short)ModDataV29(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxABV29 -- .text 0x0a4e20, 190 bytes.  Function name AB, debug string
 * ALT; see V29TX_STATE_ALT.
 *
 * Same V29TXP_SHORT_0016 shape as TxHdxQuietV29, but drives
 * SGD_symbol_gen's data-word form (word_syms=2, data_word=0x4f, set by
 * TxNextStateV29's QUIET arm) STRAIGHT INTO ModDataV29 -- no
 * ScrambleDataV29, unlike TxHdxSCR1V29 below.  V.29's alternating training
 * dibit is unscrambled.
 */
short
TxHdxABV29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	short remaining;
	unsigned short n;
	short nsamples;

	((struct v29_tx_root *)modem)->result.byte.status = 1;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV29(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	SGD_symbol_gen((struct sgd *)prm->sgd, in, (short)n);
	nsamples = (short)ModDataV29(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxQuietV29 -- .text 0x0a4ee0, 163 bytes.
 *
 * The V29TXP_SHORT_0016 countdown shape all four of QUIET/ALT/EQCOND/SCR1
 * share: report status 1, then TxNoCarrierV29 over min(remaining, *budget)
 * at a time until the countdown reaches zero, at which point call
 * TxNextStateV29 instead and report nothing.
 */
short
TxHdxQuietV29(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v29_tx_params *prm = ((struct v29_tx_root *)modem)->params;
	short remaining;
	unsigned short n;
	short nsamples;

	((struct v29_tx_root *)modem)->result.byte.status = 1;

	remaining = prm->countdown;
	if (remaining <= 0) {
		TxNextStateV29(modem);
		return 0;
	}

	n = (remaining <= (short)*budget) ? (unsigned short)remaining
					   : (unsigned short)*budget;
	prm->countdown = (short)(remaining - n);

	nsamples = (short)TxNoCarrierV29(modem, in, out, n);
	*budget = (short)((unsigned short)*budget - n);

	return nsamples;
}

/*
 * TxHdxStartV29 -- .text 0x0a4f90, 21 bytes.  Nothing but the transition.
 */
short
TxHdxStartV29(void *modem, unsigned short *in, short *out, short *budget)
{
	TxNextStateV29(modem);
	return 0;
}

/*
 * The offset assertions v29data.c carried, moved with their subjects: the
 * `struct v29tx` whose layout `TxNoCarrierV29` and `GenEQTrnSequenceV29`
 * walk.  See v29data.c's old note on the pointer-size guard.
 */
#if __SIZEOF_POINTER__ == 4
typedef char v29tx_size[(sizeof(struct v29tx) == 0x9c) ? 1 : -1];
typedef char v29tx_ring_at[(offsetof(struct v29tx, ring) == V29FP_SMC_RING) ? 1 : -1];
typedef char v29tx_smc_at[(offsetof(struct v29tx, smc) == V29FP_SMC) ? 1 : -1];
typedef char v29tx_pps_at[(offsetof(struct v29tx, pps) == V29FP_PPS) ? 1 : -1];
#endif
