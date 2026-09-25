/*
 * V32rxhdx.c -- ITU-T V.32 / V.32bis: the twelve half-duplex RECEIVE states.
 *
 *   V32RxHdxModem      .text 0x0838f0   12
 *   RxHdxTone          .text 0x083900  246
 *   RxHdxNoSignal      .text 0x083a00  223
 *   CalcTurnAroundDelay .text 0x083ae0  53
 *   RxHdxPhsReversal   .text 0x083b20  653
 *   RxHdxRateSequence  .text 0x083db0  258
 *   RxHdxSequence      .text 0x083ec0  222
 *   RxHdxSequenceE     .text 0x083fa0  339
 *   RxHdxData          .text 0x084100  139
 *   RxHdxToneData      .text 0x084190  234
 *   RxHdxSTone         .text 0x084280  258
 *   RxHdxEpoch         .text 0x084390  195
 *   RxHdxError         .text 0x084460   57
 *   RxHdxNull          .text 0x0844a0  103
 *
 * The functions are in the object's own address order, which is what
 * `docs/method/refinement.md` lever 1 asks for.  `CalcTurnAroundDelay`
 * (0x083ae0) sits between the second and the third and IS here: the object
 * inlines it into `RxHdxPhsReversal`, which is only possible within one
 * translation unit, so it is this unit's and `v32fpctl.c` no longer carries
 * it.
 *
 * `include/dsplib/v32hdx.h` is the contract these are written to and
 * `include/dsplib/v32hdxst.h` is the roster.  The receive driver does not
 * loop: one state, one call, and the fourth argument is an `unsigned short *`
 * read on entry and written on exit.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE ELEVEN OF THE TWELVE SHARE
 *
 * Every state but `RxHdxError` opens by charging one block's worth of symbols
 * against a counter and closes by posting a fault if the counter has reached
 * its bound:
 *
 *      hdx->timer += hdx->symbol_len;
 *      ... the state's own work, which may install a successor ...
 *      if (hdx->timer >= hdx->limit) {
 *              obj->flags |= V32_FLAG_FAULT;
 *              obj->status = <this state's reason code>;
 *              hdx->txstate = TxHdxNoCarrier;
 *              hdx->rxstate = RxHdxError;
 *              hdx->state   = V32_STATE_ERROR;
 *      }
 *      *count = RxClampV32(modem, in, out, *count);
 *
 * IT IS WRITTEN OUT TEN TIMES RATHER THAN FACTORED, because the object has it
 * inline ten times and a helper the compiler declined to inline would be a
 * symbol the blob does not have -- finding F605's per-function count measures
 * our factoring, not our completeness.
 *
 * ---------------------------------------------------------------------------
 * THE CONTEXT POINTER IS RE-READ AFTER EVERY CALL, AND THAT IS LOAD-BEARING
 *
 * `obj + V32_OBJ_HDX` is reloaded after every single call in all twelve --
 * after `FPM_AGC_agc`, after each tone call, after `DemodDataV32`, after the
 * `V32NextState` dispatch, and even after `dsplibs_debug_printf`.  A next-state
 * function may replace the whole context, and `RxHdxPhsReversal` then reads
 * the NEW context's +0x90 to decide whether to run the tone detector in the
 * same call.  So this file spells `HDX(modem)` at each use rather than caching
 * it in a local across a call; where the object shows one load serving several
 * uses there is no call between them and the compiler's CSE is what merged
 * them.
 *
 * ---------------------------------------------------------------------------
 * THE HALF-DUPLEX CONTEXT OPENS WITH AN `fpm_agc`
 *
 * `RxHdxTone` (8393e), `RxHdxNoSignal` (83a3e) and `RxHdxPhsReversal` (83b4a)
 * hand `FPM_AGC_agc` the context pointer ITSELF as its first argument, and
 * that function's first parameter is a `struct fpm_agc *`.  `sizeof(struct
 * fpm_agc)` is 0x2c and `V32_HDX_TONE0` is 0x2c, so the AGC occupies exactly
 * hdx + 0x00 .. +0x2b with nothing over.  Evidence class 2, a callee that
 * types it.
 *
 * The object passes a FOURTH argument to `FPM_AGC_agc` (the constant 1, at
 * 0xc(%esp)) which the callee never reads; `src/pump/v32/v32demod.c`,
 * `src/pump/v23/bwchdem.c` and `src/pump/v22/v22data.c` all record this at
 * their own call sites and pass three.  So does this file.
 *
 * ---------------------------------------------------------------------------
 * THE TWO FORMAT STRINGS, WHICH ARE THE AUTHOR'S OWN WORDS
 *
 * `RxHdxPhsReversal` carries the only two diagnostics in the file, and both
 * name the value that was just stored:
 *
 *   83d15  .rodata.str1.1:0x37f3  "v32 Turn Around Delay = %d\n"  -> hdx+0x7c
 *   83d9c  .rodata.str1.1:0x380f  "v32 RTD = %d\n"                -> hdx+0x96
 *
 * So hdx + 0x96 is the round-trip delay and is named for it, and hdx + 0x7c is
 * seeded with `CalcTurnAroundDelay`'s result before it goes on counting
 * symbols -- which is why it is named for the counting and not for the seed.
 *
 * ---------------------------------------------------------------------------
 * `CalcTurnAroundDelay` IS INLINED IN THE OBJECT AND INLINED HERE
 *
 * 83c29..83c55 is that function's 53 bytes instruction for instruction: the
 * blob's `V32rxhdx.c` holds both the out-of-line copy at 0x83ae0 and this
 * inlined one, which is what GCC does with a same-translation-unit global at
 * -O3.  That is the proof the two are one translation unit, and moving the
 * function into this file is what recovered the shape: `RxHdxPhsReversal`
 * went from 571 bytes (a `call`, 82 differing) to 619 bytes (the inline, 34
 * differing) against the object's 653, and its instruction count is now 157
 * against the object's 159.  The residual is named, not hill-climbed.
 */

#include "dsplib/v32hdxst.h"
#include "dsplib/v32struct.h"

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"

/* The instance is `struct v32_modem`; see v32hdx.h.  These are the only accessors. */

#define HDX(m)		((m)->hdx)
#define FP(m)		((m)->fp)


/* ------------------------------------------------------------------------ */
/* The instance.                                                            */

/*
 * `V32FP_recreate` copies its `cfg` argument's first 48 bytes into the
 * instance and then prints that copy through `dsplibs_debug_printf` with the
 * format string at `.rodata.str1.4:0x011000` -- "V32FP Config: protocol=%d,
 * tx_rate=%d,rx_rate=%d,timeout=%d,energy_drop_time=%d,tx_scale=%d,
 * options=0x%x,trellis=%d".  Pairing each conversion with the argument slot
 * that feeds it puts `protocol` at obj + 0x00 and `options` at obj + 0x10;
 * `include/dsplib/v32seq.h` carries the whole derivation and uses the same
 * table for `tx_rate` and `rx_rate`.  Evidence class 1, a format string.
 *
 * `RxHdxTone` (83928) compares +0x00 against 1 as SIXTEEN bits, so it is a
 * `short` there whatever width the printer promoted.
 */
#define V32_OBJ_PROTOCOL	0x00	/* short                              */
#define V32_OBJ_OPTIONS		0x10	/* int, printed as 0x%x               */

/*
 * ONE BIT OF `options`, AND ONLY ITS VALUE IS ESTABLISHED.
 *
 * The object spells the test `testb $0x4,0x11(%ebx)`, which is what GCC emits
 * for `options & 0x400` on a 32-bit field at +0x10 -- a byte test against the
 * second byte.  What the bit SELECTS is not established: its one reader is
 * `RxHdxTone`, where setting it forces the AGC and the tone detector to run on
 * a block they would otherwise skip.  Named by VALUE and 1:1 with the object,
 * per CLAUDE.md; a name for the meaning would be believed and is not earned.
 */
#define V32_OPT_0400		0x0400

/*
 * ONE BIT OF THE FLAGS BYTE, likewise named by value only.
 *
 * `RxHdxSequenceE` (8401d) is the only site in this file that sets it.  It is
 * a real flag rather than scratch -- 84e99 and 85744 clear it with
 * `andb $0xfb` and 8515e tests it -- but every one of those is in a function
 * this batch does not write, so what it stands for is open.  `v32demod.h`
 * names 0x20 and 0x40 of the same byte and `v32fpctl.h` names 0x02.
 */
#define V32_FLAG_04		0x04

/*
 * THE SIX REASON CODES THIS FILE POSTS AT `V32_OBJ_STATUS`.
 *
 * Every one is written at the same site -- the symbol counter reaching its
 * bound -- and always together with `V32_FLAG_FAULT`, which is what says the
 * byte is a reason and not a state; `V32_STATUS_BAD_MODE` (0x17) is the same
 * byte used the same way by the two mode setters.
 *
 * THEY DO NOT INDEX THE STATE and are therefore named by VALUE: 0x10 is shared
 * by `RxHdxTone` and `RxHdxNull`, 0x13 by the three sequence states and 0x14
 * by `RxHdxToneData` and `RxHdxSTone`.  Nothing in the object prints them.
 */
#define V32_STATUS_10		0x10
#define V32_STATUS_11		0x11
#define V32_STATUS_12		0x12
#define V32_STATUS_13		0x13
#define V32_STATUS_14		0x14
#define V32_STATUS_15		0x15

/* ------------------------------------------------------------------------ */
/* The half-duplex context, at V32_OBJ_HDX.                                 */

/* The embedded AGC; see the header comment.  Evidence class 2. */
#define V32HDX_AGC		0x00

/*
 * `V32HDX_REGS[4]`, the last of `v32seq.h`'s five scratch shorts (0x3c, 0x3e,
 * 0x40, 0x42, 0x44 -- `LoadReg` bounds the index at 4 and the element type is
 * `short`).  That header records that nothing in ITS batch reads them;
 * `RxHdxSequenceE` is the first reader and writer found, and it uses this one
 * as a one-field parking slot: the low half of `GetSequence`'s match goes in
 * at 84076 and comes straight back out at 8407d as `DecodeRateSeq`'s `seq`.
 *
 * The INDEX is named for that use and the register is not: what it holds at
 * any other moment is still open.
 */
#define V32HDX_REG_RATE_SEQ	4

/*
 * MODELLED, UNNAMED -- an `unsigned short` counter `RxHdxSequenceE` runs only
 * after it has decoded the rate sequence.  Zero means "not yet": the decode
 * arm sets it to one by incrementing it, and every later block adds
 * `symbol_len` to it and raises `V32_FLAG_04` once it passes 0x17.
 */
#define V32HDX_SHORT_48		0x48

/*
 * MODELLED, UNNAMED -- an `int`, and the compare against it is SIGNED
 * (`cmpl $0xb4,0x78(%edx)` with `jle`, 839ce), which is what types it.  Its
 * one reader in this file is `RxHdxTone`'s three-term gate.
 */
#define V32HDX_INT_78		0x78

/*
 * THE BLOCK COUNTER AND ITS BOUND, and both are UNSIGNED: every one of the ten
 * comparisons is `jb`/`jbe` and not `jl`/`jle`.
 *
 * Eleven of the twelve states add `V32HDX_SYMBOL_LEN` to the counter on entry
 * and post a fault once it reaches the bound, so "counter" and "bound" are
 * what the object says and are as far as this goes.  What it counts is very
 * likely symbols -- `V32HDX_SYMBOL_LEN` is what goes into it -- and what the
 * bound MEANS is not established, so neither is named for a unit.
 *
 * `RxHdxPhsReversal` (83c5d) SEEDS the counter with `CalcTurnAroundDelay`'s
 * result and prints it as "v32 Turn Around Delay = %d", so the counter is
 * pre-charged with the turnaround budget rather than starting at zero.
 */
#define V32HDX_TIMER		0x7c	/* unsigned int                       */
#define V32HDX_LIMIT		0x80	/* unsigned int                       */

/*
 * MODELLED, UNNAMED -- an `int` latch inside `RxHdxPhsReversal`, and the only
 * thing established about it is the machine it drives: clear, the state runs
 * the tone detector and counts the blocks the tone is PRESENT in; four of
 * them set it; set, the state runs the phase-reversal search instead.  It is
 * re-read after the next-state dispatch, so whatever clears it can do so from
 * there.
 */
#define V32HDX_INT_90		0x90

/*
 * THE ROUND-TRIP DELAY, and the name is the author's own: the value stored
 * here at 83d73 is printed at 83d9c through `.rodata.str1.1:0x380f`,
 * "v32 RTD = %d\n".  Evidence class 1.
 *
 * Clamped at zero on both arms, so it is never negative once the state has
 * written it.
 */
#define V32HDX_RTD		0x96	/* short                              */

/*
 * MODELLED, UNNAMED -- two `unsigned short` accumulators that take
 * `V32HDX_SYMBOL_LEN` once per block exactly as `V32HDX_TIMER` does, but in
 * sixteen bits and with no reader in this file.  +0xa8 is
 * `RxHdxPhsReversal`'s and +0xaa is `RxHdxNoSignal`'s; nothing here compares
 * either against anything.
 */
#define V32HDX_SHORT_A8		0xa8
#define V32HDX_SHORT_AA		0xaa

/*
 * MODELLED, UNNAMED -- `RxHdxPhsReversal`'s run counter, and NOTE THE SENSE,
 * which is the opposite of the obvious one: `FPM_TONE_detect` returns
 * `FPM_TONE_PRESENT` == 0 when the tone IS there (fpm_tone.h, and finding
 * F33 for why this module inverts), and 83bc5's `jne` sends the NON-zero --
 * that is, tone-absent -- verdict to the store of zero at 83d07.  So this
 * counts consecutive blocks the tone was PRESENT in and is reset by any other
 * verdict, and once it exceeds three it arms `V32HDX_INT_90` and resets.  The
 * compare is signed 16-bit (`cmpw $0x3` with `jle`).
 */
#define V32HDX_SHORT_AC		0xac

/* ------------------------------------------------------------------------ */

/*
 * `RxHdxPhsReversal`'s scaling of `FPM_TONE_find_rev`'s reported period.
 *
 * The object multiplies by 0x4ccc, adds 0x1000 and shifts right by 13, which
 * is a rounded multiply by 19660/8192 = 2.39990...  Reproduced as the three
 * constants the object holds rather than as a rate, because which two rates
 * the ratio is between is not established.
 */
#define V32_REV_SCALE		0x4ccc
#define V32_REV_ROUND		0x1000
#define V32_REV_SHIFT		13

/* ------------------------------------------------------------------------ */

/*
 * The receive half-duplex driver: one state, one call.  It is the blob's
 * `V32rxhdx.c` first function, so it is first here too.
 */
void
V32RxHdxModem(struct v32_modem *modem, short *in, unsigned short *out,
	      unsigned short *count)
{
	struct v32_hdx *hdx;

	hdx = modem->hdx;
	hdx->rx_state(modem, in, out, count);
}

void
RxHdxTone(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	/*
	 * Three terms, and the object tests them in this order with two early
	 * exits into the body: any one of them runs the detector.
	 */
	if (modem->params.protocol != 1
	    || (modem->params.options & V32_OPT_0400) != 0
	    || hdx->state_left > 180) {
		/* The object passes a fourth argument here; see the header. */
		FPM_AGC_agc((&hdx->agc), in, *count);

		if (FPM_TONE_detect(HDX(modem)->tone0, in,
				    (short)*count) == 0)
			(*V32NextState[HDX(modem)->mode])(modem);
	}

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_10;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxNoSignal(struct v32_modem *modem, short *in, unsigned short *out,
	      unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);

	hdx->timer +=
		(unsigned int)hdx->symbol_len;
	hdx->short_aa = (unsigned short)
		(hdx->short_aa
		 + hdx->symbol_len);

	/* The object passes a fourth argument here; see the header. */
	FPM_AGC_agc((&hdx->agc), in, *count);

	if (FPM_TONE_detect(HDX(modem)->tone0, in,
			    (short)*count) != 0) {
		hdx = HDX(modem);
		if (hdx->timer > 60)
			(*V32NextState[hdx->mode])(modem);
	}

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_11;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

/*
 * What is left of the turnaround budget, clamped at zero.
 *
 * The subtraction is narrowed to sixteen bits BEFORE the clamp -- the object
 * does `cwtl` and then the branchless `x & ~(x >> 31)` -- so a budget that
 * underflows past 32768 comes back positive rather than clamped.  D483.
 *
 * The four fields are loaded `movzwl` here and +0x9c `movswl` in
 * `SetECRndTripDelayV32`.  Both extensions are DEAD -- every use is truncated
 * back to sixteen bits -- so the signedness is the compiler's free choice at
 * each site (finding F614) and each site is written the way the object has it.
 */

short
CalcTurnAroundDelay(struct v32_modem *modem)
{
	struct v32_hdx *hdx = HDX(modem);
	short left;

	left = (short)(hdx->turnaround
		       - (hdx->short_9c
			  + hdx->short_98
			  + hdx->short_9a));
	return (short)(left < 0 ? 0 : left);
}

void
RxHdxPhsReversal(struct v32_modem *modem, short *in, unsigned short *out,
		 unsigned short *count)
{
	struct v32_hdx *hdx;
	short rev;
	short miss;

	/* The object passes a fourth argument here; see the header. */
	FPM_AGC_agc(&HDX(modem)->agc, in, *count);
	FPM_TONE_kill(HDX(modem)->tone1, in, (short)*count);
	FPM_TONE_kill(HDX(modem)->tone2, in, (short)*count);

	hdx = HDX(modem);
	hdx->timer +=
		(unsigned int)hdx->symbol_len;
	hdx->short_a8 = (unsigned short)
		(hdx->short_a8
		 + hdx->symbol_len);

	if (hdx->int_90 != 0) {
		int scaled;

		rev = FPM_TONE_find_rev(hdx->tone0, in,
					(short)*count);
		if (rev > 0) {
			short tad = CalcTurnAroundDelay(modem);

			hdx = HDX(modem);
			hdx->timer = (unsigned int)(int)tad;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"v32 Turn Around Delay = %d\n",
					(int)tad);

			hdx = HDX(modem);
			scaled = ((int)rev * V32_REV_SCALE + V32_REV_ROUND)
				 >> V32_REV_SHIFT;

			if (hdx->mode == V32_MODE_ORIGINATE) {
				hdx->rtd = (short)
					(scaled - 2 * hdx->turnaround);
			} else {
				hdx->rtd = (short)
					(scaled
					 + hdx->symbol_len
					 - hdx->turnaround
					 - hdx->short_9c
					 - hdx->short_98
					 - hdx->short_9a);
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("v32 RTD = %d\n",
						(int)hdx->rtd);
				hdx = HDX(modem);
			}

			if (hdx->rtd < 0)
				hdx->rtd = 0;

			(*V32NextState[hdx->mode])(modem);
		}
		hdx = HDX(modem);
	}

	/*
	 * NOT an `else`.  The dispatch above may clear +0x90 -- and may have
	 * replaced the whole context -- and the object then runs the detector
	 * in the SAME call (the `je 83bac` at 83cb4 is a backward branch into
	 * the other arm).
	 */
	if (hdx->int_90 == 0) {
		if (FPM_TONE_detect(hdx->tone0, in,
				    (short)*count) == 0)
			miss = (short)(HDX(modem)->short_ac
				       + 1);
		else
			miss = 0;

		hdx = HDX(modem);
		hdx->short_ac = miss;
		if (hdx->short_ac > 3) {
			hdx->int_90 = 1;
			hdx->short_ac = 0;
		}
	}

	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_12;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxRateSequence(struct v32_modem *modem, short *in, unsigned short *out,
		  unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	unsigned short n;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	*count = n = DemodDataV32(modem, in, out, *count);
	DescrambleDataV32(modem, (short *)out, n);

	/*
	 * TWO CALLS TO `GetSequence`, and the object really does make both
	 * (83e28 and 83e35): the high half of the detector's match word is
	 * compared against the low half, so the same rate signal has to have
	 * arrived twice.  The shift is LOGICAL and the mask is 0xffff.
	 */
	if (DetSequence(modem, (const short *)out, *count) >= 0
	    && ((unsigned int)GetSequence(modem) >> 16)
		== (unsigned int)(GetSequence(modem) & 0xffff))
		(*V32NextState[HDX(modem)->mode])(modem);

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_13;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxSequence(struct v32_modem *modem, short *in, unsigned short *out,
	      unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	unsigned short n;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	*count = n = DemodDataV32(modem, in, out, *count);
	DescrambleDataV32(modem, (short *)out, n);

	if (DetSequence(modem, (const short *)out, *count) >= 0)
		(*V32NextState[HDX(modem)->mode])(modem);

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_13;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxSequenceE(struct v32_modem *modem, short *in, unsigned short *out,
	       unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	unsigned short n;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	n = DemodDataV32(modem, in, out, *count);
	*count = n;
	DescrambleDataV32(modem, (short *)out, n);

	hdx = HDX(modem);
	if (hdx->short_48 != 0) {
		hdx->short_48 = (unsigned short)
			(hdx->short_48
			 + hdx->symbol_len);
		if (hdx->short_48 > 0x17)
			modem->flags |= V32_FLAG_04;
	} else if (DetSequence(modem, (const short *)out, *count) >= 0) {
		short *regs;

		hdx = HDX(modem);
		regs = hdx->regs;
		regs[V32HDX_REG_RATE_SEQ] = (short)GetSequence(modem);

		hdx = HDX(modem);
		regs = hdx->regs;
		hdx->rx_state = RxHdxData;
		SetRxModeV32(modem,
			     V32_RX_MODE[DecodeRateSeq(modem,
				(unsigned short)regs[V32HDX_REG_RATE_SEQ])]);

		hdx = HDX(modem);
		hdx->short_48 = (unsigned short)
			(hdx->short_48 + 1);
	} else {
		hdx = HDX(modem);
		if (hdx->timer
		    >= hdx->limit) {
			modem->flags |= V32_FLAG_FAULT;
			modem->status = V32_STATUS_13;
			hdx->tx_state = TxHdxNoCarrier;
			hdx->rx_state = RxHdxError;
			hdx->state = V32_STATE_ERROR;
		}
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxData(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	unsigned short n;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	*count = n = DemodDataV32(modem, in, out, *count);
	DescrambleDataV32(modem, (short *)out, n);

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxToneData(struct v32_modem *modem, short *in, unsigned short *out,
	      unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	unsigned short n;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	if (FPM_TONE_detect(hdx->tone0, in,
			    (short)*count) == 0)
		(*V32NextState[HDX(modem)->mode])(modem);

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_14;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	/* The demodulation comes AFTER the timeout check, not before it. */
	*count = n = DemodDataV32(modem, in, out, *count);
	DescrambleDataV32(modem, (short *)out, n);

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxSTone(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);
	short det;

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	/*
	 * WHICH BANK THE MULTI-TONE DETECTOR IS CARRYING decides which buffer
	 * it runs over.  Configured with the DATA-mode S-tone coefficients it
	 * demodulates the block first and detects over the datapump's own
	 * working buffer; configured with the other bank it detects over the
	 * caller's raw input and does not demodulate at all.
	 */
	if (hdx->mtd->cfg.coeff == V32_S_DATA_COEF) {
		struct v32_fp *fp;

		*count = DemodDataV32(modem, in, out, *count);

		fp = FP(modem);
		det = FPM_MTD_detect(HDX(modem)->mtd,
				     (const short *)fp->rx_buf,
				     fp->rx_len);
	} else {
		det = FPM_MTD_detect(hdx->mtd, in, (short)*count);
	}

	if (det == 0)
		(*V32NextState[HDX(modem)->mode])(modem);

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_14;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

void
RxHdxEpoch(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	*count = DemodDataV32(modem, in, out, *count);

	if (EpochDetectV32(modem) != 0)
		(*V32NextState[HDX(modem)->mode])(modem);

	hdx = HDX(modem);
	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_15;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}

/*
 * The terminal state, and the only one of the twelve that touches neither the
 * context nor the block counter.  It raises the fault bit unconditionally --
 * without a reason code, because whoever installed it has already posted one
 * -- demodulates the block anyway, and reports no symbols.
 */
void
RxHdxError(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	modem->flags |= V32_FLAG_FAULT;
	DemodDataV32(modem, in, out, *count);
	*count = 0;
}

void
RxHdxNull(struct v32_modem *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct v32_hdx *hdx = HDX(modem);

	hdx->timer +=
		(unsigned int)hdx->symbol_len;

	if (hdx->timer >= hdx->limit) {
		modem->flags |= V32_FLAG_FAULT;
		modem->status = V32_STATUS_10;
		hdx->tx_state = TxHdxNoCarrier;
		hdx->rx_state = RxHdxError;
		hdx->state = V32_STATE_ERROR;
	}

	*count = RxClampV32(modem, in, (short *)out, *count);
}
