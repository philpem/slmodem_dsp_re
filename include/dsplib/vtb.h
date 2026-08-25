/*
 * vtb.h -- the Viterbi trellis decoder shared by V.32bis and V.17.
 *
 * ONE DECODER, TWO PROTOCOLS.  `VTB_decoder` is a single 8-state Viterbi
 * decoder with a 16-symbol traceback; the protocol is entirely in the tables
 * a per-protocol `*_init` installs into the state.  The object carries two
 * sets: `VTBv32_*` for V.32bis (this header) and `VTBv17_*` for the fax V.17
 * receiver, which is a different batch and is not declared here.
 *
 * WHAT ONE CALL DOES.  Given one received symbol (I, Q) it
 *
 *   1. quantises the point onto a coarse square grid to pick a REGION,
 *   2. reads from `bound[]` the four (then another four) constellation
 *      points nearest that region -- one per trellis subset,
 *   3. computes a squared-distance branch metric for each,
 *   4. runs the add-compare-select over all eight states and four incoming
 *      branches each, writing the winner into a 16-deep ring of survivors,
 *   5. traces back 16 symbols from the state of least metric, and
 *   6. differentially decodes the two quadrant bits of the symbol that
 *      falls out, and writes the result through `out`.
 *
 * So the value written is the decision made SIXTEEN SYMBOLS AGO, which is
 * why the ring slot is read before it is overwritten.
 *
 * THE TWO COORDINATE FRAMES.  For the 16- and 64-point constellations
 * (`nsub` 1 and 3) the region lookup runs on the point rotated 45 degrees,
 * because those two constellations are the rotated ones and only the rotated
 * frame lines up with a rectangular region table.  The branch metric is
 * computed against the UNROTATED point, since `imap`/`qmap` hold the true
 * coordinates.  Getting that backwards is the obvious wrong reading, and
 * `t_v32vtb` counts the trials that separate them.
 */

#ifndef DSPLIB_VTB_H
#define DSPLIB_VTB_H

/*
 * One node of the survivor ring: which of the four incoming branches won,
 * and which constellation point that branch carried.
 *
 * `surv` is proven signed -- `VTB_decoder`'s traceback loads it `movswl` and
 * uses the 32-bit result as an index.  `sym` is a point index 0..127 and its
 * signedness is not forced anywhere.
 */
struct vtb_path {
	short sym;			/* +0x00 */
	short surv;			/* +0x02 */
};

/*
 * The decoder's state.  0x38 bytes, which is exactly the room V.32's
 * datapump object leaves for it between +0x18 and +0x50 (finding F1602).
 *
 * `bound` and `region` are proven `const short *`: both are loaded `movswl`
 * and both results are used as 32-bit indices.  `imap` and `qmap` are
 * `const short *` from `FPM_ECC_cancel`, which indexes the same tables
 * `movswl (%ecx,%eax,2)` through `ECCv32_IMAP` (finding F1614); here their
 * difference against the received point is re-narrowed to `short`, so the
 * load's extension carries nothing.
 */
struct vtb {
	struct vtb_path *paths;		/* +0x00 16 slots x 8 states       */
	short metric[8];		/* +0x04 per-state path metric     */
	unsigned short ring;		/* +0x14 slot index, modulo 16     */
	unsigned char pad16[2];		/* +0x16                           */
	const short *imap;		/* +0x18 constellation, in phase   */
	const short *qmap;		/* +0x1c constellation, quadrature */
	const short *bound;		/* +0x20 region -> nearest points  */
	unsigned short nsub;		/* +0x24 1..4; the bits below the
					 *       two differential ones      */
	unsigned char pad26[2];		/* +0x26                           */
	const short *region;		/* +0x28 grid cell -> bound index  */
	short grid;			/* +0x2c cells per axis, 2*nsub    */
	short depth;			/* +0x2e traceback length, 16      */
	short prev;			/* +0x30 previous quadrant         */
	unsigned short mask;		/* +0x32 (1 << (nsub + 2)) - 1     */
	short shift;			/* +0x34 = nsub                    */
};

/*
 * `out` is the only output: the decoded symbol, differentially resolved.
 * The return value is not used -- `FSE_decision_16Tpt` discards `%eax` and
 * reads its own local through `out`.
 */
void VTB_decoder(struct vtb *state, short i, short q, short *out);

/*
 * `mode` is the V.32bis rate code; `alloc` non-zero allocates the survivor
 * ring, which is why it is separate from the reset the rest of the function
 * does.  Modes 2, 3 and 4 are 9600, 7200 and 12000; anything else -- 14400
 * in practice -- takes the 128-point branch.
 */
void VTBv32_init(struct vtb *state, short mode, int alloc);

/*
 * V.32bis' four trellis constellations, and the region and boundary tables
 * that go with each.  All `.rodata` in the object, so all const.
 *
 * The lengths are not a reading of the bytes.  `region` is indexed
 * `ci + grid * (cq + (cq > ci ? grid : 0))`, whose largest value is
 * 2*grid*grid - 1, and 2*grid*grid is exactly the entry count of each of the
 * four at grid = 2, 4, 6, 8.  Every `region` entry is a multiple of 32, and
 * the decoder adds a quadrant offset of 0, 8, 16 or 24 and then reads eight
 * entries; the largest entry plus 32 is exactly the length of the matching
 * `bound` table for all four rates.  Neither closes if the element is four
 * bytes, so the widths are measured and not assumed.
 */
extern const short VTBv32_IMAP16T[16];
extern const short VTBv32_QMAP16T[16];
extern const short VTBv32_IMAP32[32];
extern const short VTBv32_QMAP32[32];
extern const short VTBv32_IMAP64[64];
extern const short VTBv32_QMAP64[64];
extern const short VTBv32_IMAP128[128];
extern const short VTBv32_QMAP128[128];

extern const short VTB_REGION_7200[8];
extern const short VTB_REGION_9600[32];
extern const short VTB_REGION_12000[72];
extern const short VTB_REGION_14400[128];

extern const short VTB_BOUND_7200[128];
extern const short VTB_BOUND_9600[416];
extern const short VTB_BOUND_12000[960];
extern const short VTB_BOUND_14400[1856];

#endif /* DSPLIB_VTB_H */
