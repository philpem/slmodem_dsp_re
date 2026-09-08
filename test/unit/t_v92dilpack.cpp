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
 *   (findings F223, F224, F230).
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
#include "dsplib/V90DilDescriptorSettings.h"

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
	 * branch, exactly as finding F262 says.  What separates them is that
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

/*
 * ===========================================================================
 * ITU-T V.92 TABLE 20, independently of V92DILdescriptorPacker/dsplibs.o.
 *
 * The differential tests above prove reconstruction/blob agreement, including
 * over many non-standard raw byte patterns.  This oracle instead creates the
 * information stream in Table 20 field order, inserts the literal one-start-
 * bit-per-sixteen framing and uses an independent bit-serial V.34 Figure 14
 * CRC.  It does not call either packer to obtain an expected value and is not
 * a packing round trip.
 *
 * Table 20 inherits Table 12 through the start bit after the Ucodes, then adds
 * nineteen rate-capability bits and thirteen reserved zeros before the CRC.
 * The implementation's fixed mask has the low sixteen capabilities enabled
 * and the high three disabled.  That is a valid fixed mask in the absence of
 * evidence that an owning modem enables a different set; the oracle therefore
 * states that precondition rather than inventing a variable input the function
 * does not have.
 *
 * Unlike V.90, V.92 requires zero fill to the next MULTIPLE OF TWELVE bits.
 * Both implementations only extend to an even length.  Conforming-length
 * controls and explicit expected-departure cases are kept separate below.
 * ===========================================================================
 */
typedef void (*table20_pack_fn)(void *, unsigned char *, int *);

struct table20_subject {
	const char *oracle_group;
	const char *kat_group;
	const char *preset_group;
	const char *odd_group;
	table20_pack_fn pack;
};

static void
table20_our_pack(void *d, unsigned char *bits, int *nbits)
{
	V92DILdescriptorPacker((tagV90DILdescriptor *)d, bits, nbits);
}

static void
table20_blob_pack(void *d, unsigned char *bits, int *nbits)
{
	blobV92Packer(d, bits, nbits);
}

static void
table20_put(unsigned char *information, unsigned int *at,
	    unsigned int value, unsigned int width)
{
	unsigned int i;

	for (i = 0; i < width; i++)
		information[(*at)++] = (unsigned char)((value >> i) & 1u);
}

/* Figure 14 in chronological order: x^16+x^12+x^5+1, preload all ones. */
static unsigned int
table20_crc(const unsigned char *information, unsigned int n)
{
	unsigned int reg = 0xffffu;
	unsigned int i;

	for (i = 0; i < n; i++) {
		unsigned int feedback =
		    (reg ^ (unsigned int)information[i]) & 1u;

		reg >>= 1;
		if (feedback)
			reg ^= 0x8408u;
	}
	return reg & 0xffffu;
}

static unsigned int
table20_information(unsigned char information[NBITS],
		    const tagV90DILdescriptor *d)
{
	unsigned int at = 0;
	unsigned int i;

	/* V.90 Table 12 frames 1 and 2. */
	table20_put(information, &at, d->dilCount, 8);
	table20_put(information, &at, 0, 8);
	table20_put(information, &at, (unsigned int)d->seq1Length - 1u, 7);
	table20_put(information, &at, 0, 1);
	table20_put(information, &at, (unsigned int)d->seq2Length - 1u, 7);
	table20_put(information, &at, 0, 1);

	/* SP and TP, zero-padded independently to complete information frames. */
	for (i = 0; i < d->seq1Length; i++)
		table20_put(information, &at, d->seq1[i], 1);
	while (at & 15u)
		table20_put(information, &at, 0, 1);
	for (i = 0; i < d->seq2Length; i++)
		table20_put(information, &at, d->seq2[i], 1);
	while (at & 15u)
		table20_put(information, &at, 0, 1);

	/* H1..H8, REF1..REF8 and exactly N active Ucodes. */
	for (i = 0; i < 8u; i++) {
		table20_put(information, &at, d->segmentSize[i], 7);
		table20_put(information, &at, 0, 1);
	}
	for (i = 0; i < 8u; i++) {
		table20_put(information, &at, d->segmentCode[i], 7);
		table20_put(information, &at, 0, 1);
	}
	for (i = 0; i < d->dilCount; i++) {
		table20_put(information, &at, d->dilCode[i], 7);
		table20_put(information, &at, 0, 1);
	}
	while (at & 15u)
		table20_put(information, &at, 0, 1);

	/* Table 20's valid fixed rate mask: low sixteen on, high three off. */
	table20_put(information, &at, 0xffffu, 16);
	table20_put(information, &at, 0, 3);
	table20_put(information, &at, 0, 13);
	return at;
}

static unsigned int
table20_frame(unsigned char expected[NBITS],
	      const tagV90DILdescriptor *d, unsigned int *crc_at,
	      unsigned int *crc_word)
{
	unsigned char information[NBITS];
	unsigned int info_len = table20_information(information, d);
	unsigned int at = 0;
	unsigned int i;

	for (i = 0; i < 17u; i++)
		expected[at++] = 1;
	for (i = 0; i < info_len; i++) {
		if ((i & 15u) == 0)
			expected[at++] = 0;
		expected[at++] = information[i];
	}
	*crc_at = at;
	expected[at++] = 0;
	*crc_word = table20_crc(information, info_len);
	for (i = 0; i < 16u; i++)
		expected[at++] = (unsigned char)((*crc_word >> i) & 1u);

	/* Bit C+17 and every bit needed to reach the next 12-bit boundary. */
	do {
		expected[at++] = 0;
	} while (at % 12u);
	return at;
}

static unsigned int
table20_wire_crc(const unsigned char *wire, unsigned int crc_at)
{
	unsigned int word = 0;
	unsigned int i;

	for (i = 0; i < 16u; i++)
		word |= (unsigned int)(wire[crc_at + 1u + i] & 1u) << i;
	return word;
}

static int
table20_guard(const unsigned char *wire, unsigned int len)
{
	unsigned int i;

	if (len > NBITS)
		return 0;
	for (i = len; i < NBITS; i++)
		if (wire[i] != 0xa5)
			return 0;
	return 1;
}

static void
table20_descriptor(tagV90DILdescriptor *d, unsigned int n,
		   unsigned int lsp, unsigned int ltp, unsigned int seed)
{
	unsigned int i;

	memset(d, 0, sizeof(*d));
	d->dilCount = (unsigned char)n;
	d->seq1Length = (unsigned char)lsp;
	d->seq2Length = (unsigned char)ltp;
	for (i = 0; i < lsp; i++)
		d->seq1[i] = (unsigned char)((i * 5u + seed) & 1u);
	for (i = 0; i < ltp; i++)
		d->seq2[i] = (unsigned char)((i * 3u + seed / 2u + 1u) & 1u);
	for (i = 0; i < 8u; i++) {
		d->segmentSize[i] = (unsigned char)((seed + 13u * i) & 0x7fu);
		d->segmentCode[i] =
		    (unsigned char)((127u - seed - 7u * i) & 0x7fu);
	}
	for (i = 0; i < n; i++)
		d->dilCode[i] = (unsigned char)((seed + 29u * i) & 0x7fu);
}

static int
run_table20_oracle_subject(const struct table20_subject *s)
{
	/* q=ceil(LSP/16)+ceil(LTP/16)+ceil(N/2) is 5, 12, 17, 24 or 144. */
	static const struct {
		unsigned char n, lsp, ltp, seed;
	} legal[] = {
		{   5,   1,   1,  3 }, {  19,   1,   1,  5 },
		{  20,   1,   1,  7 }, {   1, 128, 128, 11 },
		{   2, 128, 128, 13 }, {  43,  16,  16, 17 },
		{ 255, 128, 128, 19 }
	};
	tagV90DILdescriptor d;
	unsigned char actual[NBITS];
	unsigned char expected[NBITS];
	unsigned int trial;

	diff_begin(s->oracle_group);
	for (trial = 0; trial < sizeof(legal) / sizeof(legal[0]); trial++) {
		unsigned int crc_at, crc_word, expected_len;
		int actual_len = LEN_SENTINEL;

		table20_descriptor(&d, legal[trial].n, legal[trial].lsp,
				   legal[trial].ltp, legal[trial].seed);
		memset(actual, 0xa5, sizeof(actual));
		expected_len = table20_frame(expected, &d, &crc_at, &crc_word);
		s->pack(&d, actual, &actual_len);

		diff_eq_int("Table 20 complete length is a multiple of 12 (%ld)",
			    actual_len, expected_len, (long)trial);
		diff_eq_int("Table 20 complete framed vector is exact (%ld)",
			    memcmp(actual, expected, expected_len), 0, (long)trial);
		diff_eq_int("Table 20 CRC has the exact information extent (%ld)",
			    table20_wire_crc(actual, crc_at), crc_word,
			    (long)trial);
		diff_eq_int("Table 20 kept the output guard (%ld)",
			    table20_guard(actual, (unsigned int)actual_len), 1,
			    (long)trial);
	}
	return diff_end();
}

struct table20_kat {
	unsigned int crc_at, crc, standard_len, source_len, byte_len;
	unsigned char bytes[39];
};

static void
table20_kat_descriptor(tagV90DILdescriptor *d, unsigned int which)
{
	unsigned int i;

	memset(d, 0, sizeof(*d));
	d->seq1Length = d->seq2Length = 1;
	if (which == 1) {
		d->dilCount = 1;
		d->seq1[0] = d->seq2[0] = 1;
		for (i = 0; i < 8u; i++) {
			d->segmentSize[i] = (unsigned char)i;
			d->segmentCode[i] = (unsigned char)(16u * i);
		}
		d->dilCode[0] = 127;
	} else if (which == 2) {
		d->dilCount = 2;
		d->seq1Length = 17;
		d->seq2Length = 16;
		for (i = 0; i < 17u; i++)
			d->seq1[i] = (unsigned char)(i & 1u);
		d->seq2[0] = d->seq2[15] = 1;
		for (i = 0; i < 8u; i++) {
			d->segmentSize[i] = (unsigned char)(127u - i);
			d->segmentCode[i] = (unsigned char)(127u - 16u * i);
		}
		d->dilCode[0] = 0;
		d->dilCode[1] = 127;
	} else if (which == 3) {
		d->dilCount = 6;
	}
}

static void
table20_pack_bytes(unsigned char *bytes, const unsigned char *wire,
		   unsigned int nbits)
{
	unsigned int i;

	memset(bytes, 0, (nbits + 7u) / 8u);
	for (i = 0; i < nbits; i++)
		bytes[i / 8u] |= (unsigned char)((wire[i] & 1u) << (i & 7u));
}

static int
table20_prefix_matches(const unsigned char *wire, unsigned int nbits,
		       const unsigned char *packed)
{
	unsigned int i;

	for (i = 0; i < nbits; i++)
		if ((wire[i] & 1u) != ((packed[i / 8u] >> (i & 7u)) & 1u))
			return 0;
	return 1;
}

static int
run_table20_kat_subject(const struct table20_subject *s)
{
	static const struct table20_kat kats[] = {
		{ 255, 0xfa14, 276, 274, 35,
		  { 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f, 0x00, 0x00,
		    0x14, 0xfa, 0x00 } },
		{ 272, 0xdc67, 300, 290, 38,
		  { 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x10, 0x00,
		    0x20, 0x00, 0x00, 0x40, 0x00, 0x81, 0x01, 0x04,
		    0x05, 0x0c, 0x0e, 0x00, 0x40, 0x00, 0x81, 0x01,
		    0x04, 0x05, 0x0c, 0xce, 0x1f, 0x80, 0xff, 0x7f,
		    0x00, 0x00, 0xce, 0xb8, 0x01, 0x00 } },
		{ 289, 0xfd8d, 312, 308, 39,
		  { 0xff, 0xff, 0x09, 0x00, 0x80, 0x78, 0xa0, 0xaa,
		    0x0a, 0x00, 0x40, 0x00, 0xa0, 0x3f, 0x3f, 0x7d,
		    0x7c, 0xf6, 0xf4, 0xe4, 0xe1, 0xf9, 0x7b, 0xf3,
		    0xf5, 0xe4, 0xe7, 0xc5, 0xc7, 0x03, 0x80, 0x3f,
		    0xff, 0xff, 0x00, 0x00, 0x34, 0xf6, 0x03 } }
	};
	tagV90DILdescriptor d;
	unsigned char actual[NBITS], expected[NBITS], packed[39];
	unsigned int k;

	diff_begin(s->kat_group);
	for (k = 0; k < sizeof(kats) / sizeof(kats[0]); k++) {
		unsigned int crc_at, crc_word, standard_len;
		int actual_len = LEN_SENTINEL;

		table20_kat_descriptor(&d, k);
		memset(actual, 0xa5, sizeof(actual));
		standard_len = table20_frame(expected, &d, &crc_at, &crc_word);
		table20_pack_bytes(packed, expected, standard_len);
		s->pack(&d, actual, &actual_len);

		diff_eq_int("fixed Table 20 standard length KAT (%ld)",
			    standard_len, kats[k].standard_len, (long)k);
		diff_eq_int("fixed Table 20 standard packed-wire KAT (%ld)",
			    memcmp(packed, kats[k].bytes, kats[k].byte_len), 0,
			    (long)k);
		diff_eq_int("expected fill departure: source length KAT (%ld)",
			    actual_len, kats[k].source_len, (long)k);
		diff_eq_int("source prefix agrees with fixed Table 20 KAT (%ld)",
			    table20_prefix_matches(actual,
				(unsigned int)actual_len, kats[k].bytes), 1,
			    (long)k);
		diff_eq_int("fixed Table 20 CRC KAT (%ld)",
			    table20_wire_crc(actual, kats[k].crc_at), kats[k].crc,
			    (long)k);
		diff_eq_int("fixed Table 20 KAT kept the guard (%ld)",
			    table20_guard(actual, (unsigned int)actual_len), 1,
			    (long)k);
	}

	/* KAT D: q=5, a length at which the implementation's fill is conforming. */
	{
		unsigned int crc_at, crc_word, standard_len;
		int actual_len = LEN_SENTINEL;

		table20_kat_descriptor(&d, 3);
		memset(actual, 0xa5, sizeof(actual));
		standard_len = table20_frame(expected, &d, &crc_at, &crc_word);
		s->pack(&d, actual, &actual_len);
		diff_eq_int("fixed Table 20 conforming control length", actual_len,
			    324, 0);
		diff_eq_int("fixed Table 20 conforming control standard length",
			    standard_len, 324, 0);
		diff_eq_int("fixed Table 20 conforming control CRC",
			    table20_wire_crc(actual, crc_at), 0xf26b, 0);
		diff_eq_int("fixed Table 20 conforming control vector",
			    memcmp(actual, expected, standard_len), 0, 0);
		diff_eq_int("fixed Table 20 conforming control kept the guard",
			    table20_guard(actual, (unsigned int)actual_len), 1, 0);
	}
	return diff_end();
}

static int
run_table20_preset_subject(const struct table20_subject *s)
{
	static const struct {
		DilType type;
		unsigned int lsp, ltp, source_len, standard_len;
	} presets[] = {
		{ DIL_TYPE_ADI,    120, 120, 1736, 1740 },
		{ DIL_TYPE_ADI_QC,  60,  60, 1600, 1608 }
	};
	tagV90DILdescriptor d;
	unsigned char actual[NBITS], expected[NBITS];
	unsigned int p;

	diff_begin(s->preset_group);
	for (p = 0; p < sizeof(presets) / sizeof(presets[0]); p++) {
		unsigned int crc_at, crc_word, standard_len;
		int actual_len = LEN_SENTINEL;

		memset(&d, 0xa5, sizeof(d));
		setDilDescriptor(&d, presets[p].type);
		memset(actual, 0xa5, sizeof(actual));
		standard_len = table20_frame(expected, &d, &crc_at, &crc_word);
		s->pack(&d, actual, &actual_len);

		diff_eq_int("shipped preset has N=144 (%ld)", d.dilCount, 144,
			    (long)p);
		diff_eq_int("shipped preset has exact LSP (%ld)", d.seq1Length,
			    presets[p].lsp, (long)p);
		diff_eq_int("shipped preset has exact LTP (%ld)", d.seq2Length,
			    presets[p].ltp, (long)p);
		diff_eq_int("reachable fill departure: source preset length (%ld)",
			    actual_len, presets[p].source_len, (long)p);
		diff_eq_int("reachable Table 20 standard preset length (%ld)",
			    standard_len, presets[p].standard_len, (long)p);
		diff_eq_int("reachable preset is an exact standard prefix (%ld)",
			    memcmp(actual, expected, (unsigned int)actual_len), 0,
			    (long)p);
		diff_eq_int("reachable preset CRC has exact extent (%ld)",
			    table20_wire_crc(actual, crc_at), crc_word, (long)p);
		diff_eq_int("reachable preset kept the guard (%ld)",
			    table20_guard(actual, (unsigned int)actual_len), 1,
			    (long)p);
	}
	return diff_end();
}

static int
run_table20_odd_subject(const struct table20_subject *s)
{
	tagV90DILdescriptor d;
	unsigned char actual[NBITS];
	int actual_len = LEN_SENTINEL;
	unsigned int i;
	int inactive_ones = 1;

	diff_begin(s->odd_group);
	table20_kat_descriptor(&d, 1);
	d.dilCode[1] = 127;
	memset(actual, 0xa5, sizeof(actual));
	s->pack(&d, actual, &actual_len);
	for (i = 230; i <= 236; i++)
		if (actual[i] != 1)
			inactive_ones = 0;

	diff_eq_int("expected departure: inactive odd-N Ucode is emitted",
		    inactive_ones, 1, 0);
	diff_eq_int("expected departure: inactive Ucode changes Table 20 CRC",
		    table20_wire_crc(actual, 272), 0xa368, 0);
	diff_eq_int("odd-N departure retains the source length", actual_len,
		    290, 0);
	diff_eq_int("odd-N departure kept the guard",
		    table20_guard(actual, (unsigned int)actual_len), 1, 0);
	return diff_end();
}

static int
run_table20(void)
{
	static const struct table20_subject subjects[] = {
		{
			"V.92 Table 20 reconstruction standards oracle",
			"V.92 Table 20 reconstruction fixed KATs",
			"V.92 Table 20 reconstruction reachable fill departures",
			"V.92 Table 20 reconstruction odd-N departure",
			table20_our_pack
		},
		{
			"V.92 Table 20 blob standards oracle",
			"V.92 Table 20 blob fixed KATs",
			"V.92 Table 20 blob reachable fill departures",
			"V.92 Table 20 blob odd-N departure",
			table20_blob_pack
		}
	};
	unsigned int i;
	int rc = 0;

	for (i = 0; i < sizeof(subjects) / sizeof(subjects[0]); i++) {
		rc |= run_table20_oracle_subject(&subjects[i]);
		rc |= run_table20_kat_subject(&subjects[i]);
		rc |= run_table20_preset_subject(&subjects[i]);
		rc |= run_table20_odd_subject(&subjects[i]);
	}
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_cases();
	rc |= run_sweep();
	rc |= run_shape();
	rc |= run_table20();

	return rc;
}
