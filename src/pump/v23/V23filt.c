/*
 * V23filt.c -- ITU-T V.23: the global coefficient tables.
 *
 * This translation unit has no code in it, and did not in the original
 * either: the object's STT_FILE entry for V23filt.c is followed immediately
 * by bwchdem.c's, with no local symbols of its own in between.  A file that
 * contributes no .text and no statics can only be contributing globals, and
 * these four are the ones that sit in the address range it has to occupy.
 *
 * Extracted verbatim from dsplibs.o's .rodata by tools/tabdump.py.  These are
 * reference bytes: the maintainable form is a generator that reproduces them
 * from the filters' design parameters, as with b103_tables.c.  Until then the
 * values are the design.
 *
 * See include/dsplib/v23fp.h for which filter each one is and how that was
 * established -- from v23FP_rx_create's argument passing, not from the names.
 */

#include "dsplib/v23fp.h"

/*
 * 48 taps, symmetric about the centre pair, for FPM_MRF.  The zero crossings
 * either side of the main lobe and the ~2:1 decay of the sidelobes are a
 * windowed-sinc signature; the table is its own best documentation until the
 * generator exists.
 */
const short _V23_MRF_FILT[48] = {
	738, 802, 876, 630, 52, -708, -1366, -1592,
	-1168, -116, 1238, 2338, 2592, 1636, -446, -3024,
	-5056, -5386, -3170, 1772, 8764, 16400, 22894, 26624,
	26624, 22894, 16400, 8764, 1772, -3170, -5386, -5056,
	-3024, -446, 1636, 2592, 2338, 1238, -116, -1168,
	-1592, -1366, -708, 52, 630, 876, 802, 738,
};

/*
 * 15 taps, the demodulator's input interpolator.  Not symmetric: tap 7 is
 * 16011 of a possible 16384 and everything around it alternates sign while
 * decaying, which is a fractional delay of very nearly one sample rather than
 * a shaping filter.
 */
const short _V23RX_ANSWER_INTRP[15] = {
	22, -40, 98, -213, 421, -830, 2045, 16011,
	-1613, 738, -389, 201, -93, 39, -21,
};

/*
 * 3 biquads, Q14, in FPM_iir_filt's order: { -a1, b1, -a2, b2, b0 }.
 * The discriminator lowpass, handed to FPM_FSD_init as its config's `iir`.
 */
const short _V23RX_IIR_LPF[15] = {
	-3965, 510, 13254, 837, 510,
	-9487, 16384, 10172, 7679, 16384,
	-14380, 16384, 8308, 157, 16384,
};

/*
 * 4 biquads, Q14, in FPM_iir_filt_II's order: { b0, b2, b1, a2, a1 }.
 * The receiver's channel filter; v23FP_rx_create stores it at +0xb4 with the
 * section count 4 at +0xbc, and v23FP_rx_progress runs the input block
 * through it.
 *
 * b0 == b2 in every section, which is the ordering check -- see v23fp.h.
 */
const short V23_IIR_FILT[20] = {
	2440, 2440, 4264, 9925, -1474,
	4096, 4096, -7803, 10415, -11355,
	4096, 4096, -6698, 13932, 4857,
	8192, 8192, 9272, 14342, -17972,
};
