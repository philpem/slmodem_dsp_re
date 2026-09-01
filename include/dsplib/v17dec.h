/*
 * v17dec.h -- the V.17 fax receiver's slicer tables and its four
 *             constellation decision functions.
 *
 * WHAT THIS IS.  `V17RX_create` installs a slicer into the fractionally
 * spaced equaliser as `fse.decision`, and which one it installs depends on
 * the negotiated bit rate.  The four rate-specific slicers and the two
 * handshake ones live in `src/fax/v17dec.c`; the constellation, magnitude and
 * angle tables they index live in `src/fax/v17dec_tables.c`.
 *
 * WHY THE TABLES ARE `short`, TWICE OVER.
 *
 *   1. THE LOAD IS SIXTEEN BITS AND THE INDEX SCALE IS ONE.  Every reference
 *      in the object is `movzwl 0x0(%reg,%reg,1)` or `movswl 0x0(%reg,%reg,1)`
 *      -- the index register appears twice with scale 1, which is `2*i`, and
 *      the load is a word.  That is forced encoding in the sense of finding
 *      F613: the compiler had no freedom about the width.  Both extensions
 *      appear on the SAME table (`DECv17_IMAP16` is read `movzwl` at 0x98470
 *      and `movswl` at 0x984ba), which is finding F7803 -- the extension
 *      follows the declared type of the LOCAL, not of the array -- and not a
 *      disagreement about the element type.
 *
 *   2. NINETEEN OF THE TWENTY-TWO ARE BYTE-IDENTICAL TO V.32bis' OWN, which
 *      this tree already reconstructed and tests, as `short`, in
 *      `src/pump/v32/v32dec_tables.c`.  That is a second, independent
 *      extraction of the same numbers agreeing with the first.
 *
 * AND THE THREE THAT ARE NOT IDENTICAL ARE THE POINT OF SAYING SO.  A
 * cross-check that came out 22 of 22 would be reporting that V.17 and V.32bis
 * share a translation unit, which they do not: the two sets are separate
 * symbols in separate sections at separate addresses.  The three that differ
 * are where V.17's own Recommendation departs from V.32bis':
 *
 *   DECv17_MAP_TRN     { 3, 0, 2, 1 }  vs V.32's { 1, 2, 0, 3 }
 *   DECv17_ANGL4800    { 9870, 18062, 26254, 1678 }
 *                      vs V.32's DECv32_ANGL1200 { 9869, 18061, 26253, 1678 }
 *   DECv17_MAP_BRIDGE  { 1, 0, 2, 3 }  -- V.32bis has no such table
 *
 * `DECv17_ANGL4800`'s first three entries are each exactly ONE more than
 * V.32's and the fourth is equal.  That is recorded as an observation, not
 * explained: nothing in the object says why, and a rounding story that fits
 * three of four values is not evidence.  The values are copied from the
 * object's bytes, which is what a byte-exact reconstruction requires whatever
 * the reason.
 *
 * NAMING.  V.17 names these tables by its own BIT RATES where V.32bis names
 * them by its: V.17's 7200 bit/s uses the sixteen-point constellation
 * V.32bis calls 9600, and V.17's 4800 bit/s handshake constellation is
 * V.32's 1200.  The 9600T, 12000 and 14400 names coincide between the two.
 * This is the object's own spelling in both cases, not a convention chosen
 * here.
 */

#ifndef DSPLIB_V17DEC_H
#define DSPLIB_V17DEC_H

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_fse;

/*
 * THE TABLES.  Element counts are `st_size / 2` and every one of them is
 * corroborated by the consumer, either by a loop bound the slicer compares
 * against or by the constellation size the rate implies.  See
 * `src/fax/v17dec_tables.c` for the per-table derivation.
 */

/* The four-point handshake constellation, and the two symbol remappings. */
extern const short DECv17_MAP_BRIDGE[4];
extern const short DECv17_MAP_TRN[4];
extern const short DECv17_ANGL4800[4];
extern const short DECv17_QMAP4[4];
extern const short DECv17_IMAP4[4];

/* 7200 bit/s: sixteen points, three L2 magnitudes. */
extern const short DECv17_MAG7200[3];
extern const short DECv17_ANGL7200[16];
extern const short DECv17_QMAP16[16];
extern const short DECv17_IMAP16[16];

/* 9600 bit/s trellis: eight points by four rotations. */
extern const short DECv17_ANGL9600T[32];
extern const short DECv17_MAG9600T[32];

/* The 45-degree rotation applied before the 32- and 128-point slicers. */
extern const short DECv17_SIN_ROT_ANGLE[4];
extern const short DECv17_COS_ROT_ANGLE[4];

/* The analytic quadrant maps the 32- and 128-point slicers rail against. */
extern const short DECv17_ANA_QMAP[8];
extern const short DECv17_ANA_QMAP128[32];
extern const short DECv17_ANA_IMAP128[32];

/* 12000 bit/s trellis: sixty-four points. */
extern const short DECv17_QMAP64[64];
extern const short DECv17_IMAP64[64];
extern const short DECv17_ANGL12000[64];
extern const short DECv17_MAG12000[64];

/* 14400 bit/s trellis: one hundred and twenty-eight points. */
extern const short DECv17_ANGL14400[128];
extern const short DECv17_MAG14400[128];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V17DEC_H */
