/*
 * v32seq.c -- ITU-T V.32 / V.32bis: the rate-signal codec, the sequence
 *             generator and the sequence detector.
 *
 * Reconstructed from dsplibs.o:
 *
 *   RateToSeq          .text 0x0821e0    14
 *   SeqToRate          .text 0x0821f0   150
 *   CodeESeq           .text 0x082290   183
 *   CodeRateSeq        .text 0x082350   164
 *   CodeFinalRateSeq   .text 0x082400   164
 *   DecodeRateSeq      .text 0x0824b0   152
 *   InitGenSequence    .text 0x083670    68
 *   GenSequence        .text 0x0836c0   118
 *   InitDetSequence    .text 0x083740    52
 *   DetSequence        .text 0x083780   275
 *   GetSequence        .text 0x0838a0    11
 *   LoadReg            .text 0x0838b0    26
 *   StoreReg           .text 0x0838d0    29
 *
 *   V32_ESEQ           .data 0x007690    14
 *   V32_FINAL_RATE_SEQ .data 0x00769e    14
 *   V32_RATE_SEQ       .data 0x0076ac    14
 *
 * The header carries the layout and the tables' bit assignment.  This file
 * carries what had to be decided while writing the bodies.
 *
 * ---------------------------------------------------------------------------
 * THE LADDER IS AN INLINED STATIC, AND ITS ARM ORDER IS DESCENDING LINE RATE
 *
 * Five of the six rate functions open with the same instructions, so the
 * ladder is a `static` helper GCC inlined into each (finding F7940).  Its arms
 * are tried in the order 5, 4, {2,1}, 3, 0, which is NOT the numerical order
 * of the indices and IS the descending order of the line rates they stand for:
 * 14400, 12000, 9600 (trellis preferred), 7200, then the fallback.  The index
 * -> rate mapping is `V32FP_recreate`'s and is derived in v32seq.h, so this is
 * a fact about the object rather than a reading of it; a reader tempted to
 * "sort" these arms would be reversing 9600 and 7200.
 *
 * ---------------------------------------------------------------------------
 * WHY `SeqToRate` AND `DecodeRateSeq` BOTH EXIST
 *
 * They are the same computation.  The object's only difference is the last
 * instruction: `SeqToRate` ends `mov %ecx,%eax`, `DecodeRateSeq` ends
 * `movswl %cx,%eax`.  A 32-bit move where the other sign-extends 16 bits is
 * the return width and nothing else, so they are declared `int` and `short`
 * here.  Over the value range the ladder can produce -- 0 to 6 -- the two are
 * indistinguishable by any test, which is exactly finding F613's class of
 * difference: visible in one instruction and invisible to every differential
 * check.  It is recorded rather than hidden behind one shared signature.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT GUARDED, AND IS NOT GUARDED HERE EITHER
 *
 *   `RateToSeq` indexes `V32_RATE_SEQ` with its argument and does not range
 *   check it.  D404.
 *
 *   `InitGenSequence` divides by `width` with no test for zero.  D401.
 *
 *   `InitGenSequence` computes `(1 << width) - 1` with a variable shift, and
 *   `GenSequence` shifts by `index * width`.  Both are the x86 `shl`/`sar`
 *   with the count taken from `%cl`, which the hardware masks to five bits;
 *   in C a shift that wide is undefined.  Neither is reachable with the
 *   field widths the handshake uses (`total` and `width` are small), so this
 *   is recorded and not fixed.  D402.
 */

#include "dsplib/v32seq.h"

#include "dsplib/v32data.h"		/* V32_OBJ_FP, and only for that */

/* The instance is not modelled; see v32seq.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_S(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_US(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_I(obj, off)	(*(int *)(void *)FIELD((obj), (off)))

short V32_RATE_SEQ[V32_RATE_COUNT] = {
	0x0d11, 0x0b11, 0x0b91, 0x09d1, 0x09b1, 0x0ff9, 0x0997
};

short V32_FINAL_RATE_SEQ[V32_RATE_COUNT] = {
	0x0d11, 0x0b11, 0x0b91, 0x09d1, 0x09b1, 0x0999, 0x0997
};

/*
 * The E sequence.
 *
 * `V32_ESEQ[i] == V32_FINAL_RATE_SEQ[i] | 0xf000` at every one of the seven
 * indices -- INCLUDING index 5, where the FINAL table and `V32_RATE_SEQ`
 * differ, so it is the FINAL one this tracks and not the other.  That is a
 * property of the bytes, stated here so a reader who edits one table knows
 * which other one moved with it; the object holds three independent arrays and
 * so does this file.
 */
short V32_ESEQ[V32_RATE_COUNT] = {
	(short)0xfd11, (short)0xfb11, (short)0xfb91, (short)0xf9d1,
	(short)0xf9b1, (short)0xf999, (short)0xf997
};

/*
 * The best rate index this station and `seq` have in common.
 *
 * `static` because the object has no symbol for it: it is inlined into all
 * five of its callers and the worklist therefore never listed it.
 */
static int
v32_common_rate(void *modem, unsigned short seq)
{
	int rate = V32_RATE_NONE;
	void *fp = FIELD_PTR(modem, V32_OBJ_FP);
	short local = V32_RATE_SEQ[FIELD_S(fp, V32FP_RX_RATE_INDEX)];

	if ((seq & 0x0008) && (local & 0x0008))
		rate = 5;
	else if ((seq & 0x0020) && (local & 0x0020))
		rate = 4;
	else if ((seq & 0x0200) && (local & 0x0200))
		rate = ((seq & 0x0080) && (local & 0x0080)) ? 2 : 1;
	else if ((seq & 0x0040) && (local & 0x0040))
		rate = 3;
	else if ((seq & 0x0400) && (local & 0x0400))
		rate = 0;

	return rate;
}

unsigned short
RateToSeq(void *modem, short rate)
{
	(void)modem;			/* never read; D404 */

	return (unsigned short)V32_RATE_SEQ[rate];
}

int
SeqToRate(void *modem, unsigned short seq)
{
	return v32_common_rate(modem, seq);
}

short
DecodeRateSeq(void *modem, unsigned short seq)
{
	return (short)v32_common_rate(modem, seq);
}

unsigned short
CodeRateSeq(void *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);
	unsigned short out = V32_RATE_SEQ_NONE;

	if (rate != V32_RATE_NONE)
		out = (unsigned short)V32_RATE_SEQ[rate];

	return out;
}

unsigned short
CodeFinalRateSeq(void *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);
	unsigned short out = V32_RATE_SEQ_NONE;

	if (rate != V32_RATE_NONE)
		out = (unsigned short)V32_FINAL_RATE_SEQ[rate];

	return out;
}

/*
 * The E sequence for the negotiated rate.
 *
 * The object loads `V32_ESEQ[rate]` UNCONDITIONALLY, before the test, and
 * discards it on the no-rate path.  That is safe only because index 6 is
 * inside the table -- the ladder's "none" value is the table's last entry, not
 * one past it -- and it is why this is written as a conditional expression
 * rather than as the `if` the two rate functions above use.
 */
unsigned short
CodeESeq(void *modem, unsigned short seq)
{
	short rate = (short)v32_common_rate(modem, seq);

	return rate == V32_RATE_NONE ? V32_ESEQ_NONE
				     : (unsigned short)V32_ESEQ[rate];
}

void
InitGenSequence(void *modem, unsigned short pattern, unsigned short total,
		unsigned short width)
{
	void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);
	unsigned short top = (unsigned short)(total / width - 1);	/* D401 */

	FIELD_US(hdx, V32HDX_GEN_WIDTH) = width;
	FIELD_US(hdx, V32HDX_GEN_PATTERN) = pattern;
	FIELD_US(hdx, V32HDX_GEN_INDEX_MASK) = top;
	FIELD_US(hdx, V32HDX_GEN_INDEX) = top;
	FIELD_US(hdx, V32HDX_GEN_MASK) =
		(unsigned short)((1u << width) - 1u);		/* D402 */
}

/*
 * Emit `count` fields of the pattern.
 *
 * `width` and `pattern` are read once, before the loop; `mask` and
 * `index_mask` are read INSIDE it, every iteration.  That is not a choice
 * made here -- the first two are read before the first store to `*out` and
 * the last two after it, and a store through a `short *` may alias them, so
 * the compiler cannot hoist what follows it.  Written in the object's order
 * so the same thing happens.
 */
void
GenSequence(void *modem, short *out, unsigned short count)
{
	void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);
	int width = FIELD_US(hdx, V32HDX_GEN_WIDTH);
	int pattern = FIELD_US(hdx, V32HDX_GEN_PATTERN);
	unsigned short index = FIELD_US(hdx, V32HDX_GEN_INDEX);
	unsigned short i;

	for (i = 0; i < count; i++) {
		*out++ = (short)((pattern >> (index * width)) &
				 FIELD_US(hdx, V32HDX_GEN_MASK));	/* D402 */
		index = (unsigned short)((index - 1) &
					 FIELD_US(hdx, V32HDX_GEN_INDEX_MASK));
	}

	FIELD_US(hdx, V32HDX_GEN_INDEX) = index;
}

void
InitDetSequence(void *modem, int target, int mask, int out_mask,
		unsigned short width)
{
	void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);

	FIELD_US(hdx, V32HDX_DET_WIDTH) = width;
	FIELD_I(hdx, V32HDX_DET_OUT_MASK) = out_mask;
	FIELD_I(hdx, V32HDX_DET_TARGET) = target;
	FIELD_I(hdx, V32HDX_DET_MASK) = mask;
	FIELD_I(hdx, V32HDX_DET_REG) = 0;
	FIELD_I(hdx, V32HDX_DET_MATCH) = 0;
}

/*
 * Watch `count` words for the armed pattern.
 *
 * Two things about this are worth stating because neither is what a reader
 * expects.
 *
 * The inner loop DOES NOT STOP at the match.  It runs all `width` bits of the
 * word out, and a second match later in the same word overwrites the first --
 * so what is left at V32HDX_DET_MATCH is the LAST match in the word the
 * detector fired on, not the first.  The `found` flag is only tested after
 * the word is finished.
 *
 * The register is written back on both exits.  On the no-match path that is
 * the whole point: the detector keeps its position between calls, so a
 * pattern straddling two calls is still found.
 */
short
DetSequence(void *modem, const short *data, unsigned short count)
{
	void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);
	int nbits = FIELD_US(hdx, V32HDX_DET_WIDTH);
	int reg = FIELD_I(hdx, V32HDX_DET_REG);
	int mask = FIELD_I(hdx, V32HDX_DET_MASK);
	int target = FIELD_I(hdx, V32HDX_DET_TARGET);
	short nread = 0;
	unsigned short i;

	for (i = 0; i < count; i++) {
		unsigned int word = (unsigned short)*data++;
		int found = 0;
		short bit;

		nread++;
		for (bit = 0; bit < nbits; bit++) {
			reg = (reg << 1) |
			      (int)((word >> (nbits - 1 - bit)) & 1u);

			/*
			 * A BITWISE AND, and the object's own encoding: both
			 * tests become `sete`/`setne` bytes with no branch
			 * between them.  Both operands are pure, so `&&`
			 * cannot differ behaviourally -- `&` is written
			 * because it is what the instructions say, the same
			 * call src/pump/b103/b103fp.c makes for
			 * CarrierDetectB103.
			 */
			if (((reg & mask) == target) & (reg != -1)) {
				FIELD_I(hdx, V32HDX_DET_MATCH) =
					reg & FIELD_I(hdx, V32HDX_DET_OUT_MASK);
				FIELD_I(hdx, V32HDX_DET_REG) = reg;
				found = 1;
			}
		}

		if (found)
			return nread;
	}

	FIELD_I(hdx, V32HDX_DET_REG) = reg;

	return -1;
}

int
GetSequence(void *modem)
{
	void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);

	return FIELD_I(hdx, V32HDX_DET_MATCH);
}

short
LoadReg(void *modem, short reg)
{
	short value = 0;

	if (reg >= 0 && reg <= V32HDX_NREGS - 1) {
		void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);

		value = *((short *)(void *)FIELD(hdx, V32HDX_REGS) + reg);
	}

	return value;
}

void
StoreReg(void *modem, short value, short reg)
{
	if (reg >= 0 && reg <= V32HDX_NREGS - 1) {
		void *hdx = FIELD_PTR(modem, V32_OBJ_HDX);

		*((short *)(void *)FIELD(hdx, V32HDX_REGS) + reg) = value;
	}
}
