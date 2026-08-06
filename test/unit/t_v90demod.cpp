/*
 * t_v90demod.cpp -- V90Demodulator::enterPhase3 against the blob.
 *
 * THIS IS A GRAPH TEST WITH A LATCH IN FRONT OF IT.  The method resets six
 * subobjects reached four different ways -- two embedded (`V90PreFilter` at
 * +0x6c, `V90SpectralVerifier` at +0x210), one embedded and called on its base
 * subobject's address (the resampler at +0x94), three through pointers, and
 * two words read out of the parameter block plus one pointer read out of it --
 * so every block is allocated per side, seeded identically with varied bytes,
 * and compared whole.  The object is 0x298 and the slot is bigger.
 *
 * THE THREE VACUITY TRAPS HERE, AND WHAT IS DONE ABOUT EACH:
 *
 *   1. `if (inPhase3 == 1) return;` is the first instruction.  A seeded slot
 *      that happened to hold 1 there makes the whole method a no-op and
 *      everything compares equal for the worst possible reason.  So +0x34 is
 *      set explicitly every trial, the sweep includes 1 (which must return)
 *      and 0, 2 and 0xffffffff (which must not), and `run_latch` requires
 *      both outcomes to have been seen AND requires them to leave DIFFERENT
 *      objects behind.
 *
 *   2. `V90PreFilter::isV90WithEia6()` is called twice with
 *      `setParamEia6()` between the two calls, and `setParamEia6` writes
 *      eighteen words of the block `isV90WithEia6` reads.  The test drives
 *      +0x500 of the parameter block both ways and requires both arms.
 *
 *   3. Three exits: the early one on a negative byte in the block the
 *      parameter block points at, the retrain one on a non-zero
 *      `sessionFlag`, and the plain one.  All three are driven, and the
 *      retrain one is the only thing in wave 2 that writes a value that is
 *      neither 0 nor 1 -- +0x3c becomes 0x20 -- so it is checked by name as
 *      well as by the object comparison.
 *
 * THE FLOAT-BEARING FIELDS ARE SET, NOT SEEDED.  `setTimingOffset` and
 * `V90Equalizer::enterPhase3` do x87 arithmetic on fields of objects this
 * test owns; a random 32-bit pattern is a signalling NaN about one time in
 * 250, and this test is not the place to discover what two different
 * compilations do with one.  Everything else is seeded.
 *
 * THE SIXTEEN BLOCKS NOW LIVE IN test/harness/v90demfix.h, which this file
 * and t_vpcmep3.cpp share.  `setup`, `teardown`, `compare_all`, `set_level`,
 * `D`, `P2`, `P3`, `struct trial_args` and every slot constant come from
 * there; the paragraphs above still describe what they do, because moving
 * them changed none of it.  Everything below is what is specific to
 * `V90Demodulator::enterPhase3`.
 */

#include "v90demfix.h"

extern "C" {
void ref_enterPhase3(void *self) asm("ref__ZN14V90Demodulator11enterPhase3Ev");
}


static void
run_pair(int trial, const struct trial_args *t)
{
	setup(trial, t);
	dsplib_debug_capture_reset();
	D(0)->enterPhase3();
	ref_enterPhase3(D(1));
	teardown();
}

static void
transcripts_agree(long tag)
{
	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/*
 * The latch.  1 returns immediately; every other value, including other
 * non-zero ones, runs the whole method.  Both outcomes are required, and they
 * are required to be DIFFERENT -- otherwise "it returned early" and "it did
 * the work" would be the same observation.
 */
static const unsigned int latch_v[] = {
	0u, 1u, 2u, 0xffffffffu, 0x80000000u
};
#define NLATCH ((int)(sizeof(latch_v) / sizeof(latch_v[0])))

static int
run_latch(void)
{
	static unsigned char early[DEM_SLOT], late[DEM_SLOT];
	int i, sawEarly = 0, sawLate = 0;
	struct trial_args t;

	diff_begin("V90Demodulator::enterPhase3 -- the phase 3 latch");

	dsplib_debug_capture_on = 1;
	set_level(2);

	for (i = 0; i < NLATCH; i++) {
		long tag = i;

		t.latch = latch_v[i];
		t.flag = 0;
		t.eia6 = 6;
		t.blockByte = 0;
		t.pcmType = 0;
		t.idx = i;

		run_pair(11, &t);
		compare_all("after enterPhase3", tag);
		transcripts_agree(tag);

		diff_eq_int("the latch is set (%ld)", (long)D(1)->inPhase3,
			    1, tag);

		if (latch_v[i] == 1u) {
			memcpy(early, dem[1], DEM_SLOT);
			sawEarly = 1;
		} else {
			memcpy(late, dem[1], DEM_SLOT);
			sawLate = 1;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the early return was taken", sawEarly, 1, 0);
	diff_eq_int("the method ran to the end", sawLate, 1, 0);
	diff_eq_int("returning early is observably different",
		    memcmp(early, late, DEM_SLOT) != 0, 1, 0);

	return diff_end();
}

/*
 * The EIA-6 arm and the two late exits.  `eia6` drives the parameter block's
 * +0x500 both ways, `blockByte` drives the sign that ends the method early,
 * and `flag` drives the V.92 retrain.
 */
static int
run_branches(void)
{
	int e, b, f, lvl, p;
	int sawEia = 0, sawNoEia = 0, sawNeg = 0, sawPos = 0, sawRetrain = 0;
	int printed = 0;
	struct trial_args t;

	diff_begin("V90Demodulator::enterPhase3 -- the arms and the exits");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (e = 0; e < 2; e++) {
			for (b = 0; b < 2; b++) {
				for (f = 0; f < 2; f++) {
					for (p = 0; p < 2; p++) {
						long tag = (long)lvl * 10000 +
						    e * 1000 + b * 100 +
						    f * 10 + p;

						t.latch = 0;
						t.flag = f ? 0x5a5a5a5au : 0u;
						t.eia6 = e ? 6 : 0;
						t.blockByte = b ? -1 : 0x7f;
						t.pcmType = p;
						t.idx = lvl + e + b + f + p;

						run_pair(20 + tag % 7, &t);
						compare_all("after enterPhase3",
							    tag);
						transcripts_agree(tag);
						printed += (int)
						    dsplib_debug_capture_lines(0);

						if (t.eia6 == 6)
							sawEia = 1;
						else
							sawNoEia = 1;
						if (b)
							sawNeg = 1;
						else
							sawPos = 1;
						if (!b && f &&
						    D(1)->word_3c == 0x20)
							sawRetrain = 1;

						/*
						 * The retrain exit is the only
						 * write in wave 2 that is
						 * neither 0 nor 1.
						 */
						diff_eq_int(
						    "+0x3c after the exits (%ld)",
						    (long)D(1)->word_3c,
						    (long)((!b && f) ? 0x20 : 0),
						    tag);
					}
				}
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the EIA-6 arm was taken", sawEia, 1, 0);
	diff_eq_int("the EIA-6 arm was skipped", sawNoEia, 1, 0);
	diff_eq_int("the negative-byte exit was taken", sawNeg, 1, 0);
	diff_eq_int("the negative-byte exit was not taken", sawPos, 1, 0);
	diff_eq_int("the V.92 retrain exit was taken", sawRetrain, 1, 0);
	diff_eq_int("the diagnostics were emitted", printed > 0, 1, 0);

	return diff_end();
}

/*
 * ANTI-VACUITY: the claims the object comparisons above would agree about for
 * the wrong reason if they were not separately shown to be observable.
 */
static int
run_observable(void)
{
	static unsigned char withEia[PARM_SLOT], withoutEia[PARM_SLOT];
	static unsigned char negExit[DEM_SLOT], plainExit[DEM_SLOT];
	struct trial_args t;

	diff_begin("the arms and the exits leave different state behind");

	set_level(0);

	/*
	 * 1.  The EIA-6 arm writes the parameter block; without it the block
	 *     comes back as it went in.  If those two agreed, `setParamEia6`
	 *     could be dropped and nothing above would notice.
	 */
	t.latch = 0;
	t.flag = 0;
	t.blockByte = 0;
	t.pcmType = 0;
	t.idx = 3;

	t.eia6 = 6;
	run_pair(31, &t);
	memcpy(withEia, parm[0], PARM_SLOT);

	t.eia6 = 0;
	run_pair(31, &t);
	memcpy(withoutEia, parm[0], PARM_SLOT);

	diff_eq_int("the EIA-6 arm changes the parameter block",
		    memcmp(withEia, withoutEia, PARM_SLOT) != 0, 1, 0);

	/*
	 * 2.  The timing offset is written only on the EIA-6 arm, and from the
	 *     parameter block rather than from the object.
	 */
	t.eia6 = 6;
	t.idx = 3;
	run_pair(31, &t);
	diff_eq_int("the EIA-6 arm set the timing offset (%ld)",
		    D(0)->resampler.timingOffset != 0.0f, 1, 0);

	/*
	 * 3.  The negative-byte exit really does cut the method short: the
	 *     retrain would otherwise have fired with the same flag.
	 */
	t.eia6 = 6;
	t.flag = 1;
	t.blockByte = -1;
	run_pair(33, &t);
	memcpy(negExit, dem[0], DEM_SLOT);

	t.blockByte = 0;
	run_pair(33, &t);
	memcpy(plainExit, dem[0], DEM_SLOT);

	diff_eq_int("the negative-byte exit skips the retrain",
		    memcmp(negExit, plainExit, DEM_SLOT) != 0, 1, 0);

	/*
	 * 4.  The phase 3 demodulator's +0x410 takes the demodulator's +0x294
	 *     AFTER its reset zeroed it -- so a non-zero +0x294 must survive.
	 */
	t.latch = 0;
	t.flag = 0;
	t.blockByte = 0;
	t.eia6 = 0;
	setup(35, &t);
	D(0)->word_294 = 0x1234abcdu;
	D(1)->word_294 = 0x1234abcdu;
	D(0)->enterPhase3();
	ref_enterPhase3(D(1));
	teardown();
	diff_eq_int("+0x294 reached the phase 3 demodulator's +0x410 (%ld)",
		    (long)P3(0)->word_410, (long)0x1234abcdu, 0);
	diff_eq_int("...on the blob's side too (%ld)", (long)P3(1)->word_410,
		    (long)0x1234abcdu, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_latch();
	bad |= run_branches();
	bad |= run_observable();

	return bad;
}
