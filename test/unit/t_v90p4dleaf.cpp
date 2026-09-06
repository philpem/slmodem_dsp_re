/*
 * t_v90p4dleaf.cpp -- differential test of the seven V90Phase4Demodulator
 * members that are leaves: `enterWaitForCP`, `enterWaitForMP`,
 * `enterWaitForEd`, `resetRRNDetector`, `detectRRN`, `resetBeforRRN` and
 * `detectFPE`.
 *
 * NOTHING HERE ALLOCATES.  The two 13,596-byte objects are seeded byte for
 * byte identically and compared whole, guard included, and the ONE pointer
 * anything under test dereferences -- `params`, which `resetRRNDetector`
 * reads +0x298 of -- is re-installed to the SAME block on both sides, so it
 * compares equal like every other word.
 *
 * WHAT AN OBJECT COMPARISON CANNOT SEE, AND WHY HALF THIS FILE IS
 * TRANSCRIPTS.  The three `enterWaitFor*` bodies differ from one another in
 * exactly two tokens: the state constant, which lands in +0x20 and is
 * observable, and the `edprintf` format string, which lands nowhere.  A pair
 * with their strings swapped writes the same bytes into both objects and
 * passes every whole-object check ever written.  So each of them is also run
 * with the capture on, and the run requires:
 *
 *   - our transcript to equal the blob's, trial by trial;
 *   - the blob's transcript for `enterWaitForCP` to DIFFER from its
 *     transcript for `enterWaitForMP` and for `enterWaitForEd` at the same
 *     `countInState` -- which is the swapped-string mutation, and nothing
 *     else in this file can fail on it;
 *   - the blob's transcript for one `countInState` to DIFFER from its
 *     transcript for another -- which is what says the "@ %d" argument
 *     reaches the output at all, so "print a constant" cannot survive.
 *
 * Those two comparisons are between two runs of the BLOB, so they are
 * properties of the object and not of this reconstruction.  They are legible
 * because `edprintf` reseeds its rotating key on every call (encode.h), so
 * one message encodes to the same characters wherever it appears in a run.
 * The capture stays ENCODED: `dsplib_encode_plain` is ours alone (D40) and
 * turning it on would make our transcript readable and the blob's not.
 *
 * THE POLARITY THE TWO DETECTORS PRINT IS NOT THE SAME FIELD, and that is
 * the blob's, not a slip here.  `detectFPE` detects on `rDetector2` at
 * +0x3028 and prints `rDetector1.polarity` at +0x3020 (finding F4320).  A
 * fixture that seeded the two detectors alike could not tell that from the
 * obvious reading, so every trial gives them different polarities and every
 * `detectFPE` trial checks the transcript.
 *
 * THE DETECTORS ARE SEEDED, NOT DRIVEN.  `V90RDetector::detectR` answers 0
 * until a sample completes a group of six and `detectRf` until one completes
 * a group of twelve, so reaching the interesting arm through the front door
 * costs six or twelve calls and lands on whatever the run counters happen to
 * hold.  Seeding +0x00, +0x20, +0x04/+0x0c and +0x14/+0x18 directly puts the
 * detector one sample from each outcome, and the arms are counted so that a
 * run in which one of them never happened FAILS rather than passing quietly.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/*
 * `void *` where the member takes a `this`: the same thing to the ABI, and
 * this side has to name a C type because the blob's symbol has no class to be
 * a member of.
 */
void ref_p4d_resetrrndetector(void *)
	asm("ref__ZN20V90Phase4Demodulator16resetRRNDetectorEv");
void ref_p4d_resetbeforrrn(void *)
	asm("ref__ZN20V90Phase4Demodulator13resetBeforRRNEv");
void ref_p4d_enterwaitforcp(void *)
	asm("ref__ZN20V90Phase4Demodulator14enterWaitForCPEv");
void ref_p4d_enterwaitformp(void *)
	asm("ref__ZN20V90Phase4Demodulator14enterWaitForMPEv");
void ref_p4d_enterwaitfored(void *)
	asm("ref__ZN20V90Phase4Demodulator14enterWaitForEdEv");
int ref_p4d_detectrrn(void *, short)
	asm("ref__ZN20V90Phase4Demodulator9detectRRNEs");
int ref_p4d_detectfpe(void *, short)
	asm("ref__ZN20V90Phase4Demodulator9detectFPEEs");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0x5a;
	case 2:
		return 0xff;
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ----------------------------------------------------------- the storage */

#define P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + 64u)

static unsigned char p4d_a[P4D_SLOT] __attribute__((aligned(8)));
static unsigned char p4d_b[P4D_SLOT] __attribute__((aligned(8)));
static unsigned char p4d_s[P4D_SLOT];		/* the seed, for the guard  */
static unsigned char before_a[P4D_SLOT];
static unsigned char before_b[P4D_SLOT];

#define P4DA	((V90Phase4Demodulator *)p4d_a)
#define P4DB	((V90Phase4Demodulator *)p4d_b)

/*
 * ONE parameter block, shared by both sides, so `params` holds the same
 * address on both and compares equal.  Nothing under test writes through it.
 */
static unsigned char par_blk[sizeof(V90Parameters)] __attribute__((aligned(8)));

#define PAR	((V90Parameters *)par_blk)

/*
 * The same varied bytes into both sides.  Never zeros -- finding F230 -- and
 * the only pointer re-installed afterwards is `params`, because it is the
 * only one anything here dereferences.
 */
static void
seed_pair(int trial, int mode)
{
	unsigned i;

	lfsr = 0x37c1u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < P4D_SLOT; i++) {
		unsigned char v = next_byte(mode);

		p4d_a[i] = v;
		p4d_b[i] = v;
		p4d_s[i] = v;
	}
	for (i = 0; i < sizeof(par_blk); i++)
		par_blk[i] = next_byte(mode);

	P4DA->params = P4DB->params = PAR;
}

/* Compare the whole object and the guard past it, on both sides. */
static void
compare_pair(const char *what, long tag)
{
	unsigned n = P4D_SLOT - (unsigned)sizeof(V90Phase4Demodulator);

	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Demodulator",
		     p4d_a, p4d_b, sizeof(V90Phase4Demodulator), tag);
	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(p4d_a + sizeof(V90Phase4Demodulator),
			   p4d_s + sizeof(V90Phase4Demodulator), n) == 0,
		    1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(p4d_b + sizeof(V90Phase4Demodulator),
			   p4d_s + sizeof(V90Phase4Demodulator), n) == 0,
		    1, tag);
}

/*
 * Put `rDetector1` one sample from an R decision.  `want` picks the arm:
 *   1   the positive-first pattern 0x38 completes AND the run reaches +0x04
 *   -1  the negative-first pattern 0x07 completes and the run reaches +0x04
 *   0   a group completes on 0x38 but the run is short, so it answers 0
 *  -2   the group is not complete at all, the earliest return there is
 * Returns the sample to feed.
 */
static short
arm_r(V90RDetector *d, int want, int limit)
{
	d->rLimit = limit;
	d->positiveRunLength = 0;
	d->negativeRunLength = 0;
	d->polarity = 0x33;

	switch (want) {
	case 1:
		d->sampleCount = 5;
		d->signBits = 0x1c;		/* 0x1c * 2     = 0x38 */
		d->positiveRunLength = limit - 6;
		return -300;			/* not > 0: bit 0 stays 0 */
	case -1:
		d->sampleCount = 5;
		d->signBits = 0x03;		/* 0x03 * 2 | 1 = 0x07 */
		d->negativeRunLength = limit - 6;
		return 300;
	case 0:
		d->sampleCount = 5;
		d->signBits = 0x1c;
		d->positiveRunLength = limit - 60;
		return -300;
	default:
		d->sampleCount = 2;
		d->signBits = 0x1c;
		return 300;
	}
}

/* The same for `rDetector2` and `detectRf`: twelve to the group, +0x0c. */
static short
arm_rf(V90RDetector *d, int want, int limit)
{
	d->rfLimit = limit;
	d->positiveRunLength = 0;
	d->negativeRunLength = 0;
	d->polarity = 0x44;

	switch (want) {
	case 1:
		d->sampleCount = 11;
		d->signBits = 0x666;		/* 0x666 * 2     = 0xccc */
		d->positiveRunLength = limit - 12;
		return -300;
	case -1:
		d->sampleCount = 11;
		d->signBits = 0x199;		/* 0x199 * 2 | 1 = 0x333 */
		d->negativeRunLength = limit - 12;
		return 300;
	case 0:
		d->sampleCount = 11;
		d->signBits = 0x666;
		d->positiveRunLength = limit - 120;
		return -300;
	default:
		d->sampleCount = 4;
		d->signBits = 0x666;
		return 300;
	}
}

/* --------------------------------------------- resetRRNDetector (81 bytes) */

static int
run_p4d_resetrrndetector(void)
{
	static const int lens[] = { 0, 1, 6, 7, 12, 0x18, 0x30, 0x2ff, 0xb4 };
	int trial, varied = 0, split = 0;
	int first = 0;

	diff_begin("V90Phase4Demodulator::resetRRNDetector");
	set_level(0);

	for (trial = 0; trial < 27; trial++) {
		long tag = 3000 + trial;
		int want = lens[(unsigned)trial % (sizeof(lens) /
						   sizeof(lens[0]))];

		seed_pair(trial + 100, trial % 3);

		/*
		 * The NEIGHBOURS are set too, and to different values, so
		 * "read +0x294" or "read +0x29c" cannot pass by accident.
		 */
		PAR->PHASE4_R_DETECTION_LENGTH = 0x7ac0 + trial;
		PAR->RRN_R_DETECTION_LENGTH = want;
		PAR->SD_DETECTOR_DETECTION_COUNTER_THRESHOLD = 0x1b30 + trial;

		P4DA->resetRRNDetector();
		ref_p4d_resetrrndetector(p4d_b);

		compare_pair("after resetRRNDetector", tag);

		/*
		 * The two detectors are reset from DIFFERENT arguments -- one
		 * from the parameter block and one from the literals 0xb4 and
		 * 0xc -- so a body that reset them alike, or that swapped
		 * them, lands here as well as in the object comparison.
		 */
		if (trial == 0)
			first = P4DB->rDetector1.rLimit;
		else if (P4DB->rDetector1.rLimit != first)
			varied = 1;
		if (P4DB->rDetector1.rLimit != P4DB->rDetector2.rLimit)
			split = 1;

		diff_eq_int("the second detector ignores params (%ld)",
			    (long)P4DB->rDetector2.rLimit, 0xb4, tag);
	}

	diff_eq_int("the first detector followed the parameter", varied, 1, 0);
	diff_eq_int("the two detectors were told apart", split, 1, 0);
	return diff_end();
}

/* ----------------------------------------------- resetBeforRRN (37 bytes) */

static int
run_p4d_resetbeforrrn(void)
{
	int trial, moved = 0;

	diff_begin("V90Phase4Demodulator::resetBeforRRN");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		long tag = 3200 + trial;

		seed_pair(trial + 200, trial % 3);

		/*
		 * The five it writes, seeded to values it does not write, and
		 * the six neighbours `reset` ALSO writes seeded the same way.
		 * That pair is what says this is not `reset`: +0x24, +0x28 and
		 * +0x40 must still hold their seed afterwards.
		 */
		P4DA->int_0038 = P4DB->int_0038 = 0x7a110000 + trial;
		P4DA->int_003c = P4DB->int_003c = 0x7a220000 + trial;
		P4DA->int_0044 = P4DB->int_0044 = 0x7a330000 + trial;
		P4DA->int_0048 = P4DB->int_0048 = 0x7a440000 + trial;
		P4DA->uchar_0030 = P4DB->uchar_0030 =
		    (unsigned char)(0x41 + trial);
		P4DA->countInState = P4DB->countInState = 0x7a550000 + trial;
		P4DA->int_0028 = P4DB->int_0028 = 0x7a660000 + trial;
		P4DA->int_0040 = P4DB->int_0040 = 0x7a770000 + trial;
		P4DA->quickConnect = P4DB->quickConnect = 0x7a880000u + trial;
		P4DA->trn2dDDLength = P4DB->trn2dDDLength = 0x7a990000u + trial;
		P4DA->state = P4DB->state = P4D_STATE_WAIT_FOR_MP;

		memcpy(before_b, p4d_b, P4D_SLOT);

		P4DA->resetBeforRRN();
		ref_p4d_resetbeforrrn(p4d_b);

		compare_pair("after resetBeforRRN", tag);

		diff_eq_int("+0x38 is 1 (%ld)", (long)P4DB->int_0038, 1, tag);
		diff_eq_int("+0x3c is 1 (%ld)", (long)P4DB->int_003c, 1, tag);
		diff_eq_int("+0x44 is 0 (%ld)", (long)P4DB->int_0044, 0, tag);
		diff_eq_int("+0x48 is 0 (%ld)", (long)P4DB->int_0048, 0, tag);
		diff_eq_int("+0x30 is 0 (%ld)", (long)P4DB->uchar_0030, 0,
			    tag);

		/* It is NOT `reset`: these five are `reset`'s and not its. */
		diff_eq_int("countInState kept its seed (%ld)",
			    (long)P4DB->countInState, 0x7a550000 + trial, tag);
		diff_eq_int("+0x28 kept its seed (%ld)", (long)P4DB->int_0028,
			    0x7a660000 + trial, tag);
		diff_eq_int("+0x40 kept its seed (%ld)", (long)P4DB->int_0040,
			    0x7a770000 + trial, tag);
		diff_eq_int("+0x34 kept its seed (%ld)",
			    (long)P4DB->quickConnect, (long)(0x7a880000u + trial),
			    tag);
		diff_eq_int("the state was not touched (%ld)",
			    (long)P4DB->state, (long)P4D_STATE_WAIT_FOR_MP,
			    tag);

		if (memcmp(before_b, p4d_b, P4D_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("the object moved at all", moved, 1, 0);
	return diff_end();
}

/* -------------------------------------------- the three state entries ---- */

/*
 * One driver for all three, because the whole point is that the three bodies
 * are the same shape.  `which` selects the member, `want` the state constant
 * it has to leave behind.  The blob's transcript for the trial is copied out
 * so the caller can compare one member against another.
 */
static void
enter_once(int which, int trial, int count, long tag, char *blobtext,
	   unsigned textmax)
{
	seed_pair(trial + 300 + which * 40, trial % 3);

	P4DA->countInState = P4DB->countInState = count;
	P4DA->state = P4DB->state = P4D_STATE_RD_DETECTED;
	P4DA->int_0028 = P4DB->int_0028 = 0x6b000000 + trial;
	P4DA->int_0038 = P4DB->int_0038 = 0x6b110000 + trial;

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;

	switch (which) {
	case 0:
		P4DA->enterWaitForCP();
		ref_p4d_enterwaitforcp(p4d_b);
		break;
	case 1:
		P4DA->enterWaitForMP();
		ref_p4d_enterwaitformp(p4d_b);
		break;
	default:
		P4DA->enterWaitForEd();
		ref_p4d_enterwaitfored(p4d_b);
		break;
	}

	dsplib_debug_capture_on = 0;

	diff_eq_int("the transcripts match (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("the line counts match (%ld)",
		    (int)dsplib_debug_capture_lines(0),
		    (int)dsplib_debug_capture_lines(1), tag);

	if (blobtext != 0) {
		unsigned n = (unsigned)strlen(dsplib_debug_capture_text(1));

		if (n >= textmax)
			n = textmax - 1;
		memcpy(blobtext, dsplib_debug_capture_text(1), n);
		blobtext[n] = '\0';
	}
}

#define ENTER_TEXT	512

static int
run_p4d_enter(void)
{
	static const char *names[3] = {
		"V90Phase4Demodulator::enterWaitForCP",
		"V90Phase4Demodulator::enterWaitForMP",
		"V90Phase4Demodulator::enterWaitForEd"
	};
	static const int states[3] = {
		P4D_STATE_WAIT_FOR_V90CP, P4D_STATE_WAIT_FOR_MP,
		P4D_STATE_WAIT_FOR_ED
	};
	static char text_at_7[3][ENTER_TEXT];
	static char text_at_9999[ENTER_TEXT];
	int which, rc = 0;

	for (which = 0; which < 3; which++) {
		int trial, spoke = 0, silent = 0;

		diff_begin(names[which]);

		for (trial = 0; trial < 18; trial++) {
			long tag = 3400 + which * 100 + trial;
			int count = (trial * 137) & 0x3fff;

			set_level(2);
			enter_once(which, trial, count, tag, 0, 0);

			diff_eq_int("the state is the member's (%ld)",
				    (long)P4DB->state, (long)states[which],
				    tag);
			diff_eq_int("countInState was cleared (%ld)",
				    (long)P4DB->countInState, 0, tag);
			diff_eq_int("+0x28 was not touched (%ld)",
				    (long)P4DB->int_0028, 0x6b000000 + trial,
				    tag);
			diff_eq_int("+0x38 was not touched (%ld)",
				    (long)P4DB->int_0038, 0x6b110000 + trial,
				    tag);
			compare_pair("after the state entry", tag);

			if (strlen(dsplib_debug_capture_text(1)) > 0)
				spoke = 1;

			/* Below the gate the blob says nothing at all. */
			set_level(0);
			enter_once(which, trial + 50, count, tag + 50, 0, 0);
			diff_eq_int("below the gate the blob was silent (%ld)",
				    (long)strlen(dsplib_debug_capture_text(1)),
				    0, tag + 50);
			diff_eq_int("below the gate ours was silent (%ld)",
				    (long)strlen(dsplib_debug_capture_text(0)),
				    0, tag + 50);
			diff_eq_int("the state is still the member's (%ld)",
				    (long)P4DB->state, (long)states[which],
				    tag + 50);
			compare_pair("after the state entry, gate shut",
				     tag + 50);
			silent = 1;
		}

		diff_eq_int("the gate was tried open", spoke, 1, 0);
		diff_eq_int("the gate was tried shut", silent, 1, 0);
		rc |= diff_end();
	}

	/*
	 * THE TWO COMPARISONS AN OBJECT CANNOT MAKE, both between two runs of
	 * the BLOB.  Same `countInState`, three members: the messages must
	 * differ, or the format strings are interchangeable.  Same member,
	 * two `countInState`s: the messages must differ, or the argument
	 * never reaches the output.
	 */
	diff_begin("V90Phase4Demodulator: the three messages are distinct");
	set_level(2);
	for (which = 0; which < 3; which++)
		enter_once(which, 900, 7, 3900 + which, text_at_7[which],
			   ENTER_TEXT);
	enter_once(0, 901, 9999, 3910, text_at_9999, ENTER_TEXT);

	diff_eq_int("the blob printed something at all (%ld)",
		    (long)(strlen(text_at_7[0]) > 0), 1, 0);
	diff_eq_int("WaitForCP differs from WaitForMP (%ld)",
		    strcmp(text_at_7[0], text_at_7[1]) != 0, 1, 0);
	diff_eq_int("WaitForCP differs from WaitForEd (%ld)",
		    strcmp(text_at_7[0], text_at_7[2]) != 0, 1, 0);
	diff_eq_int("WaitForMP differs from WaitForEd (%ld)",
		    strcmp(text_at_7[1], text_at_7[2]) != 0, 1, 0);
	diff_eq_int("the printed count reaches the output (%ld)",
		    strcmp(text_at_7[0], text_at_9999) != 0, 1, 0);
	set_level(0);
	rc |= diff_end();

	return rc;
}

/* ------------------------------------------------- detectRRN (143 bytes) */

static int
run_p4d_detectrrn(void)
{
	static const int arms[] = { 1, -2, -1, 0, 1, -2, -1, 0 };
	int trial;
	int fired = 0, quiet = 0, flag_zero = 0, flag_set = 0, pol_seen = 0;

	diff_begin("V90Phase4Demodulator::detectRRN");

	for (trial = 0; trial < 32; trial++) {
		long tag = 4000 + trial;
		int arm = arms[(unsigned)trial % (sizeof(arms) /
						  sizeof(arms[0]))];
		unsigned int flag = (trial & 4) ? (0x900u + trial) : 0u;
		short sample;
		int ra, rb;

		seed_pair(trial + 500, trial % 3);
		set_level(2);

		sample = arm_r(&P4DA->rDetector1, arm, 0x30 + 6 * (trial % 5));
		P4DB->rDetector1 = P4DA->rDetector1;

		/*
		 * The OTHER detector's polarity, distinct from anything
		 * `detectR` can leave in +0x3020, so a body that printed
		 * `rDetector2.polarity` prints a different number.
		 */
		P4DA->rDetector2.polarity = P4DB->rDetector2.polarity =
		    0x5150 + trial;

		P4DA->sessionFlag = P4DB->sessionFlag = flag;
		P4DA->state = P4DB->state = P4D_STATE_WAIT_FOR_ED;
		P4DA->countInState = P4DB->countInState = 0x4c00 + trial;
		P4DA->int_0028 = P4DB->int_0028 = 0x4d00 + trial;
		P4DA->int_0038 = P4DB->int_0038 = 0x4e00 + trial;
		P4DA->int_003c = P4DB->int_003c = 0x4f00 + trial;
		P4DA->int_0044 = P4DB->int_0044 = 0x5000 + trial;

		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ra = P4DA->detectRRN(sample);
		rb = ref_p4d_detectrrn(p4d_b, sample);
		dsplib_debug_capture_on = 0;

		diff_eq_int("the answers match (%ld)", (long)ra, (long)rb,
			    tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_pair("after detectRRN", tag);

		if (rb) {
			fired = 1;
			diff_eq_int("it moved to state 9 (%ld)",
				    (long)P4DB->state,
				    (long)P4D_STATE_RD_DETECTED, tag);
			diff_eq_int("countInState was cleared (%ld)",
				    (long)P4DB->countInState, 0, tag);
			diff_eq_int("+0x28 was cleared (%ld)",
				    (long)P4DB->int_0028, 0, tag);
			diff_eq_int("+0x44 was not touched (%ld)",
				    (long)P4DB->int_0044, 0x5000 + trial, tag);
			if (strlen(dsplib_debug_capture_text(1)) > 0)
				pol_seen = 1;
			if (flag == 0) {
				flag_zero = 1;
				diff_eq_int("+0x38 became 1 (%ld)",
					    (long)P4DB->int_0038, 1, tag);
				diff_eq_int("+0x3c became 1 (%ld)",
					    (long)P4DB->int_003c, 1, tag);
			} else {
				flag_set = 1;
				diff_eq_int("+0x38 kept its seed (%ld)",
					    (long)P4DB->int_0038,
					    0x4e00 + trial, tag);
				diff_eq_int("+0x3c kept its seed (%ld)",
					    (long)P4DB->int_003c,
					    0x4f00 + trial, tag);
			}
		} else {
			quiet = 1;
			diff_eq_int("the state did not move (%ld)",
				    (long)P4DB->state,
				    (long)P4D_STATE_WAIT_FOR_ED, tag);
			diff_eq_int("nothing was printed (%ld)",
				    (long)strlen(dsplib_debug_capture_text(1)),
				    0, tag);
		}
	}

	set_level(0);
	diff_eq_int("the detector fired", fired, 1, 0);
	diff_eq_int("the detector also declined", quiet, 1, 0);
	diff_eq_int("the sessionFlag == 0 arm was taken", flag_zero, 1, 0);
	diff_eq_int("the sessionFlag != 0 arm was taken", flag_set, 1, 0);
	diff_eq_int("the message was printed", pol_seen, 1, 0);
	return diff_end();
}

/* ------------------------------------------------- detectFPE (115 bytes) */

static int
run_p4d_detectfpe(void)
{
	static const int arms[] = { 1, -2, -1, 0, 1, -2, -1, 0 };
	static char text_pol_a[ENTER_TEXT];
	static char text_pol_b[ENTER_TEXT];
	int trial, fired = 0, quiet = 0, spoke = 0;

	diff_begin("V90Phase4Demodulator::detectFPE");

	for (trial = 0; trial < 32; trial++) {
		long tag = 4400 + trial;
		int arm = arms[(unsigned)trial % (sizeof(arms) /
						  sizeof(arms[0]))];
		short sample;
		int ra, rb;

		seed_pair(trial + 600, trial % 3);
		set_level(2);

		sample = arm_rf(&P4DA->rDetector2, arm,
				0x60 + 12 * (trial % 5));
		P4DB->rDetector2 = P4DA->rDetector2;

		/*
		 * THE FIELD THE MESSAGE ACTUALLY READS, +0x3020, which is the
		 * OTHER detector's.  It is set to a value `detectRf` cannot
		 * produce, and the detector it runs on will hold 1 or -1
		 * afterwards, so the two are never confusable.
		 */
		P4DA->rDetector1.polarity = P4DB->rDetector1.polarity =
		    0x2600 + trial;

		P4DA->state = P4DB->state = P4D_STATE_WAIT_FOR_MP;
		P4DA->countInState = P4DB->countInState = 0x3c00 + trial;
		P4DA->int_0028 = P4DB->int_0028 = 0x3d00 + trial;
		P4DA->int_0038 = P4DB->int_0038 = 0x3e00 + trial;
		P4DA->int_003c = P4DB->int_003c = 0x3f00 + trial;

		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ra = P4DA->detectFPE(sample);
		rb = ref_p4d_detectfpe(p4d_b, sample);
		dsplib_debug_capture_on = 0;

		diff_eq_int("the answers match (%ld)", (long)ra, (long)rb,
			    tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_pair("after detectFPE", tag);

		if (rb) {
			fired = 1;
			diff_eq_int("it moved to state 0x10 (%ld)",
				    (long)P4DB->state, (long)P4D_STATE_FPE,
				    tag);
			diff_eq_int("countInState was cleared (%ld)",
				    (long)P4DB->countInState, 0, tag);
			diff_eq_int("+0x28 was cleared (%ld)",
				    (long)P4DB->int_0028, 0, tag);
			diff_eq_int("+0x38 was not touched (%ld)",
				    (long)P4DB->int_0038, 0x3e00 + trial, tag);
			diff_eq_int("+0x3c was not touched (%ld)",
				    (long)P4DB->int_003c, 0x3f00 + trial, tag);
			diff_eq_int("the detector it ran on kept a polarity "
				    "(%ld)",
				    (long)(P4DB->rDetector2.polarity == 1 ||
					   P4DB->rDetector2.polarity == -1), 1,
				    tag);
			diff_eq_int("the one it PRINTS was left alone (%ld)",
				    (long)P4DB->rDetector1.polarity,
				    0x2600 + trial, tag);
			if (strlen(dsplib_debug_capture_text(1)) > 0)
				spoke = 1;
		} else {
			quiet = 1;
			diff_eq_int("the state did not move (%ld)",
				    (long)P4DB->state,
				    (long)P4D_STATE_WAIT_FOR_MP, tag);
			diff_eq_int("nothing was printed (%ld)",
				    (long)strlen(dsplib_debug_capture_text(1)),
				    0, tag);
		}
	}

	/*
	 * WHICH DETECTOR'S POLARITY REACHES THE MESSAGE, answered by two runs
	 * of the BLOB that differ in +0x3020 alone.  `rDetector2` is armed
	 * identically both times, so it ends at the same polarity; only
	 * `rDetector1.polarity` moves, and the transcript has to move with it.
	 */
	{
		short sample;
		unsigned n;

		set_level(2);
		seed_pair(700, 0);
		sample = arm_rf(&P4DB->rDetector2, 1, 0x60);
		P4DB->rDetector1.polarity = 0x0111;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		diff_eq_int("the blob fired (%ld)",
			    (long)ref_p4d_detectfpe(p4d_b, sample), 1, 4900);
		dsplib_debug_capture_on = 0;
		n = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (n >= ENTER_TEXT)
			n = ENTER_TEXT - 1;
		memcpy(text_pol_a, dsplib_debug_capture_text(1), n);
		text_pol_a[n] = '\0';

		seed_pair(700, 0);
		sample = arm_rf(&P4DB->rDetector2, 1, 0x60);
		P4DB->rDetector1.polarity = 0x0222;
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		diff_eq_int("the blob fired again (%ld)",
			    (long)ref_p4d_detectfpe(p4d_b, sample), 1, 4901);
		dsplib_debug_capture_on = 0;
		n = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (n >= ENTER_TEXT)
			n = ENTER_TEXT - 1;
		memcpy(text_pol_b, dsplib_debug_capture_text(1), n);
		text_pol_b[n] = '\0';

		diff_eq_int("the message followed rDetector1 (%ld)",
			    strcmp(text_pol_a, text_pol_b) != 0, 1, 4902);
	}

	set_level(0);
	diff_eq_int("the detector fired", fired, 1, 0);
	diff_eq_int("the detector also declined", quiet, 1, 0);
	diff_eq_int("both messages were printed", spoke, 1, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_p4d_resetrrndetector();
	rc |= run_p4d_resetbeforrrn();
	rc |= run_p4d_enter();
	rc |= run_p4d_detectrrn();
	rc |= run_p4d_detectfpe();

	return rc;
}
