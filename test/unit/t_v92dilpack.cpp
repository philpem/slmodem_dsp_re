/*
 * t_v92dilpack.cpp -- differential test of V92DILdescriptorPacker.
 *
 * The V.90 sibling's fixture (t_dilpack.cpp) is the model, and three things
 * had to change because the V.92 signature is not the V.90 one:
 *
 *   THE STREAM IS BYTES.  One `unsigned char` per position, not one `short`,
 *   so the guard fill has to be a byte pattern.  It is forced to 0x80..0xff:
 *   never 0 and never 1, which are the only two values the packer writes
 *   outside the sequence fields, so "wrote a 0 here" and "did not write here"
 *   stay distinguishable.  Both buffers are seeded, NEVER zeroed, and
 *   compared whole -- 4,096 bytes against a stream that never exceeds 2,688 --
 *   so a position one side writes and the other does not is a failure
 *   whichever side it is, and the tail past the stream catches an overrun
 *   (findings 223, 224, 230).
 *
 *   THE COUNT IS AN `int`.  `*nbits` is a 32-bit store in the object.  The
 *   sentinel is 0x5bad7bcd rather than a 16-bit one, and the comparison is on
 *   the whole word, so a 16-bit store would leave 0x5bad in the top half and
 *   be caught rather than silently agreeing on the low half.
 *
 *   THE DESCRIPTOR IS NOT `const`.  The mangling says so; nothing here
 *   depends on it beyond the declaration.
 *
 * THE DESCRIPTOR IS FILLED WITH VARIED BYTES, and the three fields that are
 * lengths rather than data are then set explicitly.  They control every loop
 * bound in the function, so leaving them random would test one shape of
 * stream many times over:
 *
 *   seq1Length, seq2Length   0..128, every value, and both are what the
 *                            framing insertion and the padding key off
 *   dilCount                 0..255, every value; it sets the number of DIL
 *                            frames, the length of the CRC's own input, and
 *                            the parity that decides whether the stream ends
 *                            with one 0 or two
 *
 * Lengths above 128 are not driven: seq1 and seq2 are 128 bytes each, the
 * object bounds neither, and a longer one would be testing the read past the
 * field rather than the packing.  dilCount is driven to its full 255 because
 * dilCode is 256 entries and 255 stays inside it.
 *
 * The sweep over every length value is what proves the two CEILINGS agree.
 * The object's is FRNDINT under round-toward-+infinity; the reconstruction's
 * is `ceil()`.  They are the same function, but that is an argument and the
 * sweep is the measurement.
 *
 * The `ref_` alias is reached through an asm() label.  This symbol is
 * MANGLED, so the alias is `ref__Z22V92...` and not a plain name.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92DILdescriptorPacker.h"

extern "C" {
void blobV92Packer(void *desc, unsigned char *bits, int *nbits)
	asm("ref__Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi");
}

/*
 * 2,688 is the longest stream the fields can describe -- both sequences 128
 * and dilCount 255 -- so everything from there to the end of the buffer is a
 * guard region that neither side may touch.
 */
#define NBITS 4096

struct v92DilStream {
	unsigned char bit[NBITS];
};

static struct v92DilStream ours, theirs, fill;
static tagV90DILdescriptor desc;
static int ourLen, theirLen;

#define LEN_SENTINEL 0x5bad7bcd

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the descriptor's data bytes are.  Every field but
 * the sequences is packed seven bits at a time, so a fill whose top bit is
 * always set and one whose top bit is never set probe the dropped bit; the
 * sequences are copied whole, so a byte above 1 probes whether the copy masks
 * (it does not) and what the CRC does with it (takes the low bit).
 */
static void
seed_descriptor(int trial, int mode, int seq1Length, int seq2Length,
		int dilCount)
{
	unsigned char *p = (unsigned char *)&desc;
	unsigned int i;

	lfsr_state = 0x5eedu + 0x4f1bu * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < sizeof(desc); i++) {
		switch (mode) {
		case 1:
			p[i] = 0xff;
			break;
		case 2:
			p[i] = (unsigned char)(next_byte() | 0x80);
			break;
		case 3:
			p[i] = (unsigned char)(next_byte() & 0x7f);
			break;
		case 4:
			p[i] = (unsigned char)(i & 1);
			break;
		default:
			p[i] = next_byte();
			break;
		}
	}

	desc.seq1Length = (unsigned char)seq1Length;
	desc.seq2Length = (unsigned char)seq2Length;
	desc.dilCount = (unsigned char)dilCount;
}

static void
seed_output(int trial)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial;

	for (i = 0; i < NBITS; i++) {
		/* 0x80..0xff: never 0 and never 1. */
		unsigned char v = (unsigned char)(next_byte() | 0x80);

		ours.bit[i] = v;
		theirs.bit[i] = v;
		fill.bit[i] = v;
	}
	ourLen = theirLen = LEN_SENTINEL;
}

/* Drive one descriptor through both sides and compare everything. */
static int
one_case(int trial, int mode, int seq1Length, int seq2Length, int dilCount)
{
	seed_descriptor(trial, mode, seq1Length, seq2Length, dilCount);
	seed_output(trial);

	V92DILdescriptorPacker(&desc, ours.bit, &ourLen);
	blobV92Packer(&desc, theirs.bit, &theirLen);

	diff_eq_obj("the packed stream", v92DilStream, &ours, &theirs, trial);
	diff_eq_int("the bit count (trial %ld)", ourLen, theirLen, trial);

	return ourLen;
}

/*
 * The explicit cases: every remainder mod 16 that the sequence padding can
 * see, both parities of dilCount, and the extremes of all three.
 */
static const struct {
	int seq1Length, seq2Length, dilCount;
} cases[] = {
	{ 0, 0, 0 },	  { 0, 0, 1 },	    { 1, 1, 1 },     { 1, 0, 2 },
	{ 0, 1, 3 },	  { 15, 15, 4 },    { 16, 16, 5 },   { 17, 17, 6 },
	{ 16, 17, 7 },	  { 17, 16, 8 },    { 31, 33, 9 },   { 32, 32, 10 },
	{ 33, 31, 11 },	  { 47, 48, 12 },   { 48, 49, 13 },  { 63, 64, 14 },
	{ 64, 65, 15 },	  { 79, 80, 16 },   { 80, 81, 17 },  { 95, 96, 30 },
	{ 96, 97, 31 },	  { 111, 112, 32 }, { 112, 113, 33 }, { 127, 127, 63 },
	{ 128, 128, 64 }, { 128, 0, 254 },  { 0, 128, 255 }, { 128, 128, 255 },
	{ 1, 128, 128 },  { 128, 1, 127 },  { 5, 11, 200 },  { 100, 3, 99 }
};

#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * Nothing at or past the count may have been touched, on EITHER side.  The
 * whole-buffer comparison above only says the two sides agree; this says the
 * guard region still holds the fill, which is what makes "both agree" mean
 * "both stopped".  A vacuous version of this check is impossible to write by
 * accident here because the fill is never 0 or 1.
 */
static int
guard_intact(int len)
{
	int i;

	if (len <= 0 || len > NBITS)
		return 0;
	for (i = len; i < NBITS; i++)
		if (ours.bit[i] != fill.bit[i] || theirs.bit[i] != fill.bit[i])
			return 0;
	return 1;
}

static int
run_cases(void)
{
	int trial, mode, moved = 0, distinct = 0, first = -1;
	int guardOK = 1, wroteOK = 1, evenOK = 1;

	diff_begin("V92DILdescriptorPacker, explicit shapes");

	for (mode = 0; mode < 5; mode++) {
		for (trial = 0; trial < NCASE; trial++) {
			int len = one_case(trial + mode * NCASE, mode,
					   cases[trial].seq1Length,
					   cases[trial].seq2Length,
					   cases[trial].dilCount);

			if (len == LEN_SENTINEL)
				wroteOK = 0;
			if (len & 1)
				evenOK = 0;
			if (!guard_intact(len))
				guardOK = 0;

			if (memcmp(ours.bit, theirs.bit, sizeof(ours.bit)) != 0)
				moved = 1;
			if (first < 0)
				first = len;
			else if (len != first)
				distinct = 1;
		}
	}

	/*
	 * The count is written, and it is even.  A stream that stopped at the
	 * CRC would be odd about half the time.
	 */
	diff_eq_int("the count was written", wroteOK, 1, 0);
	diff_eq_int("the count is even", evenOK, 1, 0);
	diff_eq_int("nothing was written at or past the count", guardOK, 1, 0);
	diff_eq_int("both sides wrote the same stream", moved, 0, 0);
	diff_eq_int("the shapes gave different lengths", distinct, 1, 0);

	return diff_end();
}

/*
 * Every value of every length field.  The ceiling is floating point in both
 * the object and the reconstruction -- FRNDINT there, `ceil()` here -- and
 * this is what proves the two agree on all 256 inputs rather than on the
 * dozen the explicit cases reach.
 */
static int
run_sweep(void)
{
	int i;

	diff_begin("V92DILdescriptorPacker, every length");

	for (i = 0; i <= 128; i++)
		(void)one_case(i, i % 5, i, 128 - i, 3);
	for (i = 0; i <= 128; i++)
		(void)one_case(i + 200, (i + 1) % 5, 128 - i, i, 200);
	for (i = 0; i <= 255; i++)
		(void)one_case(i + 400, i % 5, 7, 23, i);

	return diff_end();
}

/*
 * What the stream is, checked against the reconstruction's output -- which
 * every case above has just proved identical to the blob's, so these are
 * statements about the object and not only about our copy of it.
 *
 * EVERY POSITION HERE IS COMPUTED BY HAND FROM THE CASE'S FIELD VALUES, never
 * by re-running the packer's arithmetic.  With both sequences L long and N
 * DIL codes the CRC frame's framing position is
 *
 *     17 * (13 + ceil(L/16) + ceil(L/16) + ceil(N/2))
 *
 * -- three fixed frames, one per sixteen bytes of each sequence, eight
 * segment frames, ceil(N/2) DIL frames, and then the TWO V.92 info frames
 * that the V.90 stream does not have.  The 13 is 3 + 8 + 2.
 */
static int
run_shape(void)
{
	int i, framingOK = 1, preambleOK = 1, oneBitOK = 1;
	int onesOK = 1, zerosOK = 1;
	int crcAt, infoAt, dataAt, len;

	diff_begin("V92DILdescriptorPacker, the frame structure");

	/*
	 * Both sequences 32 long, four DIL codes: 13 + 2 + 2 + 2 = 19 frames
	 * before the CRC's own.  17 * 19 = 323, which is ODD, so the stream
	 * ends with two 0s rather than one.
	 */
	crcAt = 17 * (13 + 2 + 2 + 2);
	infoAt = crcAt - 34;
	len = one_case(9001, 0, 32, 32, 4);

	diff_eq_int("the CRC frame lands where the counts put it (odd)",
		    len, crcAt + 18 + (crcAt & 1), 0);

	/* Frame 0 is seventeen 1s, which framing can never produce. */
	for (i = 0; i <= 16; i++)
		if (ours.bit[i] != 1)
			preambleOK = 0;

	/* Every later multiple of 17, up to and including the tail, is 0. */
	for (i = 17; i <= crcAt + 17; i += 17)
		if (ours.bit[i] != 0)
			framingOK = 0;

	/*
	 * The V.92 info: the frame after `infoAt` is sixteen 1s and the one
	 * after that is sixteen 0s.  This is the whole of what the V.92
	 * stream adds to the V.90 one, and the only place in the stream after
	 * frame 0 where sixteen consecutive 1s appear.
	 */
	for (i = 1; i <= 16; i++)
		if (ours.bit[infoAt + i] != 1)
			onesOK = 0;
	for (i = 18; i <= 33; i++)
		if (ours.bit[infoAt + i] != 0)
			zerosOK = 0;

	/*
	 * Outside the two sequence fields every position is 0 or 1.  The
	 * sequence fields run from 52 to 52 + 17 * ceil(32 / 16) * 2 - 1.
	 */
	dataAt = 52 + 2 * 17 * 2;
	for (i = dataAt; i <= crcAt + 17; i++)
		if (ours.bit[i] != 0 && ours.bit[i] != 1)
			oneBitOK = 0;

	diff_eq_int("frame 0 is seventeen 1s", preambleOK, 1, 0);
	diff_eq_int("every later multiple of 17 is a 0", framingOK, 1, 0);
	diff_eq_int("the V.92 info's first frame is sixteen 1s", onesOK, 1, 0);
	diff_eq_int("the V.92 info's second frame is sixteen 0s", zerosOK, 1,
		    0);
	diff_eq_int("everything after the sequences is one bit", oneBitOK, 1,
		    0);

	/*
	 * The sequences are copied byte for byte, not masked to a bit.  Mode
	 * 0 fills them from the LFSR, so at least one byte is above 1.
	 */
	{
		int sawWide = 0;

		for (i = 0; i < 32; i++)
			if (desc.seq1[i] > 1)
				sawWide = 1;
		diff_eq_int("the fixture reached a sequence byte above 1",
			    sawWide, 1, 0);
	}

	/*
	 * THE TAIL BRANCH IS WITNESSED, and this is where the V.92 packer can
	 * do what t_dilpack could not.  `*nbits` is `crcAt + 19` for an odd
	 * `crcAt` and `crcAt + 18` for an even one, and BOTH ARE EVEN -- so
	 * the length alone carries the branch's consequence and not the
	 * branch, exactly as finding 262 says.  What separates them is that
	 * `crcAt` is hand-computed here from the case's field values, so the
	 * expected length differs between the two arms by more than parity:
	 * the case above has an odd `crcAt` and the case below an even one,
	 * and each asserts its own exact length.  Forcing either arm
	 * unconditionally moves one of the two by one position and fails.
	 *
	 * Six DIL codes rather than four: 13 + 2 + 2 + 3 = 20 frames,
	 * 17 * 20 = 340, which is EVEN.
	 */
	crcAt = 17 * (13 + 2 + 2 + 3);
	len = one_case(9002, 0, 32, 32, 6);
	diff_eq_int("the CRC frame lands where the counts put it (even)",
		    len, crcAt + 18 + (crcAt & 1), 0);

	/*
	 * And the position one past the even-`crcAt` stream is untouched,
	 * which is the other half of the same branch: the odd arm writes
	 * `bits[crcAt + 18]` and the even one must not.
	 */
	diff_eq_int("the even tail wrote no second 0",
		    ours.bit[crcAt + 18] == fill.bit[crcAt + 18], 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cases();
	rc |= run_sweep();
	rc |= run_shape();

	return rc;
}
