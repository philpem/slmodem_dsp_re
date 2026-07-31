/*
 * cpfiltrs.c -- Call Progress Filters: four named bandpass designs.
 *
 * Reconstructed from dsplibs.o CPfiltrs.c, .rodata 0x626e..0x6355.  The whole
 * translation unit is data: it contributes no .text.
 *
 * `cadence_create` is the only caller; see cpfiltrs.h.  Ordered as the
 * original lays them out, which is by descending passband rather than by
 * name.
 *
 * Each section's numerator has b0 = b2 = 8192, so its zeros sit on the unit
 * circle: these are elliptic (Cauer) designs, and the zeros are what put the
 * 120 dB-deep nulls just above each passband.
 */

#include "dsplib/cpfiltrs.h"

/*
 * Busy and congestion: 480 + 620 Hz.  -6 dB from 396 to 670 Hz, peak 640 Hz,
 * null at 841 Hz.
 */
const short CP_450_630_scales[IIR_FILTER_SCALES] = { 5, 0, 0, 3, 0 };

const short CP_450_630_a[IIR_FILTER_COEFF] = {
	8192, -14092, 7471,
	8192, -14757, 7599,
	8192, -14031, 7911,
	8192, -15382, 8011
};

const short CP_450_630_b[IIR_FILTER_COEFF] = {
	8192,  -8156, 8192,
	8192, -16210, 8192,
	8192, -15893, 8192,
	8192, -12938, 8192
};

/*
 * Dial tone: 350 + 440 Hz.  -6 dB from 230 to 546 Hz, peak 416 Hz, null at
 * 1384 Hz.
 */
const short CP_276_504_scales[IIR_FILTER_SCALES] = { 5, 0, 3, 0, 0 };

const short CP_276_504_a[IIR_FILTER_COEFF] = {
	8192, -14644, 7313,
	8192, -15334, 7566,
	8192, -14686, 7832,
	8192, -15926, 8021
};

const short CP_276_504_b[IIR_FILTER_COEFF] = {
	8192,  -7618, 8192,
	8192, -16349, 8192,
	8192, -13488, 8192,
	8192, -16253, 8192
};

/*
 * The widest of the four: -6 dB from DC to 596 Hz, peak 452 Hz, null at
 * 1711 Hz.  The only one whose headroom is split between two stages
 * (shift 6 at the input and 2 after the first section) rather than taken all
 * at once, which its lower first-section Q makes possible.
 */
const short CP_100_550_scales[IIR_FILTER_SCALES] = { 6, 2, 0, 0, 0 };

const short CP_100_550_a[IIR_FILTER_COEFF] = {
	8192, -14227, 6339,
	8192, -14395, 7265,
	8192, -14513, 7880,
	8192, -14584, 8123
};

const short CP_100_550_b[IIR_FILTER_COEFF] = {
	8192,  -3688, 8192,
	8192, -13180, 8192,
	8192, -14233, 8192,
	8192, -14449, 8192
};

/*
 * Ringback: 440 + 480 Hz.  -6 dB from 263 to 898 Hz, peak 285 Hz, null at
 * 1245 Hz.  Its first section is the one design here with a positive b1, so
 * that zero sits above the Nyquist quarter rather than below.
 */
const short CP_350_600_scales[IIR_FILTER_SCALES] = { 5, 0, 0, 2, 0 };

const short CP_350_600_a[IIR_FILTER_COEFF] = {
	8192, -12974, 6657,
	8192, -14791, 7249,
	8192, -12339, 7588,
	8192, -15792, 7979
};

const short CP_350_600_b[IIR_FILTER_COEFF] = {
	8192,   1200, 8192,
	8192, -16343, 8192,
	8192, -16217, 8192,
	8192,  -9156, 8192
};
