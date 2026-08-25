/*
 * dtmf_coeffs.c -- the DTMF receiver's notch banks.
 *
 * Reconstructed from dsplibs.o `Dtmf.c`:
 *
 *   biascoef  .data 0x008280   16 bytes
 *   us_coef   .data 0x0082a0  128 bytes
 *   eur_coef  .data 0x008320  128 bytes
 *
 * Held in their own translation unit so the mutation set for the detector's
 * logic is not competing with 68 float literals for anchors (finding F1264).
 *
 * SHAPE.  Four floats per section, eight sections, indexed by tone in the
 * order 697 770 852 941 1209 1336 1477 1633 Hz -- the same order as
 * `dtmf_test`'s energy array, low group then high.  `notch()` reads the four
 * as (c0, c1, c2, c3); notch.h has the transfer function.
 *
 * DESIGN, recovered rather than guessed (finding F1411).  Each section is
 *
 *     c0 = -2 cos(w0)   c1 = 2 r cos(w0)   c2 = -r*r   c3 = r
 *
 * with w0 = 2*pi*f/4000 and r the pole radius.  Solving c0 for f gives
 * exactly the eight DTMF frequencies -- but only at fs = 4000, which is why
 * dtmf_detect processes every second 8 kHz sample.  The identities
 * c2 == -c3*c3 and c1 == -c0*c3 hold in all seventeen sections here to
 * within 1e-6 relative, which is what a float carries, so the reading is
 * checked and not asserted.
 *
 * The two plans differ ONLY in r -- the zero angles, and so the tone
 * frequencies, are bit-identical between them.  The US bank's poles sit
 * closer to the unit circle (0.965..0.98 against 0.925..0.955), which makes
 * its notches narrower and so its frequency acceptance tighter.
 *
 * Byte-exact copies: docs/coefficients.md explains why these stay literal.
 */

#include "dsplib/dtmf.h"

/*
 * The bias notch: r = 0.85, w0 = 2*pi*50/4000.  A 50 Hz section, i.e. mains
 * hum, and only the European plan runs the input through it.
 */
const float biascoef[4] = {
	-1.99383497f, 1.69475901f, -0.722500026f, 0.850000024f,
};

/* r = 0.98 0.98 0.98 0.98 0.97 0.97 0.97 0.965 */
const float us_coef[4 * DTMF_TONES] = {
	-0.916368008f, 0.89804101f, -0.960399985f, 0.980000019f,   /*  697 */
	-0.706950009f, 0.692811012f, -0.960399985f, 0.980000019f,  /*  770 */
	-0.460779011f, 0.451563001f, -0.960399985f, 0.980000019f,  /*  852 */
	-0.185089007f, 0.181387007f, -0.960399985f, 0.980000019f,  /*  941 */
	0.644861996f, -0.625515997f, -0.940900028f, 0.970000029f,  /* 1209 */
	1.00724602f, -0.977029026f, -0.940900028f, 0.970000029f,   /* 1336 */
	1.36220896f, -1.32134199f, -0.940900028f, 0.970000029f,    /* 1477 */
	1.67677104f, -1.61808395f, -0.931225002f, 0.964999974f,    /* 1633 */
};

/* r = 0.955 0.95 0.945 0.945 0.945 0.93 0.93 0.925 */
const float eur_coef[4 * DTMF_TONES] = {
	-0.916368008f, 0.875132024f, -0.912024975f, 0.954999983f,  /*  697 */
	-0.706950009f, 0.671602011f, -0.902499974f, 0.949999988f,  /*  770 */
	-0.460779011f, 0.43543601f, -0.893024981f, 0.944999993f,   /*  852 */
	-0.185089007f, 0.174908996f, -0.893024981f, 0.944999993f,  /*  941 */
	0.644861996f, -0.609394014f, -0.893024981f, 0.944999993f,  /* 1209 */
	1.00724602f, -0.936739028f, -0.864899993f, 0.930000007f,   /* 1336 */
	1.36220896f, -1.26685405f, -0.864899993f, 0.930000007f,    /* 1477 */
	1.67677104f, -1.55101299f, -0.855624974f, 0.925000012f,    /* 1633 */
};
