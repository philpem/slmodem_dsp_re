/*
 * V92DILdescriptorPacker.cpp -- pack a tagV90DILdescriptor into the V.92
 * form of its bit stream.
 *
 * Finding 837 is the record entry for this file; this comment is still the
 * full derivation and 837 is the summary that makes it findable.
 *
 * Reconstructed from dsplibs.o, 4,160 bytes at 0x50020.  A leaf apart from
 * its diagnostics: its only relocations are `dsplibs_debug_level`,
 * `dsplibs_debug_printf`, three floats in .rodata.cst4 and eight format
 * strings.  No data tables.  `V92Modem::reset` is the only caller.
 *
 * THE SIGNATURE IS FORCED BY THE MANGLING -- see the header.  What follows is
 * what the object does, and every line of it comes out of the disassembly.
 *
 * ------------------------------------------------------------------------
 * WHAT IT PACKS, and how it differs from DILdescriptorPacker
 * ------------------------------------------------------------------------
 *
 * The stream is a run of 17-position frames.  Position 17k of every frame is
 * a 0 and the sixteen after it carry data; frame 0 is the exception and is
 * seventeen 1s, which is the one pattern the framing rules out.  Every field
 * is least significant bit first.  Positions 0 to 51 are byte for byte the
 * V.90 packer's:
 *
 *   frame 0        seventeen 1s
 *   frame 1        dilCount, eight bits, then eight 0s
 *   frame 2        seq1Length - 1 and seq2Length - 1, seven bits each,
 *                  each followed by a 0
 *   frames 3..     seq1, one byte per position, padded with 0s to a whole
 *                  number of frames; then seq2 the same way
 *   next 8 frames  segmentSize[0..7] then segmentCode[0..7], seven bits
 *                  each, each followed by a 0, two to a frame
 *   next frames    dilCode[0..], seven bits each, each followed by a 0, two
 *                  to a frame; ceil(dilCount / 2) frames
 *
 * and then the V.92 stream diverges.  Where the V.90 packer puts the CRC in
 * the next frame and stops, this one inserts TWO frames first:
 *
 *   next frame     sixteen 1s
 *   next frame     sixteen 0s
 *   last frame     the CRC of every data bit so far, the two new frames
 *                  included
 *
 * and then one more 0, and a second one if that would leave the count odd.
 *
 * THE INSERTED PAIR IS THE ORIGINAL'S "V.92 info".  Two diagnostics name it:
 * `last bit before V.92 info = %d` prints the framing position of the frame
 * the V.90 packer would have put the CRC in, and `last bit after V.92 info =
 * %d` prints that position plus 34 -- two frames later -- which is where the
 * CRC frame actually starts.  The nineteen values written are a nineteen-byte
 * local the prologue fills with sixteen 1s and three 0s, and the thirteen
 * positions after them are zeroed outright; the net content of the two frames
 * is sixteen 1s and sixteen 0s.  It is spelled here the way the object spells
 * it, because a shorter spelling would be a guess about the source.
 *
 * Three further differences, all of them measured:
 *
 *  - THE CEILING IS A DIFFERENT FUNCTION.  The V.90 packer multiplies by a
 *    reciprocal and, when the product is above zero, truncates
 *    `product + 0.99999999`; this one sets the x87 rounding control to
 *    round-toward-+infinity and uses FRNDINT, with no guard on the sign.
 *    That is `ceil()`, and it is written as `ceil()`.  Over the only domain
 *    that can reach it -- lengths and counts held in an unsigned char, scaled
 *    by 0.0625 or 0.5, both exact powers of two -- every product is exact and
 *    the two spellings agree on all 256 values; t_v92dilpack drives every one
 *    of them through both sides.
 *
 *  - THE DIL FRAME COUNT IS A DOUBLE, AND IS RE-EVALUATED EVERY ITERATION.
 *    The loop's exit test is FCOMPP against an FILDL of the counter, and the
 *    body reloads `dilCount` from the descriptor and redoes the FRNDINT each
 *    time round.  That is what `unsigned char *bits` costs: the stores may
 *    alias the descriptor, so the bound is not loop-invariant to the
 *    compiler.  Written re-evaluated, which is what the object does.  No
 *    differential test can see this -- the fixture's buffers do not overlap
 *    the descriptor, and if they did the contract would already be broken.
 *
 *  - THE COUNT IS AN `int` AND THE STREAM IS BYTES.  `*nbits` is one 32-bit
 *    store; every stream position is one `movb`.
 *
 * The CRC is the V.90 packer's, unchanged: sixteen stages, feedback of
 * stage 0 plus the incoming bit added back at stages 15, 10 and 3, which is
 * x^16 + x^12 + x^5 + 1 with the oldest stage written first, the register
 * starting all 1s.  It runs over the data positions of every frame from 1 up
 * to but not including the CRC's own -- so, unlike the V.90 stream, over the
 * V.92 info as well.  Frame 0 is not covered.
 *
 * Names for the struct's fields are this tree's, from the V.90 packer, whose
 * header declares the descriptor.  The ARGUMENT names in the diagnostics are
 * the original's own -- `pParamObj`, `BitVector`, `Cnt`, `N`, `Lsp`, `Ltp`.
 */

#include <math.h>

#include "dsplib/V92DILdescriptorPacker.h"
#include "dsplib/debug.h"

/* One framing position plus sixteen data positions. */
#define DIL_FRAME	17

/*
 * The first data position of frame 3, where the seq1 field starts.  Frames 0
 * to 2 are fixed, so the object spells this as the literal 0x34 in the
 * divisibility test that inserts framing bits and in every store.
 */
#define DIL_SEQ1_AT	52

/* The segment fields are eight frames, two seven-bit fields to a frame. */
#define DIL_SEGMENT_BITS	(8 * DIL_FRAME)

/*
 * The object's ceiling: FRNDINT under a rounding control of
 * round-toward-+infinity, which is `ceil`, with the argument arriving as a
 * 16-bit integer (FILDS after a MOVZBW) scaled by a float.  It keeps the
 * result as a double because the DIL loop compares against it as one.
 */
static double
dilCeiling(int n, float scale)
{
	return ceil((double)(short)n * scale);
}

/*
 * A seven-bit field, least significant bit first, and the 0 that follows it.
 * Twenty of the object's fields are this shape and it writes all twenty out
 * longhand.  `value` is signed because two call sites pass a length less one,
 * which is -1 when the length is 0 and shifts down as -1 -- seven 1s, as the
 * object's `sar` gives.
 */
static void
dilPackSeven(unsigned char *bits, int at, int value)
{
	int i;

	for (i = 0; i < 7; i++, value >>= 1)
		bits[at + i] = (unsigned char)(value & 1);
	bits[at + 7] = 0;
}

/*
 * One bit into the CRC.  The object really does add rather than
 * exclusive-or, and masks with 1 afterwards, which is the same thing for
 * stages that are already 0 or 1 -- but not for the incoming value, which in
 * the sequence fields is a whole byte, so the addition is kept as it stands.
 */
static void
dilCrcBit(int *crc, int bit)
{
	int feedback = bit + crc[0];
	int stage3 = (crc[4] + feedback) & 1;
	int stage10 = (crc[11] + feedback) & 1;
	int stage15 = feedback & 1;
	int i;

	for (i = 0; i < 15; i++)
		crc[i] = crc[i + 1];
	crc[3] = stage3;
	crc[10] = stage10;
	crc[15] = stage15;
}

void
V92DILdescriptorPacker(tagV90DILdescriptor *desc, unsigned char *bits,
		       int *nbits)
{
	/*
	 * The nineteen values of the V.92 info field, built on the stack by
	 * nineteen immediate byte stores in the prologue.
	 */
	unsigned char v92Info[19] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
	};
	int crc[16];
	int seq1Bits, seq2Bits, segmentAt, infoAt, crcAt, frames;
	int i, j, n;

	/*
	 * Six separate gates at entry, each re-testing the level -- which is
	 * what six consecutive `if`s compile to and what the object has.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: pParamObj address = %X\r\n",
				     desc);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: BitVector address = %X\r\n",
				     bits);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: Cnt address = %X\r\n", nbits);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: pParamObj->N = %d\r\n",
				     desc->dilCount);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: pParamObj->Lsp = %d\r\n",
				     desc->seq1Length);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("## Debug: pParamObj->Ltp = %d\r\n",
				     desc->seq2Length);

	/* Frame 0, the one pattern framing cannot produce. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;

	/* Frame 1: the DIL entry count, and its frame filled out with 0s. */
	bits[17] = 0;
	n = desc->dilCount;
	for (i = 18; i <= 25; i++, n >>= 1)
		bits[i] = (unsigned char)(n & 1);
	for (i = 26; i <= 34; i++)
		bits[i] = 0;

	/*
	 * Frame 2: both sequence lengths, less one so that a full 128 fits in
	 * seven bits.  The 0 at 51 is frame 3's framing position, written
	 * here because the sequence field begins after it.
	 */
	dilPackSeven(bits, 35, desc->seq1Length - 1);
	dilPackSeven(bits, 43, desc->seq2Length - 1);
	bits[51] = 0;

	/*
	 * seq1 and seq2, copied byte for byte with a 0 inserted whenever the
	 * cursor lands on a framing position, each padded with 0s to a whole
	 * number of frames.  A framing position landed on by the padding gets
	 * its 0 from the padding, which is the same 0.
	 */
	seq1Bits = (int)(dilCeiling(desc->seq1Length, 0.0625f) * 17.0f);
	n = 0;
	for (i = 0; i < desc->seq1Length; i++) {
		if ((DIL_SEQ1_AT + n) % DIL_FRAME == 0)
			bits[DIL_SEQ1_AT + n++] = 0;
		bits[DIL_SEQ1_AT + n++] = desc->seq1[i];
	}
	while (n < seq1Bits)
		bits[DIL_SEQ1_AT + n++] = 0;

	seq2Bits = (int)(dilCeiling(desc->seq2Length, 0.0625f) * 17.0f);
	n = 0;
	for (i = 0; i < desc->seq2Length; i++) {
		if ((DIL_SEQ1_AT + seq1Bits + n) % DIL_FRAME == 0)
			bits[DIL_SEQ1_AT + seq1Bits + n++] = 0;
		bits[DIL_SEQ1_AT + seq1Bits + n++] = desc->seq2[i];
	}
	while (n < seq2Bits)
		bits[DIL_SEQ1_AT + seq1Bits + n++] = 0;

	/*
	 * The sixteen segment fields, two to a frame: seven bits, a 0, seven
	 * bits, a 0, and then the next frame's framing 0.  The object writes
	 * all sixteen out in line, as the V.90 packer does.
	 */
	segmentAt = DIL_SEQ1_AT + seq1Bits + seq2Bits;
	for (j = 0; j < 16; j++) {
		int at = segmentAt + 8 * j + j / 2;

		dilPackSeven(bits, at, j < 8 ? desc->segmentSize[j]
					    : desc->segmentCode[j - 8]);
		if (j & 1)
			bits[at + 8] = 0;
	}

	/*
	 * The DIL codes, in the same shape, two to a frame.  The bound is a
	 * double and is recomputed every iteration -- see the file comment.
	 */
	for (i = 0; i < dilCeiling(desc->dilCount, 0.5f); i++) {
		int at = segmentAt + DIL_SEGMENT_BITS + DIL_FRAME * i;

		dilPackSeven(bits, at, desc->dilCode[2 * i]);
		dilPackSeven(bits, at + 8, desc->dilCode[2 * i + 1]);
		bits[at + 16] = 0;
	}

	/*
	 * The framing position of the frame after the last DIL frame -- where
	 * the V.90 packer puts the CRC, and where this one starts the V.92
	 * info.  The object recomputes the ceiling a third time to get here.
	 */
	infoAt = segmentAt + DIL_SEGMENT_BITS
		 + (int)(dilCeiling(desc->dilCount, 0.5f) * 17.0f) - 1;
	bits[infoAt] = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"#%# Debug: last bit before V.92 info = %d\r\n",
			infoAt);

	/*
	 * The V.92 info: sixteen 1s in the frame after `infoAt`, then the
	 * next frame's framing 0, then three more values from the same array
	 * and thirteen 0s to fill that frame out.  All three of the array's
	 * tail values are 0, so the second frame is sixteen 0s.
	 */
	for (i = 0; i < 16; i++)
		bits[infoAt + 1 + i] = v92Info[i];
	bits[infoAt + 17] = 0;
	for (i = 16; i < 19; i++)
		bits[infoAt + 2 + i] = v92Info[i];
	for (i = 20; i < 33; i++)
		bits[infoAt + 1 + i] = 0;

	/*
	 * The CRC frame, two frames on.  The object computes its position
	 * from a fourth ceiling rather than from `infoAt`.
	 */
	crcAt = segmentAt + DIL_SEGMENT_BITS
		+ (int)(dilCeiling(desc->dilCount, 0.5f) * 17.0f) + 33;
	bits[crcAt] = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("#%# Debug: last bit after V.92 info = "
				     "%d, temp calculation = %d\r\n",
				     crcAt, infoAt + 33);

	/*
	 * The CRC covers the data bits of every frame from 1 up to but not
	 * including its own -- the V.92 info included.  The register starts
	 * all 1s, and if there is nothing to cover it stays that way.
	 */
	for (i = 0; i < 16; i++)
		crc[i] = 1;

	frames = (crcAt - 18) / DIL_FRAME + 1;
	for (j = 0; j < frames; j++)
		for (i = 18 + DIL_FRAME * j; i < 34 + DIL_FRAME * j; i++)
			dilCrcBit(crc, bits[i]);

	for (i = 0; i < 16; i++)
		bits[crcAt + 1 + i] = (unsigned char)crc[i];

	/*
	 * The framing position of the frame after the CRC, and then a second
	 * 0 if the count would otherwise be odd.  The count is therefore
	 * always even.
	 */
	bits[crcAt + 17] = 0;
	if (crcAt & 1) {
		*nbits = crcAt + 19;
		bits[crcAt + 18] = 0;
	} else {
		*nbits = crcAt + 18;
	}
}
