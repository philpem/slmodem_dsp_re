/*
 * V29r_prc.c -- split out of the merged v29.c / v29data.c so the definitions sit in
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
 * V29RX_modem -- .text 0x0a3f50, 127 bytes.
 *
 * THE TWO EXTENSIONS OF `*count` ARE BOTH FORCED, AND THEY DISAGREE, WHICH IS
 * WHAT DECIDES THE TYPES.
 *
 *     movswl %cx,%ebx     the value taken BEFORE the call, used as a 32-bit
 *                         subtrahend and as a scaled index -- SIGNED
 *     movzwl %cx,%edx     the value read back AFTER it, used the same way
 *                         -- UNSIGNED
 *
 * One memory location, one call between the two reads, two different
 * extensions of the 32-bit result.  A single declared type cannot produce
 * that, and the spelling that does is the obvious one: `count` points at an
 * `unsigned short` and the saved copy is a `short` local.  Everything else
 * about the loop -- the 16-bit `test %cx,%cx`, the 16-bit store of the total
 * -- is consistent with both and settles nothing.  Finding F8877.
 *
 * The opening clear is deliberately through the byte containing bit 9.  A
 * dword `&= ~V29_STATUS_ERROR` is value-equivalent, but GCC 3.4.2 emits a
 * dword AND for it; the object has `andb $0xfd,0x19`.
 */
int
V29RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short produced = 0;

	((struct v29_rx *)modem)->result.byte.flags &=
		(unsigned char)~(V29_STATUS_ERROR >> 8);

	/*
	 * A do-while: the object has no test above the loop head, only
	 * eleven bytes of alignment padding.  A zero count on entry still
	 * dispatches the slot once.
	 */
	do {
		short avail = (short)*count;
		short n;

		n = ((struct v29_rx *)modem)->det->handler(modem, in, out, count);

		in += avail - *count;
		out += n;
		produced = (short)(produced + n);
	} while (*count != 0);

	*count = (unsigned short)produced;

	return ((struct v29_rx *)modem)->result.word;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxDataV29 -- .text 0x0a3fd0, 226 bytes.
 *
 * The DATA state: demodulate and descramble while the carrier is up, and
 * grade the result.  It is `RxHdxDataV17` and `RxHdxDataV27` instruction for
 * instruction with three offsets changed, and it is NOT `RxHdxDataV21`'s
 * shape -- there is no state advance on either arm.
 *
 * THREE RETURNS FROM THE CALLEES ARE TESTED SIXTEEN BITS WIDE and that is
 * FORCED, not free: `test %ax,%ax` at 0x0a400e on `DataCarrierDetectV29`'s
 * result and `cmp $0x2,%ax` at 0x0a4077 on `QualityDetectV29`'s.  Both are
 * declared `int` in `v29fax.h` and both are read here through a narrowing
 * cast, which is what the object encodes.  The two readings AGREE over every
 * value either function can produce -- the carrier verdict is 0 or 1 and the
 * quality verdict is 0, 1 or 2 -- so the cast changes the instructions and
 * cannot change the answer.  Finding F9257.
 *
 * THE CARRIER BIT IS RAISED UNCONDITIONALLY ON ENTRY and lowered again on the
 * arm where the carrier has gone, which is not the same as assigning it: a
 * caller reading the word between two handlers in one block sees the raised
 * bit.  Same order as `RxHdxDataV21` (0x0a3ff3 before the call, 0x0a401d
 * after it).
 *
 * `V29DET_INT_0008` is the second gate and nothing reconstructed sets it, so
 * the test plants it rather than reaching it.
 *
 * THE UNITS REPORTED ARE ZERO WHEN THE QUALITY VERDICT IS `V29Q_NO_CARRIER`,
 * after `out` has already been written and every filter advanced.  The object
 * computes it `setne`/`movzbl`/`neg`/`and` (0x0a407b..0x0a4087), so there are
 * exactly two outcomes and no third; D1149.
 */
short
RxHdxDataV29(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short units;

	((struct v29_rx *)modem)->result.word |= V29_STATUS_CARRIER;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_DATA;

	if ((short)DataCarrierDetectV29(modem, in, *count) == 0
	    || ((struct v29_rx *)modem)->det->int_0008 != 0) {
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV29(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	units = (short)((short)QualityDetectV29(modem) != V29Q_NO_CARRIER
			? (short)n : 0);

	((struct v29_rx *)modem)->result.word &= ~V29_STATUS_LOW_SNR;
	if (GetSNRV29(modem) <= V29RX_SNR_THRESHOLD)
		((struct v29_rx *)modem)->result.word |= V29_STATUS_LOW_SNR;

	return units;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxErrorV29 -- .text 0x0a40c0, 59 bytes.
 *
 * The ERROR state: raise the flag, run the block through the demodulator
 * anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state, so once something has installed this
 * handler the machine stays in it until something outside installs another.
 * The flag is a one-shot: `V29RX_modem` clears it at the top of every block,
 * so a caller that does not read the returned word each block loses the
 * event.  `RxHdxErrorV21` is the same function for V.21.
 */
short
RxHdxErrorV29(void *modem, short *in, short *out, unsigned short *count)
{
	((struct v29_rx *)modem)->result.word |= V29_STATUS_ERROR;

	DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * RxNextStateV29 -- .text 0x0a4100, 467 bytes.
 *
 * The transition function: read the current state, announce it, install the
 * next state's handler and number, seed that state's block budget, and rewrite
 * the two status flags that say which steady state the machine is in.
 *
 * IT IS A `switch` AND THE JUMP TABLE IS WHAT SETTLES THE NUMBERING.  The
 * object bounds the index with `cmp $0x4` and an UNSIGNED `ja`, then jumps
 * through `.rodata + 0xc35c`; the five entries are `R_386_32` relocations
 * against the `.text` section symbol with the arm address as an inline addend,
 * so they cannot be read from a byte dump.  Finding F9320 carries the table
 * and the consistency check that fixes which string belongs to which value.
 *
 * THE STRING NAMES THE STATE BEING LEFT, NOT THE ONE BEING ENTERED, and the
 * proof is that every arm's stored number is paired with that number's own
 * handler -- START installs `RxHdxEpochDetV29` and stores 1, and EPOCH_DET is
 * 1.  Read the other way round the pairing fails at all five arms.
 *
 * THE DETECTION BLOCK IS RE-READ AFTER EVERY `dsplibs_debug_printf`, which the
 * object does at 0x0a4281, 0x0a4295, 0x0a42a6 and 0x0a42cb and is forced: the
 * printf could write through `modem`.  It is spelled here as the same
 * `((struct v29_rx *)modem)->det` the rest of this file uses, which produces the reload at each
 * of those points and one load in the arms that do not print.
 *
 * THE DEFAULT ARM TOUCHES NO STATE AT ALL.  It raises `V29_STATUS_ERROR`,
 * clears the carrier and DATA bits and reports `V29RX_STATUS_DEFAULT`, and
 * leaves `V29DET_STATE` and `V29DET_HANDLER` exactly as it found them -- so a
 * machine that reaches it stays there and every subsequent transition takes the
 * same arm.  `V29RX_STATE_ERROR` (5) is one such value, and the two handlers
 * that store it also install `RxHdxErrorV29`, which never calls this function;
 * so the ERROR state is a trap by two independent mechanisms.
 */
void
RxNextStateV29(void *modem)
{
	switch (((struct v29_rx *)modem)->det->state) {
	case V29RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_STATE_START\n");

		((struct v29_rx *)modem)->det->state_count =
					V29DET_COUNT_EPOCH_DET;
		((struct v29_rx *)modem)->det->handler = RxHdxEpochDetV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_EPOCH_DET;

		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_DATA;
		break;

	case V29RX_STATE_EPOCH_DET:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_STATE_EPOCH_DET\n");

		((struct v29_rx *)modem)->det->state_count =
					V29DET_COUNT_PROTOCOL;
		((struct v29_rx *)modem)->det->handler = RxHdxPrtcolV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_PROTOCOL;

		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_DATA;

		/*
		 * THE ONLY DSP THIS FUNCTION DOES, and it is a POINTER STEP
		 * and not arithmetic: `fpm_agc_cfg`'s `alpha` and `beta` are
		 * `const short *` supplied by address out of `AGCv29_CFG`
		 * (`fpm_agc.h`: "MIXED STRUCT: +0x0c and +0x10 are POINTERS"),
		 * so `addl $0x2` at 0x0a41c2 and 0x0a41c6 advances each to the
		 * next table entry.  Read as scalars -- which an int16 dump of
		 * the config invites -- it would be "add 2 to the smoother
		 * feedback", a plausible sentence and a completely different
		 * modem.  The declared type is what rules that out.
		 */
		((struct v29_rx *)modem)->rx->agc.cfg.alpha += 1;
		((struct v29_rx *)modem)->rx->agc.cfg.beta += 1;
		break;

	case V29RX_STATE_PROTOCOL:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_STATE_PROTOCOL\n");

		FPM_AGC_Freeze(&((struct v29_rx *)modem)->rx->agc);

		((struct v29_rx *)modem)->det->state_count = 0;
		((struct v29_rx *)modem)->det->handler = RxHdxDataV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_DATA;

		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.word |= V29_STATUS_DATA;
		break;

	case V29RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_STATE_DATA\n");

		((struct v29_rx *)modem)->det->handler = RxHdxIdleV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_IDLE;
		((struct v29_rx *)modem)->det->state_count = 0;
		((struct v29_rx *)modem)->det->int_0008 = 0;

		((struct v29_rx *)modem)->result.word |= V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_DATA;
		break;

	case V29RX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_STATE_IDLE\n");

		((struct v29_rx *)modem)->det->handler = RxHdxDataV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_DATA;

		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.word |= V29_STATUS_DATA;

		/*
		 * The IDLE arm alone does NOT seed the block budget, so the
		 * DATA state it hands over to inherits whatever is there.
		 * `RxHdxDataV29` does not read it, which is why that is not
		 * observable -- but it is the object's, and the four other
		 * arms all write it.
		 */
		((struct v29_rx *)modem)->result.byte.status =
			((unsigned short)((struct v29_rx *)modem)->det->rate) != V29_RATE_7200
				? V29RX_STATUS_DATA_9600
				: V29RX_STATUS_DATA_7200;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V29RX_DEFAULT, %d\n",
					     ((struct v29_rx *)modem)->det->state);

		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_IDLE;
		((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_DEFAULT;
		((struct v29_rx *)modem)->result.word |= V29_STATUS_ERROR;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_DATA;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
		break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxIdleV29 -- .text 0x0a42e0, 132 bytes.
 *
 * The IDLE state: keep demodulating, and go back to DATA once the equaliser
 * says its decisions are good again.
 *
 * IT IS NOT A "NO CARRIER" STATE.  The machine arrives here from DATA, which
 * `RxNextStateV29` only leaves on a transition of its own, and the test that
 * gets it out is `fpm_fse::mse <= V29RX_MSE_RECOVERED` -- the equaliser's
 * smoothed squared decision error, the same field `"V29 Decoder error too
 * big"` is about.  The author's own words for the transition are
 * `"Decision error is small back to DATA mode !!!\n"` (.rodata.str1.4 +
 * 0x12bcc), printed AFTER the transition has been made.
 *
 * THE CARRIER BIT IS ASSIGNED AND THEN RE-READ, which is the object's and is
 * forced by the memory: `andb $0xdf` clears it, `CarrierDetectV29` runs,
 * `orb $0x20` sets it again if that answered, and then `testb $0x20` reads it
 * back at 0x0a4329.  That last read is the ONE site in the whole object that
 * reads `V29_STATUS_CARRIER`, which is what makes it worth spelling as a
 * re-read rather than reusing the call's result.
 *
 * The mse test is only reached when the carrier is up, so a block with no
 * carrier leaves the machine here whatever the equaliser thinks.
 */
short
RxHdxIdleV29(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_IDLE;

	if (CarrierDetectV29(modem))
		((struct v29_rx *)modem)->result.word |= V29_STATUS_CARRIER;

	if ((((struct v29_rx *)modem)->result.word & V29_STATUS_CARRIER) != 0
	    && ((struct v29_rx *)modem)->rx->fse.mse <= V29RX_MSE_RECOVERED) {
		RxNextStateV29(modem);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"Decision error is small back to DATA mode " "!!!\n");
	}

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxPrtcolV29 -- .text 0x0a4370, 244 bytes.
 *
 * The PROTOCOL state: demodulate, DESCRAMBLE, and sit here for
 * `V29DET_COUNT_PROTOCOL` blocks before handing over to DATA.
 *
 * IT REPORTS A COUNT ON EXACTLY ONE BLOCK AND ZERO ON EVERY OTHER.  The
 * function descrambles into `out` on every call, but the value it returns --
 * which is what `V29RX_modem` advances the caller's output pointer by -- is a
 * literal zero on the two paths that do not advance the state (0x0a43eb) and
 * the demodulator's own count only on the block where the budget expires
 * (`movswl %bp,%eax` at 0x0a444d).  So everything descrambled before the
 * countdown ran out is thrown away by the caller, and the last block's worth is
 * not.  `RxHdxEpochDetV29` -- the same function without the descramble --
 * returns zero on all three of its paths, and `RxHdxDataV29` reports its count
 * on every block, so this is not the family's habit.  Deviation D1160 records
 * the identical asymmetry in `RxHdxPrtcolV27`; this is the second instance and
 * neither is derived from the other.
 *
 * LOSING THE CARRIER DROPS STRAIGHT TO ERROR -- `RxHdxErrorV29` installed and
 * `V29RX_STATE_ERROR` stored, without going through `RxNextStateV29` -- and
 * that arm also raises `V29_STATUS_ERROR` and clears the carrier bit.
 *
 * THE LOW-SNR TEST HAPPENS ONLY ON THE HANDOVER BLOCK and is one-sided: the
 * bit is SET when `GetSNRV29` comes back at or below `V29RX_SNR_THRESHOLD` and
 * is not cleared otherwise, which is where it differs from `RxHdxDataV29`
 * (0x0a4081) -- that one clears the bit first and then re-raises it.
 */
short
RxHdxPrtcolV29(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;

	n = DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV29(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	if (!CarrierDetectV29(modem)) {
		((struct v29_rx *)modem)->det->handler = RxHdxErrorV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_ERROR;
		((struct v29_rx *)modem)->result.word |= V29_STATUS_ERROR;
		((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_LOST;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
		return 0;
	}

	((struct v29_rx *)modem)->result.word |= V29_STATUS_CARRIER;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_TRAIN;

	((struct v29_rx *)modem)->det->state_count =
		(short)(((struct v29_rx *)modem)->det->state_count - 1);
	if (((struct v29_rx *)modem)->det->state_count > 0)
		return 0;

	((struct v29_rx *)modem)->result.byte.status =
		((unsigned short)((struct v29_rx *)modem)->det->rate) != V29_RATE_7200
			? V29RX_STATUS_DATA_9600 : V29RX_STATUS_DATA_7200;

	if (GetSNRV29(modem) <= V29RX_SNR_THRESHOLD)
		((struct v29_rx *)modem)->result.word |= V29_STATUS_LOW_SNR;

	RxNextStateV29(modem);

	return (short)n;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxEpochDetV29 -- .text 0x0a4470, 156 bytes.
 *
 * The EPOCH_DET state: `RxHdxPrtcolV29` without the descramble, without the
 * SNR test, and with a second way out.
 *
 * TWO CONDITIONS ADVANCE IT AND EITHER IS ENOUGH: the state's block budget
 * running out, OR `EpochDetectV29` reporting that the equaliser's `lms_force`
 * has been raised -- which is exactly what `V29RX_epoch_det` does on the call
 * where it hands the equaliser over to `V29RX_eq_train`.  So this state waits
 * for the slicer chain's first handover and gives up after
 * `V29DET_COUNT_EPOCH_DET` blocks either way.  The two are wired the short-
 * circuit way round in the object -- the budget is tested first and
 * `EpochDetectV29` is not called when it has expired.
 *
 * `EpochDetectV29`'S RESULT IS TESTED SIXTEEN BITS WIDE (`test %ax,%ax` at
 * 0x0a44ce) where `v29fax.h` declares it `int`.  The two readings agree over
 * every value it can produce -- it returns 0 or 1 -- so the narrowing changes
 * the instructions and cannot change the answer; deviation D1172 records why
 * the declaration is not changed to match.  `RxHdxDataV29` carries the same
 * shape for `DataCarrierDetectV29` and `QualityDetectV29` (F9257).
 */
short
RxHdxEpochDetV29(void *modem, short *in, short *out, unsigned short *count)
{
	DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	if (!CarrierDetectV29(modem)) {
		((struct v29_rx *)modem)->det->handler = RxHdxErrorV29;
		((struct v29_rx *)modem)->det->state = V29RX_STATE_ERROR;
		((struct v29_rx *)modem)->result.word |= V29_STATUS_ERROR;
		((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_LOST;
		((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
		return 0;
	}

	((struct v29_rx *)modem)->result.word |= V29_STATUS_CARRIER;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_TRAIN;

	((struct v29_rx *)modem)->det->state_count =
		(short)(((struct v29_rx *)modem)->det->state_count - 1);
	if (((struct v29_rx *)modem)->det->state_count > 0
	    && (short)EpochDetectV29(modem) == 0)
		return 0;

	RxNextStateV29(modem);

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxStartV29 -- .text 0x0a4510, 105 bytes.
 *
 * The START state, and the one `V29RX_create` installs: demodulate, and leave
 * as soon as a carrier appears.
 *
 * IT IS THE ONLY STATE THAT DOES NOT LOWER THE CARRIER BIT AND THEN RAISE IT
 * AGAIN CONDITIONALLY -- it lowers it BEFORE the demodulation and raises it
 * after, with the whole block's work in between, so a caller reading the word
 * mid-block sees it clear.  The other four write both stores adjacent.
 *
 * WHILE THE MACHINE IS HERE, `V29DET_STATE` IS ZERO, which is the condition
 * `DemodDataV29` reads as "run the tone pre-pass".  So the tone detector runs
 * on exactly the blocks this state consumes and on no others; F9320.
 */
short
RxHdxStartV29(void *modem, short *in, short *out, unsigned short *count)
{
	((struct v29_rx *)modem)->result.word &= ~V29_STATUS_CARRIER;
	((struct v29_rx *)modem)->result.byte.status = V29RX_STATUS_START;

	DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);

	if (CarrierDetectV29(modem)) {
		((struct v29_rx *)modem)->result.word |= V29_STATUS_CARRIER;
		RxNextStateV29(modem);
	}

	*count = 0;

	return 0;
}
