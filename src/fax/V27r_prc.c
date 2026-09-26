/*
 * V27r_prc.c -- split out of the merged v27.c so the definitions sit in the
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
 * Drive the half-duplex receive state handler until it stops asking for more.
 *
 * A DO-WHILE, not a while: the handler is entered once even when the caller
 * offers no samples at all, which is how a state that only has to emit gets
 * to run.  The accumulated output count is a `short` -- `add %edx,%eax` then
 * `cwtl` on every iteration -- and it is written back over the caller's input
 * count.
 */
int
V27RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short total = 0;

	((struct v27_rx *)modem)->result.byte.flags &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_ERROR;

	n = *count;
	do {
		short before = (short)n;
		short got;

		got = ((struct v27_rx *)modem)->shared->handler(modem, in, out,
							       count);
		n = *count;
		out += got;
		/*
		 * The samples the handler took.  `before` is signed and `n` is
		 * not, which the object states by extending the same sixteen
		 * bits two different ways -- `movswl %cx` at the top of the
		 * loop and `movzwl %cx` here.  Both are used at full width by
		 * the subtraction, so neither is free.
		 */
		in += before - n;
		total = (short)(total + got);
	} while (n != 0);

	*count = (unsigned short)total;

	return ((struct v27_rx *)modem)->result.word;
}

/*
 * RxHdxDataV27 .text 0x0a2ce0, 226 bytes.
 *
 * The DATA state of the receive machine.  See v27fax.h for the flag-bit
 * enumeration and for `V27SH_INT_0004`.
 *
 * THE `out` CASTS ARE THE RECONSTRUCTION'S AND THE OBJECT CANNOT SEE THEM: the
 * handler family's signature (`v27_rx_state_fn`) spells the third argument
 * `short *` and both callees declare theirs `unsigned short *`, and the
 * pointer is passed through untouched either way.  Deviation D1141.
 *
 * THE RESULT IS A `?:` AND THE OBJECT SPELLS IT BRANCHLESSLY -- `cmp $0x2,%ax`
 * / `setne` / `movzbl` / `neg` / `and`, which is `n & -(q != 2)`.  The `?:` is
 * what is written, for the reason `src/fax/v17.c` gives at the identical site.
 *
 * ONE CODEGEN DIFFERENCE IS EXPECTED HERE AND IT IS DECLARED RATHER THAN
 * FITTED.  The object widens `n` for the `DescrambleDataV27` call with
 * `movzwl %ax,%edi`; `DescrambleDataV27`'s third parameter is `short` in this
 * tree (from its own `movswl` of that parameter, F9118), so the implicit
 * conversion below must widen with `movswl` instead.  Declaring that parameter
 * `unsigned short` and letting the SINGLE narrowing happen inside the callee
 * would reproduce both sites -- but it is a change to a written, tested
 * function's signature that cannot be checked without the period compiler, so
 * it is left for whoever has one.  The two spellings are behaviourally
 * identical for every `n`, because the conversion to `short` happens either
 * way before `SDMv27_descrambler` sees it.  Finding F9237, deviation D1140.
 */
short
RxHdxDataV27(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short r;

	((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_CARRIER;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_DATA;

	if (DataCarrierDetectV27(modem, in, *count) == 0
	    || ((struct v27_rx *)modem)->shared->int_0004 != 0) {
		((struct v27_rx *)modem)->result.byte.flags &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV27(modem, (unsigned short *)(void *)out, (short)n);
	*count = 0;

	r = (short)(QualityDetectV27(modem) != V27_QUALITY_UNRELIABLE ? n : 0);

	((struct v27_rx *)modem)->result.byte.flags &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_LOW_SNR;
	if (GetSNRV27(modem) <= V27RX_SNR_THRESHOLD)
		((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_LOW_SNR;

	return r;
}

/*
 * RxHdxErrorV27 .text 0x0a2dd0, 59 bytes.
 *
 * The ERROR state: raise the flag, demodulate anyway so the filters keep their
 * history, and consume the block.  `DemodDataV27`'s return is DISCARDED.
 */
short
RxHdxErrorV27(void *modem, short *in, short *out, unsigned short *count)
{
	((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_ERROR;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/*
 * RxNextStateV27 .text 0x0a2e10, 518 bytes.
 *
 * The transition table of the half-duplex receive machine, and the only thing
 * in the object that writes `V27SH_STATE`, `V27SH_RX_STATE` or
 * `V27SH_COUNTDOWN` outside `V27RX_create`.
 *
 * THE STATE NAMES ARE THE AUTHOR'S OWN AND SO ARE THEIR NUMBERS.  The switch
 * goes through the jump table at `.rodata:0xc31c`, whose five entries are
 * 0x0a2e7a, 0x0a2ea7, 0x0a2ed9, 0x0a2f17 and 0x0a2f4b in that order, and each
 * of those five arms opens by printing its own name out of `.rodata.str1.1`.
 * So the pairing of NAME to NUMBER is read off the table and the strings
 * together, not guessed from the order the handlers run in.  See v27fax.h at
 * `V27SH_RX_STATE`, and finding F9300.
 *
 * EVERY ARM WRITES BOTH FLAG BITS, ALWAYS OPPOSITE WAYS.  Four arms clear
 * `V27_STATUS_FLAG2_IDLE` and clear `V27_STATUS_FLAG_DATA`; the two that hand
 * over to `RxHdxDataV27` clear IDLE and SET DATA; the one that hands over to
 * `RxHdxIdleV27` does the reverse.  That is what makes the two bits a pair
 * rather than two independent flags.
 *
 * THE ONLY ARM THAT TOUCHES THE DSP IS EPOCH_DET -> PROTOCOL, and what it does
 * there is step the AGC's two smoother coefficients on by one `short` each:
 * `addl $0x2,0x74(%eax)` and `addl $0x2,0x78(%eax)`, where `%eax` is the
 * receive block, 0x74 is `V27RX_AGC + offsetof(struct fpm_agc_cfg, alpha)` and
 * 0x78 is the same for `beta`.  Both are POINTERS -- `fpm_agc.h` says so, and
 * `AGCv27_CFG` supplies them by address -- so 2 is one element and not a
 * magnitude.  The PROTOCOL -> DATA arm then freezes the gain outright.
 * Finding F9303.
 *
 * THE DEFAULT ARM INSTALLS NOTHING.  It leaves the handler and the state
 * number alone, so a machine that reaches `V27RX_STATE_ERROR` and is advanced
 * from there stays in `RxHdxErrorV27` for ever; all the arm does is report
 * `V27_STATUS_DEFAULT` and rewrite the flags.
 *
 * IT RETURNS NOTHING.  All four callers discard `%eax` and the arms leave
 * different values in it, so there is no return to reproduce.
 */
void
RxNextStateV27(void *modem)
{
	struct v27_rx_shared *sh = ((struct v27_rx *)modem)->shared;
	struct v27_rx_block *rx;
	unsigned short blocks;
	unsigned char flags;
	int state = sh->rx_state;

	switch (state) {
	case V27RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_START\n");
		sh->countdown = V27SH_EPOCH_DET_BLOCKS;
		sh->handler = RxHdxEpochDetV27;
		sh->rx_state = V27RX_STATE_EPOCH_DET;
		((struct v27_rx *)modem)->result.byte.flags2 &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		((struct v27_rx *)modem)->result.byte.flags &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		break;

	case V27RX_STATE_EPOCH_DET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_EPOCH_DET\n");
		/*
		 * Four countdowns, one selector each.  The object spells the
		 * short pair branchlessly (`setne` on `rate == 1` then `inc`,
		 * 0x0a2f81) and the long pair as a two-constant `?:`
		 * (0x0a2ebf); both are GCC's own forms of what is written
		 * here, and the four values differ so the two spellings
		 * cannot be confused for one another.
		 */
		if (sh->train_long == 0)
			blocks = sh->rate == V27SH_RATE_4800
			       ? V27SH_PROTOCOL_SHORT_4800
			       : V27SH_PROTOCOL_SHORT_2400;
		else
			blocks = sh->rate == V27SH_RATE_4800
			       ? V27SH_PROTOCOL_LONG_4800
			       : V27SH_PROTOCOL_LONG_2400;
		sh->countdown = blocks;

		rx = ((struct v27_rx *)modem)->rx;
		sh->handler = RxHdxPrtcolV27;
		sh->rx_state = V27RX_STATE_PROTOCOL;
		((struct v27_rx *)modem)->result.byte.flags2 &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		((struct v27_rx *)modem)->result.byte.flags &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		/* One `short` along each, not two of anything.  F9303. */
		(&rx->agc)->cfg.alpha++;
		(&rx->agc)->cfg.beta++;
		break;

	case V27RX_STATE_PROTOCOL:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_PROTOCOL\n");
		FPM_AGC_Freeze(&((struct v27_rx *)modem)->rx->agc);
		/* Re-read across the call; the object does (0x0a2ef4). */
		sh = ((struct v27_rx *)modem)->shared;
		sh->countdown = 0;
		sh->handler = RxHdxDataV27;
		sh->rx_state = V27RX_STATE_DATA;
		((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_DATA;
		((struct v27_rx *)modem)->result.byte.flags2 &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		break;

	case V27RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_DATA\n");
		sh->handler = RxHdxIdleV27;
		sh->rx_state = V27RX_STATE_IDLE;
		sh->countdown = 0;
		/*
		 * The gate `RxHdxDataV27` refuses to demodulate through.
		 * Nothing in the object SETS it, and this is the only thing
		 * that clears it -- exactly as `RxNextStateV21` clears
		 * `hdx->int_0000` on the same transition.
		 */
		sh->int_0004 = 0;
		((struct v27_rx *)modem)->result.byte.flags2 |= V27_STATUS_FLAG2_IDLE;
		((struct v27_rx *)modem)->result.byte.flags &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_DATA;
		break;

	case V27RX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_STATE_IDLE\n");
		sh->handler = RxHdxDataV27;
		sh->rx_state = V27RX_STATE_DATA;
		((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_DATA;
		((struct v27_rx *)modem)->result.byte.flags2 &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		((struct v27_rx *)modem)->result.byte.status = (unsigned char)
			(sh->rate == V27SH_RATE_2400
			 ? V27_STATUS_ENTER_DATA_2400
			 : V27_STATUS_ENTER_DATA_4800);
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27RX_DEFAULT, %d\n", state);
		flags = ((struct v27_rx *)modem)->result.byte.flags;
		((struct v27_rx *)modem)->result.byte.flags2 &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG2_IDLE;
		((struct v27_rx *)modem)->result.byte.status = V27_STATUS_DEFAULT;
		((struct v27_rx *)modem)->result.byte.flags = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)
				(V27_STATUS_FLAG_CARRIER | V27_STATUS_FLAG_DATA));
		break;
	}
}

/*
 * RxHdxIdleV27 .text 0x0a3020, 132 bytes.
 *
 * The IDLE state.  It keeps demodulating -- the filters have to keep their
 * history -- reports `V27_STATUS_IDLE` every block, and goes back to DATA once
 * the carrier is up AND the equaliser's error has come back below
 * `V27RX_MSE_IDLE_OK`.
 *
 * THE RESTART TEST READS THE FLAG BYTE, NOT THE CALL.  The object clears
 * `V27_STATUS_FLAG_CARRIER`, calls `CarrierDetectV27`, raises the flag again
 * if it answered, and then tests the FLAG (`testb $0x20,0x1d(%esi)` at
 * 0x0a3069) rather than the value it just computed.  The two agree here
 * because nothing between them writes the byte, and the object's spelling is
 * what is reproduced.  `RxHdxIdleV21` does the clear-call-set half the same
 * way and has no second half.
 *
 * THE DEBUG PRINT IS AFTER THE TRANSITION, not before it, which is the reverse
 * of `RxNextStateV27`'s five arms -- and it is the author's own words for what
 * just happened: "Decision error is small back to DATA mode !!!\n", from
 * `.rodata.str1.4 + 0x12b9c`.
 */
short
RxHdxIdleV27(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	((struct v27_rx *)modem)->result.byte.flags &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_IDLE;

	if (CarrierDetectV27(modem))
		((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_CARRIER;

	if ((((struct v27_rx *)modem)->result.byte.flags & V27_STATUS_FLAG_CARRIER)
	    && ((struct v27_rx *)modem)->rx->fse.mse <= V27RX_MSE_IDLE_OK) {
		RxNextStateV27(modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Decision error is small back to" " DATA mode !!!\n");
	}

	return 0;
}

/*
 * RxHdxPrtcolV27 .text 0x0a30b0, 224 bytes.
 *
 * The PROTOCOL state: the equaliser is trained, so the block is demodulated
 * AND descrambled, and the machine sits here for `V27SH_COUNTDOWN` blocks
 * before handing over to `RxHdxDataV27`.
 *
 * IT IS THE ONLY HANDLER OF THE FIVE THAT REPORTS A NON-ZERO SAMPLE COUNT, and
 * only on the block that hands over: `movswl %bp,%eax` at 0x0a3179 against a
 * literal zero on both other paths.  So everything it descrambled before the
 * countdown expired is thrown away by `V27RX_modem`, and the last block's
 * worth is not.  That asymmetry is the object's and is not obviously
 * intentional; it is reproduced because it is there.  Deviation D1160.
 *
 * THE COUNTDOWN IS DECREMENTED UNSIGNED AND TESTED SIGNED -- `movzwl`, `dec`,
 * `test %cx,%cx`, `jle` -- so a countdown of zero on entry wraps to 0xffff and
 * hands over immediately rather than counting 65535 blocks.  Both readings are
 * reproduced; see v27fax.h at `V27SH_COUNTDOWN`.
 *
 * THE `DescrambleDataV27` WIDENING IS THE SAME DECLARED DIFFERENCE
 * `RxHdxDataV27` CARRIES: the object widens `n` with `movzwl` and this tree's
 * `DescrambleDataV27` takes a `short`, so the implicit conversion widens with
 * `movswl` instead.  Behaviourally identical for every `n`.  F9237, D1140.
 */
short
RxHdxPrtcolV27(void *modem, short *in, short *out, unsigned short *count)
{
	struct v27_rx_shared *sh;
	unsigned short n;
	unsigned short left;

	n = DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV27(modem, (unsigned short *)(void *)out, (short)n);
	*count = 0;

	if (CarrierDetectV27(modem) == 0) {
		unsigned char flags;

		sh = ((struct v27_rx *)modem)->shared;
		sh->handler = RxHdxErrorV27;
		sh->rx_state = V27RX_STATE_ERROR;
		flags = ((struct v27_rx *)modem)->result.byte.flags;
		((struct v27_rx *)modem)->result.byte.status = V27_STATUS_ERROR;
		((struct v27_rx *)modem)->result.byte.flags = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER);
		return 0;
	}

	((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_CARRIER;
	sh = ((struct v27_rx *)modem)->shared;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_TRAINING;

	left = (unsigned short)(sh->countdown - 1);
	sh->countdown = left;
	if ((short)left > 0)
		return 0;

	((struct v27_rx *)modem)->result.byte.status = (unsigned char)
		(sh->rate == V27SH_RATE_2400
		 ? V27_STATUS_ENTER_DATA_2400
		 : V27_STATUS_ENTER_DATA_4800);
	RxNextStateV27(modem);

	return (short)n;
}

/*
 * RxHdxEpochDetV27 .text 0x0a3190, 156 bytes.
 *
 * The EPOCH_DET state.  It is `RxHdxPrtcolV27` without the descramble, without
 * the sample count and without the status handover, plus one extra way out:
 * `EpochDetectV27` can advance the machine before the countdown does.
 *
 * THE TWO EXITS ARE `||`, NOT `&&`, AND THE ORDER MATTERS.  The object
 * decrements the countdown FIRST, unconditionally, and only calls
 * `EpochDetectV27` when what is left is still positive (0x0a31e4).  So the
 * detector is not consulted on the block that exhausts the countdown, and the
 * countdown advances the machine whether or not the epoch was ever found.
 *
 * `EpochDetectV27` IS TESTED IN SIXTEEN BITS (`test %ax,%ax` at 0x0a31ee)
 * although it is declared to return `int` here and returns 0 or 1.  The cast
 * below is what the object encodes and it cannot separate any value that
 * function can produce.
 */
short
RxHdxEpochDetV27(void *modem, short *in, short *out, unsigned short *count)
{
	struct v27_rx_shared *sh;
	unsigned short left;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (CarrierDetectV27(modem) == 0) {
		unsigned char flags;

		sh = ((struct v27_rx *)modem)->shared;
		sh->handler = RxHdxErrorV27;
		sh->rx_state = V27RX_STATE_ERROR;
		flags = ((struct v27_rx *)modem)->result.byte.flags;
		((struct v27_rx *)modem)->result.byte.status = V27_STATUS_ERROR;
		((struct v27_rx *)modem)->result.byte.flags = (unsigned char)
			((flags | V27_STATUS_FLAG_ERROR)
			 & (unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER);
		return 0;
	}

	((struct v27_rx *)modem)->result.byte.flags |= V27_STATUS_FLAG_CARRIER;
	sh = ((struct v27_rx *)modem)->shared;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_TRAINING;

	left = (unsigned short)(sh->countdown - 1);
	sh->countdown = left;
	if ((short)left > 0 && (short)EpochDetectV27(modem) == 0)
		return 0;

	RxNextStateV27(modem);

	return 0;
}

/*
 * RxHdxStartV27 .text 0x0a3230, 101 bytes.
 *
 * The START state, and the one `V27RX_create` installs (0x0996f9).  It is the
 * smallest of the five: lower the carrier flag, report `V27_STATUS_START`,
 * demodulate, and advance as soon as `CarrierDetectV27` answers.
 *
 * THE FLAG IS LOWERED AND NEVER RAISED HERE.  Unlike `RxHdxIdleV27`, which
 * clears it and then puts it back from the same call, this handler only ever
 * clears it -- the arm that finds a carrier goes straight to
 * `RxNextStateV27`, whose START arm does not touch that bit either.  So the
 * carrier flag stays down until `RxHdxEpochDetV27` raises it on the next
 * block.
 *
 * `*count` IS ZEROED AFTER THE BRANCH ON BOTH PATHS (0x0a3270 and 0x0a3288),
 * so `RxNextStateV27` runs while the caller's count still holds the block
 * length.  Nothing on that path reads it; the ordering is the object's.
 */
short
RxHdxStartV27(void *modem, short *in, short *out, unsigned short *count)
{
	((struct v27_rx *)modem)->result.byte.flags &=
		(unsigned char)~(unsigned char)V27_STATUS_FLAG_CARRIER;
	((struct v27_rx *)modem)->result.byte.status = V27_STATUS_START;

	DemodDataV27(modem, in, (unsigned short *)(void *)out, *count);

	if (CarrierDetectV27(modem))
		RxNextStateV27(modem);

	*count = 0;

	return 0;
}
