/*
 * dtmf_mtd_coeffs.c -- the DTMF tone bank's fixed-point coefficients.
 *
 * Reconstructed from dsplibs.o `Dtmf_Detector.c`, .data 0x007880 .. 0x00791f:
 * sixteen ten-byte tables, MTD1..MTD8 at each of the two line rates.
 *
 * SHAPE.  Five shorts, one `FPM_iir_filt` section, Q14 throughout:
 *
 *     { a2, b2, a1, b1, b0 }
 *
 * in FPM_iir_filt's own order, giving
 *
 *              b0 + b1 z^-1 + b2 z^-2       1 - 2cos(w0) z^-1 + z^-2
 *     H(z) = ------------------------  =  -----------------------------
 *              1 - a1 z^-1 - a2 z^-2       1 - 2r cos(w0) z^-1 + r^2 z^-2
 *
 * so b0 = b2 = 16384 (1.0), b1 = -round(2 cos(w0) * 16384),
 * a1 = round(2 r cos(w0) * 16384) and a2 = -round(r^2 * 16384) = -13271,
 * with r = 0.9 in every table.  A NOTCH, like the float bank in
 * dtmf_coeffs.c -- zeros on the unit circle at the tone, poles just inside
 * it -- so what comes out is the signal MINUS that tone.
 *
 * CLOSED FORM, and what it is worth (finding 1413).  Solving a1 or b1 back
 * for w0 gives the eight DTMF frequencies at the table's own rate, to within
 * 0.05 Hz, for fifteen of the sixteen tables.  The generator reproduces those
 * fifteen to within +-1 LSB but NOT bit-exactly, so these stay literal bytes
 * and the derivation serves regeneration at a third rate rather than
 * replacing the table.
 *
 * The _8000 and _9600 members of a pair are NOT related to each other: each
 * is generated independently from the tone frequency and its own rate, and
 * neither can be resampled into the other.
 *
 * MTD7_COEF_9600 is the sixteenth.  Its a1 fits 1477 Hz like every other
 * table, and its b1 does not -- it is -21143 where the design gives -18613,
 * putting the notch's ZEROS at 1328 Hz while its POLES stay at 1477.  The
 * bytes are reproduced as they are; D250 records it and nothing here
 * speculates about how it happened.  It is NOT cosmetic: at 9600 Hz eleven
 * of the sixteen DTMF pairs come back with the wrong high-group tone, and
 * all eleven are 1477 Hz being chosen when absent or missed when present
 * (finding 1416).  At 8000 Hz all sixteen decode.
 */

#include "dsplib/dtmf_rx.h"

const short MTD1_COEF_8000[5] = { -13271, 16384, 25182, -27979, 16384 };
const short MTD2_COEF_8000[5] = { -13271, 16384, 24261, -26957, 16384 };
const short MTD3_COEF_8000[5] = { -13271, 16384, 23131, -25702, 16384 };
const short MTD4_COEF_8000[5] = { -13271, 16384, 21797, -24219, 16384 };
const short MTD5_COEF_8000[5] = { -13271, 16384, 17166, -19073, 16384 };
const short MTD6_COEF_8000[5] = { -13271, 16384, 14692, -16325, 16384 };
const short MTD7_COEF_8000[5] = { -13271, 16384, 11777, -13084, 16384 };
const short MTD8_COEF_8000[5] = { -13271, 16384, 8384, -9314, 16384 };

const short MTD1_COEF_9600[5] = { -13271, 16384, 26475, -29417, 16384 };
const short MTD2_COEF_9600[5] = { -13271, 16384, 25824, -28695, 16384 };
const short MTD3_COEF_9600[5] = { -13271, 16384, 25023, -27804, 16384 };
const short MTD4_COEF_9600[5] = { -13271, 16384, 24073, -26747, 16384 };
const short MTD5_COEF_9600[5] = { -13271, 16384, 20731, -23034, 16384 };
const short MTD6_COEF_9600[5] = { -13271, 16384, 18917, -21019, 16384 };
/* b1 = -21143 is the object's; the design gives -18613.  D250. */
const short MTD7_COEF_9600[5] = { -13271, 16384, 16751, -21143, 16384 };
const short MTD8_COEF_9600[5] = { -13271, 16384, 14190, -15768, 16384 };
