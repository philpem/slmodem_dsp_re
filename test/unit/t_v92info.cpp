/*
 * t_v92info.cpp -- differential test of V92CP::infoToBits and the two weight
 * tables it reads.
 *
 * SEPARATE FROM t_v92cpcrc.cpp, which drives the seven small members.  This
 * one drives the single 1,916-byte member that writes the whole bit vector,
 * and it needs the message FIELDS set rather than the vector: the object is
 * seeded at random, the fields at +0x000..+0x10c are then overwritten from a
 * case, both sides are handed the same bytes, and all 0x918 are compared
 * afterwards.  Nothing has to be masked out -- the class allocates nothing and
 * holds no pointer.
 *
 * ---------------------------------------------------------------------------
 * THE GRID IS BOUNDED, AND SAYING WHERE IS PART OF THE TEST.  Three inputs
 * reach undefined behaviour in OUR source, so a trial that used them would be
 * the period compiler adjudicating our UB rather than the object (D561):
 *
 *   - `bitsPerSymbol == 0`.  The padded length is `n / (12 * bitsPerSymbol)` and the
 *     object's `div` is unsigned, so zero divides by zero on both sides.  The
 *     grid uses 1..6.
 *
 *   - `word_10c > 6`.  It is the group count for two blocks that hold six
 *     groups each, and NOTHING bounds it -- the object trusts the field and
 *     walks past `codecConstellationMask` into `word_104` and beyond.  The grid uses 0..6,
 *     and docs/deviations.md D570 records the missing bound.
 *
 *   - a NaN or an infinity in any of the five floats.  `-mno-ieee-fp` leaves
 *     the comparisons unordered-blind, and the first sign entry is `0 > f`
 *     where the other three are `f < 0`; those are the same predicate for
 *     every number and opposite ones for a NaN.  The grid uses finite values.
 *
 * A fourth bound is arithmetic rather than UB: the magnitudes stay under 1000,
 * because the expansion subtracts weights down to 2^-13 and a float has 24
 * bits of mantissa.  Past about 2^11 the residue stops being exactly
 * representable and the answer would depend on whether the compiler kept it in
 * an x87 register, which is not what this test is measuring.
 *
 * ---------------------------------------------------------------------------
 * THE TRIAL THAT ADJUDICATES THE `dataBitRate` TEMPORARY.  The object takes five
 * bits of +0x002 into bits[21..25] and thirteen more into bits[36..48] out of
 * the SAME register, with no second load, so the thirteen are the sign
 * extension of a signed byte.  A version that re-read the field would put bits
 * 0..12 there instead.
 *
 * `dataBitRate = -1` CANNOT TELL THEM APART: 0xff gives thirteen ones under both
 * readings.  Neither can 0.  What separates them is a value with a one below
 * bit 5 and a clear bit 7 -- `0x0f` gives thirteen zeros for the object and
 * `1,1,1,1,0,...` for a reload -- and four cases below carry one.
 *
 * ---------------------------------------------------------------------------
 * THE TABLES ARE COMPARED DIRECTLY.  `fltTable_1` and `fltTable_2` are GLOBAL
 * data symbols, so the blob's own copies are linked in beside ours as
 * `ref_fltTable_*` and the two are compared byte for byte.  That is what makes
 * fltTable_2's missing 2^-8 a tested fact rather than a transcription anybody
 * has to trust, and several cases feed magnitudes small enough to reach the
 * entries either side of it.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92CP.h"

extern "C" {
void our_cp_infotobits(void *) asm("_ZN5V92CP10infoToBitsEv");
void ref_cp_infotobits(void *) asm("ref__ZN5V92CP10infoToBitsEv");

extern float ref_fltTable_1[7];
extern float ref_fltTable_2[16];
}

extern float fltTable_1[7];
extern float fltTable_2[16];

/*
 * 256 bytes past the class.  Nothing in this function should reach them --
 * the padding stops at `vectorLen`, which the grid keeps inside `bits` -- so
 * the tail is here to PROVE that rather than to be needed.
 */
#define TAIL	256
#define SLOT	(sizeof(V92CP) + TAIL)

static unsigned char cp[2][SLOT] __attribute__((aligned(8)));

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static V92CP *
C(int s)
{
	return (V92CP *)cp[s];
}

/*
 * How the six four-byte entries and the two mask blocks are filled.  Which
 * one a case uses matters: ZEROS makes every mask entry seventeen zeros and
 * hides an off-by-one in the bit order, PATTERN makes each entry distinct in
 * every bit, and DRAW takes whatever the generator gives.
 */
#define FILL_ZEROS	0
#define FILL_PATTERN	1
#define FILL_DRAW	2

struct icase {
	const char *name;
	unsigned char byte_00;
	signed char char_01;
	signed char dataBitRate;
	unsigned char byte_03;
	unsigned char byte_04;
	unsigned int shaperSR;
	unsigned int shaperId;
	float f10, f14, f18, f1c, f20;
	unsigned char byte_24;
	unsigned short word_10c;
	unsigned char bitsPerSymbol;
	unsigned int word_104;
	unsigned int suv;
	int fill;
};

/*
 * `2^-8` IS THE ONE THAT MATTERS.  fltTable_2 has no entry for it and holds
 * 2^-9 twice, so a magnitude of exactly 0.00390625 sets the two 2^-9 entries
 * under the object's table and one 2^-8 entry under a repaired one.  Three
 * cases feed it and one feeds the value just above.
 */
static const struct icase cases[] = {
 /*                             00  01  02   03   04   w08  w0c
  *   f10        f14      f18      f1c      f20     24 10c 128 w104 suv  fill */
 { "everything zero",
      0,  0,  0, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1,  0,  0, FILL_ZEROS },
 { "the SUV form",
      1,  0,  0, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  1, FILL_ZEROS },
 { "the SUV form, sixteen and a full byte",
      1,  5, -3, 0xff, 0x01, ~0u, ~0u,
      1.0f,     1.0f,    1.0f,    1.0f,    1.0f,     1,  3,  2, 16, 0xa5, FILL_PATTERN },
 { "the SUV form, +0x104 with every low bit",
      1,  0,  0, 0x00, 0x01, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  3, 0x1f, 0xff, FILL_ZEROS },
 { "the SUV form, +0x104 negative",
      1,  0,  0, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 0xffffu, 0, FILL_ZEROS },
 { "byte_00 is two, so not the SUV form",
      2,  4,  0x0f, 0x03, 0x00, 1u, 2u,
      0.5f,     0.25f,  -0.25f,   0.125f, -0.125f,   1,  2,  1, 16,  0, FILL_PATTERN },
 { "the short form: char_01 above one",
      0,  2,  0x0f, 0x11, 0x01, 3u, 1u,
      1.5f,    -0.5f,    0.5f,   -0.75f,   0.75f,    1,  4,  2, 16,  0, FILL_PATTERN },
 { "the short form at char_01 = 127",
      0,127,  0x55, 0xaa, 0x00, 2u, 3u,
      3.5f,     0.9f,   -0.9f,    0.1f,   -0.1f,     0,  6,  5, 16,  0, FILL_DRAW },
 { "the long form at char_01 = 1",
      0,  1,  0x0f, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "the long form at char_01 = 0, which sets bitsPerSymbol",
      0,  0,  0x0f, 0x01, 0x01, 1u, 1u,
      1.0f,     0.5f,   -0.5f,    0.25f,  -0.25f,    1,  1,  4, 16,  0, FILL_PATTERN },
 { "the long form at char_01 = -1",
      0, -1,  0x0f, 0x02, 0x00, 2u, 2u,
      2.0f,    -1.0f,    1.0f,   -1.0f,    1.0f,     1,  6,  1, 16,  0, FILL_PATTERN },
 { "the long form at char_01 = -128",
      0,-128, 0x33, 0x7f, 0x01, 3u, 3u,
      7.0f,     0.03125f, -0.03125f, 0.015625f, -0.015625f,
                                                    1,  6,  3, 16,  0, FILL_DRAW },
 { "char_02 = 0x0f, the trial the reload fails",
      0,  1,  0x0f, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "char_02 = 0x01, one bit and no others",
      0,  1,  0x01, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "char_02 = -1, which cannot separate the two",
      0,  1,   -1,  0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "char_02 = -16, sign set and low bits clear",
      0,  1,  -16,  0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "char_02 = 0x7f, every bit but the sign",
      0,  1, 0x7f, 0x00, 0x00, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "2^-8, which fltTable_2 does not hold",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      0.00390625f, 0.0f, 0.0f,   0.0f,    0.0f,      0,  0,  1, 16,  0, FILL_ZEROS },
 { "2^-8 negative, and 2^-9 beside it",
      0,  1, 0x0f, 0x00, 0x01, 1u, 2u,
     -0.00390625f, 0.001953125f, -0.001953125f, 0.00390625f, -0.00390625f,
                                                    0,  0,  2, 16,  0, FILL_ZEROS },
 { "2^-7 and the entries below it",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      0.0078125f, 0.015625f, -0.015625f, 0.0078125f, -0.0078125f,
                                                    0,  1,  1, 16,  0, FILL_PATTERN },
 { "the smallest weight and less than it",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      0.0001220703125f, 0.0078125f, 0.00390625f, 0.001953125f, 0.0009765625f,
                                                    0,  2,  1, 16,  0, FILL_PATTERN },
 { "below every weight",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      0.00001f, 0.001f,  0.001f, -0.001f, -0.001f,   0,  3,  1, 16,  0, FILL_PATTERN },
 { "every entry set: the sum of the whole table",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      7.99609375f, 1.984375f, -1.984375f, 1.984375f, -1.984375f,
                                                    1,  4,  1, 16,  0, FILL_PATTERN },
 { "over the table, so every entry saturates",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      900.0f,  500.0f, -500.0f,  17.0f,  -17.0f,     1,  5,  6, 16,  0, FILL_DRAW },
 { "negative zero",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
     -0.0f,    -0.0f,   -0.0f,   -0.0f,   -0.0f,     0,  0,  1, 16,  0, FILL_ZEROS },
 { "a value between two weights",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      1.3f,     0.7f,   -0.7f,    0.37f,  -0.37f,    1,  6,  2, 16,  0, FILL_DRAW },
 { "word_08 and word_0c with only the low bits",
      0,  1, 0x0f, 0x00, 0x00, 3u, 3u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  6,  1, 16,  0, FILL_PATTERN },
 { "word_08 and word_0c with only the high bits",
      0,  1, 0x0f, 0x00, 0x00, 0xfffffffcu, 0xfffffffcu,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  6,  1, 16,  0, FILL_PATTERN },
 { "byte_24 clear, so the second block is skipped",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     0,  6,  1, 16,  0, FILL_PATTERN },
 { "byte_24 set with no groups at all",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  0,  1, 16,  0, FILL_PATTERN },
 { "byte_24 = 0xff rather than one",
      0,  1, 0x0f, 0x00, 0x00, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,  0xff,  6,  1, 16,  0, FILL_DRAW },
 { "the longest message the class can hold",
      0,  1, 0x0f, 0xff, 0x01, ~0u, ~0u,
      3.0f,     0.5f,   -0.5f,    0.5f,   -0.5f,     1,  6,  1, 16,  0, FILL_DRAW },
 { "the longest message with the largest quantum",
      0,  1, 0x0f, 0xff, 0x01, ~0u, ~0u,
      3.0f,     0.5f,   -0.5f,    0.5f,   -0.5f,     1,  6,  6, 16,  0, FILL_DRAW },
 { "bitsPerSymbol = 5, which divides nothing evenly",
      0,  1, 0x0f, 0x12, 0x00, 5u, 5u,
      1.0f,     0.5f,   -0.5f,    0.5f,   -0.5f,     1,  3,  5, 16,  0, FILL_PATTERN },
 { "byte_04 set, which raises word_110",
      0,  1, 0x0f, 0x00, 0x01, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  2,  1, 16,  0, FILL_PATTERN },
 { "byte_04 = 0xff",
      0,  1, 0x0f, 0x00, 0xff, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  2,  1, 16,  0, FILL_PATTERN },
 { "byte_04 set on the short form",
      0,  9, 0x0f, 0x00, 0x01, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  2,  1, 16,  0, FILL_PATTERN },
 { "byte_04 set on the SUV form",
      1,  0, 0x00, 0x00, 0x01, 0u, 0u,
      0.0f,     0.0f,    0.0f,    0.0f,    0.0f,     0,  0,  1, 16,  7, FILL_ZEROS },
 { "byte_03 = 0xff, stored whole",
      0,  1, 0x0f, 0xff, 0x00, 0u, 0u,
      1.0f,     1.0f,   -1.0f,    1.0f,   -1.0f,     1,  1,  1, 16,  0, FILL_PATTERN },
 { "everything drawn",
      0,  1, 0x5a, 0x37, 0x01, 0x13579bdfu, 0x2468aceu,
      2.5f,    -0.6f,    0.6f,   -0.2f,    0.2f,     1,  6,  4, 16,  0, FILL_DRAW }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * Seed both objects from one draw, then lay the case over side 0 and copy.
 * Everything the case does not name -- every pad, the CRC register, the whole
 * bit vector -- is random and identical on the two sides, so a byte the
 * function fails to write shows up as agreement and a byte it writes
 * differently shows up at once.
 */
static void
setup(int c, unsigned int seed)
{
	const struct icase *k = &cases[c];
	V92CP *o = C(0);
	unsigned int i, j;

	lfsr = seed | 1u;

	for (i = 0; i < SLOT; i++)
		cp[0][i] = nextbyte();

	o->byte_00 = k->byte_00;
	o->char_01 = k->char_01;
	o->dataBitRate = k->dataBitRate;
	o->byte_03 = k->byte_03;
	o->byte_04 = k->byte_04;
	o->shaperSR = k->shaperSR;
	o->shaperId = k->shaperId;
	o->flt_10 = k->f10;
	o->shaperA1 = k->f14;
	o->shaperA2 = k->f18;
	o->shaperB1 = k->f1c;
	o->shaperB2 = k->f20;
	o->byte_24 = k->byte_24;
	o->word_104 = k->word_104;
	o->suv = k->suv;
	o->word_10c = k->word_10c;
	o->bitsPerSymbol = k->bitsPerSymbol;

	for (i = 0; i < V92CP_GROUPS; i++) {
		switch (k->fill) {
		case FILL_ZEROS:
			o->distinctIndex[i] = 0;
			break;
		case FILL_PATTERN:
			o->distinctIndex[i] = (int)(0x11111111u * (i + 1u));
			break;
		default:
			o->distinctIndex[i] = (int)((unsigned int)nextbyte() << 24 |
					      (unsigned int)nextbyte() << 16 |
					      (unsigned int)nextbyte() << 8 |
					      (unsigned int)nextbyte());
			break;
		}

		for (j = 0; j < V92CP_MASKS; j++) {
			short a, b;

			switch (k->fill) {
			case FILL_ZEROS:
				a = 0;
				b = 0;
				break;
			case FILL_PATTERN:
				a = (short)(0x8001 + (int)(i * 8 + j));
				b = (short)(0x0f0f ^ (int)(i * 8 + j));
				break;
			default:
				a = (short)((int)nextbyte() << 8 |
					    (int)nextbyte());
				b = (short)((int)nextbyte() << 8 |
					    (int)nextbyte());
				break;
			}
			o->constellationMask[i][j] = a;
			o->codecConstellationMask[i][j] = b;
		}
	}

	memcpy(cp[1], cp[0], SLOT);
}

static void
compare(const char *what, long tag)
{
	diff_eq_obj(what, V92CP, C(0), C(1), tag);
	diff_eq_int("nothing was written past the object (%ld)",
		    memcmp(cp[0] + sizeof(V92CP), cp[1] + sizeof(V92CP),
			   TAIL) == 0, 1, tag);
}

/*
 * The tables, compared against the blob's own copies rather than against a
 * transcription anybody has to trust.
 */
static int
run_tables(void)
{
	int i;

	diff_begin("the two weight tables");

	for (i = 0; i < 7; i++)
		diff_eq_int("fltTable_1[%ld] agrees",
			    (long)memcmp(&fltTable_1[i], &ref_fltTable_1[i],
					 sizeof(float)), 0, (long)i);

	for (i = 0; i < 16; i++)
		diff_eq_int("fltTable_2[%ld] agrees",
			    (long)memcmp(&fltTable_2[i], &ref_fltTable_2[i],
					 sizeof(float)), 0, (long)i);

	/*
	 * ANTI-VACUITY.  The two entries the sequence would not have predicted
	 * are stated outright, so a table that quietly became geometric fails
	 * here as well as at the comparison above.
	 */
	diff_eq_int("fltTable_2[10] is 2^-9 and not 2^-8 (%ld)",
		    (long)(ref_fltTable_2[10] == 0.001953125f), 1, 10);
	diff_eq_int("fltTable_2[11] is 2^-9 as well (%ld)",
		    (long)(ref_fltTable_2[11] == 0.001953125f), 1, 11);
	diff_eq_int("no entry of fltTable_2 is 2^-8 (%ld)",
		    (long)(ref_fltTable_2[10] != 0.00390625f &&
			   ref_fltTable_2[11] != 0.00390625f), 1, 0);

	return diff_end();
}

static int
run_cases(void)
{
	int c;

	diff_begin("infoToBits over the grid");

	for (c = 0; c < NCASE; c++) {
		setup(c, 0x4444u + (unsigned int)c * 7u);

		our_cp_infotobits(C(0));
		ref_cp_infotobits(C(1));

		compare(cases[c].name, (long)c);

		/*
		 * The two lengths are the point of the naming, so they are
		 * checked by name and not only through the object compare --
		 * which would catch the same thing but not say what it was.
		 */
		diff_eq_int("msgLen agrees (%ld)", (long)C(0)->msgLen,
			    (long)C(1)->msgLen, (long)c);
		diff_eq_int("vectorLen agrees (%ld)", (long)C(0)->vectorLen,
			    (long)C(1)->vectorLen, (long)c);
		diff_eq_int("word_11c agrees (%ld)", (long)C(0)->bitIndex,
			    (long)C(1)->bitIndex, (long)c);

		/*
		 * ANTI-VACUITY, and the reason the grid is bounded the way it
		 * is: every trial must land inside the vector, or the
		 * agreement above is agreement about memory neither side owns.
		 */
		diff_eq_int("the padded length is inside bits (%ld)",
			    (long)(C(1)->vectorLen <= V92CP_BITS), 1, (long)c);
		diff_eq_int("the message ends before the padding (%ld)",
			    (long)(C(1)->msgLen < C(1)->vectorLen), 1,
			    (long)c);

		/*
		 * The relation this function established, checked against the
		 * blob's own answer: the padded length is a multiple of twelve
		 * quanta and is STRICTLY above the cursor.
		 */
		diff_eq_int("vectorLen is a multiple of 12*bitsPerSymbol (%ld)",
			    (long)(C(1)->vectorLen %
				   (12u * cases[c].bitsPerSymbol)), 0, (long)c);
		diff_eq_int("and strictly above the cursor (%ld)",
			    (long)(C(1)->vectorLen >
				   (unsigned int)C(1)->bitIndex), 1, (long)c);
	}

	return diff_end();
}

/*
 * A sweep with the fields drawn rather than chosen.  The chosen grid above
 * says what each input does; this says that nothing else does.  `word_10c`
 * and `bitsPerSymbol` are the two that are clamped, for the reasons at the top.
 */
static int
run_sweep(void)
{
	int n;

	diff_begin("infoToBits over drawn fields");

	for (n = 0; n < 96; n++) {
		V92CP *o;
		unsigned int i;

		setup(n % NCASE, 0x9000u + (unsigned int)n * 13u);

		o = C(0);
		o->byte_00 = (unsigned char)(nextbyte() & 3u);
		o->char_01 = (signed char)nextbyte();
		o->dataBitRate = (signed char)nextbyte();
		o->byte_03 = nextbyte();
		o->byte_04 = (unsigned char)(nextbyte() & 1u);
		o->shaperSR = (unsigned int)nextbyte();
		o->shaperId = (unsigned int)nextbyte();
		o->byte_24 = (unsigned char)(nextbyte() & 1u);
		o->word_10c = (unsigned short)(nextbyte() % 7u);
		o->bitsPerSymbol = (unsigned char)(nextbyte() % 6u + 1u);
		o->word_104 = (unsigned int)nextbyte();
		o->suv = (unsigned int)nextbyte();

		/*
		 * Magnitudes under eight for the sixteen-entry table and under
		 * two for the seven-entry one, with a quarter of the draws
		 * below the smallest weight so the tail of each table is
		 * reached rather than only its head.
		 */
		for (i = 0; i < 5; i++) {
			int raw = (int)nextbyte() - 128;
			float v = (float)raw / 32.0f;

			if ((nextbyte() & 3u) == 0u)
				v = (float)raw / 262144.0f;

			switch (i) {
			case 0:	o->flt_10 = v;	break;
			case 1:	o->shaperA1 = v;	break;
			case 2:	o->shaperA2 = v;	break;
			case 3:	o->shaperB1 = v;	break;
			default: o->shaperB2 = v;	break;
			}
		}

		memcpy(cp[1], cp[0], SLOT);

		our_cp_infotobits(C(0));
		ref_cp_infotobits(C(1));

		compare("after infoToBits", (long)n);
		diff_eq_int("msgLen agrees (%ld)", (long)C(0)->msgLen,
			    (long)C(1)->msgLen, (long)n);
		diff_eq_int("vectorLen agrees (%ld)", (long)C(0)->vectorLen,
			    (long)C(1)->vectorLen, (long)n);
		diff_eq_int("the padded length is inside bits (%ld)",
			    (long)(C(1)->vectorLen <= V92CP_BITS), 1, (long)n);
	}

	return diff_end();
}

/*
 * The grid has to REACH the three forms and the two mask blocks, or a green
 * run says only that nothing was exercised.  This counts what the reference
 * produced, and it is a count of OBSERVABLE outcomes -- distinct message
 * lengths and distinct forms -- rather than of source paths.
 */
static int
run_reach(void)
{
	int c;
	int suv = 0;
	int shortform = 0;
	int longform = 0;
	int second = 0;
	unsigned int shortest = ~0u;
	unsigned int longest = 0;

	for (c = 0; c < NCASE; c++) {
		unsigned int len;

		setup(c, 0x4444u + (unsigned int)c * 7u);
		ref_cp_infotobits(C(1));
		len = C(1)->msgLen;

		if (len < shortest)
			shortest = len;
		if (len > longest)
			longest = len;

		if (cases[c].byte_00 == 1)
			suv++;
		else if (cases[c].char_01 > 1)
			shortform++;
		else
			longform++;

		if (len > 153u)
			second++;
	}

	diff_begin("what the grid reaches");
	diff_eq_int("the SUV form is reached (%ld)", (long)(suv > 0), 1, 0);
	diff_eq_int("the short form is reached (%ld)", (long)(shortform > 0),
		    1, 0);
	diff_eq_int("the long form is reached (%ld)", (long)(longform > 0), 1,
		    0);
	diff_eq_int("a message past the fixed part is reached (%ld)",
		    (long)(second > 0), 1, 0);
	diff_eq_int("the shortest message is 51 (%ld)", (long)shortest, 51, 0);
	diff_eq_int("the longest message passes 1700 (%ld)",
		    (long)(longest > 1700u), 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the class's map");
	diff_eq_int("sizeof(V92CP) is %ld", (long)sizeof(V92CP), 0x918, 0x918);
	diff_eq_int("byte_00 is at +0x%lx", (long)offsetof(V92CP, byte_00), 0,
		    0);
	diff_eq_int("word_08 is at +0x%lx", (long)offsetof(V92CP, shaperSR),
		    0x08, 0x08);
	diff_eq_int("flt_10 is at +0x%lx", (long)offsetof(V92CP, flt_10),
		    0x10, 0x10);
	diff_eq_int("word_28 is at +0x%lx", (long)offsetof(V92CP, distinctIndex),
		    0x28, 0x28);
	diff_eq_int("short_42 is at +0x%lx", (long)offsetof(V92CP, constellationMask),
		    0x42, 0x42);
	diff_eq_int("short_a2 is at +0x%lx", (long)offsetof(V92CP, codecConstellationMask),
		    0xa2, 0xa2);
	diff_eq_int("word_10c is at +0x%lx", (long)offsetof(V92CP, word_10c),
		    0x10c, 0x10c);
	diff_eq_int("bitsPerSymbol is at +0x%lx", (long)offsetof(V92CP, bitsPerSymbol),
		    0x128, 0x128);
	diff_eq_int("vectorLen is at +0x%lx", (long)offsetof(V92CP, vectorLen),
		    0x90c, 0x90c);
	diff_eq_int("msgLen is at +0x%lx", (long)offsetof(V92CP, msgLen),
		    0x910, 0x910);
	rc |= diff_end();

	rc |= run_tables();
	rc |= run_reach();
	rc |= run_cases();
	rc |= run_sweep();

	return rc;
}
