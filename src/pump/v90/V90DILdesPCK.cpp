/*
 * DILdescriptorPacker.cpp -- pack a tagV90DILdescriptor into its bit stream.
 *
 * Reconstructed from dsplibs.o, 3,278 bytes at 0x4d200.  A leaf: it calls
 * nothing, references no data symbol, and its only relocations are the six
 * that reach three floating-point literals in .rodata.cst4/.cst8.
 *
 * THE SIGNATURE IS MEASURED.  The symbol is unmangled, so it says nothing
 * about types.  Three arguments arrive on the stack (cdecl, at +0xf0, +0xf4
 * and +0xf8 of a frame that pushed four registers and subtracted 0xdc):
 *
 *   1  read at displacements 0, 1, 2, 0x3.., 0x83.., 0x103..0x112 and
 *      0x113.., with `movzbl` throughout -- tagV90DILdescriptor, and never
 *      written, so `const`
 *   2  written only with `movw` and read back only with `movswl`, indexed by
 *      element -- `short *`
 *   3  written once, `mov %ax,(%ecx)`, with the count of elements the second
 *      argument received -- `short *`
 *
 * and `VPcmFloModem::enterPhase3`, the caller, passes `this+4`, `this+0x21e`
 * and `this+0x1736`, then reloads `this+0x1736` with `movswl` and prints it.
 * It ignores %eax, and both returns leave in %eax the value they just stored
 * through the third argument, so the return type is `void`; a caller wanting
 * the count has it through the pointer.
 *
 * The gap between the caller's first two arguments -- 0x21e - 4 = 0x21a --
 * is also the tightest bound this tree has on sizeof(tagV90DILdescriptor):
 * 538 bytes, against the 0x213 = 531 the declared fields already occupy.
 * NOTHING HERE READS PAST THE DECLARED FIELDS.  The last byte reached is
 * dilCode[2 * ceil(dilCount / 2) - 1], which for the largest dilCount a byte
 * can hold is dilCode[255] at +0x212.  The struct did not have to be
 * extended and was not touched.
 *
 * WHAT IT PACKS.  The stream is a run of 17-position frames.  Position 17k
 * of every frame is a 0, and the sixteen after it carry data; frame 0 is the
 * exception and is seventeen 1s, which is the one pattern the framing rules
 * out.  One `short` holds one bit, 0 or 1 -- except in the two sequence
 * fields, which are copied out of the descriptor byte for byte and land in
 * the stream whatever they were.
 *
 *   frame 0        seventeen 1s
 *   frame 1        dilCount, eight bits, then eight 0s
 *   frame 2        seq1Length - 1 and seq2Length - 1, seven bits each,
 *                  each followed by a 0
 *   frames 3..     seq1, one bit per byte, padded with 0s to a whole
 *                  number of frames; then seq2 the same way
 *   next 8 frames  segmentSize[0..7] then segmentCode[0..7], seven bits
 *                  each, each followed by a 0, two to a frame
 *   next frames    dilCode[0..], seven bits each, each followed by a 0, two
 *                  to a frame; ceil(dilCount / 2) frames
 *   last frame     the CRC of every data bit so far
 *
 * and then one more 0, and a second one if that would leave the count odd.
 * Every field is least significant bit first.
 *
 * All of it is derived from the disassembly.  Names are invented -- an
 * unmangled symbol carries none -- and describe what the object does with
 * the value.
 */

#include "dsplib/DILdescriptorPacker.h"

/* One framing position plus sixteen data positions. */
#define DIL_FRAME	17

/*
 * The first data position of frame 3, where the seq1 field starts.  Frames 0
 * to 2 are fixed, so the object spells this as the literal 0x34 in the
 * divisibility test that inserts framing bits and as a displacement of 0x68
 * in every store.
 */
#define DIL_SEQ1_AT	52

/* The segment fields are eight frames, two seven-bit fields to a frame. */
#define DIL_SEGMENT_BITS	(8 * DIL_FRAME)

/*
 * The object's own ceiling, and it is a floating-point one: the length goes
 * into the x87 as a 16-bit integer, is multiplied by a reciprocal, and is
 * converted toward zero -- but if the product is above zero it converts
 * `product + 0.99999999` instead.  The literals are 0.0625f, 0.5f and the
 * double 0.99999999, all three read out of .rodata.
 *
 * Over the only domain that can reach it -- a length or a count held in an
 * unsigned char, so 0 to 255 -- that is exactly the integer ceiling: the
 * fractional part of n/16 or n/2 is either 0 or at least 0.0625, which is
 * more than 1e-8, so adding 0.99999999 crosses the next integer if and only
 * if there was a remainder.  Kept in floating point rather than replaced by
 * (n + 15) / 16 because that is what the object does; t_dilpack drives every
 * one of the 256 values through both.
 */
static int
dilCeiling(int n, float reciprocal)
{
	float scaled = (float)n * reciprocal;

	if (scaled > 0.0f)
		return (int)(scaled + 0.99999999);
	return (int)scaled;
}

/*
 * A seven-bit field, least significant bit first, and the 0 that follows it.
 * Twenty of the object's fields are this shape and it writes all twenty out
 * longhand.  `value` is signed because two call sites pass a length less
 * one, which is -1 when the length is 0 and shifts down as -1 -- seven 1s,
 * as the object's `sar` gives.
 */
static void
dilPackSeven(short *bits, int at, int value)
{
	int i;

	for (i = 0; i < 7; i++, value >>= 1)
		bits[at + i] = (short)(value & 1);
	bits[at + 7] = 0;
}

/*
 * One bit into the CRC.  Sixteen stages, each 0 or 1, shifting toward stage
 * 0; the feedback is stage 0 plus the incoming bit and is added back at
 * stages 15, 10 and 3, which is x^16 + x^12 + x^5 + 1 with the oldest stage
 * written first.  The object really does add rather than exclusive-or, and
 * masks with 1 afterwards, which is the same thing for stages that are
 * already 0 or 1 -- but not for the incoming value, which in the sequence
 * fields is a whole byte, so the addition is kept as it stands.
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

extern "C" void
DILdescriptorPacker(const tagV90DILdescriptor *desc, short *bits, short *nbits)
{
	int crc[16];
	int seq1Bits, seq2Bits, dilFrames, segmentAt, crcAt, frames;
	int i, j, n;

	/* Frame 0, the one pattern framing cannot produce. */
	for (i = 0; i <= 16; i++)
		bits[i] = 1;

	/* Frame 1: the DIL entry count, and its frame filled out with 0s. */
	bits[17] = 0;
	n = desc->dilCount;
	for (i = 18; i <= 25; i++, n >>= 1)
		bits[i] = (short)(n & 1);
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
	seq1Bits = DIL_FRAME * dilCeiling(desc->seq1Length, 0.0625f);
	n = 0;
	for (i = 0; i < desc->seq1Length; i++) {
		if ((DIL_SEQ1_AT + n) % DIL_FRAME == 0)
			bits[DIL_SEQ1_AT + n++] = 0;
		bits[DIL_SEQ1_AT + n++] = desc->seq1[i];
	}
	while (n < seq1Bits)
		bits[DIL_SEQ1_AT + n++] = 0;

	seq2Bits = DIL_FRAME * dilCeiling(desc->seq2Length, 0.0625f);
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
	 * bits, a 0, and then the next frame's framing 0.
	 */
	segmentAt = DIL_SEQ1_AT + seq1Bits + seq2Bits;
	for (j = 0; j < 16; j++) {
		int at = segmentAt + 8 * j + j / 2;

		dilPackSeven(bits, at, j < 8 ? desc->segmentSize[j]
					     : desc->segmentCode[j - 8]);
		if (j & 1)
			bits[at + 8] = 0;
	}

	/* The DIL codes, in the same shape, two to a frame. */
	dilFrames = dilCeiling(desc->dilCount, 0.5f);
	for (i = 0; i < dilFrames; i++) {
		int at = segmentAt + DIL_SEGMENT_BITS + DIL_FRAME * i;

		dilPackSeven(bits, at, desc->dilCode[2 * i]);
		dilPackSeven(bits, at + 8, desc->dilCode[2 * i + 1]);
		bits[at + 16] = 0;
	}

	/*
	 * The CRC goes in the frame after the last DIL frame, over the data
	 * bits of every frame from 1 up to but not including its own.  Frame
	 * 0 is not covered.  The register starts all 1s, and if there is
	 * nothing to cover it stays that way.
	 */
	crcAt = segmentAt + DIL_SEGMENT_BITS + DIL_FRAME * dilFrames - 1;
	bits[crcAt] = 0;

	for (i = 0; i < 16; i++)
		crc[i] = 1;

	frames = (crcAt - 18) / DIL_FRAME + 1;
	for (j = 0; j < frames; j++)
		for (i = 18 + DIL_FRAME * j; i < 34 + DIL_FRAME * j; i++)
			dilCrcBit(crc, bits[i]);

	for (i = 0; i < 16; i++)
		bits[crcAt + 1 + i] = (short)crc[i];

	/*
	 * The framing position of the frame after the CRC, and then a second
	 * 0 if the count would otherwise be odd.  The count is therefore
	 * always even.
	 */
	bits[crcAt + 17] = 0;
	if (crcAt & 1) {
		bits[crcAt + 18] = 0;
		*nbits = (short)(crcAt + 19);
	} else {
		*nbits = (short)(crcAt + 18);
	}
}
