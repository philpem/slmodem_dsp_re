/*
 * t_v92txreset.cpp -- differential test of `V92Transmitter::reset`.
 *
 * SEPARATE FROM t_v92tx.cpp ON PURPOSE.  That fixture drives the constructor
 * and the destructor over sixty-four null combinations and never lets a
 * constructed object survive a call; this one needs the opposite -- a fully
 * built transmitter on each side, reset, and then compared including
 * everything reset pushed into the five sub-objects.  Keeping them apart also
 * keeps test/mutations/suites.json able to say which binary kills which
 * mutation.
 *
 * WHAT RESET ACTUALLY WRITES IS FOUR WORDS AND ONE BYTE, and every other
 * observable it has is a side effect of six calls into sub-objects that live
 * at two different heap addresses on the two sides.  So the comparison is in
 * three parts:
 *
 *   1. the transmitter block itself, with its six owned pointers poisoned --
 *      which is where +0x04 (K), +0x0c, +0x40 and +0x44 (gain) are checked;
 *   2. the one byte behind +0x58, which `*byte_58 = 0` clears;
 *   3. every sub-object's block, EXCEPT the slots that already differed
 *      before reset ran.
 *
 * PART 3'S MASK IS MEASURED, NOT ASSUMED.  Two objects constructed from the
 * same seed differ only where a heap address landed, so the fixture snapshots
 * both sides straight after construction, records which 4-byte slots differ,
 * and skips exactly those afterwards.  The mask's SIZE is asserted, so a
 * sub-object that starts differing somewhere new fails rather than being
 * quietly excused -- which is the failure mode a hand-written skip list has.
 *
 * THE PARAMETER BLOCK IS ONE BLOCK POINTED AT BY BOTH SIDES.  It is input;
 * two separately seeded copies would agree whatever was read out of them, and
 * the pointers reset installs into the precoder (`paramsAt9c`, the six
 * constellation heads) then compare equal for the right reason instead of
 * being masked out.
 *
 * AND IT IS DRIVEN AT LEVEL 2 AS WELL AS 0.  Roughly 2,000 of reset's 2,161
 * bytes are diagnostics, so a level-0 run leaves the four coefficient dumps,
 * the twelve modulus prints and the `%c%d.%07d` float formatting untested.
 * The transcripts are compared as strings, over lengths small enough that
 * neither side reaches the capture buffer's limit.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/debug.h"

extern "C" {
void our_tx_c1(void *) asm("_ZN14V92TransmitterC1Ev");
void our_tx_d1(void *) asm("_ZN14V92TransmitterD1Ev");
void our_tx_reset(void *, void *)
	asm("_ZN14V92Transmitter5resetEP16V92MappingParams");
void ref_tx_c1(void *) asm("ref__ZN14V92TransmitterC1Ev");
void ref_tx_d1(void *) asm("ref__ZN14V92TransmitterD1Ev");
void ref_tx_reset(void *, void *)
	asm("ref__ZN14V92Transmitter5resetEP16V92MappingParams");

extern unsigned int ref_dsplibs_debug_level;
}

#define GUARD		64
#define TXSLOT		(sizeof(V92Transmitter) + GUARD)
#define NCOEF		V92_PARAMSINFO_MAX_FILTER_LEN	/* 0x148 */
#define NCONST		V92_PARAMSINFO_MAX_LC		/* 0x80   */

/*
 * The five sub-objects, by the offset that holds them and the size their own
 * headers pin.  +0x58 is the one-byte buffer and is in the list because
 * `*byte_58 = 0` is a store reset makes and nothing else here would see.
 */
struct sub {
	unsigned off;
	unsigned size;
	const char *what;
};

static const struct sub subs[] = {
	{ 0x08, 0x50,   "the raw buffer at +0x08" },
	{ 0x48, 0x54,   "V92ModulusEncoder" },
	{ 0x4c, 0x80,   "V92Precoder" },
	{ 0x50, 0x14,   "V92PreFilter" },
	{ 0x54, 0x2008, "V92ConvolutionEncoder" },
	{ 0x58, 1,      "the one-byte buffer at +0x58" }
};
#define NSUB ((int)(sizeof(subs) / sizeof(subs[0])))

static unsigned char tx[2][TXSLOT] __attribute__((aligned(8)));
static unsigned char txseed[TXSLOT];

/* The shared parameter block and the ten arrays it owns. */
static struct V92ParamsInfo params;
static float coefbuf[4][NCOEF];
static int constbuf[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

/* The measured mask: one byte per 4-byte slot of each sub-object. */
#define MAXSUB 0x2008
static unsigned char masked[NSUB][MAXSUB / 4];
static int maskcount;

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill(void *dst, size_t n)
{
	unsigned char *q = (unsigned char *)dst;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextbyte();
}

static void *
subptr(int side, int k)
{
	void *p;

	memcpy(&p, tx[side] + subs[k].off, sizeof p);
	return p;
}

/*
 * The parameter block.  Lengths and sizes come from the case; everything else
 * is seed, so the twelve moduli, the trellis type and the gain are varied
 * bytes rather than zeroes.
 */
struct pcase {
	const char *name;
	unsigned int lz1, lp1, lz2, lp2;
	unsigned int lc;
};

static void
seed_params(unsigned int s, const struct pcase *c)
{
	int i;

	lfsr = 0x51e3u + 0x9e37u * s;
	fill(&params, sizeof(params));
	for (i = 0; i < 4; i++)
		fill(coefbuf[i], sizeof(coefbuf[i]));
	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++)
		fill(constbuf[i], sizeof(constbuf[i]));

	params.z1 = coefbuf[0];
	params.p1 = coefbuf[1];
	params.z2 = coefbuf[2];
	params.p2 = coefbuf[3];
	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++) {
		params.constellations[i] = constbuf[i];
		params.LC[i] = c->lc;
	}

	params.lz1 = c->lz1;
	params.lp1 = c->lp1;
	params.lz2 = c->lz2;
	params.lp2 = c->lp2;

	/*
	 * The gain and the twelve moduli stay seeded, but the gain must be a
	 * NUMBER: a random bit pattern is a NaN roughly one time in 256, and
	 * `sign_of`'s single ordered compare then prints '+' where an
	 * unordered one prints '-'.  That is a real difference between the two
	 * readings of the object (V92EchoCanceller.cpp says so at length) and
	 * it is not what this fixture is measuring, so the exponent is forced
	 * into a range where the value is finite and the sign still varies.
	 */
	{
		unsigned int bits;

		memcpy(&bits, &params.gain, sizeof bits);
		bits = (bits & 0x807fffffu) | 0x3e000000u;
		memcpy(&params.gain, &bits, sizeof bits);
	}
	for (i = 0; i < 4; i++) {
		unsigned int j;

		for (j = 0; j < NCOEF; j++) {
			unsigned int bits;

			memcpy(&bits, &coefbuf[i][j], sizeof bits);
			bits = (bits & 0x807fffffu) | 0x3e000000u;
			memcpy(&coefbuf[i][j], &bits, sizeof bits);
		}
		/*
		 * One EXACT zero per bank.  The object prints zero as '-' --
		 * `fldz; fcomps` selects on CF alone, so `0.0f < v` is false
		 * at zero -- and with every coefficient merely seeded that
		 * boundary is never reached and `<` versus `<=` in `sign_of`
		 * is untestable.
		 */
		coefbuf[i][0] = 0.0f;
	}
}


/*
 * THE CONSTRUCTOR ALREADY CLEARS TWO OF THE THREE THINGS RESET CLEARS.
 * `*byte_58 = 0` and `word_0c = 0` are both in the constructor, so a reset
 * that dropped either would leave the same bytes behind and every check would
 * still pass -- two mutations survived on exactly that before this existed.
 * Both sides are dirtied identically, after construction and before the mask
 * is measured, so the only thing that can put them back is reset.  (+0x40 is
 * not in this list because the constructor does not write it: it arrives
 * holding the seed and is dirty already.)
 */
static void
dirty_the_cleared(void)
{
	int side;

	for (side = 0; side < 2; side++) {
		V92Transmitter *t = (V92Transmitter *)tx[side];

		t->word_0c = 0xa5a5a5a5u;
		*t->byte_58 = 0x5a;
	}
}

static void
construct(unsigned int s)
{
	lfsr = 0x1d0fu + 0x4e6du * s;
	fill(txseed, TXSLOT);
	memcpy(tx[0], txseed, TXSLOT);
	memcpy(tx[1], txseed, TXSLOT);

	our_tx_c1(tx[0]);
	ref_tx_c1(tx[1]);
}

/*
 * Which 4-byte slots of each sub-object already differ.  Called straight
 * after construction and before reset, so what it finds is heap addresses and
 * nothing else.
 */
static void
measure_mask(void)
{
	int k;
	unsigned u;

	maskcount = 0;
	memset(masked, 0, sizeof(masked));

	for (k = 0; k < NSUB; k++) {
		const unsigned char *a = (const unsigned char *)subptr(0, k);
		const unsigned char *b = (const unsigned char *)subptr(1, k);

		for (u = 0; u + 4 <= subs[k].size; u += 4)
			if (memcmp(a + u, b + u, 4) != 0) {
				masked[k][u / 4] = 1;
				maskcount++;
			}
	}
}

static void
compare_subs(const char *what, long tag)
{
	int k;
	unsigned u;

	for (k = 0; k < NSUB; k++) {
		const unsigned char *a = (const unsigned char *)subptr(0, k);
		const unsigned char *b = (const unsigned char *)subptr(1, k);
		long bad = -1;

		for (u = 0; u < subs[k].size; u++) {
			if (u + 4 <= subs[k].size && masked[k][u / 4]) {
				u += 3;
				continue;
			}
			if (a[u] != b[u]) {
				bad = (long)u;
				break;
			}
		}
		diff_eq_int_(__FILE__, __LINE__,
			     "first differing byte (-1 is none) at %ld",
			     bad, -1, tag * 16 + k);
		(void)what;
	}
}

static void
compare_block(const char *what, long tag)
{
	unsigned char a[TXSLOT], b[TXSLOT];
	static const unsigned ptr_off[6] = {
		0x08, 0x48, 0x4c, 0x50, 0x54, 0x58
	};
	int i;

	memcpy(a, tx[0], TXSLOT);
	memcpy(b, tx[1], TXSLOT);
	for (i = 0; i < 6; i++) {
		memset(a + ptr_off[i], 0, 4);
		memset(b + ptr_off[i], 0, 4);
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V92Transmitter",
		     a, b, sizeof(V92Transmitter), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the guard past the object",
		     a + sizeof(V92Transmitter),
		     txseed + sizeof(V92Transmitter), GUARD, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the reference's guard",
		     b + sizeof(V92Transmitter),
		     txseed + sizeof(V92Transmitter), GUARD, tag);
}

static const struct pcase cases[] = {
	{ "short filters",           1,  2,  3,  4,  4 },
	{ "empty filters",           0,  0,  0,  0,  8 },
	{ "one empty filter",        5,  0,  5,  0,  0 },
	{ "at the filter ceiling", NCOEF, NCOEF, NCOEF, NCOEF, NCONST },
	{ "full constellations",     9,  9,  9,  9, NCONST },
	{ "empty constellations",    9,  9,  9,  9,  0 }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/* Small enough that neither transcript reaches the 16 KB capture. */
static const struct pcase tcases[] = {
	{ "transcript, short",       2,  3,  1,  2,  2 },
	{ "transcript, empty",       0,  0,  0,  0,  1 },
	{ "transcript, one empty",   2,  0,  0,  3,  1 }
};
#define NTCASE ((int)(sizeof(tcases) / sizeof(tcases[0])))

static void
run_one(const struct pcase *c, unsigned int s)
{
	seed_params(s, c);
	construct(s);
	dirty_the_cleared();
	measure_mask();

	our_tx_reset(tx[0], &params);
	ref_tx_reset(tx[1], &params);
}

static void
teardown(void)
{
	our_tx_d1(tx[0]);
	ref_tx_d1(tx[1]);
}

static int
run_level0(void)
{
	int c;

	diff_begin("V92Transmitter::reset, level 0");

	for (c = 0; c < NCASE; c++) {
		unsigned int s = (unsigned int)c + 1u;

		run_one(&cases[c], s);

		/*
		 * The mask is heap pointers only.  Six of them are the
		 * transmitter's own and are not in this count -- these are
		 * inside the sub-objects: the precoder's two FloatFIRs and
		 * the pre-filter's two.  If it ever moves, the run is
		 * excusing something it has not looked at.
		 */
		diff_eq_int("masked slots (%ld)", maskcount, 4, (long)c);

		compare_block(cases[c].name, (long)c);
		compare_subs(cases[c].name, (long)c);

		/* The four words reset writes, called out one by one. */
		diff_eq_int("K came from the block (%ld)",
			    (long)((V92Transmitter *)tx[1])->K,
			    (long)params.K, (long)c);
		diff_eq_int("gain came from the block (%ld)",
			    (long)(((V92Transmitter *)tx[1])->gain
				   == params.gain), 1, (long)c);
		diff_eq_int("+0x40 is zero (%ld)",
			    (long)((V92Transmitter *)tx[1])->word_40, 0,
			    (long)c);
		diff_eq_int("+0x0c is zero (%ld)",
			    (long)((V92Transmitter *)tx[1])->word_0c, 0,
			    (long)c);
		diff_eq_int("the one byte at +0x58 is zero (%ld)",
			    (long)*((V92Transmitter *)tx[1])->byte_58, 0,
			    (long)c);

		teardown();
	}

	return diff_end();
}

static int
run_transcripts(void)
{
	int c;

	diff_begin("V92Transmitter::reset, level 2 transcripts");

	for (c = 0; c < NTCASE; c++) {
		unsigned int s = 700u + (unsigned int)c;
		const char *o;
		const char *r;

		seed_params(s, &tcases[c]);
		construct(s);
		dirty_the_cleared();
		measure_mask();

		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		our_tx_reset(tx[0], &params);
		ref_tx_reset(tx[1], &params);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		o = dsplib_debug_capture_text(0);
		r = dsplib_debug_capture_text(1);

		diff_eq_int("transcript matches (%ld)",
			    strcmp(o, r) == 0, 1, (long)c);
		diff_eq_int("line counts match (%ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)c);
		/*
		 * Anti-vacuity.  The header, the two banners and the general
		 * block are 21 lines before any filter is dumped, and the
		 * sub-objects print of their own accord on top of that.
		 */
		diff_eq_int("the reference actually printed (%ld)",
			    (int)(dsplib_debug_capture_lines(1) >= 21), 1,
			    (long)c);
		diff_eq_int("transcript is not at the buffer limit (%ld)",
			    (int)(strlen(r) < 15000u), 1, (long)c);

		compare_block(tcases[c].name, 500 + (long)c);
		compare_subs(tcases[c].name, 500 + (long)c);

		teardown();
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("the transmitter's map");
	diff_eq_int("sizeof(V92Transmitter) is %ld",
		    (long)sizeof(V92Transmitter), 0x60, 0x60);
	diff_eq_int("K is at +0x%lx",
		    (long)offsetof(V92Transmitter, K), 0x04, 0x04);
	diff_eq_int("word_40 is at +0x%lx",
		    (long)offsetof(V92Transmitter, word_40), 0x40, 0x40);
	diff_eq_int("gain is at +0x%lx",
		    (long)offsetof(V92Transmitter, gain), 0x44, 0x44);
	rc |= diff_end();

	rc |= run_level0();
	rc |= run_transcripts();

	return rc;
}
