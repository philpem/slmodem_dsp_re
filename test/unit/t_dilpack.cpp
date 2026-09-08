/*
 * t_dilpack.cpp -- differential test of DILdescriptorPacker.
 *
 * BOTH BUFFERS ARE SEEDED WITH VARIED BYTES, NEVER ZEROED, and compared
 * whole -- 4,096 shorts against a stream that never exceeds 2,654 -- so a
 * position one side writes and the other does not is a failure whichever
 * side it is, and the tail past the stream catches an overrun (findings F223,
 * F224, F230).  The pattern is deliberately not 0 and not 1, because those are
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
	 * the differential test catches both.  Finding F262.
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

/*
 * ===========================================================================
 * ITU-T V.90 TABLE 12, independently of DILdescriptorPacker and dsplibs.o.
 *
 * The differential tests above establish reconstruction/blob agreement.  This
 * oracle instead writes Table 12's information stream in field order, inserts
 * one zero start bit before every sixteen information bits and computes the
 * V.34 Figure 14 CRC over the information bits only.  It does not call a
 * production packing or CRC helper and does not obtain expectations from the
 * blob or from a pack/unpack round trip.
 *
 * Legal LSP and LTP are 1..128.  N is 0..255; for N=0 Table 12 specifically
 * encodes LSP-1=LTP-1=0.  Each H, REF and Ucode is seven bits, followed by a
 * reserved zero.  If N is odd, all nine positions after the final seven-bit
 * Ucode are reserved zeros.  The last rule has a separate expected-departure
 * probe below because both implementations instead expose dilCode[N].
 * ===========================================================================
 */
typedef void (*table12_pack_fn)(const void *, short *, short *);

struct table12_subject {
	const char *oracle_group;
	const char *kat_group;
	const char *departure_group;
	table12_pack_fn pack;
};

static void
table12_our_pack(const void *d, short *bits, short *nbits)
{
	DILdescriptorPacker((const tagV90DILdescriptor *)d, bits, nbits);
}

static void
table12_blob_pack(const void *d, short *bits, short *nbits)
{
	blobPacker(d, bits, nbits);
}

static void
table12_put(unsigned char *information, unsigned int *at,
	    unsigned int value, unsigned int width)
{
	unsigned int i;

	for (i = 0; i < width; i++)
		information[(*at)++] = (unsigned char)((value >> i) & 1u);
}

/* Figure 14 in chronological bit order: x^16+x^12+x^5+1, preload all ones. */
static unsigned int
table12_crc(const unsigned char *information, unsigned int n)
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
table12_information(unsigned char information[NBITS],
		    const tagV90DILdescriptor *d)
{
	unsigned int at = 0;
	unsigned int i;

	/* Frames 1 and 2: N, reserved, LSP-1, reserved, LTP-1, reserved. */
	table12_put(information, &at, d->dilCount, 8);
	table12_put(information, &at, 0, 8);
	table12_put(information, &at, (unsigned int)d->seq1Length - 1u, 7);
	table12_put(information, &at, 0, 1);
	table12_put(information, &at, (unsigned int)d->seq2Length - 1u, 7);
	table12_put(information, &at, 0, 1);

	/* SP and TP, each padded with zero to a sixteen-bit boundary. */
	for (i = 0; i < d->seq1Length; i++)
		table12_put(information, &at, d->seq1[i], 1);
	while (at & 15u)
		table12_put(information, &at, 0, 1);
	for (i = 0; i < d->seq2Length; i++)
		table12_put(information, &at, d->seq2[i], 1);
	while (at & 15u)
		table12_put(information, &at, 0, 1);

	/* H1..H8, REF1..REF8 and the N active Ucodes. */
	for (i = 0; i < 8u; i++) {
		table12_put(information, &at, d->segmentSize[i], 7);
		table12_put(information, &at, 0, 1);
	}
	for (i = 0; i < 8u; i++) {
		table12_put(information, &at, d->segmentCode[i], 7);
		table12_put(information, &at, 0, 1);
	}
	for (i = 0; i < d->dilCount; i++) {
		table12_put(information, &at, d->dilCode[i], 7);
		table12_put(information, &at, 0, 1);
	}
	while (at & 15u)
		table12_put(information, &at, 0, 1);
	return at;
}

static unsigned int
table12_frame(short expected[NBITS], const tagV90DILdescriptor *d,
	      unsigned int *crc_at, unsigned int *crc_word)
{
	unsigned char information[NBITS];
	unsigned int info_len = table12_information(information, d);
	unsigned int at = 0;
	unsigned int i;

	for (i = 0; i < 17u; i++)
		expected[at++] = 1;
	for (i = 0; i < info_len; i++) {
		if ((i & 15u) == 0)
			expected[at++] = 0;
		expected[at++] = (short)information[i];
	}
	*crc_at = at;
	expected[at++] = 0;
	*crc_word = table12_crc(information, info_len);
	for (i = 0; i < 16u; i++)
		expected[at++] = (short)((*crc_word >> i) & 1u);
	expected[at++] = 0;
	if (at & 1u)
		expected[at++] = 0;
	return at;
}

static unsigned int
table12_wire_crc(const short *wire, unsigned int crc_at)
{
	unsigned int word = 0;
	unsigned int i;

	for (i = 0; i < 16u; i++)
		word |= (unsigned int)(wire[crc_at + 1u + i] & 1) << i;
	return word;
}

static int
table12_guard(const short *wire, unsigned int len)
{
	unsigned int i;

	if (len > NBITS)
		return 0;
	for (i = len; i < NBITS; i++)
		if (wire[i] != (short)0x5a5a)
			return 0;
	return 1;
}

static void
table12_descriptor(tagV90DILdescriptor *d, unsigned int n,
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
run_table12_oracle_subject(const struct table12_subject *s)
{
	static const struct {
		unsigned char n, lsp, ltp, seed;
	} legal[] = {
		{   0,   1,   1,  0 }, {   1,   1,   1,  3 },
		{   2,  16,  16,  5 }, {   3,  17,  15,  7 },
		{   7,  31,  32, 11 }, {   8,  32,  33, 13 },
		{  15,  63,  64, 17 }, {  16,  64,  65, 19 },
		{  31, 127, 128, 23 }, {  32, 128, 127, 29 },
		{ 127,   5,  11, 31 }, { 128, 100,   3, 37 },
		{ 143, 120,  60, 41 }, { 144, 120, 120, 43 },
		{ 254,   7,  23, 47 }, { 255, 128, 128, 53 }
	};
	tagV90DILdescriptor d;
	short actual[NBITS];
	short expected[NBITS];
	unsigned int trial;

	diff_begin(s->oracle_group);
	for (trial = 0; trial < sizeof(legal) / sizeof(legal[0]); trial++) {
		unsigned int crc_at, crc_word, expected_len;
		short actual_len = (short)0x7bcd;
		unsigned int i;

		table12_descriptor(&d, legal[trial].n, legal[trial].lsp,
				   legal[trial].ltp, legal[trial].seed);
		for (i = 0; i < NBITS; i++)
			actual[i] = (short)0x5a5a;
		expected_len = table12_frame(expected, &d, &crc_at, &crc_word);
		s->pack(&d, actual, &actual_len);

		diff_eq_int("Table 12 length is exact (%ld)", actual_len,
			    expected_len, (long)trial);
		diff_eq_int("Table 12 framed vector is exact (%ld)",
			    memcmp(actual, expected, expected_len * sizeof(short)),
			    0, (long)trial);
		diff_eq_int("Table 12 CRC has the exact information extent (%ld)",
			    table12_wire_crc(actual, crc_at), crc_word,
			    (long)trial);
		diff_eq_int("Table 12 kept the output guard (%ld)",
			    table12_guard(actual, (unsigned int)actual_len), 1,
			    (long)trial);
	}
	return diff_end();
}

struct table12_kat {
	unsigned int crc, crc_at, nbits;
	unsigned char bytes[35];
};

static void
table12_kat_descriptor(tagV90DILdescriptor *d, unsigned int which)
{
	unsigned int i;

	memset(d, 0, sizeof(*d));
	if (which == 0) {
		d->seq1Length = d->seq2Length = 1;
	} else if (which == 1) {
		d->dilCount = 1;
		d->seq1Length = d->seq2Length = 1;
		d->seq1[0] = d->seq2[0] = 1;
		for (i = 0; i < 8u; i++) {
			d->segmentSize[i] = (unsigned char)i;
			d->segmentCode[i] = (unsigned char)(16u * i);
		}
		d->dilCode[0] = 127;
	} else {
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
	}
}

static void
table12_bytes(unsigned char *bytes, const short *wire, unsigned int nbits)
{
	unsigned int i;

	memset(bytes, 0, (nbits + 7u) / 8u);
	for (i = 0; i < nbits; i++)
		bytes[i / 8u] |= (unsigned char)((wire[i] & 1) << (i & 7u));
}

static int
run_table12_kat_subject(const struct table12_subject *s)
{
	static const struct table12_kat kats[] = {
		{ 0x51f7, 221, 240,
		  { 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0xc0, 0x7d, 0x14 } },
		{ 0xd031, 238, 256,
		  { 0xff, 0xff, 0x05, 0x00, 0x00, 0x00, 0x10, 0x00,
		    0x20, 0x00, 0x00, 0x40, 0x00, 0x81, 0x01, 0x04,
		    0x05, 0x0c, 0x0e, 0x00, 0x40, 0x00, 0x81, 0x01,
		    0x04, 0x05, 0x0c, 0xce, 0x1f, 0x80, 0x18, 0x68 } },
		{ 0x377e, 255, 274,
		  { 0xff, 0xff, 0x09, 0x00, 0x80, 0x78, 0xa0, 0xaa,
		    0x0a, 0x00, 0x40, 0x00, 0xa0, 0x3f, 0x3f, 0x7d,
		    0x7c, 0xf6, 0xf4, 0xe4, 0xe1, 0xf9, 0x7b, 0xf3,
		    0xf5, 0xe4, 0xe7, 0xc5, 0xc7, 0x03, 0x80, 0x3f,
		    0x7e, 0x37, 0x00 } }
	};
	tagV90DILdescriptor d;
	short actual[NBITS];
	unsigned char bytes[35];
	unsigned int k;

	diff_begin(s->kat_group);
	for (k = 0; k < sizeof(kats) / sizeof(kats[0]); k++) {
		short actual_len = (short)0x7bcd;
		unsigned int i;

		table12_kat_descriptor(&d, k);
		for (i = 0; i < NBITS; i++)
			actual[i] = (short)0x5a5a;
		s->pack(&d, actual, &actual_len);
		table12_bytes(bytes, actual, kats[k].nbits);

		diff_eq_int("fixed Table 12 length KAT is exact (%ld)",
			    actual_len, kats[k].nbits, (long)k);
		diff_eq_int("fixed Table 12 packed-wire KAT is exact (%ld)",
			    memcmp(bytes, kats[k].bytes,
				   (kats[k].nbits + 7u) / 8u), 0, (long)k);
		diff_eq_int("fixed Table 12 CRC KAT is exact (%ld)",
			    table12_wire_crc(actual, kats[k].crc_at), kats[k].crc,
			    (long)k);
		diff_eq_int("fixed Table 12 KAT kept the guard (%ld)",
			    table12_guard(actual, (unsigned int)actual_len), 1,
			    (long)k);
	}
	return diff_end();
}

static int
run_table12_departure_subject(const struct table12_subject *s)
{
	tagV90DILdescriptor d;
	short actual[NBITS];
	short actual_len = (short)0x7bcd;
	unsigned int i;
	int inactive_ones = 1;

	diff_begin(s->departure_group);
	table12_kat_descriptor(&d, 1);
	d.dilCode[1] = 127; /* inactive storage: Table 12 requires reserved zero */
	for (i = 0; i < NBITS; i++)
		actual[i] = (short)0x5a5a;
	s->pack(&d, actual, &actual_len);
	for (i = 230; i <= 236; i++)
		if (actual[i] != 1)
			inactive_ones = 0;

	diff_eq_int("expected departure: inactive odd-N Ucode is emitted",
		    inactive_ones, 1, 0);
	diff_eq_int("expected departure: inactive Ucode changes the CRC",
		    table12_wire_crc(actual, 238), 0x5b41, 0);
	diff_eq_int("odd-N departure leaves the exact descriptor length",
		    actual_len, 256, 0);
	diff_eq_int("odd-N departure kept the output guard",
		    table12_guard(actual, (unsigned int)actual_len), 1, 0);
	return diff_end();
}

static int
run_table12(void)
{
	static const struct table12_subject subjects[] = {
		{
			"V.90 Table 12 reconstruction standards oracle",
			"V.90 Table 12 reconstruction fixed KATs",
			"V.90 Table 12 reconstruction expected departure",
			table12_our_pack
		},
		{
			"V.90 Table 12 blob standards oracle",
			"V.90 Table 12 blob fixed KATs",
			"V.90 Table 12 blob expected departure",
			table12_blob_pack
		}
	};
	unsigned int i;
	int rc = 0;

	for (i = 0; i < sizeof(subjects) / sizeof(subjects[0]); i++) {
		rc |= run_table12_oracle_subject(&subjects[i]);
		rc |= run_table12_kat_subject(&subjects[i]);
		rc |= run_table12_departure_subject(&subjects[i]);
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
	rc |= run_table12();

	return rc;
}
