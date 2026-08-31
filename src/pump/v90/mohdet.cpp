/*
 * mohdet.cpp -- the V.92 modem-on-hold retrain-request detector.
 *
 *   retrainDetector        .text 0x005f80   390 bytes
 *   resetRetrainDetector   .text 0x006110   118 bytes
 *   interpretMohTimeout    .text 0x006190   102 bytes
 *
 * `include/dsplib/mohdet.h` carries what the object is, where it sits in
 * the blob, and why the three names are C++-mangled; this file carries the
 * arithmetic.  Three notes on shapes the disassembly forces:
 *
 * THE PRODUCTS ARE TRUNCATED ONE AT A TIME, AND THAT IS A CODEGEN CLAIM,
 * NOT A BEHAVIOURAL ONE.  Each Q14 term is narrowed to short before the sum
 * -- `sar $0xe` then `movswl %dx,%edx` at 0x6010/0x6013 and its two
 * siblings -- and the b1 product genuinely overflows a short (x[n-1] at
 * full range times 23170 reaches 46341).  But the whole sum is narrowed to
 * short again on assignment, and a term that wrapped differs from its
 * unwrapped self by a multiple of 65536, which the outer narrowing absorbs
 * -- so the per-term casts change NO output on ANY input.  What they buy is
 * the object's own `movswl`s; the claim is adjudicated by the codegen tier,
 * and test/mutations/mohdet.json records the corresponding mutation as
 * equivalent rather than deleting it.  Same shape as fpm_iir's inner cast
 * (that suite's `equivalent` entry) and F8322's dead `(short)`.
 *
 * THE COEFFICIENTS ARE LOCALS.  All three are hoisted out of the loop
 * (movswl at 0x5f99..0x5fa1, before the first sample), which the compiler
 * may not do to a struct field on its own: the loop stores shorts through
 * `det` every iteration and `in` is a short pointer, so field re-reads
 * could alias either.  The hoist therefore records the AUTHOR's locals,
 * not an optimisation.
 *
 * THE STORE-BACKS ARE UNCONDITIONAL.  The accumulators and the sample count
 * go back to the struct on every path -- as the running values mid-block
 * (0x605d..0x6063), as themselves on a detection (0x60cf..0x60d8, where the
 * count's value is the constant 64 the compare just established), and as
 * zeros on a failed block.  One unconditional write-back before the
 * block-end test produces all three; the branches differ only in what the
 * block-end test then overwrites.
 */

#include "dsplib/debug.h"
#include "dsplib/mohdet.h"

int
retrainDetector(tag_retrainReqDet *det, short *in, int nSamples)
{
	int b1 = det->b1_q14;
	int a1 = det->a1_q14;
	int a2 = det->a2_q14;
	int i;

	for (i = 0; i < nSamples; i++) {
		short x = *in++;
		short y;
		int energyInp, energyOut, cnt;

		y = (short)(x
			    + (short)((det->y1 * a1 + 0x2000) >> 14)
			    - (short)((det->y2 * a2 + 0x2000) >> 14)
			    - (short)((det->x1 * b1 + 0x2000) >> 14)
			    + det->x2);
		det->y2 = det->y1;
		det->x2 = det->x1;
		det->y1 = y;
		det->x1 = x;

		energyInp = det->energyInp + ((x * x + 32) >> 6);
		energyOut = det->energyOut + ((y * y + 32) >> 6);
		cnt = det->nsamples + 1;

		det->energyInp = energyInp;
		det->energyOut = energyOut;
		det->nsamples = cnt;

		if (cnt == 64) {
			if ((energyInp >> NOTCH_IN_OUT_RATIO_SHIFT) > energyOut
			    && energyInp > 150000
			    && energyOut < 2250000) {
				det->notchDetectSigCnt++;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "********** retrainDetector() "
					    "notchDetectSigCnt = %d "
					    "energyInp>>NOTCH_IN_OUT_RATIO_"
					    "SHIFT = %d energyOut = %d\r\n",
					    det->notchDetectSigCnt,
					    energyInp
					    >> NOTCH_IN_OUT_RATIO_SHIFT,
					    energyOut);
			} else {
				det->notchDetectSigCnt = 0;
			}
			/*
			 * Every COMPLETED block starts the next one clean --
			 * the detect arm reaches this too (0x60ea jumps to
			 * 0x60a7, INSIDE the reset tail).  The running-value
			 * stores above survive on that arm only because
			 * dsplibs_debug_printf is an opaque call that might
			 * read the struct, exactly dcr.c's store pattern.
			 */
			det->energyInp = 0;
			det->energyOut = 0;
			det->nsamples = 0;
		}

		if (det->notchDetectSigCnt > 5)
			return 1;
	}

	return 0;
}

void
resetRetrainDetector(tag_retrainReqDet *det, short which)
{
	det->y1 = 0;
	det->y2 = 0;
	det->x1 = 0;
	det->x2 = 0;
	det->notchDetectSigCnt = 0;
	det->energyInp = 0;
	det->energyOut = 0;
	det->nsamples = 0;

	/*
	 * Coefficients only for the two selectors the object knows; any
	 * other value leaves whatever was there.  0x65: notch at Fs/4.
	 * 0x66: notch at Fs/8.  See mohdet.h on why the selectors have no
	 * names.
	 */
	if (which == 0x65) {
		det->b1_q14 = 0;
		det->a1_q14 = 0;
		det->a2_q14 = 0x39c3;
	} else if (which == 0x66) {
		det->b1_q14 = 0x5a82;
		det->a1_q14 = 0x55fc;
		det->a2_q14 = 0x39c3;
	}
}

int
interpretMohTimeout(short code)
{
	switch (code) {
	case 0:
		return 0;
	case 1:
		return 10;
	case 2:
		return 20;
	case 3:
		return 30;
	case 4:
		return 40;
	case 5:
		return 60;
	case 6:
		return 120;
	case 7:
		return 180;
	case 8:
		return 240;
	case 9:
		return 360;
	case 10:
		return 480;
	case 11:
		return 720;
	case 12:
		return 960;
	case 13:
		return -1;
	default:
		return 0;
	}
}
