/*
 * t_v90p3dreset.cpp -- V90Phase3Demodulator::reset against the blob, and the
 * two Descrambler<int,int> members it reaches.
 *
 * WHAT THIS HAS TO CATCH IS PLACEMENT AND SELECTION, NOT ARITHMETIC.  The
 * method has eleven arguments, stores eight of them, resets five subobjects
 * and then picks one of four openings.  So every block is allocated
 * separately per side, seeded identically with varied bytes, and compared
 * WHOLE -- an argument landing at the wrong offset lands in a `pad_` region
 * that both sides would otherwise still agree on, and the object's size is
 * known (0x42c), so the slot is bigger than the object and the slack is
 * compared too.
 *
 * THE THREE WAYS THIS TEST COULD PASS WHILE PROVING NOTHING, AND WHAT IS DONE
 * ABOUT EACH:
 *
 *   1. `WaitForSd` (state 0) and `IRREGULAR` (everything unnamed) run exactly
 *      the same code -- resetLinearMapping, then the modulator with a zero
 *      fourth argument.  The ONLY difference is the text.  So the state sweep
 *      runs at `dsplibs_debug_level` 0, 2 and 3 and compares transcripts, and
 *      `run_states` asserts a non-empty transcript was seen.  At level 0 those
 *      two branches are genuinely indistinguishable and this test says so
 *      rather than pretending otherwise.
 *
 *   2. `WaitForQTS` (state 26) differs from `WaitForSd` only by NOT calling
 *      `V90AutoDigitalImpDetector::resetLinearMapping`.  If the detector were
 *      seeded so that `resetLinearMapping` wrote nothing distinguishable, the
 *      two would compare equal for the wrong reason.  `run_states` therefore
 *      runs the same trial twice, once as state 0 and once as state 26, and
 *      asserts the two detectors DIFFER.  That check is satisfiable and is
 *      verified to fire, which findings 247 and 262 are the reason for.
 *
 *   3. `TRN1dKnownData` (state 3) differs from the rest only in passing
 *      `reset`'s fourth argument, rather than 0, as the modulator's symbol
 *      count.  With a zero fourth argument that is invisible, so the state-3
 *      trials use non-zero counts -- and small ones, because the count is a
 *      loop bound in V90Phase3Modulator::reset and a large value would run
 *      the modulator for hours.
 *
 * PCM TYPE IS SWEPT OVER 0 AND 1 ONLY.  The blob branches on
 * `test %esi,%esi`, so `== PCM_TYPE_MU_LAW` and `!= PCM_TYPE_A_LAW` are
 * distinguishable only by a third value, and `enum PcmType` has exactly two
 * enumerators -- a third is outside its value range and passing one is
 * undefined in C++.  Held fixed: within the type's domain the two spellings
 * are equivalent, so the mutation that swaps them is not a defect this test
 * could have caught.
 */

#include <string.h>

#include "harness.h"

extern "C" {
void ref_p3d_reset(void *self, int pcmType, unsigned char ucode, int state,
		   unsigned int word2c, void *jd, void *jdV92, void *dil,
		   short altRbs, short short414, float float418,
		   unsigned int word14)
	asm("ref__ZN20V90Phase3Demodulator5resetE7PcmTypeh22Phase3Demodulator"
	    "StatejP5V90JdP5V92JdP19tagV90DILdescriptorssfj");

/*
 * The two weak `Descrambler<int,int>` members.  They are `W` in the blob and
 * live in their own `.gnu.linkonce.t.*` sections; symmap.py renames weak
 * symbols like everything else, so they can be driven directly and not only
 * through the demodulator.
 */
void ref_desc_reset(void *self, int value)
	asm("ref__ZN11DescramblerIiiE5resetEi");
void ref_desc_resetHistoryIndexes(void *self)
	asm("ref__ZN11DescramblerIiiE19resetHistoryIndexesEv");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"
#include "dsplib/V90Phase3Demodulator.h"

/* The object is 0x42c; the slot is larger so an overrunning store shows up. */
#define SLOT		(0x42c + 64)

union p3d_slot {
	V90Phase3Demodulator o;
	unsigned char raw[SLOT];
};

/*
 * The constructor builds the `<int,int>` descrambler with (0x12, 0x17, 0x63),
 * so its buffer is (1 + 0x17 + 0x63) words, `pInitOut` is 0x63 in and the two
 * taps 0x12 and 0x17 past that.  Reproduced exactly rather than approximated:
 * `reset` fills from `pInitOut + 1` up to `pInitTap2`, so both of those have
 * to be where the object puts them for the fill to cover the same words.
 */
#define DSC_TAP1	0x12u
#define DSC_TAIL	0x17u
#define DSC_OUT		0x63u
#define DSC_WORDS	(1u + DSC_TAIL + DSC_OUT)

/* The modulator's own Scrambler<unsigned char,int>; see t_v90p3mod.cpp. */
#define SCR_BUF		64u
#define SCR_OUT		40u
#define SCR_TAP1	45u
#define SCR_TAP2	63u
#define SCR_TAIL	23u

#define SDD_HIST	12u
#define PARAMS_BYTES	0x504

static union p3d_slot slot[2];
static V90AutoDigitalImpDetector adid[2];
static V90SdDetector sdd[2];
static float sdhist[2][SDD_HIST];
static V90Jd jdo[2];
static V92Jd jd92o[2];
static tagV90DILdescriptor dilo[2];
static int dbuf[2][DSC_WORDS];
static unsigned char sbuf[2][SCR_BUF];

/*
 * BOTH SIDES SHARE ONE PARAMETER BLOCK, as t_v90adid.cpp does: nothing on
 * this path writes through it, and sharing it removes a pointer that would
 * otherwise have to be neutralised in the detector comparison.
 */
static unsigned char params_block[PARAMS_BYTES];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/* VARIED BYTES, NEVER ZEROS (finding 230). */
static void
fill_pair(void *a, void *b, size_t n)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

static void
place_descrambler(int side, unsigned int out)
{
	Descrambler<int, int> *d = &slot[side].o.descrambler;
	int *buf = dbuf[side];

	d->pLimit = buf;
	d->pInitOut = buf + DSC_OUT;
	d->pInitTap1 = buf + DSC_OUT + DSC_TAP1;
	d->pInitTap2 = buf + DSC_OUT + DSC_TAIL;
	/*
	 * The three running pointers start somewhere OTHER than their initial
	 * values, or `resetHistoryIndexes` would have nothing to do and the
	 * three copies it makes would be unobservable.
	 */
	d->pOut = buf + out;
	d->pTap1 = buf + out + DSC_TAP1;
	d->pTap2 = buf + out + DSC_TAIL;
	d->tailLength = DSC_TAIL;
}

static void
place_scrambler(int side, unsigned int out)
{
	Scrambler<unsigned char, int> *s = &slot[side].o.phase3Modulator.scrambler;
	unsigned char *buf = sbuf[side];

	s->pLimit = buf;
	s->pInitOut = buf + SCR_OUT;
	s->pInitTap1 = buf + SCR_TAP1;
	s->pInitTap2 = buf + SCR_TAP2;
	s->pOut = buf + out;
	s->pTap1 = buf + out + (SCR_TAP1 - SCR_OUT);
	s->pTap2 = buf + out + (SCR_TAP2 - SCR_OUT);
	s->tailLength = SCR_TAIL;
}

/*
 * Seed everything, identically on both sides, then wire each side to its own
 * blocks.  `trial` picks the fill; `dilCount` is clamped because it is a loop
 * bound in calculateDilLength and in the modulator's DIL expansion.
 */
static void
seed(int trial)
{
	int side;

	lfsr_state = 0xa37u + 0x9e37u * (unsigned)trial;

	fill_pair(slot[0].raw, slot[1].raw, SLOT);
	fill_pair(&adid[0], &adid[1], sizeof(adid[0]));
	fill_pair(&sdd[0], &sdd[1], sizeof(sdd[0]));
	fill_pair(sdhist[0], sdhist[1], sizeof(sdhist[0]));
	fill_pair(&jdo[0], &jdo[1], sizeof(jdo[0]));
	fill_pair(&jd92o[0], &jd92o[1], sizeof(jd92o[0]));
	fill_pair(&dilo[0], &dilo[1], sizeof(dilo[0]));
	fill_pair(dbuf[0], dbuf[1], sizeof(dbuf[0]));
	fill_pair(sbuf[0], sbuf[1], sizeof(sbuf[0]));

	for (side = 0; side < 2; side++) {
		slot[side].o.autoDigitalImpDetector = &adid[side];
		slot[side].o.sdDetector = &sdd[side];
		adid[side].params = (V90Parameters *)params_block;
		sdd[side].history = sdhist[side];
		sdd[side].historyLength = SDD_HIST;
		dilo[side].dilCount = (unsigned char)(trial * 37u + 1u);
		dilo[side].seq1Length =
		    (unsigned char)((trial * 13u + 1u) % 129u);
		dilo[side].seq2Length =
		    (unsigned char)((trial * 29u + 7u) % 129u);
		place_descrambler(side, (unsigned)trial % (DSC_OUT + 1u));
		place_scrambler(side, (unsigned)trial % 41u);
	}
}

/*
 * A copy of one side's slot with every pointer replaced by something the two
 * sides can agree about: an offset into that side's own buffer, or whether it
 * still points where seed() put it.  Same idiom as snap_mod in
 * t_v90sessionflag.cpp and scr_compare in t_v90p3mod.cpp -- two static arrays
 * at two addresses never compare equal and never will.
 */
static void
snap(unsigned char *dst, int side)
{
	union p3d_slot *s = (union p3d_slot *)dst;
	V90Phase3Demodulator *l = &slot[side].o;

	memcpy(dst, slot[side].raw, SLOT);

	s->o.autoDigitalImpDetector = (V90AutoDigitalImpDetector *)(long)
	    (l->autoDigitalImpDetector == &adid[side]);
	s->o.sdDetector = (V90SdDetector *)(long)(l->sdDetector == &sdd[side]);
	s->o.dil = (tagV90DILdescriptor *)(long)
	    (l->dil == NULL ? 2 : (l->dil == &dilo[side]));
	s->o.jd = (V90Jd *)(long)(l->jd == NULL ? 2 : (l->jd == &jdo[side]));
	s->o.jdV92 = (V92Jd *)(long)
	    (l->jdV92 == NULL ? 2 : (l->jdV92 == &jd92o[side]));

	s->o.descrambler.pLimit = (int *)(l->descrambler.pLimit - dbuf[side]);
	s->o.descrambler.pInitOut = (int *)(l->descrambler.pInitOut - dbuf[side]);
	s->o.descrambler.pInitTap1 = (int *)(l->descrambler.pInitTap1 - dbuf[side]);
	s->o.descrambler.pInitTap2 = (int *)(l->descrambler.pInitTap2 - dbuf[side]);
	s->o.descrambler.pOut = (int *)(l->descrambler.pOut - dbuf[side]);
	s->o.descrambler.pTap1 = (int *)(l->descrambler.pTap1 - dbuf[side]);
	s->o.descrambler.pTap2 = (int *)(l->descrambler.pTap2 - dbuf[side]);

	s->o.phase3Modulator.scrambler.pLimit = (unsigned char *)
	    (l->phase3Modulator.scrambler.pLimit - sbuf[side]);
	s->o.phase3Modulator.scrambler.pInitOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitOut - sbuf[side]);
	s->o.phase3Modulator.scrambler.pInitTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap1 - sbuf[side]);
	s->o.phase3Modulator.scrambler.pInitTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap2 - sbuf[side]);
	s->o.phase3Modulator.scrambler.pOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pOut - sbuf[side]);
	s->o.phase3Modulator.scrambler.pTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap1 - sbuf[side]);
	s->o.phase3Modulator.scrambler.pTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap2 - sbuf[side]);

	/* The detector's shared parameter block is the same address on both. */
}

static void
compare_all(const char *what, long tag)
{
	static unsigned char a[SLOT], b[SLOT];

	snap(a, 0);
	snap(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Demodulator slot",
		     a, b, SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90AutoDigitalImpDetector",
		     &adid[0], &adid[1], sizeof(adid[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector",
		     &sdd[0], &sdd[1], (size_t)__builtin_offsetof(
			 V90SdDetector, history), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector history",
		     sdhist[0], sdhist[1], sizeof(sdhist[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Jd",
		     &jdo[0], &jdo[1], sizeof(jdo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Jd",
		     &jd92o[0], &jd92o[1], sizeof(jd92o[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "tagV90DILdescriptor",
		     &dilo[0], &dilo[1], sizeof(dilo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "descrambler buffer",
		     dbuf[0], dbuf[1], sizeof(dbuf[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "scrambler buffer",
		     sbuf[0], sbuf[1], sizeof(sbuf[0]), tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/* One trial, both sides, with the arguments given. */
static void
run_pair(int trial, PcmType law, unsigned char ucode,
	 Phase3DemodulatorState state, unsigned int word2c, int withJd,
	 int withJd92, int withDil, short altRbs, short short414,
	 float float418, unsigned int word14)
{
	seed(trial);
	dsplib_debug_capture_reset();

	slot[0].o.reset(law, ucode, state, word2c,
			withJd ? &jdo[0] : NULL,
			withJd92 ? &jd92o[0] : NULL,
			withDil ? &dilo[0] : NULL,
			altRbs, short414, float418, word14);
	ref_p3d_reset(&slot[1].o, (int)law, ucode, (int)state, word2c,
		      withJd ? (void *)&jdo[1] : NULL,
		      withJd92 ? (void *)&jd92o[1] : NULL,
		      withDil ? (void *)&dilo[1] : NULL,
		      altRbs, short414, float418, word14);
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
 * The two weak Descrambler members, driven directly.  `reset` masks its
 * argument with 1 before the fill, so the values below have to include some
 * whose low bit is set and some whose low bit is clear but which are not
 * zero -- otherwise a reconstruction that stored the whole argument would
 * pass.
 */
static const int desc_v[] = {
	0, 1, 2, 3, -1, -2, 0x100, 0x101, 0x7fffffff, (int)0x80000000, 0x5a5a5a5a
};
#define NDESC ((int)(sizeof(desc_v) / sizeof(desc_v[0])))

static int
run_descrambler(void)
{
	int i, trial, sawOne = 0, sawZero = 0;

	diff_begin("Descrambler<int,int>::reset and ::resetHistoryIndexes");

	for (trial = 0; trial < 8; trial++) {
		for (i = 0; i < NDESC; i++) {
			long tag = (long)trial * 100 + i;
			Descrambler<int, int> a, b;

			lfsr_state = 0x71cu + 0x4f1bu * (unsigned)(trial * 32 + i);
			fill_pair(dbuf[0], dbuf[1], sizeof(dbuf[0]));

			a.pLimit = dbuf[0];
			a.pInitOut = dbuf[0] + DSC_OUT;
			a.pInitTap1 = dbuf[0] + DSC_OUT + DSC_TAP1;
			a.pInitTap2 = dbuf[0] + DSC_OUT + DSC_TAIL;
			a.pOut = dbuf[0] + (unsigned)trial;
			a.pTap1 = dbuf[0] + (unsigned)trial + DSC_TAP1;
			a.pTap2 = dbuf[0] + (unsigned)trial + DSC_TAIL;
			a.tailLength = DSC_TAIL;

			b = a;
			b.pLimit = dbuf[1];
			b.pInitOut = dbuf[1] + DSC_OUT;
			b.pInitTap1 = dbuf[1] + DSC_OUT + DSC_TAP1;
			b.pInitTap2 = dbuf[1] + DSC_OUT + DSC_TAIL;
			b.pOut = dbuf[1] + (unsigned)trial;
			b.pTap1 = dbuf[1] + (unsigned)trial + DSC_TAP1;
			b.pTap2 = dbuf[1] + (unsigned)trial + DSC_TAIL;

			a.reset(desc_v[i]);
			ref_desc_reset(&b, desc_v[i]);

			diff_eq_int("pOut (%ld)", a.pOut - dbuf[0],
				    b.pOut - dbuf[1], tag);
			diff_eq_int("pTap1 (%ld)", a.pTap1 - dbuf[0],
				    b.pTap1 - dbuf[1], tag);
			diff_eq_int("pTap2 (%ld)", a.pTap2 - dbuf[0],
				    b.pTap2 - dbuf[1], tag);
			diff_eq_int("tailLength (%ld)", a.tailLength,
				    b.tailLength, tag);
			diff_eq_obj_(__FILE__, __LINE__, "after reset",
				     "descrambler buffer", dbuf[0], dbuf[1],
				     sizeof(dbuf[0]), tag);

			if ((desc_v[i] & 1) != 0)
				sawOne = 1;
			else if (desc_v[i] != 0)
				sawZero = 1;

			/* resetHistoryIndexes on its own, from a fresh skew. */
			a.pOut = dbuf[0] + (unsigned)i;
			a.pTap1 = dbuf[0] + (unsigned)i + 1;
			a.pTap2 = dbuf[0] + (unsigned)i + 2;
			b.pOut = dbuf[1] + (unsigned)i;
			b.pTap1 = dbuf[1] + (unsigned)i + 1;
			b.pTap2 = dbuf[1] + (unsigned)i + 2;

			a.resetHistoryIndexes();
			ref_desc_resetHistoryIndexes(&b);

			diff_eq_int("rhi pOut (%ld)", a.pOut - dbuf[0],
				    b.pOut - dbuf[1], tag);
			diff_eq_int("rhi pTap1 (%ld)", a.pTap1 - dbuf[0],
				    b.pTap1 - dbuf[1], tag);
			diff_eq_int("rhi pTap2 (%ld)", a.pTap2 - dbuf[0],
				    b.pTap2 - dbuf[1], tag);
		}
	}

	diff_eq_int("a value with the low bit set was used", sawOne, 1, 0);
	diff_eq_int("a non-zero value with the low bit clear was used",
		    sawZero, 1, 0);
	return diff_end();
}

/*
 * The argument sweep.  Every one of the eight stored arguments gets values
 * that differ in their top half as well as their bottom, because a store of
 * the wrong WIDTH is exactly what a single small value hides.
 */
static const unsigned int word_v[] = {
	0u, 1u, 0x7fu, 0xffu, 0x8000u, 0xffffu, 0x12345678u, 0xffffffffu
};
#define NWORD ((int)(sizeof(word_v) / sizeof(word_v[0])))

static const short short_v[] = {
	0, 1, -1, 0x7f, (short)0x80, 0x7fff, (short)0x8000, 0x0102
};
#define NSHORT ((int)(sizeof(short_v) / sizeof(short_v[0])))

static const float float_v[] = {
	0.0f, 1.0f, -1.0f, 1e-6f, -3.5e7f, 12345.678f
};
#define NFLOAT ((int)(sizeof(float_v) / sizeof(float_v[0])))

static int
run_arguments(void)
{
	int trial, sawMu = 0, sawA = 0, sawJd = 0, sawNoJd = 0;
	int sawJd92 = 0, sawNoJd92 = 0, sawDilLength = 0;

	diff_begin("V90Phase3Demodulator::reset -- the arguments");

	set_level(0);

	for (trial = 0; trial < 96; trial++) {
		PcmType law = (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;
		unsigned char ucode = (unsigned char)(trial * 17u + trial / 3u);
		int withJd = (trial / 2) & 1;
		int withJd92 = (trial / 4) & 1;
		int withDil = (trial / 8) & 1;
		long tag = trial;

		run_pair(trial, law, ucode, P3D_STATE_WAIT_FOR_SD,
			 word_v[trial % NWORD], withJd, withJd92, withDil,
			 short_v[trial % NSHORT], short_v[(trial + 3) % NSHORT],
			 float_v[trial % NFLOAT], word_v[(trial + 5) % NWORD]);

		compare_all("after reset", tag);

		/*
		 * The eight stored arguments, named individually as well as
		 * covered by the whole-object compare: when one moves, the
		 * object compare says "a run of four bytes at +0x2c differs"
		 * and this says which argument it was.
		 */
		diff_eq_int("pcmType stored (%ld)", (long)slot[1].o.pcmType,
			    (long)law, tag);
		diff_eq_int("ucode stored (%ld)", (long)slot[1].o.ucode,
			    (long)ucode, tag);
		diff_eq_int("state stored (%ld)", (long)slot[1].o.state,
			    (long)P3D_STATE_WAIT_FOR_SD, tag);
		diff_eq_int("word_2c stored (%ld)", (long)slot[1].o.word_2c,
			    (long)word_v[trial % NWORD], tag);
		diff_eq_int("short_414 stored (%ld)",
			    (long)slot[1].o.short_414,
			    (long)short_v[(trial + 3) % NSHORT], tag);
		diff_eq_int("word_14 stored (%ld)", (long)slot[1].o.word_14,
			    (long)word_v[(trial + 5) % NWORD], tag);
		diff_eq_int("float_418 stored (%ld)",
		    memcmp(&slot[1].o.float_418, &float_v[trial % NFLOAT],
			   sizeof(float)) == 0, 1, tag);

		if (law == PCM_TYPE_MU_LAW)
			sawMu = 1;
		else
			sawA = 1;
		if (withJd)
			sawJd = 1;
		else
			sawNoJd = 1;
		if (withJd92)
			sawJd92 = 1;
		else
			sawNoJd92 = 1;
		if (withDil && slot[1].o.dilLength != 0)
			sawDilLength = 1;
	}

	diff_eq_int("the mu-law arm was taken", sawMu, 1, 0);
	diff_eq_int("the A-law arm was taken", sawA, 1, 0);
	diff_eq_int("a non-null V90Jd was passed", sawJd, 1, 0);
	diff_eq_int("a null V90Jd was passed", sawNoJd, 1, 0);
	diff_eq_int("a non-null V92Jd was passed", sawJd92, 1, 0);
	diff_eq_int("a null V92Jd was passed", sawNoJd92, 1, 0);
	diff_eq_int("calculateDilLength returned non-zero at least once",
		    sawDilLength, 1, 0);

	return diff_end();
}

/*
 * The state selector.  The four openings and the two that are invisible
 * without the transcript; see the file comment.
 */
static const int state_v[] = {
	0, 1, 2, 3, 4, 5, 25, 26, 27, 31, -1, 100, 0x7fffffff
};
#define NSTATE ((int)(sizeof(state_v) / sizeof(state_v[0])))

static int
run_states(void)
{
	int s, lvl, trial, printed = 0, sawIrregular = 0, sawNamed = 0;

	diff_begin("V90Phase3Demodulator::reset -- the state selector");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (s = 0; s < NSTATE; s++) {
			for (trial = 0; trial < 3; trial++) {
				long tag = (long)lvl * 10000 + s * 100 + trial;
				int seedNo = s * 3 + trial + 5;
				/*
				 * SMALL, AND NON-ZERO.  This is the modulator's
				 * symbol count in the TRN1dKnownData opening,
				 * which is a loop bound; and a zero would make
				 * that opening indistinguishable from the rest.
				 */
				unsigned int count = (unsigned)trial + 1u;

				run_pair(seedNo, PCM_TYPE_MU_LAW,
					 (unsigned char)(s * 11 + trial),
					 (Phase3DemodulatorState)state_v[s],
					 count, 1, 1, 1, short_v[s % NSHORT],
					 short_v[trial % NSHORT],
					 float_v[s % NFLOAT], 0x1111u);

				compare_all("after reset", tag);
				transcripts_agree(tag);
				printed += (int)dsplib_debug_capture_lines(0);

				if (state_v[s] == 0 || state_v[s] == 3 ||
				    state_v[s] == 26)
					sawNamed = 1;
				else
					sawIrregular = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("a named state was reached", sawNamed, 1, 0);
	diff_eq_int("an irregular state was reached", sawIrregular, 1, 0);
	diff_eq_int("the state diagnostics were emitted", printed > 0, 1, 0);

	return diff_end();
}

/*
 * ANTI-VACUITY, and the reason this file exists in the shape it does.
 *
 * Each check below asserts that two openings are DISTINGUISHABLE by what they
 * leave behind, so that the whole-object comparisons above are testing a
 * difference rather than agreeing about nothing.  Every one of them is run
 * and required to fire; a check that cannot be satisfied is the failure
 * findings 247 and 262 record.
 */
static int
run_openings_differ(void)
{
	static unsigned char adidRef[sizeof(adid[0])];
	static unsigned char modRef[0x398];
	int differs;

	diff_begin("the four openings leave different state behind");

	set_level(0);

	/*
	 * 1.  WaitForQTS does not reset the linear mapping and WaitForSd
	 *     does, so the detectors must differ from the SAME seed.
	 */
	run_pair(3, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	memcpy(adidRef, &adid[0], sizeof(adidRef));

	run_pair(3, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_QTS, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	differs = memcmp(adidRef, &adid[0], sizeof(adidRef)) != 0;
	diff_eq_int("WaitForSd and WaitForQTS leave different detectors",
		    differs, 1, 0);

	/*
	 * 2.  TRN1dKnownData forwards reset's fourth argument as the
	 *     modulator's symbol count, and every other opening forwards 0.
	 *     With a non-zero count the modulator must end up different.
	 */
	run_pair(4, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_TRN1D_KNOWN_DATA, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	memcpy(modRef, &slot[0].o.phase3Modulator, sizeof(modRef));

	run_pair(4, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_TRN1D_KNOWN_DATA, 3,
		 1, 1, 1, 1, 1, 0.0f, 0);
	differs = memcmp(modRef, &slot[0].o.phase3Modulator,
			 sizeof(modRef)) != 0;
	diff_eq_int("a non-zero symbol count reaches the modulator",
		    differs, 1, 0);

	/*
	 * 3.  ...and that count comes from the fourth argument only in the
	 *     TRN1dKnownData opening.  The same non-zero argument in
	 *     WaitForSd must leave the modulator where a zero one did.
	 */
	run_pair(4, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	memcpy(modRef, &slot[0].o.phase3Modulator, sizeof(modRef));

	run_pair(4, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 3,
		 1, 1, 1, 1, 1, 0.0f, 0);
	differs = memcmp(modRef, &slot[0].o.phase3Modulator,
			 sizeof(modRef)) != 0;
	diff_eq_int("WaitForSd ignores the symbol count", differs, 0, 0);

	/*
	 * 4.  The descrambler fill is observable: `reset(0)` writes zeros over
	 *     bytes the seed made non-zero.
	 */
	run_pair(6, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	differs = 0;
	{
		unsigned int i;

		for (i = DSC_OUT + 1u; i <= DSC_OUT + DSC_TAIL; i++)
			if (dbuf[0][i] != 0)
				differs = 1;
	}
	diff_eq_int("the descrambler history was cleared", differs, 0, 0);
	diff_eq_int("the descrambler history index was restored",
		    slot[0].o.descrambler.pOut ==
		    slot[0].o.descrambler.pInitOut, 1, 0);

	/*
	 * 5.  The two companding arms give different levels for the same
	 *     code, or the mu-law/A-law selection could not be seen at all --
	 *     finding 253 is that exact trap one class down.
	 */
	run_pair(9, PCM_TYPE_MU_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 0,
		 1, 1, 1, 1, 1, 0.0f, 0);
	{
		short muLevel = slot[0].o.ucodeLevel;

		run_pair(9, PCM_TYPE_A_LAW, 0x40, P3D_STATE_WAIT_FOR_SD, 0,
			 1, 1, 1, 1, 1, 0.0f, 0);
		diff_eq_int("the two laws decode 0x40 differently (%ld)",
			    muLevel != slot[0].o.ucodeLevel, 1, 0);
	}

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_descrambler();
	bad |= run_arguments();
	bad |= run_states();
	bad |= run_openings_differ();

	return bad;
}
