/*
 * callprog_cfg.c -- Call Progress: the band filter's coefficients.
 *
 * Reconstructed from dsplibs.o Callprog.c, .rodata+0x5d84.  Three tables laid
 * out contiguously -- shifts, then numerator, then denominator -- and passed
 * straight to `_iir_filter_create`.  They carry no symbol in the object (they
 * are file statics, reached through a section-relative relocation), so they
 * cannot be differentially compared on their own.  They are checked instead
 * where they are used: the CALLPROG_Create test compares the whole 220-byte
 * filter object our create builds against the original's, coefficients
 * included, which fails on a single wrong word.
 *
 * The engine is described in src/callprog/toneiir.c.  Q13, and every
 * frequency below is at the call-progress rate of 8000 Hz.
 *
 * End to end: -6 dB from 135 Hz to 1280 Hz, within 1.3 dB across the whole
 * 350..620 Hz call-progress range, and a 105 dB null at 2260 Hz.  It is a
 * band-limiter, not a tone filter -- picking tones out of this band is
 * Cadence.c's job.
 */

#include "dsplib/toneiir.h"
#include "dsplib/callprog_cfg.h"

/*
 * All the headroom is taken from the input.  The cascade has about +30 dB of
 * passband gain and 32 is 30.1 dB, so the net is roughly unity.
 */
const short CALLPROG_BandFilter_shift[IIR_FILTER_SCALES] = {
	5, 0, 0, 0, 0
};

/* { b0, b1, b2 } per section, Q13.  Section 3's b0 is 2.0 -- hence Q13. */
const short CALLPROG_BandFilter_b[3 * IIR_FILTER_SECTIONS] = {
	 8192,  13289,  8192,	/* zero at 3204 Hz, on the unit circle */
	 8192, -16379,  8192,	/* zero at   31 Hz                     */
	 8192,   3322,  8192,	/* zero at 2260 Hz -- the deep null    */
	16384, -16182,  7781	/* zero at  983 Hz, radius 0.689       */
};

/*
 * { a0, a1, a2 } per section, Q13.  a0 is the normalised leading coefficient:
 * stored by create, never read by progress.  Sections 0 to 2 carry 1.0 in it
 * and section 3 carries 0.340 -- the same value as its own a2, which reads
 * like a copy-paste in the original's table and is equally harmless, since
 * nothing looks at it.
 */
const short CALLPROG_BandFilter_a[3 * IIR_FILTER_SECTIONS] = {
	8192,  -9150,  3671,	/* pole  744 Hz, r 0.669, Q  3.9 */
	8192,  -6770,  5246,	/* pole 1309 Hz, r 0.800, Q  7.0 */
	8192, -15686,  7643,	/* pole  169 Hz, r 0.966, Q 45.3 */
	2787,   1540,  2787	/* pole 2206 Hz, r 0.583, Q  2.9 */
};
