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
void our_cp_pack(void *) asm("_ZN5V92CP10infoToBitsEv");
void our_cp_info(void *) asm("_ZN5V92CP12evaluateInfoEv");

unsigned char *ref_cp_getbv(void *, unsigned int &)
	asm("ref__ZN5V92CP12getBitVectorERj");
void ref_cp_setsuv(void *, unsigned int) asm("ref__ZN5V92CP6setSUVEj");
void ref_cp_resetcrc(void *) asm("ref__ZN5V92CP8resetCRCEv");
void ref_cp_resetdet(void *) asm("ref__ZN5V92CP13resetDetectorEv");
void ref_cp_reset(void *) asm("ref__ZN5V92CP5resetEv");
void ref_cp_calccrc(void *) asm("ref__ZN5V92CP7calcCRCEv");
int ref_cp_evalcrc(void *) asm("ref__ZN5V92CP11evaluateCRCEv");
void ref_cp_pack(void *) asm("ref__ZN5V92CP10infoToBitsEv");
void ref_cp_info(void *) asm("ref__ZN5V92CP12evaluateInfoEv");
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

/*
 * Independent standards tier.  V.92 (11/2000), Tables 23/24 specify CPt,
 * CPu and CPus.  Table 30 supplies additional CPd layouts for the CRC LEAF
 * ONLY: V92CP's packer and parser do not implement CPd.  V.34 (02/1998),
 * 10.1.2.3.2/Figure 14 supplies the CRC generator.
 *
 * Build payload WORDS from the tables, then serialize them with framing.
 * The CRC oracle consumes the words before framing exists, using a scalar
 * normal-polynomial register.  It does not inspect production bits, copy
 * the production skip loop, or obtain any answer from the other side.
 */
typedef void (*spec_void_fn)(void *);
typedef int (*spec_eval_fn)(void *);

#define SPEC_GUARD 64u
static unsigned char spec_store[SPEC_GUARD + sizeof(V92CP) + SPEC_GUARD]
	__attribute__((aligned(8)));
static unsigned char spec_bits[V92CP_BITS];
static unsigned short spec_words[112];
static unsigned int spec_nwords;

static V92CP *
spec_reset(void)
{
	V92CP *p = (V92CP *)(spec_store + SPEC_GUARD);
	memset(spec_store, 0x5a, sizeof(spec_store));
	memset((unsigned char *)p, 0, sizeof(*p));
	memset(p->bits, 0xa5, sizeof(p->bits));
	memset(spec_bits, 0xa5, sizeof(spec_bits));
	spec_nwords = 0;
	return p;
}

static int
spec_guards(void)
{
	unsigned int i;
	for (i = 0; i < SPEC_GUARD; i++)
		if (spec_store[i] != 0x5a ||
		    spec_store[SPEC_GUARD + sizeof(V92CP) + i] != 0x5a)
			return 0;
	return 1;
}

static unsigned int
spec_normal_crc(void)
{
	unsigned int r = 0xffffu, w, b;
	for (w = 0; w < spec_nwords; w++) {
		for (b = 0; b < 16u; b++) {
			unsigned int feedback = (r >> 15) ^
				((spec_words[w] >> b) & 1u);
			r = (r << 1) & 0xffffu;
			if (feedback)
				r ^= 0x1021u;
		}
	}
	return r;
}

static void
spec_word(unsigned int w)
{
	spec_words[spec_nwords++] = (unsigned short)w;
}

/* Return length INCLUDING CRC, EXCLUDING the mandatory zero fill bit. */
static unsigned int
spec_wire(unsigned int quantum)
{
	unsigned int pos = 17u, w, b, r = spec_normal_crc();
	unsigned int length, vector;
	memset(spec_bits, 0, sizeof(spec_bits));
	memset(spec_bits, 1, 17u);
	for (w = 0; w < spec_nwords; w++) {
		pos++;
		for (b = 0; b < 16u; b++)
			spec_bits[pos++] = (unsigned char)
				((spec_words[w] >> b) & 1u);
	}
	pos++;
	/* Figure 14's bit-zero output is the MSB of our normal register. */
	for (b = 0; b < 16u; b++)
		spec_bits[pos++] = (unsigned char)((r >> (15u - b)) & 1u);
	length = pos;
	vector = ((length + 1u + quantum - 1u) / quantum) * quantum;
	memset(spec_bits + vector, 0xa5, sizeof(spec_bits) - vector);
	return length;
}

static unsigned int
spec_mask(unsigned int block, unsigned int constellation, unsigned int chord)
{
	/* Sparse, asymmetric words expose order errors in every chord/block. */
	return (1u << ((constellation + chord + 1u) & 15u)) |
	       (1u << ((constellation + 2u * chord + 7u + block) & 15u));
}

static void
spec_table23(V92CP *p, unsigned int type, unsigned int m,
	     unsigned int paired, unsigned int rate)
{
	unsigned int i, j, block, first_indices = 0, last_indices = 0;
	p->char_01 = (signed char)type;
	p->char_02 = (signed char)rate;
	p->byte_03 = (unsigned char)(m & 1u);
	p->byte_04 = (unsigned char)paired;
	p->word_08 = m % 3u;
	p->word_0c = m % 3u;
	p->flt_10 = 1.0f;
	p->flt_14 = 0.5f;
	p->flt_18 = 0.25f;
	p->flt_1c = 0.125f;
	p->flt_20 = 0.0625f;
	p->byte_24 = (unsigned char)paired;
	p->word_10c = (unsigned short)(m + 1u);
	for (i = 0; i < 6u; i++) {
		unsigned int index = i == 5u ? m : i % (m + 1u);
		p->word_28[i] = index;
		if (i < 4u)
			first_indices |= index << (4u * i);
		else
			last_indices |= index << (4u * (i - 4u));
	}
	/* Table 23's seven fixed information words; reserved bits stay zero. */
	spec_word((type << 1) | (rate << 3) | ((m % 3u) << 13) |
		  (paired << 15));
	spec_word((m & 1u) | ((m % 3u) << 14));
	spec_word(8192u); /* 1.0 in unsigned Q3.13 */
	spec_word(32u | (16u << 8)); /* +1/2, +1/4 in signed Q1.6 */
	spec_word(8u | (4u << 8)); /* +1/8, +1/16 */
	spec_word(first_indices);
	spec_word(last_indices | (paired << 8));
	for (block = 0; block <= paired; block++)
		for (i = 0; i <= m; i++)
			for (j = 0; j < 8u; j++) {
				unsigned int mask = spec_mask(block, i, j);
				if (block == 0)
					p->short_42[i][j] = (short)mask;
				else
					p->short_a2[i][j] = (short)mask;
				spec_word(mask);
			}
}

static void
spec_check_message(V92CP *p, spec_void_fn calc, spec_eval_fn eval,
		   unsigned int length, unsigned int vector, long tag, int packed)
{
	unsigned int i, crc_at = length - 16u, r = spec_normal_crc();
	int good = 1;
	if (packed) {
		diff_eq_int("standard msgLen (%ld)", p->msgLen, length, tag);
		diff_eq_int("standard vectorLen (%ld)", p->vectorLen, vector, tag);
		diff_eq_int("standard fields, framing, CRC order and fill (%ld)",
			memcmp(p->bits, spec_bits, vector) == 0, 1, tag);
		for (i = vector; i < V92CP_BITS; i++)
			if (p->bits[i] != 0xa5)
				good = 0;
		diff_eq_int("untouched bit-vector tail (%ld)", good, 1, tag);
	}
	/* Test calcCRC independently of resetCRC and independently of packing. */
	memcpy(p->bits, spec_bits, sizeof(spec_bits));
	p->msgLen = length;
	memset(p->crc, 1, sizeof(p->crc));
	calc(p);
	good = 1;
	for (i = 0; i < 16u; i++)
		if (p->crc[i] != ((r >> (15u - i)) & 1u))
			good = 0;
	diff_eq_int("independent V.34 CRC register (%ld)", good, 1, tag);
	diff_eq_int("CRC leaves message and unused tail intact (%ld)",
		memcmp(p->bits, spec_bits, sizeof(spec_bits)) == 0, 1, tag);
	diff_eq_int("independently supplied CRC accepted (%ld)", eval(p), 1, tag);
	for (i = 0; i < 16u; i++) {
		p->bits[crc_at + i] ^= 1u;
		diff_eq_int("each corrupted CRC bit rejected (%ld)", eval(p), 0,
			tag * 16 + i);
		p->bits[crc_at + i] ^= 1u;
	}
	/* First/last payload positions belong to the CRC, unlike all markers. */
	p->bits[18] ^= 1u;
	diff_eq_int("first information bit protected (%ld)", eval(p), 0, tag);
	p->bits[18] ^= 1u;
	p->bits[crc_at - 2u] ^= 1u;
	diff_eq_int("last information bit protected (%ld)", eval(p), 0, tag);
	p->bits[crc_at - 2u] ^= 1u;
	for (i = 0; i < 17u; i++)
		p->bits[i] ^= 1u;
	for (i = 17u; i < crc_at; i += 17u)
		p->bits[i] ^= 1u;
	for (i = length; i < vector; i++)
		p->bits[i] ^= 1u;
	diff_eq_int("CRC excludes sync, starts and fill (%ld)", eval(p), 1, tag);
	diff_eq_int("object guards intact (%ld)", spec_guards(), 1, tag);
}

static int
run_standard_pack(const char *name, spec_void_fn pack, spec_void_fn calc,
		  spec_eval_fn eval)
{
	unsigned int type, m, paired, rate;
	long tag = 0;
	diff_begin(name);
	for (type = 0; type < 2u; type++)
		for (m = 0; m < 6u; m++)
			for (paired = 0; paired < 2u; paired++) {
				V92CP *p = spec_reset();
				unsigned int length, quantum, vector;
				p->bitsPerSymbol = (unsigned char)(2u + paired);
				spec_table23(p, type, m, paired,
					     (m * 4u + paired) % 23u);
				quantum = 12u * (type == 0 ? 1u : 2u + paired);
				length = spec_wire(quantum);
				vector = ((length + quantum) / quantum) * quantum;
				pack(p);
				spec_check_message(p, calc, eval, length, vector, tag++, 1);
			}
	for (rate = 0; rate <= 22u; rate++) {
		V92CP *p = spec_reset();
		unsigned int length, quantum = 12u * (2u + (rate & 1u));
		unsigned int vector;
		p->char_01 = 2;
		p->char_02 = (signed char)rate;
		p->byte_04 = (unsigned char)(rate & 1u);
		p->bitsPerSymbol = (unsigned char)(quantum / 12u);
		/* Nonzero irrelevant fields make accidentally taking the long arm visible. */
		p->word_08 = 3;
		p->word_10c = 6;
		spec_word(4u | (rate << 3) | ((rate & 1u) << 15));
		length = spec_wire(quantum);
		vector = ((length + quantum) / quantum) * quantum;
		pack(p);
		spec_check_message(p, calc, eval, length, vector, tag++, 1);
	}
	return diff_end();
}

/* Table 30 shapes, not CPd behavioral integration: all eight optional-part
 * combinations, with two filter/constellation lengths.  CRC sees the packed
 * words only; this cannot validate the actual CPd parser or its parameters. */
static int
run_standard_cpd(const char *name, spec_void_fn calc, spec_eval_fn eval)
{
	unsigned int flags, variant, i;
	diff_begin(name);
	for (flags = 0; flags < 8u; flags++)
		for (variant = 0; variant < 2u; variant++) {
			V92CP *p = spec_reset();
			unsigned int n = variant + 1u, length, vector;
			spec_word((flags << 1) | (19u << 4) | (variant << 15));
			spec_word(16384u); /* Table 30's 4G field, positive Q0.16 */
			if (flags & 1u)
				for (i = 0; i < 6u; i++)
					spec_word(0x0202u);
			if (flags & 2u) {
				spec_word(n); spec_word(n); spec_word(1u); spec_word(0u);
				for (i = 0; i < 2u * n; i++)
					spec_word(0u);
				spec_word(16384u); /* prefilter z2(0)=1/2 */
			}
			if (flags & 4u) {
				spec_word(0u); spec_word(0u); /* all indices select set zero */
				spec_word(n); spec_word(0u); spec_word(0u); /* LC1=n */
				for (i = 0; i < n; i++)
					spec_word((i + 1u) * 128u); /* positive, increasing points */
			}
			length = spec_wire(6u);
			vector = ((length + 6u) / 6u) * 6u;
			p->msgLen = length;
			p->vectorLen = vector;
			memcpy(p->bits, spec_bits, sizeof(spec_bits));
			spec_check_message(p, calc, eval, length, vector,
					   flags * 2u + variant, 0);
		}
	return diff_end();
}

static int
run_standard_departures(const char *name, spec_void_fn pack, spec_void_fn info)
{
	V92CP *p = spec_reset();
	unsigned int i, actual = 0;
	diff_begin(name);
	p->bitsPerSymbol = 1;
	spec_table23(p, 0, 0, 0, 1);
	p->flt_10 = 1.0f / 256.0f;
	pack(p);
	for (i = 0; i < 16u; i++)
		actual |= (unsigned int)p->bits[52u + i] << i;
	/* 2^13/256 = 32.  Passing these checks records the shared departure. */
	diff_eq_int("Q3.13 mathematical value differs from wire (%ld)",
		actual != 8192u / 256u, 1, 0);
	diff_eq_int("duplicated 2^-9 weight emits code 48 (%ld)", actual, 48, 0);
	diff_eq_int("object guards intact (%ld)", spec_guards(), 1, 0);
	p = spec_reset();
	memset(p->bits, 0, sizeof(p->bits));
	p->bits[57] = 1; /* independent Table 23 Q3.13 code 32 */
	p->rxState = 6;
	info(p);
	diff_eq_int("Q3.13 received code 32 departs from 1/256 (%ld)",
		p->flt_10 != 1.0f / 256.0f, 1, 0);
	diff_eq_float("Q3.13 received code 32 becomes 1/512 (%ld)",
		p->flt_10, 1.0f / 512.0f, 0);
	diff_eq_int("Q3.13 decode guards intact (%ld)", spec_guards(), 1, 0);
	p = spec_reset();
	memset(p->bits, 0, sizeof(p->bits));
	p->bits[137] = 1; /* Table 23 explicitly assigns this bit to Ucode 0 */
	p->rxState = 7;
	p->word_10c = 1;
	p->word_124 = 136;
	info(p);
	diff_eq_int("D920 Ucode 0 departs from mask bit zero (%ld)",
		(unsigned short)p->short_42[0][0] != 1u, 1, 0);
	diff_eq_int("D920 Ucode 0 becomes mask bit fifteen (%ld)",
		(unsigned short)p->short_42[0][0], 0x8000u, 0);
	diff_eq_int("mask decode guards intact (%ld)", spec_guards(), 1, 0);
	/* V.92 3.5 defines signed Qa.b as TWO'S COMPLEMENT.  Table 23's
	 * a1, a2, b1 and b2 all use Q1.6.  -1/4 therefore has integer code
	 * -16, represented by 0xf0; each implementation instead uses a sign
	 * bit and magnitude.  Positive values in the general conformance
	 * vectors above avoid declaring that shared mistake to be correct. */
	{
		static const unsigned int at[4] = { 69u, 77u, 86u, 94u };
		unsigned int field, bit;
		float decoded[4];
		p = spec_reset();
		p->bitsPerSymbol = 1;
		spec_table23(p, 0, 0, 0, 1);
		p->flt_14 = p->flt_18 = p->flt_1c = p->flt_20 = -0.25f;
		pack(p);
		for (field = 0; field < 4u; field++) {
			actual = 0;
			for (bit = 0; bit < 8u; bit++)
				actual |= (unsigned int)p->bits[at[field] + bit] << bit;
			diff_eq_int("negative Q1.6 field departs from two's complement (%ld)",
				actual != ((256u - 16u) & 255u), 1, field);
			diff_eq_int("negative Q1.6 field emits sign-magnitude 0x90 (%ld)",
				actual, 0x90u, field);
		}
		diff_eq_int("signed Q1.6 encode guards intact (%ld)", spec_guards(), 1, 0);
		p = spec_reset();
		memset(p->bits, 0, sizeof(p->bits));
		for (field = 0; field < 4u; field++)
			for (bit = 0; bit < 8u; bit++)
				p->bits[at[field] + bit] = (unsigned char)
					((0xf0u >> bit) & 1u);
		p->rxState = 6;
		info(p);
		decoded[0] = p->flt_14;
		decoded[1] = p->flt_18;
		decoded[2] = p->flt_1c;
		decoded[3] = p->flt_20;
		for (field = 0; field < 4u; field++) {
			diff_eq_int("received Q1.6 0xf0 departs from -1/4 (%ld)",
				decoded[field] != -0.25f, 1, field);
			/* Sign-magnitude reads 0x70/64 = 7/4, then negates it. */
			diff_eq_float("received Q1.6 0xf0 becomes -7/4 (%ld)",
				decoded[field], -7.0f / 4.0f, field);
		}
		diff_eq_int("signed Q1.6 decode guards intact (%ld)", spec_guards(), 1, 0);
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
	rc |= run_standard_pack("V.92 Tables 23/24 reconstruction standards oracle",
		our_cp_pack, our_cp_calccrc, our_cp_evalcrc);
	rc |= run_standard_pack("V.92 Tables 23/24 blob standards oracle",
		ref_cp_pack, ref_cp_calccrc, ref_cp_evalcrc);
	rc |= run_standard_cpd("V.92 Table 30 reconstruction CRC leaf only",
		our_cp_calccrc, our_cp_evalcrc);
	rc |= run_standard_cpd("V.92 Table 30 blob CRC leaf only",
		ref_cp_calccrc, ref_cp_evalcrc);
	rc |= run_standard_departures("V.92 reconstruction expected departures",
		our_cp_pack, our_cp_info);
	rc |= run_standard_departures("V.92 blob expected departures",
		ref_cp_pack, ref_cp_info);

	return rc;
}
