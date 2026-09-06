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
 *                      is caught here (finding F1224).
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
 *                      asserts finding F1386's defect: the type-zero arm pads
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
 *   run_mp_calccrc     the CRC register's write side.  Three claims that are
 *                      each an ABSENCE and so need their own trial: it does
 *                      not seed (the incoming register must reach the
 *                      answer), it does not read +0x119 (two runs differing
 *                      only in that byte must agree), and its extent comes
 *                      from the type flag (the other type must move it).
 *
 *   run_mp_evaluatecrc the read side, which DOES seed and DOES read +0x119 --
 *                      and takes its two extents from two different fields,
 *                      unlike the V.90 CP twin.  The trials that matter are
 *                      the ones where +0x18 and +0x119 disagree, because the
 *                      CP form and the MP form are the same arithmetic on
 *                      every object the class can build.
 *
 *   run_mp_crc_spec    NOT A DIFFERENTIAL BLOCK.  The CRC judged against
 *                      ITU-T V.34 10.1.2.3.2 and Table 16/V.90 rather than
 *                      against the blob, because "we match the object" and
 *                      "the object implements the standard" are two
 *                      questions and a defect can live in the gap.  Both
 *                      sides are held to the same spec-derived numbers.
 *
 * THE RULES, applied everywhere below: both sides are seeded with the SAME
 * varied pseudorandom bytes and are NEVER zeroed; the slot is 64 bytes longer
 * than the object and the tail is compared against the seed on both sides; and
 * every block asserts that the call changed something and that it did not
 * change it to the same thing on every trial (findings F223, F224, F230).
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

/*
 * The CRC pair.  `evaluateCRC` is NOT void -- 0x1f6ec builds its answer with
 * `xor %eax,%eax` / `test %bl,%bl` / `sete %al` -- and the mangling cannot
 * say so, since return types are not mangled.
 */
void ref_mp_calccrc(void *) asm("ref__ZN5V90MP7calcCRCEv");
int ref_mp_evalcrc(void *) asm("ref__ZN5V90MP11evaluateCRCEv");

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

/* The same varied bytes into both sides.  Never zeros -- finding F230. */
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
	diff_eq_int("rxState (%ld)", (long)o->rxState, 0, tag);
	diff_eq_int("onesRun (%ld)", (long)o->onesRun, 0, tag);
	diff_eq_int("zerosRun (%ld)", (long)o->zerosRun, 0, tag);
	diff_eq_int("bitIndex (%ld)", (long)o->bitIndex, 18, tag);
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
		 * finding F224's check, and the only thing that makes the
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
		 * F1224).  BOTH SIDES are compared against the seed, since the
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
		 * byte: that is finding F1237's claim stated as a test rather
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
		MPA->seqLength = lens[i];
		MPB->seqLength = lens[i];
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
		MPA->groupSize = MPB->groupSize = g;
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
					    (long)MPB->seqLength, (long)len,
					    tag);
			else
				diff_eq_int("+0x118 padded over itself (%ld)",
					    (long)MPB->seqLength, 0, tag);

			if (len <= 0xfd)
				diff_eq_int("+0x119 is the sequence's bit "
					    "count (%ld)", (long)MPB->bodyLength,
					    (long)want, tag);
			else
				diff_eq_int("+0x119 was padded over (%ld)",
					    (long)MPB->bodyLength, 0, tag);

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
			 * the half of finding F1386 that is NOT a defect, and
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
			 * THE SHORT ARM DESTROYS ITS OWN CRC -- finding F1386.
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
		MPA->groupSize = MPB->groupSize = groups[trial % NGROUPS];
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

			MPA->rxState = MPB->rxState = st;
			MPA->groupSize = MPB->groupSize =
			    groups[trial % NGROUPS];
			MPA->type = MPB->type = (char)(trial & 1);
			/*
			 * Ed IS A RUN OF ZEROS AGAINST TWICE THE GROUP SIZE
			 * with the index still at 18, so one trial in three
			 * is set up to reach it and the rest are not.
			 */
			if (trial % 3 == 0) {
				MPA->groupSize = MPB->groupSize = 4;
				MPA->zerosRun = MPB->zerosRun = 7;
				/*
				 * ...and half of those with the index NOT at
				 * 18, because the run length alone is not the
				 * condition: `cmpb $0x12,0x1b(%ebx); jne` at
				 * 0x202d0 is the other half of it.
				 */
				MPA->bitIndex = MPB->bitIndex =
				    (unsigned char)(((trial / 6) & 1) ? 19
								     : 18);
			} else {
				MPA->zerosRun = MPB->zerosRun =
				    (unsigned char)trial;
				MPA->bitIndex = MPB->bitIndex =
				    (unsigned char)(0x12 + (trial % 40));
			}
			MPA->onesRun = MPB->onesRun =
			    (unsigned char)(trial % 20 == 0 ? 0x10 : trial);
			MPA->seqLength = MPB->seqLength =
			    (unsigned char)(MPB->bitIndex + (trial % 3));
			MPA->bodyLength = MPB->bodyLength =
			    (unsigned char)(MPB->bitIndex + (trial % 2));

			if (trial == 0)
				memcpy(first, mp_b, MP_SLOT);

			drive_bit(bit, tag);

			states[st] = 1;
			if (st <= 4 && MPB->rxState > 4)
				diff_eq_int("a known state stayed known (%ld)",
					    (long)MPB->rxState, 0, tag);
			if (memcmp(first, mp_b, MP_SLOT) != 0)
				varied = 1;

			/* The run counters, checked against the blob. */
			if (bit != 0)
				diff_eq_int("a one cleared the zero run (%ld)",
					    (long)MPB->zerosRun, 0, tag);
			else
				diff_eq_int("a zero cleared the one run (%ld)",
					    (long)MPB->onesRun, 0, tag);
		}
	}

	/* State 0 leaves for state 1 only past sixteen ones. */
	for (trial = 0; trial < 8; trial++) {
		long tag = 6900 + trial;
		unsigned char run = (unsigned char)(0x0e + trial);

		set_level(0);
		seed_pair(trial + 810, trial & 3);
		MPA->rxState = MPB->rxState = 0;
		MPA->groupSize = MPB->groupSize = 4;
		MPA->onesRun = MPB->onesRun = run;
		MPA->zerosRun = MPB->zerosRun = 0;
		MPA->bitIndex = MPB->bitIndex = 18;
		drive_bit(1, tag);
		diff_eq_int("seventeen ones and not sixteen (%ld)",
			    (long)MPB->rxState, run + 1 > 0x10 ? 1 : 0, tag);
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
			MPA->groupSize = MPB->groupSize = 4;
			MPA->infoToBits();
			ref_mp_infotobits(mp_b);

			n = MPB->bodyLength;
			if (type == 0) {
				/*
				 * Put back the CRC the short arm destroyed
				 * (finding F1386), from the register it left
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

			MPA->rxState = MPB->rxState = 3;
			MPA->bitIndex = MPB->bitIndex = (unsigned char)(n - 1);
			MPA->onesRun = MPB->onesRun = 3;
			MPA->zerosRun = MPB->zerosRun = 3;

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
					    "(%ld)", (long)MPB->rxState, 4,
					    tag);
				diff_eq_int("...and keeps the index (%ld)",
					    (long)MPB->bitIndex, (long)n, tag);
			} else if (damage == 1) {
				modified = 1;
				diff_eq_int("the mended CRC advances too "
					    "(%ld)", (long)MPB->rxState, 4,
					    tag);
				diff_eq_int("...and the bit stayed mended "
					    "(%ld)", (long)MPB->bits[0x70],
					    (long)MPA->bits[0x70], tag);
			} else {
				if (type == 0)
					shortbad = 1;
				bad = 1;
				diff_eq_int("a bad CRC resets the detector "
					    "(%ld)", (long)MPB->rxState, 0,
					    tag);
				diff_eq_int("...to bit 18 (%ld)",
					    (long)MPB->bitIndex, 18, tag);
			}
		}
	}

	/*
	 * A sequence length below 0x10 makes the comparison read BELOW the bit
	 * vector -- the object indexes `this + bodyLength + k + 0xc` with no
	 * check at all -- and both sides must read the same bytes.
	 */
	for (trial = 0; trial < 8; trial++) {
		long tag = 7900 + trial;

		set_level(0);
		seed_pair(trial + 950, trial & 3);
		MPA->rxState = MPB->rxState = 3;
		MPA->groupSize = MPB->groupSize = 4;
		MPA->type = MPB->type = (char)(trial & 1);
		MPA->bodyLength = MPB->bodyLength = (unsigned char)(trial + 1);
		MPA->bitIndex = MPB->bitIndex = (unsigned char)trial;
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
		MPA->groupSize = MPB->groupSize = 4;
		MPA->infoToBits();
		ref_mp_infotobits(mp_b);
		n = MPB->bodyLength;

		MPA->rxState = MPB->rxState = 3;
		MPA->bitIndex = MPB->bitIndex = (unsigned char)(n - 1);
		MPA->onesRun = MPB->onesRun = 1;
		MPA->zerosRun = MPB->zerosRun = 1;
		drive_bit(MPB->bits[n - 1], tag);

		for (k = 0; k < 16; k++) {
			unsigned char v = (unsigned char)(MPB->crc[k] + 16);

			MPA->bits[n - 0x10 + k] = v;
			MPB->bits[n - 0x10 + k] = v;
		}
		MPA->rxState = MPB->rxState = 3;
		MPA->bitIndex = MPB->bitIndex = (unsigned char)(n - 1);
		MPA->onesRun = MPB->onesRun = 1;
		MPA->zerosRun = MPB->zerosRun = 1;
		drive_bit(MPB->bits[n - 1], tag + 1);

		diff_eq_int("sixteen differences of 16 sum to zero in a byte "
			    "and read as a match (%ld)", (long)MPB->rxState, 4,
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
			MPA->groupSize = MPB->groupSize = 4;
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

			MPA->rxState = MPB->rxState = 4;
			MPA->seqLength = MPB->seqLength =
			    (unsigned char)(0x20 + trial);
			MPA->bitIndex = MPB->bitIndex =
			    (unsigned char)(0x1f + trial);
			MPA->onesRun = MPB->onesRun = 5;
			MPA->zerosRun = MPB->zerosRun = 5;
			MPA->nofRecievedMp = MPB->nofRecievedMp = start;
			MPA->nofRecievedMpNot = MPB->nofRecievedMpNot = start;

			drive_bit(trial & 1, tag);

			if (isMp) {
				mp = 1;
				diff_eq_int("an MP answers 1 (%ld)",
					    (long)MPB->rxState, 0, tag);
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

/* ------------------------------------------------------------ the CRC pair */

/*
 * WHAT THESE TWO BLOCKS HAVE TO SEPARATE, and it is not obvious.
 *
 * `V90MP::calcCRC` and `V90CP::calcCRC` are the same shift register at two
 * message sizes, and the reconstruction of the second was written first.  The
 * temptation is to transcribe it -- and a transcription PASSES every trial in
 * which +0x18 and +0x119 agree with each other, because
 *
 *     0xbb - 0x11 == 0xaa      0x55 - 0x11 == 0x44
 *     0xbb - 0x10 == 0xab      0x55 - 0x10 == 0x45
 *
 * so the CP form `end = bodyLength - 0x11` and the MP form
 * `end = type ? 0xaa : 0x44` are indistinguishable on every object the class
 * itself can build.  The MP member really does read the TYPE FLAG for the
 * extent and the LENGTH FIELD for where the peer's CRC sits, and the only
 * trials that can tell the two apart are the ones where those two fields
 * DISAGREE:
 *
 *     type != 0 with +0x119 == 0x55     long extent, CRC read at 0x45
 *     type == 0 with +0x119 == 0xbb     short extent, CRC read at 0xab
 *
 * Both are entirely inside `bits`, so no overrun is needed to reach them.
 * They are driven below with `crc_split` asserting that the two formulas
 * actually parted on some trial, because a sweep that never separates them
 * is the same evidence a sweep that was never run gives.
 *
 * The other three claims each get their own assertion:
 *
 *   - `calcCRC` NEVER SEEDS.  There is no store of 1 in its 583 bytes, so it
 *     continues from whatever is in `crc`.  Two runs from the same bits and
 *     different incoming registers must differ (`carried`).
 *   - `calcCRC` NEVER READS +0x119.  Two runs differing only in that byte
 *     must agree (`ignored_119`).
 *   - `evaluateCRC` DOES seed, so the converse: two runs differing only in
 *     the incoming register must agree (`seeded`).
 */

/* A bit vector in both objects.  mode 0: real 0/1 bits.  mode 1: arbitrary
 * bytes, which only the parity of reaches the register but which a wrong
 * mask would expose. */
static void
fill_bits(int mode)
{
	unsigned i;

	for (i = 0; i < V90MP_BITS; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)(mode ? (lfsr >> 3) : (lfsr & 1u));
		MPA->bits[i] = MPB->bits[i] = v;
	}
}

/*
 * The register in both objects.  mode 0 is the sixteen ones `resetCRC`
 * writes; the others are states no reset can produce, which is what makes
 * "carried in, not seeded" a testable claim.  `crc[k] = crc[k + 1]` copies a
 * WHOLE byte where the three taps mask with 1, so mode 3 is not decoration.
 */
static void
fill_crc(int mode, unsigned char *keep)
{
	unsigned i;

	for (i = 0; i < V90MP_CRC; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		switch (mode) {
		case 0:  v = 1;					break;
		case 1:  v = (unsigned char)((i + 1u) & 1u);	break;
		case 2:  v = (unsigned char)(lfsr & 1u);	break;
		default: v = (unsigned char)((lfsr >> 3) | 2u);	break;
		}
		MPA->crc[i] = MPB->crc[i] = v;
		if (keep != 0)
			keep[i] = v;
	}
}

/*
 * The extents the class can be put in.  0x55 and 0xbb are the two Table
 * 16/V.90 message lengths; the rest are values +0x119 can hold and
 * `bitsToInfo` never writes.  0x10 is the smallest that keeps the sixteen
 * compared positions inside `bits[]`, and 0xf0 and 0xff run PAST it, into
 * `crc` and the two fields above it -- reads only, and the widest address the
 * byte can name is `this + 0x11a` against a `sizeof` of 0x124, so nothing
 * leaves the object and `guard_intact` still has to hold.
 */
static const unsigned char crc_lens[] = {
	0x55, 0xbb, 0x10, 0x30, 0x45, 0x70, 0xab, 0xe6, 0xf0, 0xff
};
#define CRC_NLENS	((int)(sizeof(crc_lens) / sizeof(crc_lens[0])))

static int
run_mp_calccrc(void)
{
	unsigned char keep[MP_SLOT];
	unsigned char r1[V90MP_CRC], r2[V90MP_CRC];
	int trial;
	int moved = 0, carried = 0, ignored_119 = 0, type_split = 0;

	diff_begin("V90MP::calcCRC");
	set_level(0);

	for (trial = 0; trial < 60; trial++) {
		long tag = 3400 + trial;
		int t = trial & 1;
		unsigned char in[V90MP_CRC];
		unsigned char alt;

		seed_pair(trial + 400, trial % 4);
		MPA->type = MPB->type =
		    (char)(t ? (0x80 | (trial + 1)) & 0xff : 0);
		MPA->bodyLength = MPB->bodyLength = crc_lens[trial % CRC_NLENS];
		fill_bits(trial & 1);
		fill_crc(trial % 4, in);
		memcpy(keep, mp_b, MP_SLOT);

		/*
		 * +0x119 IS NOT READ.  Same state, a different length byte,
		 * and the register must come out identical.  Run first, so
		 * the differential comparison below sees a pristine object.
		 */
		alt = (unsigned char)(crc_lens[(trial + 3) % CRC_NLENS]);
		ref_mp_calccrc(mp_b);
		memcpy(r1, MPB->crc, V90MP_CRC);
		memcpy(mp_b, keep, MP_SLOT);
		MPB->bodyLength = alt;
		ref_mp_calccrc(mp_b);
		memcpy(r2, MPB->crc, V90MP_CRC);
		if (alt != crc_lens[trial % CRC_NLENS]) {
			diff_eq_int("+0x119 did not reach calcCRC (%ld)",
				    memcmp(r1, r2, V90MP_CRC) == 0, 1, tag);
			if (memcmp(r1, in, V90MP_CRC) != 0)
				ignored_119 = 1;
		}

		/*
		 * THE TYPE FLAG DOES.  Same state, the other type, and the
		 * register must move -- 0x12..0x43 against 0x12..0xa9 is a
		 * different sequence through the same generator.
		 */
		memcpy(mp_b, keep, MP_SLOT);
		MPB->type = (char)(t ? 0 : 1);
		ref_mp_calccrc(mp_b);
		if (memcmp(r1, MPB->crc, V90MP_CRC) != 0)
			type_split = 1;

		/*
		 * THE REGISTER IS CARRIED IN.  Same bits, a different
		 * incoming register, and the answer must change; if it did
		 * not, the member would be seeding and `resetCRC` would be
		 * dead code.
		 */
		memcpy(mp_b, keep, MP_SLOT);
		MPB->crc[0] = (unsigned char)(MPB->crc[0] ^ 1);
		MPB->crc[7] = (unsigned char)(MPB->crc[7] ^ 1);
		ref_mp_calccrc(mp_b);
		if (memcmp(r1, MPB->crc, V90MP_CRC) != 0)
			carried = 1;

		/* ------------------------------- the differential trial */
		memcpy(mp_b, keep, MP_SLOT);

		MPA->calcCRC();
		ref_mp_calccrc(mp_b);

		diff_eq_obj("after calcCRC", V90MP, MPA, MPB, tag);
		guard_intact(tag);

		if (memcmp(keep, mp_b, sizeof(V90MP)) != 0)
			moved = 1;

		/*
		 * The extent is never empty -- 0x12 is below both 0x44 and
		 * 0xaa -- so the register is written on EVERY trial, and the
		 * guard at 0x1f194 is unreachable.  Say so as a check rather
		 * than as a comment.
		 */
		diff_eq_int("the register was written (%ld)",
			    memcmp(in, MPB->crc, V90MP_CRC) != 0 ||
			    memcmp(in, r1, V90MP_CRC) != 0, 1, tag);
	}

	diff_eq_int("calcCRC changed the object", moved, 1, 0);
	diff_eq_int("the incoming register reached the answer", carried, 1, 0);
	diff_eq_int("a live +0x119 sweep proved it unread", ignored_119, 1, 0);
	diff_eq_int("the type flag moved the extent", type_split, 1, 0);
	return diff_end();
}

/*
 * A message the BLOB built, with the BLOB's own CRC in it, so that
 * `evaluateCRC` is asked about a vector our source never touched.  Returns
 * the length byte the message wants at +0x119.
 *
 * `infoToBits` cannot be used for the short form: finding F1386's reproduced
 * defect pads from 0x45, which is where the type-zero CRC has just gone, so
 * a type-zero message always fails its own CRC.  This builds both forms the
 * other way round -- seed the register the way `resetCRC` does, let the
 * blob's `calcCRC` run, and lay the sixteen result bits where Table 16 puts
 * them -- which is the only route to a passing SHORT message.
 */
static unsigned char
blob_good_message(int type1)
{
	unsigned int end = type1 ? 0xaau : 0x44u;
	unsigned int i;

	for (i = 0; i < V90MP_BITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		MPB->bits[i] = (unsigned char)(lfsr & 1u);
	}
	for (i = 0; i <= 0x10; i++)
		MPB->bits[i] = 1;
	for (i = 0x11; i <= 0xaa; i += 0x11)
		MPB->bits[i] = 0;
	MPB->bits[0x12] = (unsigned char)(type1 ? 1 : 0);
	MPB->type = (char)(type1 ? 1 : 0);

	for (i = 0; i < V90MP_CRC; i++)
		MPB->crc[i] = 1;
	ref_mp_calccrc(mp_b);
	for (i = 0; i < V90MP_CRC; i++)
		MPB->bits[end + 1 + i] = MPB->crc[i];

	return (unsigned char)(end + 0x11);
}

/*
 * A message whose CRC really is where +0x119 says and is NOT where the type
 * flag would put it.
 *
 * THIS IS THE ONLY SHAPE THAT CAN SEPARATE `bits[bodyLength - 0x10 + k]` FROM
 * `bits[end + 1 + k]`, and the mutation set proves it: with only the trials
 * above, "evaluateCRC finds the peer's CRC from the type flag" survived.  The
 * two expressions name the same sixteen positions for every +0x119 the class
 * writes -- 0xbb - 0x10 is 0xab and 0xaa + 1 is 0xab -- and on a message that
 * fails its CRC both readings answer 0, so an inconsistent pair alone is not
 * enough either.  What is needed is a message that PASSES under one reading
 * and fails under the other.
 *
 * Type 0, so the register covers 0x12..0x43 and the sixteen positions at
 * 0xab are outside it: moving the CRC up there does not change what the CRC
 * is computed over.  The positions the type flag would have named, 0x45:0x54,
 * are filled with the complement, so the wrong reading cannot agree by luck.
 */
static unsigned char
blob_displaced_message(void)
{
	unsigned int i;

	for (i = 0; i < V90MP_BITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		MPB->bits[i] = (unsigned char)(lfsr & 1u);
	}
	for (i = 0; i <= 0x10; i++)
		MPB->bits[i] = 1;
	for (i = 0x11; i <= 0xaa; i += 0x11)
		MPB->bits[i] = 0;
	MPB->bits[0x12] = 0;
	MPB->type = 0;

	for (i = 0; i < V90MP_CRC; i++)
		MPB->crc[i] = 1;
	ref_mp_calccrc(mp_b);
	for (i = 0; i < V90MP_CRC; i++) {
		MPB->bits[0xab + i] = MPB->crc[i];
		MPB->bits[0x45 + i] = (unsigned char)(MPB->crc[i] == 0);
	}
	return 0xbb;
}

static int
run_mp_evaluatecrc(void)
{
	unsigned char keep[MP_SLOT];
	unsigned char alt_reg[V90MP_CRC];
	int trial;
	int moved = 0, seeded = 0, crc_split = 0, displaced = 0;
	int said[2];

	said[0] = said[1] = 0;

	diff_begin("V90MP::evaluateCRC");
	set_level(0);

	for (trial = 0; trial < 72; trial++) {
		long tag = 3500 + trial;
		int t = trial & 1;
		int good = (trial % 3) == 0;
		int ours, theirs, other;

		seed_pair(trial + 600, trial % 4);
		fill_bits(trial & 1);
		fill_crc(trial % 4, 0);
		MPA->type = MPB->type = (char)(t ? 1 : 0);
		MPA->bodyLength = MPB->bodyLength = crc_lens[trial % CRC_NLENS];

		if (good) {
			/*
			 * A message that must be ACCEPTED, and its length
			 * byte taken from Table 16 rather than from the
			 * sweep.  Built in the blob's object and copied
			 * across, so both sides see bits nothing of ours
			 * wrote.
			 */
			MPB->bodyLength = blob_good_message(t);
			memcpy(mp_a, mp_b, sizeof(V90MP));
			if (trial % 6 == 3) {
				/* ... unless one information bit is bent. */
				MPA->bits[0x20] = MPB->bits[0x20] =
				    (unsigned char)(MPB->bits[0x20] == 0);
				good = 0;
			}
		} else if (trial % 6 == 1) {
			/* The CRC where +0x119 says and nowhere else. */
			MPB->bodyLength = blob_displaced_message();
			memcpy(mp_a, mp_b, sizeof(V90MP));
			good = 1;
			displaced = 1;
		}
		memcpy(keep, mp_b, MP_SLOT);

		/*
		 * IT SEEDS, unlike `calcCRC`: a different incoming register
		 * must not change the answer.  Runs first, on a copy.
		 */
		MPB->crc[0] = (unsigned char)(MPB->crc[0] ^ 3);
		MPB->crc[9] = (unsigned char)(MPB->crc[9] ^ 5);
		other = ref_mp_evalcrc(mp_b);
		memcpy(alt_reg, MPB->crc, V90MP_CRC);
		memcpy(mp_b, keep, MP_SLOT);
		theirs = ref_mp_evalcrc(mp_b);
		diff_eq_int("the incoming register did not reach the "
			    "answer (%ld)", (long)other, (long)theirs, tag);
		diff_eq_int("nor the register it left behind (%ld)",
			    memcmp(alt_reg, MPB->crc, V90MP_CRC) == 0, 1, tag);

		/*
		 * AND THE MARKER HAS TO BE ABLE TO FAIL.  `seeded = 1` here
		 * unconditionally would be true whatever the member did, which
		 * is the dead-detector shape CLAUDE.md names.  What is
		 * recorded instead is that the call OVERWROTE the register it
		 * was handed on some trial -- if `evaluateCRC` left `crc`
		 * alone, the two checks above would still pass and this would
		 * stay 0.
		 */
		if (memcmp(keep + 0x102, MPB->crc, V90MP_CRC) != 0)
			seeded = 1;

		/*
		 * THE TWO FIELDS ARE READ SEPARATELY.  Flip the type flag
		 * alone and the extent moves without the compared positions
		 * moving; move +0x119 alone and the compared positions move
		 * without the extent.  Either parting from the run above is
		 * what the CP transcription cannot do.
		 */
		{
			unsigned char alt[V90MP_CRC];

			memcpy(alt, MPB->crc, V90MP_CRC);
			memcpy(mp_b, keep, MP_SLOT);
			MPB->type = (char)(t ? 0 : 1);
			ref_mp_evalcrc(mp_b);
			if (memcmp(alt, MPB->crc, V90MP_CRC) != 0)
				crc_split = 1;
			memcpy(mp_b, keep, MP_SLOT);
		}

		/* ------------------------------- the differential trial */
		memcpy(mp_a, keep, sizeof(V90MP));
		memcpy(mp_b, keep, MP_SLOT);

		ours = MPA->evaluateCRC();
		theirs = ref_mp_evalcrc(mp_b);

		diff_eq_int("evaluateCRC returned (%ld)", (long)ours,
			    (long)theirs, tag);
		diff_eq_obj("after evaluateCRC", V90MP, MPA, MPB, tag);
		guard_intact(tag);

		if (memcmp(keep, mp_b, sizeof(V90MP)) != 0)
			moved = 1;
		if (theirs == 0 || theirs == 1)
			said[theirs] = 1;
		if (good)
			diff_eq_int("a message carrying the blob's own CRC "
				    "was accepted (%ld)", (long)theirs, 1,
				    tag);
	}

	diff_eq_int("evaluateCRC changed the object", moved, 1, 0);
	diff_eq_int("the seed was shown to override the incoming register",
		    seeded, 1, 0);
	diff_eq_int("the extent and the compared positions were separated",
		    crc_split, 1, 0);
	diff_eq_int("a message whose CRC is not where the extent implies was "
		    "accepted", displaced, 1, 0);
	diff_eq_int("the blob answered 0 at least once", said[0], 1, 0);
	diff_eq_int("the blob answered 1 at least once", said[1], 1, 0);
	return diff_end();
}

/* ------------------------------------------ the CRC against 10.1.2.3.2/V.34 */

/*
 * WHY THIS BLOCK IS NOT THE TWO ABOVE.
 *
 * Everything else in this file asks "does our source do what the blob does".
 * That question cannot answer "does the blob do what the Recommendation
 * says", and a defect living in the gap between the two would pass every
 * differential check ever written here.  So this block asks the second
 * question and never consults the blob for an expected value: the expected
 * values come from ITU-T V.34 (02/98) 10.1.2.3.2 and Figure 14, and from
 * Table 16/V.90.  It judges OUR source and the blob against the same
 * spec-derived numbers, so it fires whichever of the two is wrong.
 *
 * WHAT V.90 SAYS ABOUT THE GENERATOR: nothing of its own.  8.6.3 says only
 * "The CRC generator used is described in 10.1.2.3.2/V.34", and every other
 * MP, CP and CPt clause says the same, so V.34 is the normative text.
 *
 * WHAT 10.1.2.3.2/V.34 SAYS, clause by clause:
 *
 *   "The CRC is formed by passing all of the information bits in a sequence,
 *    except the frame sync bits, the start bits, and the fill bits, through
 *    the CRC generator described in Figure 14."
 *   "The polynomial used to compute the CRC is: x16 + x12 + x5 + 1."
 *   "1) load the shift register in the CRC generator with all ones;"
 *   "2) shift in the binary sequence;"
 *   "3) output the contents of the shift register, starting with bit 0 in
 *    Figure 14.  Bit 0 of the CRC is the LSB."
 *
 * NEITHER RECOMMENDATION CARRIES A TEST VECTOR.  V.34 and V.90 were both
 * searched for a worked CRC example -- no numeric result, no sample sequence,
 * nothing in the V.34 Annex A precode-CRC clause either -- so the
 * known-answer had to come from outside, and this comment says so rather
 * than leaving a reader to assume the number is ITU's.
 *
 * The variant is derived from the figure, not guessed.  Figure 14 draws
 * sixteen stages numbered 15..0 with "Information Bits In" entering at the
 * bit-0 end and its two adders between stages 11/10 and 4/3, which is the
 * REFLECTED form of x^16 + x^12 + x^5 + 1: 0x1021 reversed is 0x8408, whose
 * set bits are 15, 10 and 3.  With the all-ones preload of clause 1 and no
 * final inversion (clause 3 outputs the register itself) that is the
 * catalogued CRC-16/MCRF4XX, whose published check value over the ASCII
 * string "123456789" shifted in LSB first is 0x6F91.  `spec_crc16` is
 * asserted against that number before it is used to judge anything, so the
 * reference implementation is pinned to something outside this tree.
 *
 * AND IT HAS BEEN SHOWN TO FIRE, which finding F134 requires of anything that
 * can report a clean tree.  Changing clause 1's preload from 0xffff to 0
 * inside `spec_crc16` failed 337 of this block's 1,084 checks while every
 * differential block above stayed green -- so the block really is comparing
 * the two sides against the Recommendation and not against each other.
 * Restored, and green again, before the commit.
 */

/* Figure 14/V.34, written from the Recommendation and not from the object. */
static unsigned int
spec_crc16(const unsigned char *bit, unsigned int n)
{
	unsigned int reg = 0xffffu;		/* clause 1: all ones */
	unsigned int k;

	for (k = 0; k < n; k++) {		/* clause 2: shift it in */
		unsigned int fb = (reg ^ (unsigned int)bit[k]) & 1u;

		reg >>= 1;
		if (fb)
			reg ^= 0x8408u;
	}
	return reg & 0xffffu;
}

/*
 * Table 16/V.90's start bits, copied off the table rather than computed.
 * They happen to be the multiples of seventeen, which is what the object's
 * `i % 17` skip exploits, but writing them out is the point: a test that
 * derived them the same way the code does could not catch the code deriving
 * them wrongly.
 */
static const unsigned char spec_start_bits[] = {
	17, 34, 51, 68, 85, 102, 119, 136, 153, 170
};
#define SPEC_NSTART	((int)(sizeof(spec_start_bits) / \
			       sizeof(spec_start_bits[0])))

static int
spec_is_start_bit(unsigned int i)
{
	int k;

	for (k = 0; k < SPEC_NSTART; k++)
		if ((unsigned int)spec_start_bits[k] == i)
			return 1;
	return 0;
}

/* Where Table 16 puts the CRC: 171:186 for Type 1, 69:84 for Type 0. */
static unsigned int
spec_crc_at(int type1)
{
	return type1 ? 171u : 69u;
}

/*
 * The information bits of a Table 16 sequence, gathered the way the clause
 * words it: everything from the MP Type bit at 18 up to the CRC field, less
 * the start bits.  The frame sync (0:16) is excluded by starting at 18 and
 * the fill bits are excluded by stopping below the CRC.
 */
static unsigned int
spec_info_bits(const unsigned char *b, int type1, unsigned char *out)
{
	unsigned int stop = spec_crc_at(type1) - 1u;	/* the last start bit */
	unsigned int i, n = 0;

	for (i = 18; i < stop; i++) {
		if (spec_is_start_bit(i))
			continue;
		out[n++] = (unsigned char)(b[i] & 1u);
	}
	return n;
}

/* A Table 16 sequence, laid out from the table.  NOT `infoToBits`, whose
 * reproduced defects (finding F1386) are exactly what a spec test must not
 * inherit. */
static void
spec_layout(unsigned char *b, int type1, unsigned seed)
{
	unsigned int i;

	lfsr = 0x2f19u + 0x9e37u * seed;
	for (i = 0; i < V90MP_BITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		b[i] = (unsigned char)((lfsr >> 5) & 1u);
	}
	for (i = 0; i <= 16; i++)			/* frame sync   */
		b[i] = 1;
	for (i = 0; i < (unsigned)SPEC_NSTART; i++)	/* start bits   */
		b[spec_start_bits[i]] = 0;
	b[18] = (unsigned char)(type1 ? 1 : 0);		/* MP Type bit  */
}

/* Seed the register the way clause 1 and `resetCRC` both do. */
static void
spec_seed(V90MP *o)
{
	int k;

	for (k = 0; k < V90MP_CRC; k++)
		o->crc[k] = 1;
}

static int
run_mp_crc_spec(void)
{
	unsigned char info[V90MP_BITS];
	unsigned char base[V90MP_CRC];
	int trial;
	int order_split = 0;

	diff_begin("V90MP CRC against 10.1.2.3.2/V.34");
	set_level(0);

	/* The reference implementation, pinned to a published number. */
	{
		static const char probe[] = "123456789";
		unsigned char b[72];
		unsigned int n = 0;
		unsigned j, k;

		for (j = 0; j < 9; j++)
			for (k = 0; k < 8; k++)
				b[n++] = (unsigned char)
				    (((unsigned)(unsigned char)probe[j] >> k)
				     & 1u);
		diff_eq_int("Figure 14/V.34 over \"123456789\" is the "
			    "published 0x6f91 (%ld)",
			    (long)spec_crc16(b, n), 0x6f91L, 0);
	}

	/* Table 16's two message lengths are the object's two constants. */
	diff_eq_int("Type 1: CRC ends at 186, so +0x119 is 0xbb (%ld)",
		    (long)(spec_crc_at(1) + 16u), 0xbbL, 0);
	diff_eq_int("Type 0: CRC ends at 84, so +0x119 is 0x55 (%ld)",
		    (long)(spec_crc_at(0) + 16u), 0x55L, 0);

	for (trial = 0; trial < 24; trial++) {
		long tag = 3600 + trial;
		int t1 = trial & 1;
		unsigned int crc_at = spec_crc_at(t1);
		unsigned int n, reg, k;

		seed_pair(trial + 900, trial % 3);
		spec_layout(MPB->bits, t1, (unsigned)trial);
		memcpy(MPA->bits, MPB->bits, V90MP_BITS);
		MPA->type = MPB->type = (char)(t1 ? 1 : 0);
		MPA->bodyLength = MPB->bodyLength =
		    (unsigned char)(crc_at + 16u);
		spec_seed(MPA);
		spec_seed(MPB);

		MPA->calcCRC();
		ref_mp_calccrc(mp_b);

		n = spec_info_bits(MPB->bits, t1, info);
		reg = spec_crc16(info, n);
		memcpy(base, MPB->crc, V90MP_CRC);

		/*
		 * Nine sixteen-bit groups for Type 1 and three for Type 0 --
		 * the count Table 16 implies, checked so that a layout bug in
		 * this test cannot quietly shrink what is being covered.
		 */
		diff_eq_int("information bits per Table 16 (%ld)", (long)n,
			    t1 ? 144L : 48L, tag);

		for (k = 0; k < 16u; k++) {
			long want = (long)((reg >> k) & 1u);

			diff_eq_int("ours: register bit against Figure 14 "
				    "(%ld)", (long)MPA->crc[k], want,
				    tag * 100 + (long)k);
			diff_eq_int("blob: register bit against Figure 14 "
				    "(%ld)", (long)MPB->crc[k], want,
				    tag * 100 + (long)k);
		}

		/*
		 * "except the frame sync bits, the start bits, and the fill
		 * bits".  Flip one of each -- and one bit of the CRC field
		 * itself, which is not an information bit either -- and the
		 * register must not move.  Flip an information bit and it
		 * must: a single-bit change inside one CRC period always
		 * changes the remainder.
		 */
		{
			unsigned int excluded[4];
			unsigned int included[3];
			unsigned int e, m;

			excluded[0] = 5u;		/* frame sync   */
			excluded[1] = 34u;		/* start bit    */
			excluded[2] = crc_at + 3u;	/* the CRC      */
			excluded[3] = crc_at + 18u;	/* a fill bit   */

			included[0] = 18u;		/* MP Type bit  */
			included[1] = 24u;		/* drn          */
			included[2] = crc_at - 3u;	/* last group   */

			for (e = 0; e < 4u; e++) {
				m = excluded[e];
				MPB->bits[m] = (unsigned char)
				    (MPB->bits[m] == 0);
				spec_seed(MPB);
				ref_mp_calccrc(mp_b);
				diff_eq_int("an excluded bit did not reach "
					    "the CRC (%ld)",
					    memcmp(base, MPB->crc,
						   V90MP_CRC) == 0, 1,
					    tag * 100 + (long)m);
				MPB->bits[m] = (unsigned char)
				    (MPB->bits[m] == 0);
			}
			for (e = 0; e < 3u; e++) {
				m = included[e];
				MPB->bits[m] = (unsigned char)
				    (MPB->bits[m] == 0);
				spec_seed(MPB);
				ref_mp_calccrc(mp_b);
				diff_eq_int("an information bit did reach "
					    "the CRC (%ld)",
					    memcmp(base, MPB->crc,
						   V90MP_CRC) != 0, 1,
					    tag * 100 + (long)m);
				MPB->bits[m] = (unsigned char)
				    (MPB->bits[m] == 0);
			}
			spec_seed(MPB);
			ref_mp_calccrc(mp_b);
			diff_eq_int("the vector was restored (%ld)",
				    memcmp(base, MPB->crc, V90MP_CRC) == 0,
				    1, tag);
		}

		/*
		 * Clause 3, and it is D920's question asked of this class:
		 * the register is output "starting with bit 0", and Table 16
		 * says "Bit 0 is transmitted first", so position crc_at + k
		 * carries register bit k.  Lay the SPEC's register -- not the
		 * blob's -- that way and both sides must accept; lay it the
		 * other way round and both must reject.
		 */
		for (k = 0; k < 16u; k++)
			MPB->bits[crc_at + k] =
			    (unsigned char)((reg >> k) & 1u);
		memcpy(MPA->bits, MPB->bits, V90MP_BITS);
		diff_eq_int("ours accepts the spec's own CRC (%ld)",
			    (long)MPA->evaluateCRC(), 1L, tag);
		diff_eq_int("the blob accepts the spec's own CRC (%ld)",
			    (long)ref_mp_evalcrc(mp_b), 1L, tag);

		for (k = 0; k < 16u; k++)
			MPB->bits[crc_at + k] =
			    (unsigned char)((reg >> (15u - k)) & 1u);
		memcpy(MPA->bits, MPB->bits, V90MP_BITS);
		{
			/*
			 * Only meaningful where the register is not its own
			 * reverse -- a palindrome reads the same either way
			 * and proves nothing.  `order_split` records that
			 * some trial really did put the two orders apart.
			 */
			int rev_differs = 0;

			for (k = 0; k < 16u; k++)
				if (((reg >> k) & 1u) !=
				    ((reg >> (15u - k)) & 1u))
					rev_differs = 1;
			if (rev_differs) {
				order_split = 1;
				diff_eq_int("ours rejects the reversed CRC "
					    "(%ld)", (long)MPA->evaluateCRC(),
					    0L, tag);
				diff_eq_int("the blob rejects the reversed "
					    "CRC (%ld)",
					    (long)ref_mp_evalcrc(mp_b), 0L,
					    tag);
			}
		}
	}

	diff_eq_int("a trial separated the two output bit orders",
		    order_split, 1, 0);
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
	rc |= run_mp_calccrc();
	rc |= run_mp_evaluatecrc();
	rc |= run_mp_crc_spec();

	set_level(0);
	dsplib_debug_capture_on = 0;
	return rc;
}
