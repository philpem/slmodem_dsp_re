/*
 * t_v92cpcrc.cpp -- differential test of the seven V92CP members that are not
 * the constructor or the destructor.
 *
 * SEPARATE FROM t_v90cp.cpp, which drives V90CP's and V92CP's construction and
 * destruction and never lets an object survive a call.  This one needs the
 * opposite: an object in a chosen state, one member run on each side, and the
 * whole 0x918 bytes compared afterwards.
 *
 * NO HEAP AND NO CONSTRUCTOR.  `V92CP` allocates nothing -- its destructor is
 * a bare `ret` -- so both sides are plain byte arrays seeded from one draw and
 * driven through the mangled names.  Every field is therefore comparable by
 * name and nothing has to be masked out, which is what makes this fixture
 * sharper than the ones that build a chain of sub-objects.
 *
 * ---------------------------------------------------------------------------
 * THE TRIAL THAT ADJUDICATES `calcCRC` IS THE ONE THAT OVERRUNS `bits`.
 *
 * The object copies the sixteen-byte CRC register into stack slots before its
 * loop and back afterwards, and reads `bits[i]` inside without ever writing to
 * the object.  Written the short way -- updating `crc[]` in place -- the
 * function behaves identically for every message that fits, because `bits` is
 * 2,000 entries and the loop stops 17 short of `msgLen`.
 *
 * The two separate the moment the loop reaches index 2000, which is where
 * `crc` begins: the in-place reading would then feed the register's own
 * updated bytes back through the shift, and the object's reading feeds the
 * values it started with.  `msgLen` above 2,017 is what gets there, and
 * three cases below do.  Nothing a modem sends is that long; this is finding
 * F3052's shape and the trials leave the plausible range on purpose.
 *
 * THE OBJECT IS GIVEN A TAIL, and that is not slack.  At `msgLen = 2200`
 * the loop reads `bits[2182]`, which is `this + 2479` -- 151 bytes past the
 * 0x918 the class occupies.  The blob reads it, so the fixture has to own it;
 * both sides get the same bytes there and the tail is compared like the rest.
 *
 * ---------------------------------------------------------------------------
 * BITS THAT ARE NOT BITS.  Half the cases fill `bits` with 0/1 as the protocol
 * would and half with arbitrary bytes.  The second half is what reaches the
 * byte arithmetic the object actually wrote: `calcCRC`'s feedback is a byte
 * sum that is masked only where it is stored, and `evaluateCRC` accumulates
 * sixteen ABSOLUTE DIFFERENCES into one byte, which wraps.  Neither is
 * observable on a 0/1 alphabet.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92CP.h"

extern "C" {
unsigned char *our_cp_getbv(void *, unsigned int &)
	asm("_ZN5V92CP12getBitVectorERj");
void our_cp_setsuv(void *, unsigned int) asm("_ZN5V92CP6setSUVEj");
void our_cp_resetcrc(void *) asm("_ZN5V92CP8resetCRCEv");
void our_cp_resetdet(void *) asm("_ZN5V92CP13resetDetectorEv");
void our_cp_reset(void *) asm("_ZN5V92CP5resetEv");
void our_cp_calccrc(void *) asm("_ZN5V92CP7calcCRCEv");
int our_cp_evalcrc(void *) asm("_ZN5V92CP11evaluateCRCEv");

unsigned char *ref_cp_getbv(void *, unsigned int &)
	asm("ref__ZN5V92CP12getBitVectorERj");
void ref_cp_setsuv(void *, unsigned int) asm("ref__ZN5V92CP6setSUVEj");
void ref_cp_resetcrc(void *) asm("ref__ZN5V92CP8resetCRCEv");
void ref_cp_resetdet(void *) asm("ref__ZN5V92CP13resetDetectorEv");
void ref_cp_reset(void *) asm("ref__ZN5V92CP5resetEv");
void ref_cp_calccrc(void *) asm("ref__ZN5V92CP7calcCRCEv");
int ref_cp_evalcrc(void *) asm("ref__ZN5V92CP11evaluateCRCEv");
}

/*
 * 256 bytes past the class.  See the header comment: at the longest message
 * the grid uses, the blob reads 151 bytes past `sizeof(V92CP)`.
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
 * `len` is what goes into +0x910.  `binary` says whether `bits` is filled with
 * the 0/1 the protocol uses or with whatever the generator draws.
 */
struct cpcase {
	const char *name;
	unsigned int len;
	int binary;
};

/*
 * NOTHING BELOW SEVENTEEN.  `calcCRC` computes its bound as `msgLen - 17`
 * in unsigned arithmetic and compares 18 against it with `jae`, so a length of
 * sixteen or less wraps to about four billion and the loop walks the whole
 * address space.  The blob does exactly that -- there is no guard anywhere in
 * .text+0x4e5f0 -- so it is a deviation and not a difference, recorded as
 * docs/deviations.md D504 and kept out of the grid rather than "tested".
 * Seventeen is the first length that behaves, and it behaves by returning.
 */
static const struct cpcase cases[] = {
	{ "the shortest length that does not wrap", 17, 1 },
	{ "length below the start",	30,	1 },
	{ "length exactly at the start",35,	1 },
	{ "one clock",			36,	1 },
	{ "just past the first skip",	52,	1 },
	{ "two skips",			70,	1 },
	{ "a short message",		120,	1 },
	{ "a short message, raw bytes",	120,	0 },
	{ "a typical message",		400,	1 },
	{ "a typical message, raw",	400,	0 },
	{ "the whole bit vector",	2000,	1 },
	{ "up to the register's start",	2017,	1 },
	/*
	 * PAST THE REGISTER.  These three are the only trials in which the
	 * loop reads the bytes it is updating, and therefore the only ones
	 * that can tell the object's copied register from an in-place one.
	 */
	{ "one clock into the register",2018,	1 },
	{ "well into the register",	2100,	1 },
	{ "well into the register, raw",2100,	0 },
	{ "past the object entirely",	2200,	0 }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

static void
setup(int c, unsigned int seed)
{
	unsigned int i;

	lfsr = seed | 1u;

	/* One draw, copied, so the two sides start byte for byte identical. */
	for (i = 0; i < SLOT; i++)
		cp[0][i] = nextbyte();
	memcpy(cp[1], cp[0], SLOT);

	for (i = 0; i < V92CP_BITS + TAIL; i++) {
		unsigned char b = nextbyte();

		if (cases[c].binary)
			b = (unsigned char)(b & 1u);
		C(0)->bits[i] = b;
	}
	memcpy(cp[1], cp[0], SLOT);

	C(0)->msgLen = cases[c].len;
	C(1)->msgLen = cases[c].len;
}

static void
compare(const char *what, long tag)
{
	diff_eq_obj(what, V92CP, C(0), C(1), tag);
	diff_eq_int("the tail past the object agrees (%ld)",
		    memcmp(cp[0] + sizeof(V92CP), cp[1] + sizeof(V92CP),
			   TAIL) == 0, 1, tag);
}

static int
run_small(void)
{
	int c;
	int rc;

	diff_begin("getBitVector");

	for (c = 0; c < NCASE; c++) {
		unsigned int ol = 0xdeadbeefu;
		unsigned int rl = 0xdeadbeefu;
		unsigned char *op;
		unsigned char *rp;

		setup(c, 0x1111u + (unsigned int)c);
		op = our_cp_getbv(C(0), ol);
		rp = ref_cp_getbv(C(1), rl);

		diff_eq_int("the length agrees (%ld)", (long)ol, (long)rl,
			    (long)c);
		diff_eq_int("the length is +0x90c (%ld)", (long)rl,
			    (long)C(1)->vectorLen, (long)c);
		diff_eq_int("the vector is at +0x129 (%ld)",
			    (long)(op - cp[0]), (long)(rp - cp[1]), (long)c);
		diff_eq_int("and that offset is 0x129 (%ld)",
			    (long)(rp - cp[1]), 0x129, (long)c);
		compare("after getBitVector", (long)c);
	}

	rc = diff_end();

	diff_begin("setSUV");

	for (c = 0; c < NCASE; c++) {
		static const unsigned int v[] = {
			0, 1, 16, 0x7fffffffu, 0xffffffffu
		};
		int k;

		for (k = 0; k < (int)(sizeof(v) / sizeof(v[0])); k++) {
			long tag = (long)(c * 10 + k);

			setup(c, 0x2222u + (unsigned int)tag);
			our_cp_setsuv(C(0), v[k]);
			ref_cp_setsuv(C(1), v[k]);

			compare("after setSUV", tag);
			diff_eq_int("the argument landed at +0x108 (%ld)",
				    (long)C(1)->suv, (long)v[k], tag);
			diff_eq_int("+0x104 is sixteen (%ld)",
				    (long)C(1)->word_104, 16, tag);
		}
	}

	rc |= diff_end();

	diff_begin("resetCRC, resetDetector and reset");

	for (c = 0; c < NCASE; c++) {
		setup(c, 0x3333u + (unsigned int)c);
		our_cp_resetcrc(C(0));
		ref_cp_resetcrc(C(1));
		compare("after resetCRC", (long)c);
		diff_eq_int("every stage is one (%ld)",
			    (int)(C(1)->crc[0] == 1 && C(1)->crc[15] == 1), 1,
			    (long)c);

		setup(c, 0x4444u + (unsigned int)c);
		our_cp_resetdet(C(0));
		ref_cp_resetdet(C(1));
		compare("after resetDetector", 100 + (long)c);
		diff_eq_int("+0x11c is eighteen (%ld)", (long)C(1)->word_11c,
			    18, (long)c);

		/*
		 * ANTI-VACUITY FOR THE ONE STORE THAT SEPARATES `reset` FROM
		 * `resetDetector`.  The seed leaves +0x914 holding rubbish, so
		 * a `reset` that dropped the -1 would leave the two sides
		 * agreeing on rubbish; the absolute check is what makes the
		 * store observable, and the differential one catches a wrong
		 * value.
		 */
		setup(c, 0x5555u + (unsigned int)c);
		our_cp_reset(C(0));
		ref_cp_reset(C(1));
		compare("after reset", 200 + (long)c);
		diff_eq_int("+0x914 is -1 (%ld)", (long)C(1)->word_914, -1,
			    (long)c);
		diff_eq_int("+0x11c is eighteen (%ld)", (long)C(1)->word_11c,
			    18, (long)c);

		/*
		 * And the difference itself: `resetDetector` must NOT write
		 * +0x914.  Both sides are seeded the same, so this is an
		 * absolute claim about the reference and a differential one
		 * about ours.
		 */
		setup(c, 0x6666u + (unsigned int)c);
		{
			int before = C(1)->word_914;

			our_cp_resetdet(C(0));
			ref_cp_resetdet(C(1));
			diff_eq_int("resetDetector left +0x914 alone (%ld)",
				    (long)C(1)->word_914, (long)before,
				    (long)c);
		}
	}

	return rc | diff_end();
}

static int
run_crc(void)
{
	int c;
	int rc;
	int nonzero = 0;
	int matched = 0;
	int mismatched = 0;

	diff_begin("calcCRC");

	for (c = 0; c < NCASE; c++) {
		unsigned char before[V92CP_CRC];

		setup(c, 0x7777u + (unsigned int)c);

		/* Start from the protocol's all-ones register. */
		our_cp_resetcrc(C(0));
		ref_cp_resetcrc(C(1));
		memcpy(before, C(1)->crc, V92CP_CRC);

		our_cp_calccrc(C(0));
		ref_cp_calccrc(C(1));

		compare("after calcCRC", (long)c);

		if (memcmp(C(1)->crc, before, V92CP_CRC) != 0)
			nonzero++;
	}

	/*
	 * ANTI-VACUITY.  A grid on which the register never moved would pass
	 * every comparison above while proving nothing about the shift.  The
	 * short cases are supposed to leave it alone -- that is what the
	 * `18 >= n` guard does -- so the count is over the grid and not per
	 * case.
	 */
	diff_eq_int("the register actually moved somewhere (%ld)",
		    (int)(nonzero >= NCASE / 2), 1, 0);

	rc = diff_end();

	diff_begin("calcCRC is not idempotent, and clocks per bit");

	for (c = 0; c < NCASE; c++) {
		unsigned char once[V92CP_CRC];

		if (cases[c].len < 40)
			continue;

		setup(c, 0x8888u + (unsigned int)c);
		our_cp_resetcrc(C(0));
		ref_cp_resetcrc(C(1));
		our_cp_calccrc(C(0));
		ref_cp_calccrc(C(1));
		memcpy(once, C(1)->crc, V92CP_CRC);

		/* A second pass over the same message from the new state. */
		our_cp_calccrc(C(0));
		ref_cp_calccrc(C(1));
		compare("after a second calcCRC", (long)c);

		/*
		 * Flip one message bit and the register must move.  This is
		 * the check that separates a shift register from any function
		 * that ignores its input.
		 */
		setup(c, 0x8888u + (unsigned int)c);
		C(0)->bits[cases[c].len / 2] ^= 1u;
		C(1)->bits[cases[c].len / 2] ^= 1u;
		our_cp_resetcrc(C(0));
		ref_cp_resetcrc(C(1));
		our_cp_calccrc(C(0));
		ref_cp_calccrc(C(1));
		compare("after a flipped bit", 100 + (long)c);
		diff_eq_int("a flipped bit moved the register (%ld)",
			    memcmp(C(1)->crc, once, V92CP_CRC) != 0, 1,
			    (long)c);
	}

	rc |= diff_end();

	diff_begin("evaluateCRC");

	for (c = 0; c < NCASE; c++) {
		int o;
		int r;
		unsigned int i;
		long tag = (long)c;

		/* As received: whatever the draw put in the last sixteen. */
		setup(c, 0x9999u + (unsigned int)c);
		o = our_cp_evalcrc(C(0));
		r = ref_cp_evalcrc(C(1));
		diff_eq_int("the verdict agrees (%ld)", (long)o, (long)r, tag);
		compare("after evaluateCRC", tag);
		if (r != 0)
			matched++;
		else
			mismatched++;

		/*
		 * And the arm that must succeed: run the register over the
		 * message, write what came out into the sixteen slots the
		 * check reads, and evaluate again.  Both sides are prepared
		 * from the REFERENCE's register, so a wrong `calcCRC` on our
		 * side shows up as a wrong verdict rather than as two
		 * self-consistent wrong answers.
		 */
		if (cases[c].len >= 40 && cases[c].len <= V92CP_BITS) {
			setup(c, 0x9999u + (unsigned int)c);
			ref_cp_resetcrc(C(1));
			ref_cp_calccrc(C(1));

			for (i = 0; i < V92CP_CRC; i++) {
				unsigned char b = C(1)->crc[i];

				C(0)->bits[cases[c].len - V92CP_CRC + i] = b;
				C(1)->bits[cases[c].len - V92CP_CRC + i] = b;
			}
			memcpy(cp[0], cp[1], sizeof(V92CP));
			C(0)->msgLen = cases[c].len;

			o = our_cp_evalcrc(C(0));
			r = ref_cp_evalcrc(C(1));

			diff_eq_int("a planted CRC verifies (%ld)", (long)r, 1,
				    tag);
			diff_eq_int("and both sides say so (%ld)", (long)o,
				    (long)r, tag);
			compare("after a verifying evaluateCRC", 100 + tag);
			matched++;
		}
	}

	diff_eq_int("the grid produced a match (%ld)", (int)(matched > 0), 1,
		    0);
	diff_eq_int("the grid produced a mismatch (%ld)",
		    (int)(mismatched > 0), 1, 0);

	return rc | diff_end();
}

/*
 * THE TWO CLAIMS THE RANDOM GRID CANNOT REACH, and they are both about
 * `evaluateCRC`'s accumulator rather than about the CRC.
 *
 * The object sums SIXTEEN ABSOLUTE DIFFERENCES INTO ONE BYTE and asks whether
 * the byte is zero.  On a 0/1 alphabet with a message that nearly verifies,
 * that is indistinguishable from three other things -- a signed sum, a wider
 * accumulator, a boolean OR -- and a random grid will never separate them.
 * Two crafted messages do:
 *
 *   - PLANT +1 AND -1.  One received bit above its computed value and one
 *     below, the other fourteen equal.  The absolute sum is 2 and says
 *     MISMATCH; a signed sum is 0 and says MATCH.
 *   - PLANT SIXTEEN DIFFERENCES OF SIXTEEN.  The absolute sum is 256, which is
 *     zero in a byte and says MATCH; in anything wider it is 256 and says
 *     MISMATCH.  This one needs `bits` to hold values outside 0/1, which the
 *     protocol never produces and the object never checks.
 *
 * Both are prepared from the REFERENCE's register, so a wrong `calcCRC` on our
 * side shows up as a wrong verdict rather than as two self-consistent wrong
 * answers.
 */
static int
run_crafted(void)
{
	static const char *what[2] = {
		"a +1 and a -1: the absolute sum is 2",
		"sixteen differences of sixteen: the absolute sum is 256"
	};
	int k;

	diff_begin("evaluateCRC over crafted differences");

	for (k = 0; k < 2; k++) {
		const unsigned int len = 400;
		unsigned int i;
		int o;
		int r;

		/* Case 9 is `a typical message, raw`, which is length 400. */
		setup(9, 0xaaaau + (unsigned int)k);

		ref_cp_resetcrc(C(1));
		ref_cp_calccrc(C(1));

		if (k == 0) {
			/*
			 * THE +1 MUST GO ON A ZERO STAGE AND THE -1 ON A ONE,
			 * or the byte underflows and the trial turns into the
			 * other one: planting -1 on a stage that holds zero
			 * gives 255, and 255 + 1 is 256, which is the very
			 * wrap the second case is for.  That is not a
			 * hypothetical -- it is what the first revision of
			 * this function did, and the reference answered MATCH
			 * where the comment said MISMATCH.
			 */
			int up = -1;
			int down = -1;

			for (i = 0; i < V92CP_CRC; i++) {
				if (up < 0 && C(1)->crc[i] == 0)
					up = (int)i;
				if (down < 0 && C(1)->crc[i] == 1)
					down = (int)i;
			}

			diff_eq_int("the register has a zero to raise (%ld)",
				    (int)(up >= 0), 1, (long)k);
			diff_eq_int("and a one to lower (%ld)",
				    (int)(down >= 0), 1, (long)k);
			if (up < 0 || down < 0)
				continue;

			for (i = 0; i < V92CP_CRC; i++)
				C(1)->bits[len - V92CP_CRC + i] =
					C(1)->crc[i];

			C(1)->bits[len - V92CP_CRC + up] = 1;
			C(1)->bits[len - V92CP_CRC + down] = 0;
		} else {
			for (i = 0; i < V92CP_CRC; i++)
				C(1)->bits[len - V92CP_CRC + i] =
					(unsigned char)(C(1)->crc[i] + 16);
		}

		memcpy(cp[0], cp[1], SLOT);

		o = our_cp_evalcrc(C(0));
		r = ref_cp_evalcrc(C(1));

		diff_eq_int("the verdict agrees, %s (%ld)", (long)o, (long)r,
			    (long)k);
		compare(what[k], (long)k);

		/*
		 * ANTI-VACUITY, and it is the point of the whole function: the
		 * reference must give the answer the BYTE arithmetic gives,
		 * which is MISMATCH for the first and MATCH for the second.
		 * If either flipped, the crafted message stopped crafting
		 * what it says it does and neither mutation would be caught.
		 */
		diff_eq_int("the reference says what the byte says (%ld)",
			    (long)r, k == 0 ? 0 : 1, (long)k);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the class's map");
	diff_eq_int("sizeof(V92CP) is %ld", (long)sizeof(V92CP), 0x918, 0x918);
	diff_eq_int("bits is at +0x%lx", (long)offsetof(V92CP, bits), 0x129,
		    0x129);
	diff_eq_int("crc is at +0x%lx", (long)offsetof(V92CP, crc), 0x8f9,
		    0x8f9);
	diff_eq_int("vectorLen is at +0x%lx", (long)offsetof(V92CP, vectorLen),
		    0x90c, 0x90c);
	diff_eq_int("msgLen is at +0x%lx", (long)offsetof(V92CP, msgLen),
		    0x910, 0x910);
	diff_eq_int("word_914 is at +0x%lx", (long)offsetof(V92CP, word_914),
		    0x914, 0x914);
	rc |= diff_end();

	rc |= run_small();
	rc |= run_crc();
	rc |= run_crafted();

	return rc;
}
