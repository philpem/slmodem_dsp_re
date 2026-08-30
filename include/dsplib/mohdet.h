/*
 * mohdet.h -- the V.92 modem-on-hold retrain-request detector's object, and
 * the three C++ leaves that use it.
 *
 *   retrainDetector        .text 0x005f80   390 bytes
 *   resetRetrainDetector   .text 0x006110   118 bytes
 *   interpretMohTimeout    .text 0x006190   102 bytes
 *
 * All three are C++-mangled free functions -- `_Z15retrainDetector
 * P17tag_retrainReqDetPsi` and friends -- so the tag below has to be a real
 * class type spelled exactly `tag_retrainReqDet`, and the definitions live
 * in a `.cpp` (src/pump/v90/mohdet.cpp).  None of the three has a caller
 * anywhere in the object: they are the exported-API-with-no-internal-caller
 * bucket CLAUDE.md describes, so every use fact below comes from the three
 * bodies themselves and from the one debug format string retrainDetector
 * owns (.rodata.str1.4+0x86c), which names `notchDetectSigCnt`, `energyInp`,
 * `NOTCH_IN_OUT_RATIO_SHIFT` and `energyOut` in the author's own words.
 *
 * WHAT THE DETECTOR IS.  One notch biquad plus two energy accumulators.
 * Per sample, in Q14 with round-to-nearest:
 *
 *     y[n] = x[n] - (b1*x[n-1] >> 14) + x[n-2]
 *                 + (a1*y[n-1] >> 14) - (a2*y[n-2] >> 14)
 *
 * every term truncated to short individually, then input and output energy
 * are both accumulated as (v*v + 32) >> 6 over 64-sample blocks.  A block
 * whose input energy is loud (> 150000), whose notch output is quiet in
 * absolute terms (< 2250000) and quiet RELATIVE to the input (more than
 * 4:1, the ratio shift) counts one detection; any other completed block
 * clears the count.  EVERY completed block clears the accumulators -- the
 * detect arm rejoins the reset tail at 0x60a7.  More than 5 consecutive
 * detections returns 1.
 *
 * `resetRetrainDetector` knows two coefficient sets, selected by its second
 * argument -- 0x65 puts the notch at Fs/4 (b1 = 0, a1 = 0) and 0x66 at Fs/8
 * (b1 = 0x5a82 = 2cos(pi/4) in Q14, a1 = 0x55fc), both with a2 = 0x39c3, a
 * pole radius near 0.95.  What 0x65 and 0x66 MEAN is not established: no
 * caller exists to read a name from, so the two selectors stay numeric.
 *
 * WHERE IT SITS IN THE BLOB, since the span label misleads: 0x5f80..0x61f6
 * falls in a span labelled `b103.c`, between `dp_wrapper_run` and the first
 * `VPcmV34Main.cpp` exports -- and the debug string sits between b103.c's
 * strings and VPcmV34Main.cpp's in .rodata.str1.4 the same way.  Whether
 * the original TU was b103.c's tail compiled as C++ or a file of its own is
 * not decidable from the layout; the MODULE is V.92 MOH either way, which
 * is why the file lives with the V.90/V.92 pump.
 */

#ifndef DSPLIB_MOHDET_H
#define DSPLIB_MOHDET_H

struct tag_retrainReqDet {
	short y1;			/* +0x00 y[n-1], the notch output   */
	short y2;			/* +0x02 y[n-2]                     */
	short x1;			/* +0x04 x[n-1]                     */
	short x2;			/* +0x06 x[n-2]                     */
	int notchDetectSigCnt;		/* +0x08 blocks that looked like a
					 *       tone; > 5 means detected   */
	short b1_q14;			/* +0x0c numerator z^-1, negated    */
	short a1_q14;			/* +0x0e denominator z^-1           */
	short a2_q14;			/* +0x10 denominator z^-2, negated  */
	short pad_12;			/* +0x12 alignment, never touched   */
	int energyInp;			/* +0x14 sum (x*x + 32) >> 6        */
	int energyOut;			/* +0x18 sum (y*y + 32) >> 6        */
	int nsamples;			/* +0x1c samples in this block,
					 *       checked at 64 (usage
					 *       inference, no author name) */
};

typedef int mohdet_size_is_32[sizeof(struct tag_retrainReqDet) == 32
			      ? 1 : -1];

/* The author's own name for the 4:1 energy ratio, from the format string. */
#define NOTCH_IN_OUT_RATIO_SHIFT	2

#ifdef __cplusplus
/*
 * C++ linkage on purpose: the object's names are mangled, so these three
 * are only declarable (and only testable) from C++.
 */
int retrainDetector(tag_retrainReqDet *det, short *in, int nSamples);
void resetRetrainDetector(tag_retrainReqDet *det, short which);

/*
 * The V.92 MOH timeout code, 0..13, to its value in seconds: 0 stays 0
 * (hold disallowed), 13 is -1 (no limit), everything else the T.MOH ladder
 * 10..960.  Out of range -- including any negative, the compare is on the
 * full unsigned 16-bit value -- answers 0.
 */
int interpretMohTimeout(short code);
#endif

#endif /* DSPLIB_MOHDET_H */
