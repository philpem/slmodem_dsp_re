/*
 * v32seq.h -- ITU-T V.32 / V.32bis: the rate-signal codec, the sequence
 *             generator and the sequence detector.
 *
 * Thirteen functions and three tables.  They are the arithmetic under V.32's
 * call setup: turning a rate-signal word into a rate index and back, shifting
 * a fixed bit pattern out one field at a time, and watching a bit stream for a
 * pattern to arrive.  Nothing here touches the signal path.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE IS NOT MODELLED, following `include/dsplib/v32data.h`'s ruling
 * and `include/dsplib/v22data.h`'s before it: the parameter is `void *` and
 * the offsets are named constants.  Two pointers hang off the modem object --
 *
 *     obj + 0x64   the half-duplex / handshake context, `V32_OBJ_HDX`
 *     obj + 0x68   the DSP block, `V32_OBJ_FP`, v32data.h's
 *
 * -- and everything in this file except the rate codec lives in the first.
 * The 0x64 pointer is established by `V32FP_recreate` at 0x7e870, which loads
 * it and stores the initial `TxHdxNull` / `RxHdxTone` state pointers into
 * +0x6c and +0x70 of what it points at.
 *
 * ---------------------------------------------------------------------------
 * ONE INLINED HELPER, SIX COPIES OF IT
 *
 * `SeqToRate`, `DecodeRateSeq`, `CodeRateSeq`, `CodeFinalRateSeq` and
 * `CodeESeq` all open with the SAME 0x60-odd bytes: the far end's rate signal
 * in one register, `V32_RATE_SEQ[fp->0x2a]` in another, and a five-armed
 * ladder that picks the best rate both ends support.  That is a `static`
 * helper the compiler inlined into each of them, and an inlined static has no
 * symbol of its own (finding F7940), which is why the worklist shows five
 * functions and not six.  It is written here as `v32_common_rate` and is
 * static for the same reason.
 *
 * ---------------------------------------------------------------------------
 * THE TABLES ARE IN `.data`, NOT `.rodata`, SO THEY ARE NOT `const`
 *
 * All three sit in section 143, which is `.data` (CLAUDE.md: `.data` rather
 * than `.rodata` says not `const`), and all three are GLOBAL, so they are not
 * `static` either.  Nothing in the object writes them; the author simply did
 * not write `const`.  Reproduced as declared rather than as used.
 */

#ifndef DSPLIB_V32SEQ_H
#define DSPLIB_V32SEQ_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The half-duplex / handshake context pointer.  Also spelled in the headers
 * of the sibling files that reach it; the guard is so that including two of
 * them is not an error.
 */
#ifndef V32_OBJ_HDX
#define V32_OBJ_HDX		0x64
#endif

/*
 * The five scratch registers `LoadReg` and `StoreReg` address.  What they
 * HOLD is not established -- nothing in this batch reads them -- so they are
 * named for their shape and not for a meaning.  The bound is the object's:
 * `cmp $0x4,%dx` with an unsigned branch, so index 4 is the last valid one
 * and the array has five entries.
 */
#define V32HDX_REGS		0x3c
#define V32HDX_NREGS		5

/*
 * The sequence GENERATOR's state.  `InitGenSequence` sets all five and
 * `GenSequence` walks `index` down through them.
 *
 * `pattern` is a 16-bit word holding `total / width` fields of `width` bits
 * each; `index` selects which field comes out next and counts DOWNWARDS,
 * starting at the top field and wrapping through `index_mask`.
 */
#define V32HDX_GEN_INDEX	0x4a	/* unsigned short, the next field      */
#define V32HDX_GEN_INDEX_MASK	0x4c	/* unsigned short, total/width - 1     */
#define V32HDX_GEN_WIDTH	0x4e	/* unsigned short, bits per field      */
#define V32HDX_GEN_MASK		0x50	/* unsigned short, (1 << width) - 1    */
#define V32HDX_GEN_PATTERN	0x52	/* unsigned short, the word itself     */

/*
 * The sequence DETECTOR's state.  `InitDetSequence` sets the first four and
 * clears the last two; `DetSequence` shifts bits into `reg` and reports a hit
 * when `(reg & mask) == target` and `reg` is not all-ones.
 */
#define V32HDX_DET_WIDTH	0x54	/* unsigned short, bits per input word */
#define V32HDX_DET_OUT_MASK	0x58	/* int, applied to reg on a hit        */
#define V32HDX_DET_TARGET	0x5c	/* int, what a match looks like        */
#define V32HDX_DET_MASK		0x60	/* int, which bits are compared        */
#define V32HDX_DET_REG		0x64	/* int, the shift register             */
#define V32HDX_DET_MATCH	0x68	/* int, reg & out_mask at the hit      */

/*
 * A `short` in the DSP block that indexes `V32_RATE_SEQ`, so its entry is this
 * station's own rate signal.  THAT IS ALL THAT IS ESTABLISHED and the name
 * says no more: the object gives no format string for it in this batch, and
 * `V32FP_status`'s "tx_rate=%d,rx_rate=%d" -- which is where a real name would
 * come from -- is not reconstructed yet.  Naming it `tx_rate` would be a guess
 * a future reader could not tell from a derivation.
 */
#define V32FP_RATE_INDEX	0x2a

/*
 * The rate indices.  0..5 are real rates and 6 means "no rate in common",
 * which is the value every caller in this file tests against.  Which line
 * rate each index IS is not established here -- it comes from the rate-signal
 * bit assignments in V.32bis, and the object only ever uses the index -- so
 * they are numbered and not named.
 */
#define V32_RATE_NONE		6
#define V32_RATE_COUNT		7

/*
 * The three rate-signal tables, 7 shorts each, `.data`, GLOBAL.
 *
 *   V32_RATE_SEQ         the rate sequence sent during negotiation
 *   V32_FINAL_RATE_SEQ   the same, differing only at index 5
 *   V32_ESEQ             the E sequence, which is FINAL_RATE_SEQ | 0xf000
 *                        at every index -- index 5 included, so it follows
 *                        the FINAL table and not RATE_SEQ
 *
 * Each index owns a distinctive bit, which is what makes the ladder in
 * `v32_common_rate` a search for the best rate both ends set:
 *
 *   index | RATE_SEQ | bit tested | FINAL_RATE_SEQ | ESEQ
 *   ------|----------|------------|----------------|-------
 *     0   |  0x0d11  |   0x0400   |     0x0d11     | 0xfd11
 *     1   |  0x0b11  |   0x0200   |     0x0b11     | 0xfb11
 *     2   |  0x0b91  | 0x0200+0x80|     0x0b91     | 0xfb91
 *     3   |  0x09d1  |   0x0040   |     0x09d1     | 0xf9d1
 *     4   |  0x09b1  |   0x0020   |     0x09b1     | 0xf9b1
 *     5   |  0x0ff9  |   0x0008   |     0x0999     | 0xf999
 *     6   |  0x0997  |    none    |     0x0997     | 0xf997
 *
 * Index 5's RATE_SEQ entry has every one of those bits, which is why it is
 * the first arm of the ladder.  Index 6 has none of them and is never
 * selected by the ladder -- `CodeRateSeq` and `CodeESeq` substitute a literal
 * for it instead.  The correspondence is READ OFF THE BYTES; it is not a
 * derivation of the V.32bis bit assignments, and regenerating these from the
 * Recommendation is deferred work like every other coefficient table here.
 */
extern short V32_RATE_SEQ[V32_RATE_COUNT];
extern short V32_FINAL_RATE_SEQ[V32_RATE_COUNT];
extern short V32_ESEQ[V32_RATE_COUNT];

/*
 * What `CodeRateSeq` and `CodeESeq` send when there is no rate in common.
 * Bare literals in the object, named here because the two differ only in the
 * top nibble and a reader needs to see that.
 */
#define V32_RATE_SEQ_NONE	0x111
#define V32_ESEQ_NONE		0xf111

/* ------------------------------------------------------------------------ */

/*
 * The rate signal for `rate`, straight out of the table.  `modem` IS NOT READ
 * -- the object never touches 0x4(%esp) -- but it is the first parameter of
 * every other function here and is kept for that reason.  There is no bound
 * check: the object indexes the table with whatever it is given.
 */
unsigned short RateToSeq(void *modem, short rate);

/*
 * The best rate index this station and `seq` have in common, or
 * V32_RATE_NONE.  `SeqToRate` and `DecodeRateSeq` compute exactly the same
 * thing and differ only in the width they return it in; see the note in
 * v32seq.c.
 */
int SeqToRate(void *modem, unsigned short seq);
short DecodeRateSeq(void *modem, unsigned short seq);

/* The same decision, re-encoded as the word to send back. */
unsigned short CodeRateSeq(void *modem, unsigned short seq);
unsigned short CodeFinalRateSeq(void *modem, unsigned short seq);
unsigned short CodeESeq(void *modem, unsigned short seq);

/*
 * Set the generator up to emit `total / width` fields of `width` bits each
 * out of `pattern`, most significant field first.
 *
 * `total / width` is an UNSIGNED divide (`div`, with `%edx` zeroed) and there
 * is no guard on `width` being zero, so asking for zero-bit fields divides by
 * zero in the object exactly as it does here.
 */
void InitGenSequence(void *modem, unsigned short pattern,
		     unsigned short total, unsigned short width);

/* Emit `count` fields into `out`, one short each, wrapping round the word. */
void GenSequence(void *modem, short *out, unsigned short count);

/*
 * Arm the detector.  `target` is what `reg & mask` must equal; `out_mask` is
 * applied to `reg` and left at V32HDX_DET_MATCH when it does; `width` is how
 * many bits of each input word are shifted in, most significant first.
 */
void InitDetSequence(void *modem, int target, int mask, int out_mask,
		     unsigned short width);

/*
 * Shift `count` words of `width` bits through the register and return the
 * 1-based index of the word the match completed in, or -1 if none did.
 *
 * The register is written back on BOTH paths, so a run that finds nothing
 * still leaves the detector where it got to and the next call continues.
 */
short DetSequence(void *modem, const short *data, unsigned short count);

/* The value the last successful DetSequence left at V32HDX_DET_MATCH. */
int GetSequence(void *modem);

/*
 * The five scratch registers.  Out of range is a no-op on the way in and zero
 * on the way out -- the object's own behaviour, not a guard added here.
 *
 * NOTE THE ARGUMENT ORDER, which is the object's: `LoadReg` takes the index
 * second and `StoreReg` takes it THIRD, with the value second.
 */
short LoadReg(void *modem, short reg);
void StoreReg(void *modem, short value, short reg);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32SEQ_H */
