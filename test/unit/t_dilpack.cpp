/*
 * t_dilpack.cpp -- differential test of DILdescriptorPacker.
 *
 * BOTH BUFFERS ARE SEEDED WITH VARIED BYTES, NEVER ZEROED, and compared
 * whole -- 4,096 shorts against a stream that never exceeds 2,654 -- so a
 * position one side writes and the other does not is a failure whichever
 * side it is, and the tail past the stream catches an overrun (findings 223,
 * 224, 230).  The pattern is deliberately not 0 and not 1, because those are
 * the only two values the packer itself writes outside the sequence fields.
 *
 * THE DESCRIPTOR IS FILLED WITH VARIED BYTES TOO, and the three fields that
 * are lengths rather than data are then set explicitly.  They control every
 * loop bound in the function, so leaving them random would test one shape of
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
 * The `ref_` alias is reached through an asm() label rather than by spelling
 * the alias as an identifier, which is the convention the C++ tests here
 * already use.  The blob's symbol is unmangled, so the alias is plain
 * `ref_DILdescriptorPacker`.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/DILdescriptorPacker.h"

extern "C" {
void blobPacker(const void *desc, short *bits, short *nbits)
	asm("ref_DILdescriptorPacker");
}

/*
 * 2,654 is the longest stream the fields can describe -- both sequences 128
 * and dilCount 255 -- so everything from there to the end of the buffer is a
 * guard region that neither side may touch.
 */
#define NBITS 4096

struct dilStream {
	short bit[NBITS];
};

static struct dilStream ours, theirs;
static tagV90DILdescriptor desc;
static short ourLen, theirLen;

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
 * sequences are copied whole, so a byte above 1 probes whether the copy
 * masks (it does not) and what the CRC does with it (takes the low bit).
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
		short v = (short)(0x5a00 + (next_byte() & 0xff));

		ours.bit[i] = v;
		theirs.bit[i] = v;
	}
	ourLen = theirLen = (short)0x7bcd;
}

/* Drive one descriptor through both sides and compare everything. */
static int
one_case(int trial, int mode, int seq1Length, int seq2Length, int dilCount)
{
	seed_descriptor(trial, mode, seq1Length, seq2Length, dilCount);
	seed_output(trial);

	DILdescriptorPacker(&desc, ours.bit, &ourLen);
	blobPacker(&desc, theirs.bit, &theirLen);

	diff_eq_obj("the packed stream", dilStream, &ours, &theirs, trial);
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

static int
run_cases(void)
{
	int trial, mode, moved = 0, distinct = 0, first = -1;

	diff_begin("DILdescriptorPacker, explicit shapes");

	for (mode = 0; mode < 5; mode++) {
		for (trial = 0; trial < NCASE; trial++) {
			int len = one_case(trial + mode * NCASE, mode,
					   cases[trial].seq1Length,
					   cases[trial].seq2Length,
					   cases[trial].dilCount);

			/*
			 * The count is written, and it is even.  A stream
			 * that stopped at the CRC would be odd about half
			 * the time.
			 */
			diff_eq_int("the count was written (trial %ld)",
				    len != (short)0x7bcd, 1, trial);
			diff_eq_int("the count is even (trial %ld)",
				    len & 1, 0, trial);

			if (memcmp(ours.bit, theirs.bit, sizeof(ours.bit)) != 0)
				moved = 1;
			if (first < 0)
				first = len;
			else if (len != first)
				distinct = 1;
		}
	}

	diff_eq_int("both sides wrote the same stream", moved, 0, 0);
	diff_eq_int("the shapes gave different lengths", distinct, 1, 0);

	/*
	 * THE TAIL BRANCH IS NOT WITNESSED FROM `len`, and a check that tried
	 * to be was here and was a tautology.
	 *
	 * The packer ends `bits[crcAt + 17] = 0;` and then, only when
	 * `crcAt & 1`, a second 0 -- so the count is `crcAt + 19` for an odd
	 * `crcAt` and `crcAt + 18` for an even one.  BOTH ARE EVEN, which is
	 * exactly what the assertion above proves, so `(len - 18) & 1` is
	 * identically zero and "a stream ending two 0s past the CRC was
	 * reached" could never be set no matter what the trials contained.
	 * `len` carries the branch's consequence and not the branch.
	 *
	 * Reconstructing `crcAt` in the test would mean recomputing
	 * `segmentAt + DIL_SEGMENT_BITS + DIL_FRAME * dilCeiling(...)`, which
	 * is the packer's own arithmetic and would assert it against itself.
	 * So the branch is proved the other way, by mutation:
	 * `test/mutations/dilpack.json` forces each tail unconditionally and
	 * the differential test catches both.  Finding 262.
	 */

	return diff_end();
}

/*
 * Every value of every length field.  dilCeiling() is floating point in both
 * the object and the reconstruction and this is what proves the two agree on
 * all 256 inputs rather than on the dozen the explicit cases reach.
 */
static int
run_sweep(void)
{
	int i;

	diff_begin("DILdescriptorPacker, every length");

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
 */
static int
run_shape(void)
{
	int i, framingOK = 1, preambleOK = 1, oneBitOK = 1;
	int crcAt, dataAt;

	diff_begin("DILdescriptorPacker, the frame structure");

	/*
	 * Both sequences 32 long, four DIL codes: three fixed frames, two
	 * frames for each sequence, eight segment frames and two DIL frames,
	 * so the CRC's framing position is at 17 * 17.  It is odd, so the
	 * stream ends with two 0s rather than one.
	 */
	crcAt = 17 * (3 + 2 + 2 + 8 + 2);
	(void)one_case(9001, 0, 32, 32, 4);

	diff_eq_int("the CRC frame lands where the counts put it",
		    ourLen, crcAt + 18 + (crcAt & 1), 0);

	/* Frame 0 is seventeen 1s, which framing can never produce. */
	for (i = 0; i <= 16; i++)
		if (ours.bit[i] != 1)
			preambleOK = 0;

	/* Every later multiple of 17, up to and including the tail, is 0. */
	for (i = 17; i <= crcAt + 17; i += 17)
		if (ours.bit[i] != 0)
			framingOK = 0;

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
