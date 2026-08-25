/*
 * t_v92ecparams.cpp -- differential test of the three V92EchoCanceller
 * members that set the adaption constants:
 *
 *     V92EchoCanceller::setEchoBeta(float)          .text+0x10cc0, 160 B
 *     V92EchoCanceller::setDecayFactor(float)       .text+0x10d60, 147 B
 *     V92EchoCanceller::setEchoParams(float, float, unsigned)
 *                                                   .text+0x10e00, 339 B
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS ITS OWN BINARY AND NOT THREE ROWS IN t_v90leaves.cpp
 *
 * That file already drives seven `V92EchoCanceller` members and has the
 * fixture for it, so three more rows is where these belong -- and they cannot
 * go there.  `t_v90leaves` is in `tools/gccdiverge.json` for
 * `V92EchoCanceller::process`, whose frozen-compare site GCC 13 provably
 * cannot reproduce (finding F2304), so under the modern compiler that binary
 * exits NON-ZERO on the unmutated source.  `tools/mutate.py` judges a mutant
 * caught by a non-zero exit, so it refuses a red baseline rather than score
 * every mutation as caught -- and it refuses the whole suite, not the row.
 *
 * That refusal is the tool working, and splitting the check out is what it
 * asks for.  `t_v90p4dnan` is the worked precedent: the divergent check was
 * moved into its own binary so the rest could be mutation-tested.  Here the
 * NEW members move instead, because moving `process` would mean moving the
 * state machine and both signal paths with it.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE GRID HAS TO SEPARATE, AND NONE OF IT IS DECORATION
 *
 *   1. THE SIGN COMES FROM THE FIELD AND THE MAGNITUDE FROM THE ARGUMENT.
 *      The object stores +0x30 and then RELOADS it -- `fsts 0x30(%ecx)` at
 *      +0x11 and `fcomps 0x30(%ecx)` at +0x71 -- for the sign, while the
 *      magnitude and the fraction come from the value still in the register.
 *      They hold the same float by then, so nothing separates the two
 *      spellings by VALUE; what separates them is WHICH FIELD is read.  So
 *      `echoBeta` and `echoBetaDecay` are seeded to DIFFERENT values and
 *      neither is ever seeded to the other's argument.  Two fields seeded
 *      equal is finding F4756's second fixture fault.
 *   2. BOTH SIGNS, AND ZERO.  `sign_of` is `0.0f < v`, so zero prints '-'.
 *      A grid of positive values alone cannot tell that from `0.0f <= v`,
 *      and `-0.0f` is in the table because it compares equal to `0.0f` and
 *      is a different store.
 *   3. A NON-ZERO FRACTION.  `frac_of` scales by 1e6 and takes the absolute
 *      value; a grid of whole numbers prints `.000000` every time and cannot
 *      see the scale at all.
 *   4. FINITE, AND NOT ENORMOUS.  `(int)v` outside `int`'s range is undefined
 *      in OUR source, and a trial there is the compiler adjudicating our
 *      undefined behaviour rather than the object adjudicating our reading --
 *      D561's argument.  The grid stays inside +/-3000.
 *
 * The two floats are compared as OBJECT BYTES, so a store that landed one bit
 * out fails here; and `only_wrote` asserts which four-byte words of the
 * BLOB's object moved, which is what says the header's offsets are the
 * blob's rather than merely self-consistent.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/debug.h"

extern "C" {
void ref_ec_setEchoBeta(void *self, float b)
	asm("ref__ZN16V92EchoCanceller11setEchoBetaEf");
void ref_ec_setDecayFactor(void *self, float d)
	asm("ref__ZN16V92EchoCanceller14setDecayFactorEf");
void ref_ec_setEchoParams(void *self, float b, float d, unsigned int n)
	asm("ref__ZN16V92EchoCanceller13setEchoParamsEffj");

extern unsigned int ref_dsplibs_debug_level;
}

/* Sixty bytes of object and a guard past it. */
#define EC_SLOT		96

/*
 * Written out rather than a union, for the reason t_v90leaves gives: the
 * class declares a destructor, and a union member with a non-trivial one
 * deletes the union's.  Nothing here constructs or destroys the slot; the raw
 * bytes are seeded and the members are called on them.
 */
struct ec_slot {
	unsigned char raw[EC_SLOT] __attribute__((aligned(8)));
	V92EchoCanceller &o;

	ec_slot() : o(*(V92EchoCanceller *)raw) { }
};

static struct ec_slot ec_a, ec_b;

static unsigned int lfsr;

static unsigned char
next_byte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill_pair(int trial)
{
	unsigned i;

	lfsr = 0x1234u + 0x9e37u * (unsigned)trial + 1u;
	for (i = 0; i < EC_SLOT; i++)
		ec_a.raw[i] = ec_b.raw[i] = next_byte();
}

/*
 * Which four-byte words of the BLOB's object moved, by absolute offset.
 * t_v90leaves.cpp's, and the same argument: a whole-object comparison proves
 * the two objects ended up identical and says nothing about WHERE the fields
 * are, because our seed went through the same header on both sides.
 */
static int
only_wrote(const unsigned char *before, const unsigned char *after,
	   const int *allow, int nallow, int *seen, int *first_bad)
{
	unsigned i;
	int bad = 0;

	*first_bad = -1;
	for (i = 0; i + 4 <= EC_SLOT; i += 4) {
		int k, ok = 0;

		if (memcmp(before + i, after + i, 4) == 0)
			continue;
		for (k = 0; k < nallow; k++)
			if (allow[k] == (int)i) {
				ok = 1;
				seen[k] = 1;
			}
		if (!ok) {
			if (*first_bad < 0)
				*first_bad = (int)i;
			bad++;
		}
	}
	return bad;
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * The float compared AS BITS.  `-0.0f == 0.0f` is true and they are different
 * stores, so a member that wrote the wrong one of the two would pass a
 * numeric check and fail this.
 */
static unsigned int
float_bits(float v)
{
	unsigned int u;

	memcpy(&u, &v, sizeof(u));
	return u;
}

static const float fv[] = {
	0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 0.125f, -0.125f,
	2.75f, -2.75f, 1234.5f, -1234.5f, 0.000001f, -0.000001f,
	2999.9375f, -2999.9375f
};
#define NF	((int)(sizeof(fv) / sizeof(fv[0])))

static const unsigned int dv[] = { 0u, 1u, 7u, 64u, 4095u, 0xffffffffu };
#define ND	((int)(sizeof(dv) / sizeof(dv[0])))

static int seen[4];
static int saw_plus, saw_minus, saw_frac, saw_differ, saw_printed;

static int
run_params(unsigned int lvl)
{
	char title[128];
	static const int allow[] = { 0x2c, 0x30, 0x34, 0x38 };
	int trial;

	strcpy(title, "V92EchoCanceller::setEchoBeta / setDecayFactor / "
		      "setEchoParams, level ");
	strcat(title, lvl == 0 ? "0" : (lvl == 1 ? "1" : "2"));
	diff_begin(title);

	set_level(lvl);

	for (trial = 0; trial < 3 * NF * ND; trial++) {
		unsigned char before[EC_SLOT];
		int which = trial % 3;
		int fi = (trial / 3) % NF;
		int di = (trial / (3 * NF)) % ND;
		/*
		 * A DIFFERENT entry of the same table, so `setEchoParams`
		 * never gets one value twice and neither float is ever seeded
		 * to the other's argument.
		 */
		int gi = (fi + 5) % NF;
		unsigned int old = dv[(di + 3) % ND];
		unsigned int len = 0x800u + 17u * (unsigned int)trial;
		long tag = (long)lvl * 100000 + trial;
		int bad, first;

		fill_pair(trial);

		ec_a.o.echoDelay = ec_b.o.echoDelay = old;
		ec_a.o.echoLength = ec_b.o.echoLength = len;
		ec_a.o.echoBeta = ec_b.o.echoBeta = fv[(fi + 2) % NF];
		ec_a.o.echoBetaDecay = ec_b.o.echoBetaDecay = fv[(fi + 9) % NF];

		memcpy(before, ec_b.raw, EC_SLOT);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();

		if (which == 0) {
			ec_a.o.setEchoBeta(fv[fi]);
			ref_ec_setEchoBeta(&ec_b.o, fv[fi]);
		} else if (which == 1) {
			ec_a.o.setDecayFactor(fv[fi]);
			ref_ec_setDecayFactor(&ec_b.o, fv[fi]);
		} else {
			ec_a.o.setEchoParams(fv[fi], fv[gi], dv[di]);
			ref_ec_setEchoParams(&ec_b.o, fv[fi], fv[gi], dv[di]);
		}

		dsplib_debug_capture_on = 0;

		diff_eq_obj("after the setter", V92EchoCanceller, &ec_a.o,
			    &ec_b.o, tag);
		diff_eq_int("no store past the object (%ld)",
			    memcmp(ec_a.raw + sizeof(ec_a.o),
				   ec_b.raw + sizeof(ec_b.o),
				   EC_SLOT - sizeof(ec_a.o)) == 0, 1, tag);

		bad = only_wrote(before, ec_b.raw, allow, 4, seen, &first);
		diff_eq_int("the blob wrote outside +0x2c/+0x30/+0x34/+0x38 "
			    "at +0x%lx", bad == 0 ? -1 : first, -1, tag);

		/* The arithmetic, spelled independently of the source. */
		if (which != 1)
			diff_eq_int("blob's echoBeta bits (%ld)",
				    (long)float_bits(ec_b.o.echoBeta),
				    (long)float_bits(fv[fi]), tag);
		if (which == 1)
			diff_eq_int("blob's echoBetaDecay bits (%ld)",
				    (long)float_bits(ec_b.o.echoBetaDecay),
				    (long)float_bits(fv[fi]), tag);
		if (which == 2) {
			diff_eq_int("blob's echoBetaDecay bits (%ld)",
				    (long)float_bits(ec_b.o.echoBetaDecay),
				    (long)float_bits(fv[gi]), tag);
			diff_eq_int("blob's echoDelay (%ld)",
				    (long)ec_b.o.echoDelay, (long)dv[di], tag);
			diff_eq_int("blob's echoLength (%ld)",
				    (long)ec_b.o.echoLength,
				    (long)(unsigned int)(len + dv[di] - old),
				    tag);
		}

		diff_eq_int("transcript matches (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("line counts match (%ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), tag);
		if (lvl > 1) {
			diff_eq_int("above the gate the blob printed (%ld)",
				    dsplib_debug_capture_lines(1) > 0, 1, tag);
			saw_printed = 1;
		} else {
			diff_eq_int("below the gate ours was silent (%ld)",
				    (int)dsplib_debug_capture_lines(0), 0,
				    tag);
		}

		if (fv[fi] > 0.0f)
			saw_plus = 1;
		if (fv[fi] <= 0.0f)
			saw_minus = 1;
		if (fv[fi] != (float)(int)fv[fi])
			saw_frac = 1;
		if (float_bits(ec_b.o.echoBeta)
		    != float_bits(ec_b.o.echoBetaDecay))
			saw_differ = 1;
	}

	set_level(0);
	return diff_end();
}

static int
run_antivacuity(void)
{
	diff_begin("the grid reached the paths it claims to");

	diff_eq_int("echoLength is written", seen[0], 1, 0);
	diff_eq_int("echoBeta is written", seen[1], 1, 0);
	diff_eq_int("echoBetaDecay is written", seen[2], 1, 0);
	diff_eq_int("echoDelay is written", seen[3], 1, 0);
	diff_eq_int("some trial printed a '+'", saw_plus, 1, 0);
	diff_eq_int("some trial printed a '-'", saw_minus, 1, 0);
	diff_eq_int("some trial had a non-zero fraction", saw_frac, 1, 0);
	diff_eq_int("the two floats differed at least once", saw_differ, 1, 0);
	diff_eq_int("the diagnostic was reached", saw_printed, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_params(0);
	rc |= run_params(1);
	rc |= run_params(2);
	rc |= run_antivacuity();

	dsplib_debug_capture_on = 0;
	set_level(0);
	return rc;
}
