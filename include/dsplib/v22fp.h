/*
 * v22fp.h -- V.22 / V.22bis Fixed Point: the datapump object itself.
 *
 * `V22FP_create` is what lays this object out, and until it was reconstructed
 * nothing in the tree could say what any of it meant: `v22prc.h` reaches nine
 * of its fields through named constants and `void *` parameters precisely
 * because naming a struct would have been guessing.  This header is the
 * struct those constants were waiting for.
 *
 * Reconstructed from dsplibs.o:
 *   V22FP_create   .text 0x087990  2,449 bytes
 *   V22FP_delete   .text 0x088330    332 bytes
 *
 * THE ALLOCATION TREE.  Three allocations deep, and every size is a literal
 * in `V22FP_create`'s `sysdep_malloc` calls:
 *
 *     struct v22fp          0x5c  92 bytes, what create returns
 *       +0x50 -> hdx        0x40  64 bytes, the tone/detector context
 *         +0x24 -> iir      0x20  32 bytes, V22IIRFilterInit's state
 *       +0x54 -> dsp       0x1f8 504 bytes, every DSP block and buffer
 *         +0xa8  ->         0x18   24 bytes, never written by create
 *         +0xb4  ->         0xf0  240 bytes, 120 shorts: PPS I coefficients
 *         +0xb8  ->         0xf0  240 bytes, 120 shorts: PPS Q coefficients
 *         +0x1e4 ->        0x21c  540 bytes, 270 shorts: MRF coefficients
 *         +0x1e8 ->         0x62   98 bytes,  49 shorts: FSE I coefficients
 *         +0x1ec ->         0x62   98 bytes,  49 shorts: FSE Q coefficients
 *         +0x1f4 ->        0x154  340 bytes, never written by create
 *
 * plus whatever the four `FPM_*_create` calls and the six `*_init` calls
 * allocate underneath.  `V22FP_delete` releases exactly these eleven and the
 * four sub-objects, in a different order, and nothing else.
 *
 * THE DSP BLOCK IS FULLY TILED, which is the strongest single check on this
 * reading.  Nine sub-objects sit end to end with no slack at all --
 *
 *     sdm  +0x30 (0x18)   smc  +0x48 (0x30)   pps  +0x78 (0x28)
 *     mrf  +0xbc (0x14)   agc  +0xd0 (0x2c)   agc2 +0xfc (0x2c)
 *     sre +0x128 (0x3c)   fse +0x164 (0x64)   sdm2 +0x1cc (0x18)
 *
 * -- each starting exactly where the previous one ends, with sizes this tree
 * established independently from each block's own `init`.  Nine sizes and
 * nine addresses agreeing at once is not something a wrong layout does.
 *
 * 32-BIT LAYOUT.  These structs describe the memory of a 32-bit object, so
 * their reserved regions are byte counts that only hold when pointers are
 * four bytes wide.  The offset assertions in src/pump/v22/v22fp.c are
 * compiled only under that ABI and say so.
 *
 * WHAT IS NAMED, AND WHAT IS NOT.  The rule b103fp.h and v22_fse.h already
 * follow: a field gets a name only where an instruction forces its meaning.
 * A pointer handed to `FPM_SDM_init` is a `struct fpm_sdm` and there is
 * nothing else it could be; a word set to 1 and read by nothing is `rNN`.
 * Most of this object is still `rNN`, and that is the honest state of it --
 * `V22FP_process` and the seven `V22_PROTOCOL` handlers are what would give
 * the rest meaning, and none of them is reconstructed.
 */

#ifndef DSPLIB_V22FP_H
#define DSPLIB_V22FP_H

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"

struct fpm_mtd;
struct fpm_tone;

/*
 * ---------------------------------------------------------------------------
 * The CALLER'S configuration: seven ints, 28 bytes, built on the stack by
 * `v22_create` (.text 0x4fb0) and handed straight to `V22FP_create`.
 *
 * All seven are read as 32-bit words there, which is what settles the type;
 * three of them are read back here through a byte load, but only bit 0 is
 * used and on this ABI the two readings cannot be told apart.
 *
 * This is NOT the object's own parameter block -- see `struct v22fp_params`.
 * The two are the same size and different shapes, and confusing them is the
 * obvious mistake to make here.
 */
struct v22fp_cfg {
	/*
	 * +0x00 selects the station's role.  0, 1 and 2 each land in
	 * `params.mode`; ANYTHING ELSE, including a negative, leaves the
	 * V22_CFG template's value alone rather than selecting a default.
	 * `v22_create` passes `arg3 == 0`, so it only ever asks for 0 or 1.
	 */
	int mode;		/* +0x00 */
	/*
	 * +0x04 selects the bit rate: 0 gives 2400, 1 and 2 both give 1200,
	 * and anything else again leaves the template alone.  `v22_create`
	 * derives it from the datapump id -- 212 gives 2, 22 gives 1, and
	 * every other id gives 0.
	 */
	int rate;		/* +0x04 */
	int f08;		/* +0x08 copied to params.r08 and thence to
				 *       hdx.r04.  60000 from v22_create     */
	int f0c;		/* +0x0c bit 0 -> params.flags bit 10        */
	int f10;		/* +0x10 low 16 bits -> params.r18.  700     */
	int f14;		/* +0x14 bit 0 -> params.flags bit 11, which
				 *       forces hdx.r0e to 0 whatever the
				 *       mode selected                       */
	int f18;		/* +0x18 bit 0 -> params.flags bit 9         */
};

/*
 * ---------------------------------------------------------------------------
 * The OBJECT'S parameter block: 28 bytes, and `V22_CFG` is its template.
 *
 * `V22FP_create` copies `V22_CFG` to the stack, patches six of its fields
 * from `struct v22fp_cfg`, and copies the result into the first 28 bytes of
 * the object -- the same "copy the static, then patch" shape `b103_cfg`,
 * `v22_pps_cfg` and `v22_mrf_cfg` all have.
 *
 * The field widths come from the patch instructions: 16-bit stores at +0x00,
 * +0x02, +0x04, +0x14, +0x16 and +0x18, a 32-bit one at +0x08, and a
 * read-modify-write of the byte at +0x11.  Nothing here forces a SIGNEDNESS:
 * every load of a 16-bit field in this function discards its upper half, so
 * `movzwl` and `movswl` are interchangeable at every site (finding 614's
 * free column).
 */
struct v22fp_params {
	short mode;		/* +0x00 0, 1 or 2; selects hdx.r0e and the
				 *       (r2c, r2e) pair in the DSP block    */
	short bps;		/* +0x02 1200 or 2400                       */
	/*
	 * +0x04 is UNCONDITIONALLY a copy of +0x02 -- the store runs on every
	 * path -- so the two are always equal and no differential test can
	 * tell a swap of them apart.  Recorded rather than named.
	 */
	short bps2;		/* +0x04                                    */
	short r06;		/* +0x06 template 0                         */
	int r08;		/* +0x08 <- cfg.f08; template 120000        */
	int r0c;		/* +0x0c template 13014; read by nothing    */
	/*
	 * +0x10 is a bit set.  Bits 0, 1 and 2 come from the template and are
	 * copied out into three ints in the DSP block; bits 9, 10 and 11 are
	 * patched from the caller's configuration and bit 11 is read back
	 * here.  Bits 4 and 6 are set in the template and read by nothing.
	 *
	 * The original almost certainly wrote this as bitfields -- the
	 * accesses are byte-wide with shifts and masks.  It is one `unsigned
	 * int` here because the bytes are what the object settles and a
	 * bitfield declaration would additionally claim a packing order.
	 */
	unsigned int flags;	/* +0x10 template 0x65b                     */
	unsigned short r14;	/* +0x14 template 1; set to 1 again when the
				 *       mode is 2, which is invisible      */
	/*
	 * +0x16 is overwritten from V22DiconnectThreshTable[3] -- 150 -- on
	 * every path, so the template's 103 never survives.  Named for the
	 * table it is loaded from, which is the only evidence there is.
	 */
	short disconnect_thresh; /* +0x16                                   */
	short r18;		/* +0x18 <- (short)cfg.f10; template 0      */
	short r1a;		/* +0x1a template 0                         */
};

/*
 * ---------------------------------------------------------------------------
 * The tone and detector context, `struct v22fp`'s +0x50.  64 bytes.
 *
 * `ReadGTimer` in v22prc.c reaches +0x00 of this block through the object's
 * +0x50 and adds 20 to it, which is what names `gtimer`; nothing else here
 * is read by anything reconstructed.
 */
struct v22fp_hdx {
	int gtimer;		/* +0x00 the shared millisecond clock, zeroed
				 *       by create.  V22_OBJ_GTIMER         */
	int r04;		/* +0x04 <- params.r08                      */
	short r08;		/* +0x08 init 0                             */
	short r0a;		/* +0x0a init 0                             */
	short r0c;		/* +0x0c init 0                             */
	/*
	 * +0x0e is 1, 2 or 3 for modes 0, 1 and "anything else", and is then
	 * forced to 0 if `params.flags` bit 11 is set.  Nothing reconstructed
	 * reads it, so it keeps its offset for a name.
	 */
	short r0e;		/* +0x0e                                    */
	int r10;		/* +0x10 init 0                             */
	/*
	 * The four sub-objects, named for the configuration each is built
	 * with.  `FPM_TONE_create` and `FPM_MTD_create` take the existing
	 * pointer, so create zeroes all four before the first call and reuses
	 * them on any later one.
	 */
	struct fpm_tone *tone;	/* +0x14 TONEv22_CFG                        */
	struct fpm_mtd *mtd;	/* +0x18 MTDv22_CFG                         */
	struct fpm_mtd *mtd_s1;	/* +0x1c MTDs1_CFG                          */
	struct fpm_mtd *mtd2;	/* +0x20 MTDv22_CFG2                        */
	/*
	 * +0x24 is 32 bytes and is handed to `V22IIRFilterInit`, which is
	 * what makes it `short *`.  It is initialised only when the DSP
	 * block's r2e is 2 -- that is, only in mode 0 -- and stays at the
	 * allocator's fill otherwise.
	 */
	short *iir;		/* +0x24                                    */
	short r28;		/* +0x28 init 0                             */
	short pad2a;		/* +0x2a                                    */
	int r2c;		/* +0x2c init 0                             */
	short r30;		/* +0x30 init 0x2454                        */
	short r32;		/* +0x32 init 0                             */
	short r34;		/* +0x34 init 0                             */
	/*
	 * NOT written by create.  Six bytes rather than a shape, because
	 * nothing reconstructed reads any of it.
	 */
	unsigned char r36[6];	/* +0x36 .. +0x3b                           */
	int r3c;		/* +0x3c init 0                             */
};

/*
 * ---------------------------------------------------------------------------
 * The DSP block, `struct v22fp`'s +0x54.  504 bytes.
 *
 * Nine embedded sub-objects and seven heap buffers.  See the tiling note at
 * the top of this file: every sub-object's address here is `V22FP_create`
 * passing `dsp + N` to that block's own `init`, and every size is what this
 * tree already established for that block, so the two meet with no slack.
 */
struct v22fp_dsp {
	int r00;		/* +0x00 init 1                             */
	int r04;		/* +0x04 init 1                             */
	int r08;		/* +0x08 init 1                             */
	int r0c;		/* +0x0c init 1                             */
	/*
	 * +0x10 is `SetAdaptEqV22`'s "adapt" flag -- v22prc.h reaches it as
	 * V22FP_EQ_ADAPT through the object's +0x54, and mode 2 of that
	 * function sets it to 1, which is what create leaves here.  The
	 * assertion in v22fp.c ties the two spellings together.
	 */
	int eq_adapt;		/* +0x10 init 1                             */
	int r14;		/* +0x14 init 0                             */
	int r18;		/* +0x18 <- params.flags bit 0              */
	int r1c;		/* +0x1c <- params.flags bit 1              */
	int r20;		/* +0x20 <- params.flags bit 2              */
	unsigned char r24[4];	/* +0x24 not written by create              */
	/*
	 * +0x28 and +0x2a are `bps != 1200` and `bps2 != 1200`, and since
	 * `bps2` is always a copy of `bps` the two are always equal.  +0x28
	 * is read back by create to pick the scrambler width -- 4 bits at
	 * 2400, 2 at 1200 -- which is what says it means "2400".
	 */
	short r28;		/* +0x28                                    */
	short r2a;		/* +0x2a                                    */
	/*
	 * +0x2c and +0x2e are (1,2) for mode 0, (2,1) for mode 1 and
	 * (params.r14, params.r14) otherwise.  create reads both back: +0x2c
	 * picks the pulse-shaping carrier and +0x2e decides whether the IIR
	 * filter is initialised at all.
	 */
	short r2c;		/* +0x2c                                    */
	short r2e;		/* +0x2e                                    */
	struct fpm_sdm sdm;	/* +0x30 the scrambler                      */
	struct fpm_smc smc;	/* +0x48 the symbol mapper                  */
	struct v22_pps pps;	/* +0x78 the pulse-shaping interpolator     */
	unsigned char ra0[8];	/* +0xa0 not written by create              */
	void *ra8;		/* +0xa8 24 bytes, never written by create  */
	short rac;		/* +0xac init 0                             */
	short rae;		/* +0xae init 0                             */
	short rb0;		/* +0xb0 init 12                            */
	short padb2;		/* +0xb2                                    */
	/*
	 * The pulse shaper's two coefficient arrays: `PPSv22_COFFS` mixed up
	 * to the transmit carrier, cosine into I and sine into Q.  create
	 * fills them and then hands both to `V22_PPS_init` through
	 * `v22_pps_cfg`, which is what names them.
	 */
	short *pps_coff_i;	/* +0xb4 V22_PPS_COEFFS entries             */
	short *pps_coff_q;	/* +0xb8 V22_PPS_COEFFS entries             */
	struct v22_mrf mrf;	/* +0xbc the receive rate converter         */
	struct fpm_agc agc;	/* +0xd0 AGCv22_CFG                         */
	struct fpm_agc agc2;	/* +0xfc AGCv22_CFG2                        */
	struct v22_sre sre;	/* +0x128 the symbol-rate recovery loop     */
	struct v22_fse fse;	/* +0x164 the equaliser                     */
	/*
	 * +0x1c8 is where `fse.prev_quad` points.  v22_fse.h had already
	 * established that the slicers keep the previous quadrant OUTSIDE the
	 * equaliser and that "the datapump supplies it"; this is the datapump
	 * supplying it, and it is the two bytes immediately past the end of
	 * the equaliser.
	 */
	short prev_quad;	/* +0x1c8                                   */
	short pad1ca;		/* +0x1ca                                   */
	struct fpm_sdm sdm2;	/* +0x1cc the descrambler                   */
	/*
	 * `MRFv22_COFFS` mixed up to a carrier, cosine only.  Handed to
	 * `V22_MRF_init` through `v22_mrf_cfg`, which PERMUTES IT IN PLACE --
	 * see v22_mrf.h.  That is why create regenerates it from the
	 * prototype on every call rather than permuting it twice.
	 */
	short *mrf_coeff;	/* +0x1e4 V22_MRF_COEFFS entries            */
	/*
	 * `FSEv22_COFFS` mixed up to a carrier, cosine into I and sine into
	 * Q, and handed to `V22_FSE_init` through `v22_fse_cfg`.
	 */
	short *fse_coff_i;	/* +0x1e8 V22_FSE_TAPS entries              */
	short *fse_coff_q;	/* +0x1ec V22_FSE_TAPS entries              */
	unsigned char r1f0[4];	/* +0x1f0 not written by create             */
	void *r1f4;		/* +0x1f4 340 bytes, never written by create*/
};

/*
 * ---------------------------------------------------------------------------
 * The object, 92 bytes.
 *
 * Its first 28 bytes ARE the parameter block, embedded rather than duplicated
 * field by field, exactly as `struct b103fp` embeds `struct b103_cfg`.
 *
 * The five pointers at +0x20 .. +0x33 are copies of five `struct v22_fse`
 * fields, lifted to the top of the object so a caller can reach the
 * equaliser's output without knowing where the equaliser lives.  They are
 * named after the fields they are copied from, which v22_fse.h had already
 * established.
 */
struct v22fp {
	struct v22fp_params params;	/* +0x00 .. +0x1b                   */
	/*
	 * +0x1c .. +0x1f are cleared as one 32-bit word and then two bytes of
	 * it are set.  `status` and `flags` are b103fp.h's names for the two
	 * bytes in the same place in the same author's other datapump, and
	 * the shape is identical -- a byte set to 1 at the end of create and
	 * a flags byte that gets 0x40 -- so the analogy is recorded here and
	 * is NOT evidence.  Nothing reconstructed reads either.
	 */
	unsigned char status;		/* +0x1c set to 1 by create         */
	unsigned char flags;		/* +0x1d |= 0x40                    */
	unsigned char r1e[2];		/* +0x1e cleared with the above     */
	short *out_i;			/* +0x20 <- dsp->fse.out_i          */
	short *out_q;			/* +0x24 <- dsp->fse.out_q          */
	short *n_out;			/* +0x28 <- &dsp->fse.n_out         */
	short *icoeff;			/* +0x2c <- dsp->fse.icoeff         */
	short *qcoeff;			/* +0x30 <- dsp->fse.qcoeff         */
	short r34;			/* +0x34 init 0x31                  */
	short pad36;			/* +0x36                            */
	int r38;			/* +0x38 init 0                     */
	int r3c;			/* +0x3c init 0                     */
	short r40;			/* +0x40 init 0                     */
	short pad42;			/* +0x42                            */
	int r44;			/* +0x44 init 0                     */
	int r48;			/* +0x48 init 0                     */
	short r4c;			/* +0x4c init 0                     */
	short pad4e;			/* +0x4e                            */
	struct v22fp_hdx *hdx;		/* +0x50 V22_OBJ_GTIMER points here */
	struct v22fp_dsp *dsp;		/* +0x54 V22_OBJ_FP                 */
	unsigned char r58[4];		/* +0x58 never written              */
};

/*
 * Build a V.22 datapump.
 *
 * A NULL `fp` allocates the whole tree; a non-NULL one is RE-INITIALISED --
 * its sub-object pointers are reused, not tested for NULL, so unlike
 * `B103FP_create` this cannot be handed a zeroed buffer.  `cfg` is never
 * tested for NULL either.  Both are the object's behaviour and not a
 * simplification: `v22_create` is the only caller and always passes
 * (NULL, &cfg).
 */
struct v22fp *V22FP_create(struct v22fp *fp, const struct v22fp_cfg *cfg);

/*
 * Tear one down.  Frees the object itself unconditionally, exactly as
 * `B103FP_delete` does -- so this must not be called on anything
 * `V22FP_create(NULL, ...)` did not build.
 */
void V22FP_delete(struct v22fp *fp);

#endif /* DSPLIB_V22FP_H */
