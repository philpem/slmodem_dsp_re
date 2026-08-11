/*
 * t_v90mp.cpp -- differential test of V90MP: its lifecycle, `reset`,
 * `getBitVector`, `printNofRecievedMpMpNot`, and the three members that
 * actually carry the message -- `evaluateInfo`, `infoToBits` and `bitsToInfo`.
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
 *   run_mp_evaluateinfo
 *                      thirteen fields unpacked out of the bit vector.  +0x18
 *                      is the gate on six of them and is swept, not seeded;
 *                      the vector is driven BOTH as real bits and as arbitrary
 *                      bytes, because eleven of the reads mask with 1 and
 *                      three do not; and what the blob decoded is checked
 *                      against the blob's OWN vector rather than against our
 *                      source.
 *
 *   run_mp_infotobits  the same thirteen packed back.  The group size is swept
 *                      over values that make the sequence length land exactly
 *                      on a group and not, that reach 253 (where the padding
 *                      overwrites the length field it was reading) and that
 *                      pass 0xe6 (where it overwrites the CRC register).  It
 *                      asserts finding 1386's defect: the type-zero arm pads
 *                      from 0x45 and destroys the CRC it has just written.
 *
 *   run_mp_roundtrip   pack then unpack, and require the message to come back.
 *                      This is the only block that could see the two functions
 *                      agreeing on a bit position that is wrong in both.
 *
 *   run_mp_bitstoinfo* the receiver.  The argument is swept over both of its
 *                      widths (`test %esi,%esi` is the whole int, but state 2
 *                      stores only %cl), every state 0..5 is entered, the CRC
 *                      is driven to all three of its outcomes with a message
 *                      the BLOB built, the two counters are driven across
 *                      0x7fffffff where their signed and unsigned readings
 *                      part, and every level 0..3 is swept because the MP and
 *                      MPnot arms do not gate at the same one.  A final block
 *                      asserts that all four answers -- 0, 1, 2 and 3 -- came
 *                      back from the blob at least once.
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

void ref_mp_evaluateinfo(void *) asm("ref__ZN5V90MP12evaluateInfoEv");
void ref_mp_infotobits(void *) asm("ref__ZN5V90MP10infoToBitsEv");

/* NOT void: the blob leaves 0, 1, 2 or 3 in %eax.  See run_mp_bitstoinfo. */
int ref_mp_bitstoinfo(void *, int) asm("ref__ZN5V90MP10bitsToInfoEi");
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

/* ------------------------------------------------------- the message itself */

/*
 * THE VALUES THE MESSAGE CARRIES, swept rather than left to the seed.  Every
 * one has the top bit set somewhere in the list, because `Type`, `NonLin`,
 * `Shaping` and `CPack` are whole bytes out of `bits` and the six h-values are
 * sixteen bits: their `%d` in the diagnostic is the only thing that says
 * `movsbl`/`movswl` rather than `movzbl`/`movzwl`, and it says nothing at all
 * below 0x80.
 */
static const short info_vals[] = {
	0, 1, -1, 2, 0x7fff, (short)0x8000, 0x1234, (short)0xabcd, 0x5555,
	(short)0xaaaa, 0x3fff, (short)0xc000, 0x00ff, (short)0xff00
};
#define NINFO	((int)(sizeof(info_vals) / sizeof(info_vals[0])))

/*
 * The group sizes.  NEVER ZERO -- the sequence length is computed with `div`
 * and the blob faults on zero exactly as we do, so a trial that reached it
 * would kill the run rather than test anything.  The list is chosen for what
 * +0x118 comes out as: exact multiples of 0xbc and 0x56 (the `je`
 * short-circuit), inexact ones, 253/254/255 (where the pad loop reaches
 * +0x118 itself), 0x100 (which truncates to zero) and 120/231 (which pad past
 * the end of the bit vector and into the CRC register).
 */
static const unsigned int groups[] = {
	1, 2, 3, 4, 8, 0x2f, 0x5e, 0xbc, 0x56, 43, 0x1c, 100, 120, 231, 253,
	254, 255, 0x100, 0x101
};
#define NGROUPS	((int)(sizeof(groups) / sizeof(groups[0])))

/* The same message into both objects. */
static void
set_info(int trial)
{
	V90MP *o[2];
	int s;

	o[0] = MPA;
	o[1] = MPB;
	for (s = 0; s < 2; s++) {
		o[s]->Type = (char)((trial % 5) == 0 ? 0 : trial);
		o[s]->Rate = (char)info_vals[(trial + 1) % NINFO];
		o[s]->Trellis = (char)(trial % 6);	/* 4 and 5 miss the switch */
		o[s]->NonLin = (char)info_vals[(trial + 2) % NINFO];
		o[s]->Shaping = (char)info_vals[(trial + 3) % NINFO];
		o[s]->CPack = (char)info_vals[(trial + 4) % NINFO];
		o[s]->rateMask = info_vals[(trial + 5) % NINFO];
		o[s]->h1Real = info_vals[(trial + 6) % NINFO];
		o[s]->h1Imag = info_vals[(trial + 7) % NINFO];
		o[s]->h2Real = info_vals[(trial + 8) % NINFO];
		o[s]->h2Imag = info_vals[(trial + 9) % NINFO];
		o[s]->h3Real = info_vals[(trial + 10) % NINFO];
		o[s]->h3Imag = info_vals[(trial + 11) % NINFO];
	}
}

/*
 * Some trials want a bit vector of real bits and some want arbitrary bytes:
 * `evaluateInfo` masks with 1 in eleven places and copies three bytes whole,
 * so a vector that was only ever 0 or 1 cannot tell the two apart.
 */
static void
narrow_bits(int trial)
{
	unsigned i;

	if ((trial & 1) != 0)
		return;
	for (i = 0; i < V90MP_BITS; i++) {
		MPA->bits[i] = (unsigned char)(MPA->bits[i] & 1);
		MPB->bits[i] = MPA->bits[i];
	}
}

/* ---------------------------------------------- evaluateInfo (482 bytes) */

static int
run_mp_evaluateinfo(void)
{
	int trial, varied = 0, zero_type = 0, nonzero_type = 0, wide = 0;
	unsigned char first[MP_SLOT];

	diff_begin("V90MP::evaluateInfo");
	set_level(0);

	for (trial = 0; trial < 64; trial++) {
		unsigned char before[MP_SLOT];
		long tag = 3000 + trial;
		int exp;

		seed_pair(trial + 500, trial & 3);
		narrow_bits(trial);

		/*
		 * +0x18 IS THE GATE, so it is swept and not seeded: a run that
		 * only ever saw a non-zero type would never take the early
		 * return, and one that only ever saw zero would never read a
		 * single h-value.
		 */
		MPA->type = MPB->type =
		    (char)((trial % 3) == 0 ? 0 : (trial % 3) == 1 ? 1 : trial);
		if (MPB->type == 0)
			zero_type = 1;
		else
			nonzero_type = 1;

		memcpy(before, mp_b, MP_SLOT);
		MPA->evaluateInfo();
		ref_mp_evaluateinfo(mp_b);

		diff_eq_obj("after evaluateInfo", V90MP, MPA, MPB, tag);
		guard_intact(tag);
		diff_eq_int("evaluateInfo changed the object (%ld)",
			    memcmp(before, mp_b, sizeof(V90MP)) != 0, 1, tag);

		/*
		 * The bit vector is READ ONLY here -- twelve loops and not one
		 * store below +0x14 -- so the whole of it must still be the
		 * seed.  This is what makes "evaluateInfo packs" a catchable
		 * mistake.
		 */
		diff_eq_int("the bit vector is untouched (%ld)",
			    memcmp(before + 0x1c, mp_b + 0x1c,
				   MP_SLOT - 0x1c) == 0, 1, tag);

		/*
		 * WHAT THE BLOB DECODED, computed here from its own bit
		 * vector rather than from our source.
		 */
		diff_eq_int("Type is +0x18 (%ld)", (long)MPB->Type,
			    (long)MPB->type, tag);

		exp = (MPB->bits[0x1b] & 1) * 8 + (MPB->bits[0x1a] & 1) * 4 +
		      (MPB->bits[0x19] & 1) * 2 + (MPB->bits[0x18] & 1);
		diff_eq_int("Rate is bits[0x18..0x1b], 0x1b highest (%ld)",
			    (long)MPB->Rate, exp, tag);

		exp = (MPB->bits[0x1e] & 1) * 2 + (MPB->bits[0x1d] & 1);
		diff_eq_int("Trellis is bits[0x1d..0x1e] (%ld)",
			    (long)MPB->Trellis, exp, tag);

		/* Whole bytes: no `and $0x1` on these three. */
		diff_eq_int("NonLin is the whole byte bits[0x1f] (%ld)",
			    (long)(unsigned char)MPB->NonLin,
			    (long)MPB->bits[0x1f], tag);
		diff_eq_int("Shaping is the whole byte bits[0x20] (%ld)",
			    (long)(unsigned char)MPB->Shaping,
			    (long)MPB->bits[0x20], tag);
		diff_eq_int("CPack is the whole byte bits[0x21] (%ld)",
			    (long)(unsigned char)MPB->CPack,
			    (long)MPB->bits[0x21], tag);
		if (MPB->bits[0x1f] > 1 || MPB->bits[0x20] > 1 ||
		    MPB->bits[0x21] > 1)
			wide = 1;

		exp = 0;
		{
			int i;

			for (i = 0x24; i <= 0x31; i++)
				if (MPB->bits[i])
					exp |= 1 << (i - 0x24);
		}
		diff_eq_int("rateMask is bits[0x24..0x31], fourteen of them "
			    "(%ld)", (long)(unsigned short)MPB->rateMask,
			    (long)exp, tag);

		if (MPB->type == 0) {
			diff_eq_int("a type-zero message leaves h1Real zero "
				    "(%ld)", (long)MPB->h1Real, 0, tag);
			diff_eq_int("...and h3Imag (%ld)", (long)MPB->h3Imag,
				    0, tag);
		} else {
			int i;

			exp = 0;
			for (i = 0x43; i > 0x33; i--)
				exp = (exp << 1) | (MPB->bits[i] & 1);
			diff_eq_int("h1Real is bits[0x34..0x43], 0x43 highest "
				    "(%ld)", (long)(unsigned short)MPB->h1Real,
				    (long)exp, tag);
			exp = 0;
			for (i = 0x98; i > 0x88; i--)
				exp = (exp << 1) | (MPB->bits[i] & 1);
			diff_eq_int("h3Imag is bits[0x89..0x98] (%ld)",
				    (long)(unsigned short)MPB->h3Imag,
				    (long)exp, tag);
		}

		if (trial == 0)
			memcpy(first, mp_b, MP_SLOT);
		else if (memcmp(first, mp_b, MP_SLOT) != 0)
			varied = 1;
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	diff_eq_int("a type-zero message was decoded", zero_type, 1, 0);
	diff_eq_int("a type-nonzero message was decoded", nonzero_type, 1, 0);
	diff_eq_int("a bit vector holding values above 1 was decoded", wide, 1,
		    0);
	return diff_end();
}

/* ------------------------------------------------ infoToBits (1948 bytes) */

static int
run_mp_infotobits(void)
{
	int trial, varied = 0, long_msg = 0, short_msg = 0;
	int exact = 0, inexact = 0, wraps = 0, past_end = 0;
	unsigned char first[MP_SLOT];

	diff_begin("V90MP::infoToBits");
	set_level(0);

	for (trial = 0; trial < 96; trial++) {
		unsigned char before[MP_SLOT];
		long tag = 4000 + trial;
		unsigned int g = groups[trial % NGROUPS];
		unsigned int want;
		int i;

		seed_pair(trial + 600, trial & 3);
		set_info(trial);
		MPA->word_114 = MPB->word_114 = g;
		memcpy(before, mp_b, MP_SLOT);

		MPA->infoToBits();
		ref_mp_infotobits(mp_b);

		diff_eq_obj("after infoToBits", V90MP, MPA, MPB, tag);
		guard_intact(tag);
		diff_eq_int("infoToBits changed the object (%ld)",
			    memcmp(before, mp_b, sizeof(V90MP)) != 0, 1, tag);

		/* The frame structure, read off the blob's own vector. */
		for (i = 0; i <= 0x10; i++)
			if (MPB->bits[i] != 1)
				break;
		diff_eq_int("the preamble is seventeen ones (%ld)", i, 0x11,
			    tag);
		diff_eq_int("bit 17 is the framing zero (%ld)",
			    (long)MPB->bits[0x11], 0, tag);
		diff_eq_int("bit 18 is Type (%ld)", (long)MPB->bits[0x12],
			    (long)(unsigned char)MPB->Type, tag);
		diff_eq_int("+0x18 took Type (%ld)", (long)MPB->type,
			    (long)MPB->Type, tag);

		/*
		 * THE PAD LOOP CAN REACH THE FIELDS THESE CHECKS READ.  It
		 * runs to +0x118 - 1 and `bits` is only 0xe6 long, so a
		 * sequence length above 0xe6 zeroes the CRC register, above
		 * 0xfc zeroes +0x118 itself and above 0xfd zeroes +0x119.
		 * That is the object's own behaviour and not a fault, so what
		 * is expected has to be computed from the group size rather
		 * than read back out of the object afterwards.
		 */
		want = MPB->Type ? 0xbb : 0x55;
		{
			unsigned int n = want + 1;
			unsigned char len = (unsigned char)
			    (n % g == 0 ? n : (n / g + 1) * g);

			if (n % g == 0)
				exact = 1;
			else
				inexact = 1;
			if (len >= 253)
				wraps = 1;
			if (len > 0xe6)
				past_end = 1;

			if (len <= 0xfc)
				diff_eq_int("+0x118 is the length rounded up "
					    "to a group (%ld)",
					    (long)MPB->byte_118, (long)len,
					    tag);
			else
				diff_eq_int("+0x118 padded over itself (%ld)",
					    (long)MPB->byte_118, 0, tag);

			if (len <= 0xfd)
				diff_eq_int("+0x119 is the sequence's bit "
					    "count (%ld)", (long)MPB->byte_119,
					    (long)want, tag);
			else
				diff_eq_int("+0x119 was padded over (%ld)",
					    (long)MPB->byte_119, 0, tag);

			want = len;
		}

		if (MPB->Type != 0) {
			long_msg = 1;
			diff_eq_int("the framing bits are zero (%ld)",
				    (long)(MPB->bits[0x22] | MPB->bits[0x33] |
					   MPB->bits[0x44] | MPB->bits[0x55] |
					   MPB->bits[0x66] | MPB->bits[0x77] |
					   MPB->bits[0x88] | MPB->bits[0x99] |
					   MPB->bits[0xaa]), 0, tag);
			/*
			 * THE LONG ARM KEEPS ITS CRC: the bit after it is
			 * zeroed and the padding starts beyond that.  This is
			 * the half of finding 1386 that is NOT a defect, and
			 * it is here so that the short arm's failure below is
			 * a difference and not an assumption.
			 */
			diff_eq_int("bits[0xbb] is the first pad bit (%ld)",
				    (long)MPB->bits[0xbb], 0, tag);
			if (want <= 0xe6) {
				for (i = 0; i < 16; i++)
					if (MPB->bits[0xab + i] != MPB->crc[i])
						break;
				diff_eq_int("the CRC reached bits[0xab..0xba] "
					    "(%ld)", i, 16, tag);
			}
		} else {
			short_msg = 1;
			/*
			 * THE SHORT ARM DESTROYS ITS OWN CRC -- finding 1386.
			 * +0x118 is at least 0x56 for any group size, so the
			 * pad loop always runs and always starts at 0x45.
			 */
			diff_eq_int("the short arm zeroed its own first CRC "
				    "bit (%ld)", (long)MPB->bits[0x45], 0, tag);
			if (want > 0x54)
				for (i = 0; i < 16; i++)
					diff_eq_int("...and the whole of it "
						    "(%ld)",
						    (long)MPB->bits[0x45 + i],
						    0, tag);
			for (i = 0x34; i <= 0x44; i++)
				diff_eq_int("frame 3 is zero (%ld)",
					    (long)MPB->bits[i], 0, tag);
		}

		if (trial == 0)
			memcpy(first, mp_b, MP_SLOT);
		else if (memcmp(first, mp_b, MP_SLOT) != 0)
			varied = 1;
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	diff_eq_int("a long message was packed", long_msg, 1, 0);
	diff_eq_int("a short message was packed", short_msg, 1, 0);
	diff_eq_int("the length came out exact on some trial", exact, 1, 0);
	diff_eq_int("and inexact on some other", inexact, 1, 0);
	diff_eq_int("a length of 253 or more was reached", wraps, 1, 0);
	diff_eq_int("a length that pads past the bit vector was reached",
		    past_end, 1, 0);
	return diff_end();
}

/* -------------------------------------------------------- the round trip */

/*
 * PACK, THEN UNPACK.  Everything above compares two objects; this compares the
 * message against itself, which is the only check that can see the two
 * functions agreeing on a bit position that is wrong in both.
 */
static int
run_mp_roundtrip(void)
{
	int trial, seen_long = 0, seen_short = 0;

	diff_begin("V90MP: infoToBits then evaluateInfo");
	set_level(0);

	for (trial = 0; trial < 64; trial++) {
		long tag = 5000 + trial;
		unsigned char origbuf[sizeof(V90MP)];
		/* A byte copy: V90MP has a destructor and is not trivially
		 * copyable, so `memcpy` on the class itself is a warning. */
		const V90MP *orig = (const V90MP *)(const void *)origbuf;

		seed_pair(trial + 700, trial & 3);
		set_info(trial);
		MPA->word_114 = MPB->word_114 = groups[trial % NGROUPS];
		memcpy(origbuf, mp_b, sizeof(V90MP));

		MPA->infoToBits();
		ref_mp_infotobits(mp_b);
		MPA->evaluateInfo();
		ref_mp_evaluateinfo(mp_b);

		diff_eq_obj("after the round trip", V90MP, MPA, MPB, tag);
		guard_intact(tag);

		diff_eq_int("Type came back (%ld)", (long)MPB->Type,
			    (long)orig->Type, tag);
		diff_eq_int("Rate came back, four bits of it (%ld)",
			    (long)MPB->Rate, (long)(orig->Rate & 0xf), tag);
		if (orig->Trellis >= 0 && orig->Trellis <= 3)
			diff_eq_int("Trellis came back (%ld)",
				    (long)MPB->Trellis, (long)orig->Trellis,
				    tag);
		diff_eq_int("NonLin came back whole (%ld)", (long)MPB->NonLin,
			    (long)orig->NonLin, tag);
		diff_eq_int("Shaping came back whole (%ld)",
			    (long)MPB->Shaping, (long)orig->Shaping, tag);
		diff_eq_int("CPack came back whole (%ld)", (long)MPB->CPack,
			    (long)orig->CPack, tag);
		diff_eq_int("rateMask came back, fourteen bits of it (%ld)",
			    (long)MPB->rateMask, (long)(orig->rateMask & 0x3fff),
			    tag);

		if (orig->Type != 0) {
			seen_long = 1;
			diff_eq_int("h1Real came back (%ld)", (long)MPB->h1Real,
				    (long)orig->h1Real, tag);
			diff_eq_int("h1Imag came back (%ld)", (long)MPB->h1Imag,
				    (long)orig->h1Imag, tag);
			diff_eq_int("h2Real came back (%ld)", (long)MPB->h2Real,
				    (long)orig->h2Real, tag);
			diff_eq_int("h2Imag came back (%ld)", (long)MPB->h2Imag,
				    (long)orig->h2Imag, tag);
			diff_eq_int("h3Real came back (%ld)", (long)MPB->h3Real,
				    (long)orig->h3Real, tag);
			diff_eq_int("h3Imag came back (%ld)", (long)MPB->h3Imag,
				    (long)orig->h3Imag, tag);
		} else {
			seen_short = 1;
			diff_eq_int("a short message brings back no h1Real "
				    "(%ld)", (long)MPB->h1Real, 0, tag);
		}
	}

	diff_eq_int("a long message went round", seen_long, 1, 0);
	diff_eq_int("a short message went round", seen_short, 1, 0);
	return diff_end();
}

/* ------------------------------------------------ bitsToInfo (2597 bytes) */

static int rc_seen[4];

/*
 * One received bit into both objects, with the transcript captured.  Every
 * check that has to hold for every call lives here.
 */
static void
drive_bit(int bit, long tag)
{
	int ra, rb;

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();

	ra = MPA->bitsToInfo(bit);
	rb = ref_mp_bitstoinfo(mp_b, bit);

	dsplib_debug_capture_on = 0;

	diff_eq_int("the answer matches (%ld)", (long)ra, (long)rb, tag);
	diff_eq_obj("after bitsToInfo", V90MP, MPA, MPB, tag);
	guard_intact(tag);
	diff_eq_int("transcript matches (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);

	if (rb >= 0 && rb <= 3)
		rc_seen[rb] = 1;
	else
		diff_eq_int("the answer is one of 0..3 (%ld)", (long)rb, 0,
			    tag);
}

/*
 * THE ARGUMENT'S TWO WIDTHS.  `test %esi,%esi` at entry looks at the whole
 * int, but state 2 stores `%cl` into +0x18 and takes the sequence length from
 * that byte -- so 0x100 is a one to the run counters and a type of zero to the
 * message.  Both edges are in the sweep.
 */
static const int bit_vals[] = {
	0, 1, 2, 0xff, 0x100, 0x101, -1, 0x7fffffff, (-0x7fffffff - 1)
};
#define NBITS	((int)(sizeof(bit_vals) / sizeof(bit_vals[0])))

static int
run_mp_bitstoinfo(void)
{
	int trial, lvl, varied = 0, ed = 0, states[6];
	unsigned char first[MP_SLOT];

	diff_begin("V90MP::bitsToInfo -- the state machine");
	memset(states, 0, sizeof(states));

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);
		/*
		 * EVERY STATE AGAINST EVERY ARGUMENT.  Indexing both by
		 * `trial` looked like a sweep and was not: six states and nine
		 * arguments share the factor three, so `trial % 6 == 1` and
		 * `trial % 9 == 0` never met and state 1 was never once
		 * offered a zero -- the only bit it reacts to.  The mutation
		 * set found that (`the framing zero goes straight to state 3`
		 * survived); the cross product fixes it.
		 */
		for (trial = 0; trial < 108; trial++) {
			long tag = (long)lvl * 10000 + 6000 + trial;
			int bit = bit_vals[(trial / 6) % NBITS];
			unsigned int st = (unsigned int)(trial % 6);

			seed_pair(trial + 800, trial & 3);
			narrow_bits(trial);

			MPA->word_14 = MPB->word_14 = st;
			MPA->word_114 = MPB->word_114 =
			    groups[trial % NGROUPS];
			MPA->type = MPB->type = (char)(trial & 1);
			/*
			 * Ed IS A RUN OF ZEROS AGAINST TWICE THE GROUP SIZE
			 * with the index still at 18, so one trial in three
			 * is set up to reach it and the rest are not.
			 */
			if (trial % 3 == 0) {
				MPA->word_114 = MPB->word_114 = 4;
				MPA->byte_1a = MPB->byte_1a = 7;
				/*
				 * ...and half of those with the index NOT at
				 * 18, because the run length alone is not the
				 * condition: `cmpb $0x12,0x1b(%ebx); jne` at
				 * 0x202d0 is the other half of it.
				 */
				MPA->byte_1b = MPB->byte_1b =
				    (unsigned char)(((trial / 6) & 1) ? 19
								     : 18);
			} else {
				MPA->byte_1a = MPB->byte_1a =
				    (unsigned char)trial;
				MPA->byte_1b = MPB->byte_1b =
				    (unsigned char)(0x12 + (trial % 40));
			}
			MPA->byte_19 = MPB->byte_19 =
			    (unsigned char)(trial % 20 == 0 ? 0x10 : trial);
			MPA->byte_118 = MPB->byte_118 =
			    (unsigned char)(MPB->byte_1b + (trial % 3));
			MPA->byte_119 = MPB->byte_119 =
			    (unsigned char)(MPB->byte_1b + (trial % 2));

			if (trial == 0)
				memcpy(first, mp_b, MP_SLOT);

			drive_bit(bit, tag);

			states[st] = 1;
			if (st <= 4 && MPB->word_14 > 4)
				diff_eq_int("a known state stayed known (%ld)",
					    (long)MPB->word_14, 0, tag);
			if (memcmp(first, mp_b, MP_SLOT) != 0)
				varied = 1;

			/* The run counters, checked against the blob. */
			if (bit != 0)
				diff_eq_int("a one cleared the zero run (%ld)",
					    (long)MPB->byte_1a, 0, tag);
			else
				diff_eq_int("a zero cleared the one run (%ld)",
					    (long)MPB->byte_19, 0, tag);
		}
	}

	/* State 0 leaves for state 1 only past sixteen ones. */
	for (trial = 0; trial < 8; trial++) {
		long tag = 6900 + trial;
		unsigned char run = (unsigned char)(0x0e + trial);

		set_level(0);
		seed_pair(trial + 810, trial & 3);
		MPA->word_14 = MPB->word_14 = 0;
		MPA->word_114 = MPB->word_114 = 4;
		MPA->byte_19 = MPB->byte_19 = run;
		MPA->byte_1a = MPB->byte_1a = 0;
		MPA->byte_1b = MPB->byte_1b = 18;
		drive_bit(1, tag);
		diff_eq_int("seventeen ones and not sixteen (%ld)",
			    (long)MPB->word_14, run + 1 > 0x10 ? 1 : 0, tag);
		ed = 1;
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	diff_eq_int("the preamble threshold was crossed", ed, 1, 0);
	for (trial = 0; trial < 6; trial++)
		diff_eq_int("state %ld was entered", states[trial], 1, trial);
	return diff_end();
}

/*
 * The CRC, driven with a message the BLOB built.  Three outcomes: it matches,
 * it matches once bits[0x70] has been inverted, and it does not match at all.
 */
static int
run_mp_bitstoinfo_crc(void)
{
	int trial, lvl, good = 0, modified = 0, bad = 0, shortbad = 0, wrapped = 0;

	diff_begin("V90MP::bitsToInfo -- the CRC");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);
		for (trial = 0; trial < 24; trial++) {
			long tag = (long)lvl * 10000 + 7000 + trial;
			int type = (trial & 1);
			int damage = trial % 3;
			unsigned char n;
			int k;

			seed_pair(trial + 900, trial & 3);
			set_info(trial);
			MPA->Type = MPB->Type = (char)type;
			MPA->word_114 = MPB->word_114 = 4;
			MPA->infoToBits();
			ref_mp_infotobits(mp_b);

			n = MPB->byte_119;
			if (type == 0) {
				/*
				 * Put back the CRC the short arm destroyed
				 * (finding 1386), from the register it left
				 * it in, so that this block tests the CHECK
				 * and not the defect.
				 */
				for (k = 0; k < 16; k++) {
					MPA->bits[0x45 + k] = MPA->crc[k];
					MPB->bits[0x45 + k] = MPB->crc[k];
				}
			}

			if (damage == 1) {
				/* The one bit the object knows how to mend. */
				MPA->bits[0x70] = (unsigned char)
				    (MPA->bits[0x70] == 0);
				MPB->bits[0x70] = MPA->bits[0x70];
			} else if (damage == 2) {
				MPA->bits[0x30] = (unsigned char)
				    (MPA->bits[0x30] == 0);
				MPB->bits[0x30] = MPA->bits[0x30];
			}

			MPA->word_14 = MPB->word_14 = 3;
			MPA->byte_1b = MPB->byte_1b = (unsigned char)(n - 1);
			MPA->byte_19 = MPB->byte_19 = 3;
			MPA->byte_1a = MPB->byte_1a = 3;

			drive_bit(MPB->bits[n - 1], tag);

			/*
			 * bits[0x70] IS NOT IN A SHORT MESSAGE.  The CRC there
			 * covers [0x12, 0x44) and the received copy sits at
			 * 0x45..0x54, so inverting 0x70 damages nothing and
			 * the first check still passes -- which is worth
			 * asserting, because it is the same constant the long
			 * message repairs.
			 */
			if (damage == 0 || (damage == 1 && type == 0)) {
				good = 1;
				diff_eq_int("a good CRC advances to state 4 "
					    "(%ld)", (long)MPB->word_14, 4,
					    tag);
				diff_eq_int("...and keeps the index (%ld)",
					    (long)MPB->byte_1b, (long)n, tag);
			} else if (damage == 1) {
				modified = 1;
				diff_eq_int("the mended CRC advances too "
					    "(%ld)", (long)MPB->word_14, 4,
					    tag);
				diff_eq_int("...and the bit stayed mended "
					    "(%ld)", (long)MPB->bits[0x70],
					    (long)MPA->bits[0x70], tag);
			} else {
				if (type == 0)
					shortbad = 1;
				bad = 1;
				diff_eq_int("a bad CRC resets the detector "
					    "(%ld)", (long)MPB->word_14, 0,
					    tag);
				diff_eq_int("...to bit 18 (%ld)",
					    (long)MPB->byte_1b, 18, tag);
			}
		}
	}

	/*
	 * A sequence length below 0x10 makes the comparison read BELOW the bit
	 * vector -- the object indexes `this + byte_119 + k + 0xc` with no
	 * check at all -- and both sides must read the same bytes.
	 */
	for (trial = 0; trial < 8; trial++) {
		long tag = 7900 + trial;

		set_level(0);
		seed_pair(trial + 950, trial & 3);
		MPA->word_14 = MPB->word_14 = 3;
		MPA->word_114 = MPB->word_114 = 4;
		MPA->type = MPB->type = (char)(trial & 1);
		MPA->byte_119 = MPB->byte_119 = (unsigned char)(trial + 1);
		MPA->byte_1b = MPB->byte_1b = (unsigned char)trial;
		drive_bit(trial & 1, tag);
	}

	/*
	 * THE SUM IS ONE BYTE.  `add %al,0x4e(%esp)` over sixteen absolute
	 * differences and then `cmpb $0x0`, so sixteen differences of 16 come
	 * to 256, which is zero in a byte and "the CRC matched" to the object.
	 * The comparison has to be against the CRC the blob itself computes,
	 * so it is taken out of the register the blob leaves it in after one
	 * pass over the message -- and the sixteen bytes being rewritten are
	 * OUTSIDE the span the CRC covers, so the second pass computes the
	 * same sixteen bits as the first.
	 */
	{
		long tag = 7950;
		unsigned char n;
		int k;

		set_level(0);
		seed_pair(77, 1);
		set_info(3);
		MPA->Type = MPB->Type = 1;
		MPA->word_114 = MPB->word_114 = 4;
		MPA->infoToBits();
		ref_mp_infotobits(mp_b);
		n = MPB->byte_119;

		MPA->word_14 = MPB->word_14 = 3;
		MPA->byte_1b = MPB->byte_1b = (unsigned char)(n - 1);
		MPA->byte_19 = MPB->byte_19 = 1;
		MPA->byte_1a = MPB->byte_1a = 1;
		drive_bit(MPB->bits[n - 1], tag);

		for (k = 0; k < 16; k++) {
			unsigned char v = (unsigned char)(MPB->crc[k] + 16);

			MPA->bits[n - 0x10 + k] = v;
			MPB->bits[n - 0x10 + k] = v;
		}
		MPA->word_14 = MPB->word_14 = 3;
		MPA->byte_1b = MPB->byte_1b = (unsigned char)(n - 1);
		MPA->byte_19 = MPB->byte_19 = 1;
		MPA->byte_1a = MPB->byte_1a = 1;
		drive_bit(MPB->bits[n - 1], tag + 1);

		diff_eq_int("sixteen differences of 16 sum to zero in a byte "
			    "and read as a match (%ld)", (long)MPB->word_14, 4,
			    tag + 1);
		wrapped = 1;
	}

	diff_eq_int("a sum that wraps the byte was driven", wrapped, 1, 0);
	diff_eq_int("a good CRC was seen", good, 1, 0);
	diff_eq_int("a CRC mended by inverting bits[0x70] was seen", modified,
		    1, 0);
	diff_eq_int("a bad CRC was seen", bad, 1, 0);
	diff_eq_int("a short message never gets the repair", shortbad, 1, 0);
	return diff_end();
}

/*
 * State 4 running out, `evaluateInfo` firing, and the two counters -- which is
 * where the diagnostics are, and where the MP and MPnot arms disagree about
 * what level they need.
 */
static int
run_mp_bitstoinfo_report(void)
{
	static const unsigned int starts[] = {
		0, 1, 2, 3, 0x7fffffffu, 0x80000000u, 0xffffffffu
	};
	int trial, lvl, mp = 0, mpnot = 0, quiet = 0, loud = 0;
	int unsigned_seen = 0;
	int nstart = (int)(sizeof(starts) / sizeof(starts[0]));

	diff_begin("V90MP::bitsToInfo -- MP, MPnot and the counters");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned)lvl);
		for (trial = 0; trial < 56; trial++) {
			long tag = (long)lvl * 10000 + 8000 + trial;
			int isMp = (trial & 1) == 0;
			unsigned int start = starts[trial % nstart];

			seed_pair(trial + 1000, trial & 3);
			set_info(trial);
			MPA->word_114 = MPB->word_114 = 4;
			/*
			 * THE TOP BIT, on half the trials.  `Type` and `CPack`
			 * are printed with `%d` after a `movsbl`, and that is
			 * the ONLY place either one's signedness is visible --
			 * at 0..7 the two readings agree and the claim in the
			 * header cannot be defended.
			 */
			MPA->Type = MPB->Type = (char)((trial & 2) ? 0x80 : 0);
			/*
			 * ...and the same for a short.  `info_vals` alone will
			 * not do it: its negative entries sit at odd indices,
			 * the MP arm takes even trials, and the counter's
			 * seven starts share a factor with the fourteen
			 * values -- so every trial that reached the h1 line at
			 * level 3 carried a positive h1Real.  The mutation set
			 * found that; this line is the fix.
			 */
			MPA->h1Real = MPB->h1Real =
			    (short)((trial & 2) ? 0x8001 : 0x1234);
			MPA->infoToBits();
			ref_mp_infotobits(mp_b);

			/*
			 * CPack decides which counter moves, and it is a bit
			 * of the message: `evaluateInfo` takes it from
			 * bits[0x21] before anything looks at it.
			 */
			MPA->bits[0x21] = MPB->bits[0x21] = (unsigned char)
			    (isMp ? 0 : (trial & 2) ? 0x80 : 1 + (trial % 7));

			MPA->word_14 = MPB->word_14 = 4;
			MPA->byte_118 = MPB->byte_118 =
			    (unsigned char)(0x20 + trial);
			MPA->byte_1b = MPB->byte_1b =
			    (unsigned char)(0x1f + trial);
			MPA->byte_19 = MPB->byte_19 = 5;
			MPA->byte_1a = MPB->byte_1a = 5;
			MPA->nofRecievedMp = MPB->nofRecievedMp = start;
			MPA->nofRecievedMpNot = MPB->nofRecievedMpNot = start;

			drive_bit(trial & 1, tag);

			if (isMp) {
				mp = 1;
				diff_eq_int("an MP answers 1 (%ld)",
					    (long)MPB->word_14, 0, tag);
				diff_eq_int("the MP counter moved (%ld)",
					    (long)MPB->nofRecievedMp,
					    (long)(start + 1), tag);
				diff_eq_int("the MPnot counter did not (%ld)",
					    (long)MPB->nofRecievedMpNot,
					    (long)start, tag);
			} else {
				mpnot = 1;
				diff_eq_int("the MPnot counter moved (%ld)",
					    (long)MPB->nofRecievedMpNot,
					    (long)(start + 1), tag);
				diff_eq_int("the MP counter did not (%ld)",
					    (long)MPB->nofRecievedMp,
					    (long)start, tag);
			}

			/*
			 * THE COUNTER IS UNSIGNED.  At 0x7fffffff the bumped
			 * value is 0x80000000, which is above two unsigned and
			 * below it signed -- so the blob says nothing and a
			 * signed reading would say four lines.  This is the
			 * only check in the tree that can tell them apart.
			 */
			if (lvl > 1 && (start == 0x7fffffffu ||
					start == 0x80000000u)) {
				diff_eq_int("a counter past 2 unsigned is "
					    "silent (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				unsigned_seen = 1;
			}
			if (lvl > 1 && start <= 1)
				loud = 1;
			if (lvl <= 1)
				quiet = 1;
			if (lvl > 1 && start <= 1 &&
			    dsplib_debug_capture_lines(1) == 0)
				diff_eq_int("a fresh counter is not silent "
					    "(%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    1, tag);
		}
	}

	set_level(0);
	diff_eq_int("an MP was reported", mp, 1, 0);
	diff_eq_int("an MPnot was reported", mpnot, 1, 0);
	diff_eq_int("the gate was shut on some trial", quiet, 1, 0);
	diff_eq_int("and open on another", loud, 1, 0);
	diff_eq_int("a counter whose signed and unsigned readings differ was "
		    "driven", unsigned_seen, 1, 0);
	return diff_end();
}

/* Every answer the function can give, and the proof that each was seen. */
static int
run_mp_bitstoinfo_answers(void)
{
	int i;

	diff_begin("V90MP::bitsToInfo -- every answer observed");
	for (i = 0; i < 4; i++)
		diff_eq_int("answer %ld came back from the blob", rc_seen[i],
			    1, i);
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
	rc |= run_mp_evaluateinfo();
	rc |= run_mp_infotobits();
	rc |= run_mp_roundtrip();
	rc |= run_mp_bitstoinfo();
	rc |= run_mp_bitstoinfo_crc();
	rc |= run_mp_bitstoinfo_report();
	rc |= run_mp_bitstoinfo_answers();

	set_level(0);
	dsplib_debug_capture_on = 0;
	return rc;
}
