/*
 * V17r_prc.c -- the V.17 receive process and its state handlers, split out
 * of the merged v17.c into the blob's V17r_prc.c translation unit.  Bodies
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

int
V17RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short total;
	short before;
	unsigned short left;

	RXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17RX_FLAG_ERROR;

	total = 0;
	do {
		struct v17rx_priv *ctl;
		short got;

		/*
		 * `before` is signed and `left` is not, and both are what the
		 * object encodes: ONE 16-bit load feeds a `movswl` at the top
		 * of the loop and a `movzwl` after the call, because the
		 * compiler shared the read across the back edge.
		 */
		before = (short)*count;

		ctl = RXCTL(modem);
		got = ctl->process
				(modem, in, out, count);

		left = *count;
		in += before - left;
		out += got;
		total = (short)(total + got);
	} while (left != 0);

	*count = (unsigned short)total;

	return RXROOT(modem)->result.word;
}

/*
 * RxHdxDataV17 -- .text 0x0a0000, 226 bytes.
 *
 * The DATA state of the receive machine.  See v17fax.h for the flag-bit
 * enumeration and for `V17RXC_INT_0008`.
 *
 * THE `out` CASTS ARE THE RECONSTRUCTION'S AND THE OBJECT CANNOT SEE THEM.
 * The state handlers share one signature (`v17rx_process_fn`) whose third
 * argument this tree already spells `short *`, while `DemodDataV17` and
 * `DescrambleDataV17` declare theirs `unsigned short *`.  Both pointers are
 * passed through untouched, so nothing in the object distinguishes the two
 * spellings and the casts cost no instruction.  Deviation D1141.
 *
 * THE RESULT IS A `?:` AND THE OBJECT SPELLS IT BRANCHLESSLY.  It emits
 * `cmp $0x2,%ax` / `setne %al` / `movzbl %al,%esi` / `neg %esi` /
 * `and %edi,%esi`, which is `n & -(q != 2)` -- the standard shape GCC folds a
 * two-armed conditional into when one arm is a constant zero and the guard is
 * already a flag.  The `?:` is what is written here, because it is the source
 * that expression is the compilation of and because writing the mask by hand
 * would be fitting the object rather than reading it.  `n` is `unsigned short`
 * and the object zero-extends it (`movzwl %ax,%edi`), which is the local's
 * declared type showing through (finding F7803); the final `movswl %si` is the
 * function's own `short` return.
 *
 * THE `andb $0x7f` SITS BETWEEN THE `setne` AND THE `and` in the object.  That
 * is the scheduler moving a store with no dependence on either, not a
 * statement order to reproduce: the clear of `V17RX_FLAG_LOW_SNR` belongs with
 * the `GetSNRV17` test it precedes.
 */
short
RxHdxDataV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short r;

	RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
	RXROOT(modem)->result.byte.status = V17RX_STATUS_DATA;

	if (DataCarrierDetectV17(modem, in, *count) == 0
	    || RXCTL(modem)->r08 != 0) {
		RXROOT(modem)->result.byte.flags &=
			(unsigned char)~V17RX_FLAG_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	r = (short)(QualityDetectV17(modem) != V17_QUALITY_UNRELIABLE ? n : 0);

	RXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17RX_FLAG_LOW_SNR;
	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_LOW_SNR;

	return r;
}

/*
 * RxHdxErrorV17 -- .text 0x0a00f0, 59 bytes.
 *
 * The ERROR state: raise the flag, demodulate anyway so the filters keep their
 * history, and consume the block.  `DemodDataV17`'s return is DISCARDED, which
 * is the one thing separating this from a data handler that happened to fail.
 */
short
RxHdxErrorV17(void *modem, short *in, short *out, unsigned short *count)
{
	RXROOT(modem)->result.byte.flags |= V17RX_FLAG_ERROR;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/*
 * RxNextStateV17 -- .text 0x0a0130, 730 bytes.
 *
 * The receive machine's state advance.  See `v17fax.h` for what each arm does
 * and for the eight state names, which are the author's own out of
 * `.rodata.str1.1`.
 *
 * THE TABLE IS THE COMPILER'S AND THERE IS NOTHING TO REPRODUCE.  The object
 * dispatches through `jmp *0xc2d0(,%eax,4)` after `cmp $0x6` / `ja`, which is
 * what GCC emits for a dense `switch` on 0..6 with a `default`.  The `ja` is
 * unsigned over a `movswl`-widened `short`, so a negative state takes the
 * default rather than indexing backwards, and `t_v17rxstate.c` drives -1 for
 * exactly that reason.
 *
 * EVERY ARM RE-READS `V17RX_OBJ_CTL` AFTER ITS DIAGNOSTIC AND THAT IS FORCED.
 * The object reloads `0x5c(%ebx)` at 0x0a0398, 0x0a0384, 0x0a035c, 0x0a03d1,
 * 0x0a03ac and 0x0a0370 -- once per arm that prints -- because the call to
 * `dsplibs_debug_printf` clobbers memory it cannot see through.  The `CTL()`
 * macro expands at each use, so the reloads come out of the C rather than
 * being imitated; the SCRAM arm has none because its own reload happens after
 * `FPM_AGC_Freeze` instead.
 *
 * THE EPOCH_DET ARM TESTS `V17RXC_INT_0010` TWICE AND THE SECOND TEST IS DEAD.
 * `Restore_rateV17` writes only `V17RXS_RATE` and `V17RXS_SHORT_01F8`, both in
 * the demodulator state, so it cannot change a control-block field -- but the
 * compiler does not know that, and the object's `mov 0x10(%edx),%ecx` /
 * `test` / `jne` at 0x0a0340 is the reload it is forced into.  On the arm
 * where the call did NOT happen it CSEs the two tests and jumps straight to
 * the 62, which is why 0x0a01d9's not-taken edge goes to 0x0a01df.  Written as
 * the two reads the source had; deviation D1212.
 */
void
RxNextStateV17(void *modem)
{
	switch (RXCTL(modem)->state) {
	case V17RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_START\n");
		RXCTL(modem)->countdown = 5;
		CTL(modem)->process = RxHdxEpochDetV17;
		RXCTL(modem)->state = V17RX_STATE_EPOCH_DET;
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_EPOCH_DET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_EPOCH_DET\n");
		if (RXCTL(modem)->short_train != 0)
			Restore_rateV17(modem);
		RXCTL(modem)->countdown = (short)
			(RXCTL(modem)->short_train != 0 ? 1 : 62);
		CTL(modem)->process = RxHdxPrtcolV17;
		RXCTL(modem)->state = V17RX_STATE_PROTOCOL;
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags &=
			(unsigned char)~V17RX_FLAG_DATA;
		/*
		 * Acquisition to tracking: one `short` along each of the two
		 * Q15 coefficient tables `AGCv17_CFG` points at, which the
		 * object spells `addl $0x2` because that is what `const short *`
		 * arithmetic compiles to.  See v17fax.h and D1216.
		 */
		(&RXS(modem)->agc.value)->cfg.alpha++;
		(&RXS(modem)->agc.value)->cfg.beta++;
		break;

	case V17RX_STATE_PROTOCOL:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_PROTOCOL\n");
		if (RXCTL(modem)->short_train != 0) {
			RXCTL(modem)->countdown = 1;
			CTL(modem)->process = RxHdxScramV17;
			RXCTL(modem)->state = V17RX_STATE_SCRAM;
			RXROOT(modem)->result.byte.flags &=
				(unsigned char)~V17RX_FLAG_DATA;
			/* No write to V17RX_OBJ_RESULT_B2 here.  D1213. */
		} else {
			RXCTL(modem)->countdown = 1;
			CTL(modem)->process = RxHdxBridgeV17;
			RXCTL(modem)->state = V17RX_STATE_BRIDGE;
			RXROOT(modem)->result.byte.flags &=
				(unsigned char)~V17RX_FLAG_DATA;
			StoreCoefV17(modem);
			RXROOT(modem)->result.byte.flags2 &=
				(unsigned char)~V17RX_RESULT_B2_BIT0;
		}
		break;

	case V17RX_STATE_BRIDGE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_BRIDGE\n");
		RXCTL(modem)->countdown = 1;
		CTL(modem)->process = RxHdxScramV17;
		RXCTL(modem)->state = V17RX_STATE_SCRAM;
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_SCRAM:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_SCRAM\n");
		FPM_AGC_Freeze(&RXS(modem)->agc.value);
		RXCTL(modem)->countdown = 0;
		CTL(modem)->process = RxHdxDataV17;
		RXCTL(modem)->state = V17RX_STATE_DATA;
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_DATA:
		/*
		 * UNREACHABLE IN THE OBJECT, AND WRITTEN ANYWAY.  Nothing that
		 * runs while the state is DATA calls this function --
		 * `RxHdxDataV17` and `RxHdxErrorV17` are the two handlers that
		 * never do -- so this arm is 52 bytes of code the machine
		 * cannot enter.  Deviation D1210.  The store order differs from
		 * every other arm's (handler, state, countdown, rather than
		 * countdown, handler, state) and is the object's.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_DATA\n");
		CTL(modem)->process = RxHdxIdleV17;
		RXCTL(modem)->state = V17RX_STATE_IDLE;
		RXCTL(modem)->countdown = 0;
		RXCTL(modem)->r08 = 0;
		RXROOT(modem)->result.byte.flags2 |= V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags &=
			(unsigned char)~V17RX_FLAG_DATA;
		break;

	case V17RX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_STATE_IDLE\n");
		CTL(modem)->process = RxHdxDataV17;
		RXCTL(modem)->state = V17RX_STATE_DATA;
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_DATA;
		/* The one arm that does not seed the countdown.  D1215. */
		if (RXCTL(modem)->rate_code == V17RX_RATE_7200)
			RXROOT(modem)->result.byte.status =
				V17RX_STATUS_RATE_7200;
		else if (RXCTL(modem)->rate_code == V17RX_RATE_9600)
			RXROOT(modem)->result.byte.status =
				V17RX_STATUS_RATE_9600;
		else if (RXCTL(modem)->rate_code == V17RX_RATE_12000)
			RXROOT(modem)->result.byte.status =
				V17RX_STATUS_RATE_12000;
		else
			RXROOT(modem)->result.byte.status =
				V17RX_STATUS_RATE_14400;
		break;

	default:
		/*
		 * Reached by `V17RX_STATE_ERROR`, which has no arm of its own,
		 * and by any state outside 0..6.  D1211.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V17RX_DEFAULT: %d\n",
					     RXCTL(modem)->state);
		RXROOT(modem)->result.byte.flags2 &=
			(unsigned char)~V17RX_RESULT_B2_BIT0;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_DEFAULT;
		/*
		 * THE MASK IS 0xde AND NOT 0xdf: this arm clears CARRIER *and*
		 * `V17RX_FLAG_DATA`, where the four handlers' error arms clear
		 * CARRIER alone (`and $0xdf`).  That is consistent with every
		 * other arm of this function writing the DATA bit -- the
		 * default is not a transition, so it clears it -- and it is
		 * one of the two places the two masks differ by exactly that
		 * bit.  Read the bytes at 0x0a0168 and 0x0a0192.
		 */
		RXROOT(modem)->result.byte.flags = (unsigned char)
			((RXROOT(modem)->result.byte.flags | V17RX_FLAG_ERROR)
			 & ~(V17RX_FLAG_CARRIER | V17RX_FLAG_DATA));
		break;
	}
}

/*
 * RxHdxIdleV17 -- .text 0x0a0410, 132 bytes.
 *
 * See v17fax.h.  The `out` cast is D1141's, as in `RxHdxDataV17`: the handler
 * signature spells the buffer `short *` and `DemodDataV17` spells it
 * `unsigned short *`, the pointer is passed through untouched, and the cast
 * costs no instruction.
 */
short
RxHdxIdleV17(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	RXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17RX_FLAG_CARRIER;
	RXROOT(modem)->result.byte.status = V17RX_STATUS_IDLE;

	if (CarrierDetectV17(modem) != 0)
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;

	if ((RXROOT(modem)->result.byte.flags & V17RX_FLAG_CARRIER) != 0
	    && RXSTATE(modem)->fse.mse <= V17RXS_DEC_ERROR_SMALL) {
		RxNextStateV17(modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Decision error is small back to DATA mode !!!\n");
	}

	return 0;
}

/*
 * RxHdxScramV17 -- .text 0x0a04a0, 266 bytes.
 *
 * The rate ladder on the expiry path is the ONLY thing that separates this
 * from `RxHdxBridgeV17` and `RxHdxPrtcolV17` below, which are 210 bytes each
 * and byte-for-byte identical to one another.  See v17fax.h and F9444.
 *
 * THE COUNTDOWN IS LOADED UNSIGNED, STORED AS SIXTEEN BITS AND TESTED SIGNED,
 * and all three are the object's; see `V17RXC_COUNTDOWN`.  The read-back on
 * the field is what keeps the test at 16 bits -- a `short` local carries a
 * `cwtl` and a 32-bit test the object does not have.
 */
short
RxHdxScramV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short rc = 0;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) != 0) {
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_CARRIER;

		RXCTL(modem)->countdown =
			(short)((unsigned short)RXCTL(modem)->countdown - 1);
		if ((short)RXCTL(modem)->countdown <= 0) {
			if (RXCTL(modem)->rate_code == V17RX_RATE_7200)
				RXROOT(modem)->result.byte.status = V17RX_STATUS_RATE_7200;
			else if (RXCTL(modem)->rate_code == V17RX_RATE_9600)
				RXROOT(modem)->result.byte.status = V17RX_STATUS_RATE_9600;
			else if (RXCTL(modem)->rate_code == V17RX_RATE_12000)
				RXROOT(modem)->result.byte.status = V17RX_STATUS_RATE_12000;
			else
				RXROOT(modem)->result.byte.status = V17RX_STATUS_RATE_14400;

			/* SET and never cleared; only RxHdxDataV17 clears it.  D1217. */
			if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
				RXROOT(modem)->result.byte.flags |= V17RX_FLAG_LOW_SNR;

			RxNextStateV17(modem);

			return (short)n;
		}
	}
	else {
		CTL(modem)->process = RxHdxErrorV17;
		RXCTL(modem)->state = V17RX_STATE_ERROR;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_ERROR;
		RXROOT(modem)->result.byte.flags = (unsigned char)
			((RXROOT(modem)->result.byte.flags | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
	}

	return rc;
}

/*
 * RxHdxBridgeV17 -- .text 0x0a05b0, 210 bytes.
 *
 * IDENTICAL TO `RxHdxPrtcolV17` BELOW, BYTE FOR BYTE, and the two bodies are
 * written out twice for that reason: a shared static helper would be one
 * symbol where the object has two, and would move the code generation of both.
 * Do not fold them.  Finding F9444.
 */
short
RxHdxBridgeV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short rc = 0;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) != 0) {
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_CARRIER;

		RXCTL(modem)->countdown =
			(short)((unsigned short)RXCTL(modem)->countdown - 1);
		if ((short)RXCTL(modem)->countdown <= 0) {
			if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
				RXROOT(modem)->result.byte.flags |= V17RX_FLAG_LOW_SNR;

			RxNextStateV17(modem);

			return (short)n;
		}
	}
	else {
		CTL(modem)->process = RxHdxErrorV17;
		RXCTL(modem)->state = V17RX_STATE_ERROR;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_ERROR;
		RXROOT(modem)->result.byte.flags = (unsigned char)
			((RXROOT(modem)->result.byte.flags | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
	}

	return rc;
}

/*
 * RxHdxPrtcolV17 -- .text 0x0a0690, 210 bytes.  The other copy; see above.
 */
short
RxHdxPrtcolV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short rc = 0;

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (CarrierDetectV17(modem) != 0) {
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_CARRIER;

		RXCTL(modem)->countdown =
			(short)((unsigned short)RXCTL(modem)->countdown - 1);
		if ((short)RXCTL(modem)->countdown <= 0) {
			if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
				RXROOT(modem)->result.byte.flags |= V17RX_FLAG_LOW_SNR;

			RxNextStateV17(modem);

			return (short)n;
		}
	}
	else {
		CTL(modem)->process = RxHdxErrorV17;
		RXCTL(modem)->state = V17RX_STATE_ERROR;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_ERROR;
		RXROOT(modem)->result.byte.flags = (unsigned char)
			((RXROOT(modem)->result.byte.flags | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
	}

	return rc;
}

/*
 * RxHdxEpochDetV17 -- .text 0x0a0770, 156 bytes.
 *
 * THE `||` IS SHORT-CIRCUIT AND THE OBJECT PROVES IT: `jle` at 0x0a07c4 jumps
 * over the `EpochDetectV17` call at 0x0a07c9 to the `RxNextStateV17` call at
 * 0x0a07d3.  An expired countdown advances the machine without asking about the
 * epoch, and `t_v17rxstate.c` drives that case with the epoch flag CLEAR so
 * that a non-short-circuiting reading would stay put.
 */
short
RxHdxEpochDetV17(void *modem, short *in, short *out, unsigned short *count)
{
	short left;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (CarrierDetectV17(modem) == 0) {
		CTL(modem)->process = RxHdxErrorV17;
		RXCTL(modem)->state = V17RX_STATE_ERROR;
		RXROOT(modem)->result.byte.status = V17RX_STATUS_ERROR;
		RXROOT(modem)->result.byte.flags = (unsigned char)
			((RXROOT(modem)->result.byte.flags | V17RX_FLAG_ERROR)
			 & ~V17RX_FLAG_CARRIER);
		return 0;
	}

	RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
	RXROOT(modem)->result.byte.status = V17RX_STATUS_CARRIER;

	left = (short)((unsigned short)RXCTL(modem)->countdown - 1);
	RXCTL(modem)->countdown = left;
	if (left <= 0 || EpochDetectV17(modem) != 0)
		RxNextStateV17(modem);

	return 0;
}

/*
 * RxHdxStartV17 -- .text 0x0a0810, 105 bytes.
 *
 * The only handler with no error arm; see v17fax.h.  `*count = 0` is ONE
 * statement that the compiler tail-duplicated into both arms of the carrier
 * test, which is why the object stores it at 0x0a0850 and again at 0x0a086c.
 */
short
RxHdxStartV17(void *modem, short *in, short *out, unsigned short *count)
{
	RXROOT(modem)->result.byte.flags &=
		(unsigned char)~V17RX_FLAG_CARRIER;
	RXROOT(modem)->result.byte.status = V17RX_STATUS_START;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);

	if (CarrierDetectV17(modem) != 0) {
		RXROOT(modem)->result.byte.flags |= V17RX_FLAG_CARRIER;
		RxNextStateV17(modem);
	}

	*count = 0;

	return 0;
}
