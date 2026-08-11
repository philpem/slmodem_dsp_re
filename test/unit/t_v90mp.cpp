/*
 * t_v90mp.cpp -- differential test of V90MP's lifecycle and of the three
 * members reconstructed alongside it: `reset`, `getBitVector` and
 * `printNofRecievedMpMpNot`.
 *
 * THIS BINARY IS THE `v90mp` MUTATION SUITE'S TARGET, which is why it repeats
 * the constructor and destructor coverage `t_v90cp.cpp` already has.
 * `test/mutations/suites.json` names ONE binary per set, and the twelve
 * mutations recorded against `V90MP.cpp` are all against the lifecycle; a set
 * repointed at a binary that does not drive them would report NOT CAUGHT for
 * all twelve, which is the same output an untested claim gives.  So the
 * lifecycle is driven here too, and the duplication is deliberate.
 *
 * WHAT EACH BLOCK IS FOR
 *
 *   run_mp_lifecycle   the constructor and destructor, all four symbol
 *                      variants, against seeded storage no constructor has
 *                      run over.  The destructor is one byte and the claim is
 *                      that it writes NOTHING -- checked against the object's
 *                      own pre-call image, because comparing two objects
 *                      neither side wrote is the archetypal vacuous pass.
 *                      `-fno-lifetime-dse` is in CXXFLAGS, so a store to
 *                      *this in a destructor really does survive and really
 *                      is caught here (finding 1224).
 *
 *   run_mp_reset       the same six stores as the constructor, at 0x1f3e0
 *                      rather than 0x1f410.  Both are driven, so the second
 *                      forty bytes are measured rather than assumed to be a
 *                      copy of the first.
 *
 *   run_mp_getbitvec   the length is ONE BYTE widened WITHOUT SIGN.  That is
 *                      the whole content of the function beyond the +0x1c,
 *                      and it is invisible below 0x80 -- so +0x118 is swept
 *                      over the boundary and the run asserts that a value
 *                      with the top bit set was actually tried.
 *
 *   run_mp_printnof    the only diagnostic in the class that is NOT an
 *                      `edprintf`: `cmpl $0x1,dsplibs_debug_level; ja` and a
 *                      direct `dsplibs_debug_printf`.  Both arms of the gate
 *                      are driven and the run asserts both were seen.
 *
 * THE RULES, applied everywhere below: both sides are seeded with the SAME
 * varied pseudorandom bytes and are NEVER zeroed; the slot is 64 bytes longer
 * than the object and the tail is compared against the seed on both sides; and
 * every block asserts that the call changed something and that it did not
 * change it to the same thing on every trial (findings 223, 224, 230).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90MP.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void our_mp_c1(void *) asm("_ZN5V90MPC1Ev");
void our_mp_c2(void *) asm("_ZN5V90MPC2Ev");
void our_mp_d1(void *) asm("_ZN5V90MPD1Ev");
void our_mp_d2(void *) asm("_ZN5V90MPD2Ev");
void ref_mp_c1(void *) asm("ref__ZN5V90MPC1Ev");
void ref_mp_c2(void *) asm("ref__ZN5V90MPC2Ev");
void ref_mp_d1(void *) asm("ref__ZN5V90MPD1Ev");
void ref_mp_d2(void *) asm("ref__ZN5V90MPD2Ev");

void ref_mp_reset(void *) asm("ref__ZN5V90MP5resetEv");

/*
 * `unsigned int *` where the member takes `unsigned int &`.  The two are the
 * same thing to the ABI -- a reference is passed as the address -- and this
 * side of the declaration has to name a C type, because the blob's symbol has
 * no class to be a member of.
 */
unsigned char *ref_mp_getbitvector(void *, unsigned int *)
	asm("ref__ZN5V90MP12getBitVectorERj");

void ref_mp_printnof(void *) asm("ref__ZN5V90MP23printNofRecievedMpMpNotEv");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode, unsigned i)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0x12;			/* what the ctor puts at +0x1b */
	case 2:
		return 0xff;			/* every bit set               */
	case 3:
		return (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1u));
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

/* The same varied bytes into both sides.  Never zeros -- finding 230. */
static void
fill_pair(void *a, void *b, unsigned n, int trial, int mode)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned i;

	lfsr = 0x1234u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte(mode, i);
}

/* Both sides' levels move together, or they take different branches. */
static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ----------------------------------------------------------- the storage */

#define MP_SLOT	((unsigned)sizeof(V90MP) + 64u)

static unsigned char mp_a[MP_SLOT] __attribute__((aligned(8)));
static unsigned char mp_b[MP_SLOT] __attribute__((aligned(8)));
static unsigned char mp_s[MP_SLOT];		/* the seed, for the guard  */

#define MPA	((V90MP *)mp_a)
#define MPB	((V90MP *)mp_b)

static void
seed_pair(int trial, int mode)
{
	fill_pair(mp_a, mp_b, MP_SLOT, trial, mode);
	memcpy(mp_s, mp_b, MP_SLOT);
}

/* The guard past the end of the object, on BOTH sides, against the seed. */
static void
guard_intact(long tag)
{
	unsigned n = MP_SLOT - (unsigned)sizeof(V90MP);

	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(mp_a + sizeof(V90MP), mp_s + sizeof(V90MP), n) == 0,
		    1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(mp_b + sizeof(V90MP), mp_s + sizeof(V90MP), n) == 0,
		    1, tag);
}

/*
 * What the six stores must leave behind.  Read off the BLOB's object, so it
 * is a claim about the object and not a restatement of our own source.
 */
static void
check_constructed(V90MP *o, long tag)
{
	diff_eq_int("word_14 (%ld)", (long)o->word_14, 0, tag);
	diff_eq_int("byte_19 (%ld)", (long)o->byte_19, 0, tag);
	diff_eq_int("byte_1a (%ld)", (long)o->byte_1a, 0, tag);
	diff_eq_int("byte_1b (%ld)", (long)o->byte_1b, 18, tag);
	diff_eq_int("nofRecievedMp (%ld)", (long)o->nofRecievedMp, 0, tag);
	diff_eq_int("nofRecievedMpNot (%ld)", (long)o->nofRecievedMpNot, 0,
		    tag);

	/*
	 * +0x1b IS ONE BYTE.  The three bytes above it are not part of it, so
	 * the seed must still be there -- this is what makes "the CP classes'
	 * four-byte field" a catchable mutation rather than an invisible one.
	 */
	diff_eq_int("+0x1c survived the byte at +0x1b (%ld)",
		    (long)o->bits[0], (long)mp_s[0x1c], tag);
}

/* --------------------------------------- the constructor and destructor */

typedef void (*member)(void *);

static int
run_mp_lifecycle(void)
{
	static const member ours[2] = { our_mp_c1, our_mp_c2 };
	static const member theirs[2] = { ref_mp_c1, ref_mp_c2 };
	static const member ourd[2] = { our_mp_d1, our_mp_d2 };
	static const member theird[2] = { ref_mp_d1, ref_mp_d2 };
	unsigned char first[MP_SLOT];
	int trial, varied = 0;

	diff_begin("V90MP: constructor and destructor");
	set_level(0);

	for (trial = 0; trial < 32; trial++) {
		unsigned char before[MP_SLOT];
		long tag = trial;
		int v = trial & 1;

		seed_pair(trial + 100, trial & 3);
		memcpy(before, mp_b, MP_SLOT);

		ours[v](mp_a);
		theirs[v](mp_b);

		diff_eq_obj("after construction", V90MP, MPA, MPB, tag);
		guard_intact(tag);
		check_constructed(MPB, tag);
		diff_eq_int("the constructor changed the object (%ld)",
			    memcmp(before, mp_b, sizeof(V90MP)) != 0, 1, tag);

		/*
		 * NOT THE SAME OBJECT EVERY TRIAL.  Six of 292 bytes are
		 * written and the other 286 are the seed, so two trials that
		 * came out equal would mean the seed had stopped varying --
		 * finding 224's check, and the only thing that makes the
		 * comparisons above worth anything.
		 */
		if (trial == 0)
			memcpy(first, mp_b, MP_SLOT);
		else if (memcmp(first, mp_b, MP_SLOT) != 0)
			varied = 1;

		/* The destructor: one byte, and it must write nothing. */
		memcpy(before, mp_b, MP_SLOT);
		ourd[v](mp_a);
		theird[v](mp_b);
		diff_eq_int("the destructor wrote nothing (%ld)",
			    memcmp(before, mp_b, MP_SLOT) == 0, 1, tag);
		diff_eq_obj("after destruction", V90MP, MPA, MPB, tag);
		guard_intact(tag);

		/*
		 * AND AGAIN OVER STORAGE NO CONSTRUCTOR HAS TOUCHED.  Run on
		 * a constructed object the check above cannot see a
		 * destructor that clears a counter: the constructor has
		 * already left both counters at zero, so `nofRecievedMp = 0`
		 * in the destructor writes the value that is there.  Over a
		 * seeded object it is a four-byte difference.  This is why
		 * `-fno-lifetime-dse` is in CXXFLAGS -- without it the store
		 * would be deleted and the mutation would be uncatchable for
		 * a reason that has nothing to do with the test (finding
		 * 1224).  BOTH SIDES are compared against the seed, since the
		 * mutation lands on OURS.
		 */
		{
			unsigned char seeded[MP_SLOT];

			seed_pair(trial + 700, (trial + 1) & 3);
			memcpy(seeded, mp_b, MP_SLOT);
			ourd[v](mp_a);
			theird[v](mp_b);
			diff_eq_int("over seeded storage ours wrote nothing "
				    "(%ld)",
				    memcmp(seeded, mp_a, MP_SLOT) == 0, 1,
				    tag);
			diff_eq_int("over seeded storage the blob wrote "
				    "nothing (%ld)",
				    memcmp(seeded, mp_b, MP_SLOT) == 0, 1,
				    tag);
			diff_eq_obj("after destroying seeded storage", V90MP,
				    MPA, MPB, tag);
		}
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------ reset (40 bytes) */

static int
run_mp_reset(void)
{
	unsigned char first[MP_SLOT];
	int trial, varied = 0;

	diff_begin("V90MP::reset");
	set_level(0);

	for (trial = 0; trial < 32; trial++) {
		unsigned char before[MP_SLOT];
		long tag = 1000 + trial;

		seed_pair(trial + 200, trial & 3);
		memcpy(before, mp_b, MP_SLOT);

		MPA->reset();
		ref_mp_reset(mp_b);

		diff_eq_obj("after reset", V90MP, MPA, MPB, tag);
		guard_intact(tag);
		check_constructed(MPB, tag);
		diff_eq_int("reset changed the object (%ld)",
			    memcmp(before, mp_b, sizeof(V90MP)) != 0, 1, tag);

		if (trial == 0)
			memcpy(first, mp_b, MP_SLOT);
		else if (memcmp(first, mp_b, MP_SLOT) != 0)
			varied = 1;

		/*
		 * AND IT IS THE CONSTRUCTOR'S OWN SIX STORES.  Keep the image
		 * `reset` produced, put the SAME seed back and run the blob's
		 * constructor over it, then require the two to agree byte for
		 * byte: that is finding 1237's claim stated as a test rather
		 * than as a comment.
		 */
		{
			unsigned char after_reset[MP_SLOT];

			memcpy(after_reset, mp_b, MP_SLOT);
			seed_pair(trial + 200, trial & 3);
			ref_mp_c1(mp_b);
			diff_eq_int("reset and the constructor agree (%ld)",
				    memcmp(after_reset, mp_b, MP_SLOT) == 0,
				    1, tag);
		}
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	return diff_end();
}

/* ----------------------------------------------- getBitVector (21 bytes) */

static int
run_mp_getbitvector(void)
{
	/*
	 * The lengths swept.  0x80 AND ABOVE ARE THE POINT: `movzbl` and
	 * `movsbl` agree on every value below it and differ on every value
	 * above, so a sweep that stopped at 0x7f would pass with the field
	 * declared `char` and the reconstruction silently wrong.
	 */
	static const unsigned char lens[] = {
		0x00, 0x01, 0x02, 0x40, 0x7e, 0x7f, 0x80, 0x81, 0xc0, 0xfe,
		0xff
	};
	unsigned i, high_seen = 0;
	int trial, varied = 0;
	unsigned first_len = 0;

	diff_begin("V90MP::getBitVector");
	set_level(0);

	for (trial = 0; trial < 22; trial++) {
		unsigned char before_a[MP_SLOT], before_b[MP_SLOT];
		long tag = 2000 + trial;
		unsigned char *ra, *rb;
		unsigned int la, lb;

		i = (unsigned)trial % (sizeof(lens) / sizeof(lens[0]));
		seed_pair(trial + 300, trial & 3);
		MPA->byte_118 = lens[i];
		MPB->byte_118 = lens[i];
		if (lens[i] >= 0x80)
			high_seen = 1;
		memcpy(before_a, mp_a, MP_SLOT);
		memcpy(before_b, mp_b, MP_SLOT);

		/*
		 * THE OUT PARAMETERS ARE SEEDED AND NOT ZEROED, and with
		 * DIFFERENT values, so "the call wrote it" is answerable and
		 * a call that wrote nothing cannot pass by leaving two zeros
		 * behind.
		 */
		la = 0xa5a5a5a5u;
		lb = 0x5a5a5a5au;

		ra = MPA->getBitVector(la);
		rb = ref_mp_getbitvector(mp_b, &lb);

		diff_eq_int("the length matches (%ld)", (long)la, (long)lb,
			    tag);
		diff_eq_int("the length is +0x118 zero-extended (%ld)",
			    (long)lb, (long)lens[i], tag);
		diff_eq_int("ours returned this+0x%lx",
			    (long)(ra - mp_a), 0x1c, tag);
		diff_eq_int("the blob returned this+0x%lx",
			    (long)(rb - mp_b), 0x1c, tag);
		diff_eq_int("and that is &bits[0] (%ld)",
			    (long)((unsigned char *)MPB->bits - mp_b), 0x1c,
			    tag);

		/* Twenty-one bytes and no store: the object cannot move. */
		diff_eq_int("ours wrote nothing (%ld)",
			    memcmp(before_a, mp_a, MP_SLOT) == 0, 1, tag);
		diff_eq_int("the blob wrote nothing (%ld)",
			    memcmp(before_b, mp_b, MP_SLOT) == 0, 1, tag);

		if (trial == 0)
			first_len = lb;
		else if (lb != first_len)
			varied = 1;
	}

	diff_eq_int("a length with the top bit set was tried",
		    (int)high_seen, 1, 0);
	diff_eq_int("the length varied between trials", varied, 1, 0);
	return diff_end();
}

/* ------------------------------- printNofRecievedMpMpNot (56 bytes) */

static int
run_mp_printnof(void)
{
	/*
	 * The counters are `int`, and the diagnostic prints them with %d, so
	 * a negative one is a different string from its unsigned reading.
	 */
	static const int counts[] = {
		0, 1, -1, 255, -255, 0x7fffffff, (-0x7fffffff - 1), 12345
	};
	int trial, above = 0, below = 0;
	unsigned lvl;

	diff_begin("V90MP::printNofRecievedMpMpNot");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 16; trial++) {
			unsigned char before_a[MP_SLOT], before_b[MP_SLOT];
			long tag = (long)lvl * 1000 + trial;
			unsigned n = sizeof(counts) / sizeof(counts[0]);

			seed_pair(trial + 400, trial & 3);
			MPA->nofRecievedMp = counts[(unsigned)trial % n];
			MPB->nofRecievedMp = counts[(unsigned)trial % n];
			MPA->nofRecievedMpNot =
			    counts[((unsigned)trial + 3) % n];
			MPB->nofRecievedMpNot =
			    counts[((unsigned)trial + 3) % n];
			memcpy(before_a, mp_a, MP_SLOT);
			memcpy(before_b, mp_b, MP_SLOT);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			MPA->printNofRecievedMpMpNot();
			ref_mp_printnof(mp_b);

			dsplib_debug_capture_on = 0;

			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);

			if (lvl > 1) {
				diff_eq_int("above the gate the blob printed "
					    "one line (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    1, tag);
				above = 1;
			} else {
				diff_eq_int("below the gate the blob was "
					    "silent (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				diff_eq_int("below the gate ours was silent "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(0),
					    0, tag);
				below = 1;
			}

			/* It prints; it does not count. */
			diff_eq_int("ours wrote nothing (%ld)",
				    memcmp(before_a, mp_a, MP_SLOT) == 0, 1,
				    tag);
			diff_eq_int("the blob wrote nothing (%ld)",
				    memcmp(before_b, mp_b, MP_SLOT) == 0, 1,
				    tag);
		}
	}

	set_level(0);
	diff_eq_int("the gate was open on some trial", above, 1, 0);
	diff_eq_int("the gate was shut on some trial", below, 1, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_mp_lifecycle();
	rc |= run_mp_reset();
	rc |= run_mp_getbitvector();
	rc |= run_mp_printnof();

	set_level(0);
	dsplib_debug_capture_on = 0;
	return rc;
}
