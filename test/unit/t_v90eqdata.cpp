/*
 * t_v90eqdata.cpp -- differential test of V90Equalizer::enterDataPhase.
 *
 * IT IS ITS OWN BINARY AND THAT IS THE WHOLE REASON THIS FILE EXISTS.
 * `t_v90equ` is in `tools/gccdiverge.json` -- six of its checks are the
 * object's one-ordered-compare equality, which GCC 13 cannot emit (findings
 * F2300, F2304) -- so that binary exits non-zero on the modern build.
 * `tools/mutate.py` judges a mutant CAUGHT by a non-zero exit, so it refuses
 * to score a set against an already-red binary: caught and already-red are
 * indistinguishable.  `t_v90p4dnan.cpp` was split out of `t_v90p4ddec.cpp`
 * for exactly this reason and the same remedy applies here -- `enterDataPhase`
 * reaches none of the six sites (its two step sizes are planted at zero, and
 * zero is ordered against zero on both compilers), so on its own it is green
 * under both and mutation-testable.
 *
 * THE FIXTURE IS `t_v90equ.cpp`'S, copied rather than shared: the object lives
 * in a byte array carried by a union for its alignment, both sides are seeded
 * with the SAME varied pseudorandom bytes and never with zeros (finding F230),
 * the whole object is compared with `diff_eq_obj`, and the bytes from `sizeof`
 * to the end of an over-large slot are compared separately so a store past the
 * object's end is a failure rather than silence (findings F223, F224).  One
 * arena carries every array both sides walk; it is snapshotted before our side
 * runs, restored before the reference does, and the two results compared.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Equalizer.h"

/*
 * Our `dsplibs_debug_level` comes from dsplib/debug.h; the object's own copy
 * is renamed like every other symbol in the reference object, so it has to be
 * declared here.  The two are always set together -- otherwise the sides gate
 * differently and the transcripts diverge for a reason belonging to the
 * harness rather than to the function.
 */
extern "C" {
extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/V90Resampler.h"
/*
 * Explicitly: this fixture takes `sizeof(V90Parameters)` for its arena, and
 * `V90Resampler.h` now only DECLARES the class.
 */
#include "dsplib/V90Parameters.h"

#define SLOT 400

union equ_slot {
	unsigned char raw[SLOT];
	double align_;
};

static union equ_slot ours, theirs;

#define OURS	(*(V90Equalizer *)ours.raw)
#define THEIRS	(*(V90Equalizer *)theirs.raw)

static void
seed(long trial)
{
	unsigned lfsr = 0x2f6du + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)(lfsr >> 3);
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Equalizer),
		      theirs.raw + sizeof(V90Equalizer),
		      SLOT - sizeof(V90Equalizer)) == 0;
}

#define ARR_F	64		/* entries in each float array  */
#define ARR_S	64		/* entries in each short array  */
#define RSLOT	(0xb4 + 32)	/* the V90Resampler, plus slack */

struct equ_arena {
	float		lecoefs[ARR_F];
	float		a18[ARR_F];
	float		lewin[ARR_F];
	float		dfewin[ARR_F];
	float		dfecoefs[ARR_F];
	float		a44[ARR_F];
	short		lemmx[ARR_S];
	short		ad8[ARR_S];
	short		aec[ARR_S];
	short		dfemmx[ARR_S];
	short		a118[ARR_S];
	short		a12c[ARR_S];
	unsigned char	rsamp[RSLOT];
	unsigned char	parm[sizeof(V90Parameters) + 32];
};

static struct equ_arena arena, arena_save, arena_ours;

#define ARENA_PARAMS ((V90Parameters *)arena.parm)
#define ARENA_RSAMP  ((V90Resampler *)arena.rsamp)

/* Varied bytes, never zeros (finding F230). */
static void
fill_arena(long trial)
{
	unsigned char *p = (unsigned char *)&arena;
	unsigned s = 0x4d2fu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < sizeof(arena); i++) {
		s = (s >> 1) ^ (-(int)(s & 1u) & 0xb400u);
		p[i] = (unsigned char)((s >> 3) | 1u);
	}
}

/* Point both objects at the one arena, so every stored pointer agrees. */
static void
wire(V90Equalizer *o)
{
	o->linearEquCoefs = arena.lecoefs;
	o->array_18 = arena.a18;
	o->linearEquWindow = arena.lewin;
	o->dfeWindow = arena.dfewin;
	o->dfeCoefs = arena.dfecoefs;
	o->array_44 = arena.a44;
	o->linearEquMmxCoefs = arena.lemmx;
	o->array_d8 = arena.ad8;
	o->array_ec = arena.aec;
	o->dfeMmxCoefs = arena.dfemmx;
	o->array_118 = arena.a118;
	o->array_12c = arena.a12c;
	o->params = ARENA_PARAMS;
	o->resampler = ARENA_RSAMP;
}

/* One SHORT of skew on every aligned view, so raw and aligned cannot pass
 * for one another. */
static void
wire_mmx(V90Equalizer *o)
{
	o->linearEquMmxCoefsAligned = arena.lemmx + 1;
	o->array_d8Aligned = arena.ad8 + 1;
	o->array_ecAligned = arena.aec + 1;
	o->dfeMmxCoefsAligned = arena.dfemmx + 1;
	o->array_118Aligned = arena.a118 + 1;
	o->array_12cAligned = arena.a12c + 1;
	o->linearEquMmxCoefsSkew = o->array_d8Skew = o->array_ecSkew = 1;
	o->dfeMmxCoefsSkew = o->array_118Skew = o->array_12cSkew = 1;
}

/* The two sides run against the same starting arena, one after the other. */
static void
arena_snapshot(void)
{
	memcpy(&arena_save, &arena, sizeof(arena));
}

static void
arena_switch(void)
{
	memcpy(&arena_ours, &arena, sizeof(arena));
	memcpy(&arena, &arena_save, sizeof(arena));
}

static void
arena_compare(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "struct equ_arena",
		     &arena_ours, &arena, sizeof(arena), tag);
}

/*
 * The fixed-point halves get planted by hand rather than left to the fill:
 * the high half must go negative and the low half must go above 0x7fff, which
 * is the pair `movswl`/`movzwl` exists for, and a fill that never produced
 * 0x8000 in a low half would let a sign-extending reading pass.
 */
static void
plant_mmx_words(unsigned int n, long tag)
{
	static const unsigned short hi_v[] = {
		0x0000u, 0xffffu, 0x8000u, 0x7fffu, 0x0001u, 0xfffeu
	};
	static const unsigned short lo_v[] = {
		0x0000u, 0x8000u, 0xffffu, 0x7fffu, 0x0001u, 0x1234u
	};
	unsigned int i;

	for (i = 0; i < n + 8 && i + 1 < 64; i++) {
		int a = (int)((i + (unsigned)tag) % 6);
		int b = (int)((i * 5u + (unsigned)tag) % 6);

		arena.lemmx[i + 1]  = (short)hi_v[a];
		arena.ad8[i + 1]    = (short)lo_v[b];
		arena.dfemmx[i + 1] = (short)hi_v[b];
		arena.a118[i + 1]   = (short)lo_v[a];
		arena.aec[i + 1]    = (short)hi_v[(a + b) % 6];
		arena.a12c[i + 1]   = (short)lo_v[(a + 2 * b) % 6];
	}
}

/* Floats for the two history arrays: both ends of a short, two values outside
 * it, and -32768 itself, whose magnitude comes back negative. */
static const float mmxhist_v[] = {
	-32768.0f, 32767.0f, 40000.0f, -40000.0f, 0.0f,
	-0.5f, 100.75f, -100.75f, 1.0f
};

/* Exactly float-representable 32-bit words, planted through a factor of 2**30:
 * the high half goes negative and the low half above 0x7fff. */
static const int mmxwit_v[] = {
	32768, -32768, 2147450880, -2147450880, 98304, -98304
};

/* The equaliser, its arena and both filters set up for one trial. */
static void
mmx_setup(long tag, unsigned int le, unsigned int dfe, unsigned int w1c,
	  int pat, float ml, float md, float beta, float dbeta)
{
	unsigned int k;

	seed(tag);
	fill_arena(tag);
	plant_mmx_words(le > dfe ? le : dfe, tag);
	wire(&OURS);
	wire(&THEIRS);
	wire_mmx(&OURS);
	wire_mmx(&THEIRS);

	for (k = 0; k < 64; k++) {
		switch (pat) {
		case 0:
			arena.lecoefs[k] = (float)((int)k - 20) * 0.0625f;
			arena.dfecoefs[k] = (float)((int)k - 9) * 0.125f;
			break;
		case 1:
			arena.lecoefs[k] = (float)mmxwit_v[k % 6]
			    * (1.0f / 1073741824.0f);
			arena.dfecoefs[k] = (float)mmxwit_v[(k + 3) % 6]
			    * (1.0f / 1073741824.0f);
			break;
		default:
			/*
			 * One large tap and a tail of small ones, so a sum
			 * accumulated in extended precision differs from one
			 * accumulated in `float` in the digits that are
			 * printed.
			 */
			arena.lecoefs[k] = (k == 0) ? 1024.0f
			    : 0.0001f * (float)(k % 7 + 1);
			arena.dfecoefs[k] = (k == 0) ? 512.0f
			    : 0.00013f * (float)(k % 5 + 1);
			break;
		}
		arena.a18[k] = mmxhist_v[(k + (unsigned)tag) % 9];
		arena.a44[k] = mmxhist_v[(k * 3u + (unsigned)tag) % 9];
	}

	OURS.linearEquLength = THEIRS.linearEquLength = le;
	OURS.dfeLength = THEIRS.dfeLength = dfe;
	OURS.linearEquHistoryLength = THEIRS.linearEquHistoryLength = w1c;
	OURS.historyIndex = THEIRS.historyIndex = 0x5a5a0000u + (unsigned)pat;
	OURS.historyIndexSaved = THEIRS.historyIndexSaved = 0xdeadbeefu;
	OURS.maxLeCoefValue = THEIRS.maxLeCoefValue = ml;
	OURS.maxDfeCoefValue = THEIRS.maxDfeCoefValue = md;
	OURS.linearEquBeta = THEIRS.linearEquBeta = beta;
	OURS.dfeBeta = THEIRS.dfeBeta = dbeta;

	OURS.mmxMode = THEIRS.mmxMode = 0;
	OURS.mmxArraysPresent = THEIRS.mmxArraysPresent = 1;
	ARENA_PARAMS->ENABLE_EQUALIZER_MMX = 1;
}

/* ================================================== enterDataPhase */

/*
 * enterDataPhase is `enterPhase4`'s dump with a state of 3, a call out to the
 * phase 4 demodulator, and `convertEqualizerToMmx` on the end -- so this
 * fixture is `run_enterphase4`'s and `run_converttommx`'s at once, plus a
 * peer object neither of them needed.
 *
 * THE PEER IS SPLIT AND NOT SHARED, because it is WRITTEN.  Every other
 * pointer the equaliser holds goes to one arena that is snapshotted, replayed
 * and compared; `phase4Demod` cannot, because our side calls our
 * `resetRRNDetector` and the reference side calls the blob's, and the two must
 * be able to disagree.  So each side gets its own 0x351c-byte block seeded
 * from the same bytes -- never zeros, finding F230 -- and the two blocks are
 * compared afterwards.  `params` is the one pointer re-installed in both, and
 * it is the same address on both sides so the field itself compares equal.
 *
 * THREE THINGS ONLY THIS TEST CAN SEE, and each has a grid axis of its own:
 *
 *   - THE ENTRY GUARD ON `mmxMode`.  With it set, the state is still entered
 *     and the detector still re-armed, and nothing at all is printed.  A
 *     reconstruction that put the guard after the dump, or that returned
 *     early before the detector, differs only here.
 *   - THE RETURN, WHICH IS A SECOND READ OF `mmxMode` AFTER THE CONVERSION.
 *     `ENABLE_EQUALIZER_MMX` decides whether `convertEqualizerToMmx` takes,
 *     so the same entry state returns 1 on one arm and 0 on the other; a body
 *     that returned a constant, or that cached the field it read at the top,
 *     passes every object comparison and fails here.
 *   - THE FOUR SUMS, which are locals the object never stores.  Only the
 *     transcript carries them, so the levels are raised and the two captures
 *     compared exactly, as `run_enterphase4` does.
 *
 * THE COUNTERS BELOW COUNT OBSERVABLE DIFFERENCES AND NOT BELIEFS (finding
 * F3509): `sep_ret` counts trials whose RETURN VALUE differs from the previous
 * trial's, and `sep_lines` trials whose printed LINE COUNT differs from the
 * previous one's -- both read off the reference side, both quantities a
 * failing reconstruction would move.
 */

#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
int ref_equ_enterDataPhase(void *self)
	asm("ref__ZN12V90Equalizer14enterDataPhaseEv");
}

#define EDP_P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + 64u)

static unsigned char edp_p4d[2][EDP_P4D_SLOT] __attribute__((aligned(8)));
static unsigned char edp_p4d_seed[EDP_P4D_SLOT];

/* The same varied bytes into both peers, and `params` back on top of them. */
static void
seed_p4d_pair(long trial)
{
	unsigned lfsr = 0x1a7fu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < EDP_P4D_SLOT; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)((lfsr >> 3) | 1u);
		edp_p4d[0][i] = v;
		edp_p4d[1][i] = v;
	}

	((V90Phase4Demodulator *)edp_p4d[0])->params =
	    ((V90Phase4Demodulator *)edp_p4d[1])->params = ARENA_PARAMS;

	/* The seed AFTER `params` is installed, so it is the untouched state. */
	memcpy(edp_p4d_seed, edp_p4d[0], EDP_P4D_SLOT);
}

static int
run_enterdataphase(void)
{
	static const unsigned int len_v[] = { 0u, 1u, 3u, 16u };
	long tag = 1012000;
	int li, pat, mmxin, enable, state;
	int saw_early = 0, saw_mmxin = 0, saw_ret1 = 0, saw_ret0 = 0;
	int saw_zero_len = 0, saw_printed = 0;
	int sep_ret = 0, sep_lines = 0;
	long prev_ret = -1, prev_lines = -1;

	diff_begin("V90Equalizer::enterDataPhase");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;

	for (state = 2; state <= 3; state++)
	    for (li = 0; li < 4; li++)
		for (pat = 0; pat < 3; pat++)
		    for (mmxin = 0; mmxin < 2; mmxin++)
			for (enable = 0; enable < 2; enable++) {
				unsigned int len = len_v[li];
				unsigned int guardlen;
				int early = (state == V90EQU_STATE_DATA);
				int got, want;
				long lines;

				tag++;
				mmx_setup(tag, len, len, len, pat,
					  1024.0f, 512.0f, 0.0f, 0.0f);

				OURS.state = THEIRS.state = state;
				OURS.stateCount = THEIRS.stateCount =
				    0x3c3c0000 + (int)len;
				OURS.mmxMode = THEIRS.mmxMode = mmxin;
				ARENA_PARAMS->ENABLE_EQUALIZER_MMX = enable;

				/*
				 * The detector's argument, moved every trial
				 * and set apart from its two neighbours, so a
				 * body that reached a different parameter
				 * slot shows in the peer comparison.
				 */
				ARENA_PARAMS->PHASE4_R_DETECTION_LENGTH =
				    0x7ac0 + (int)tag;
				ARENA_PARAMS->RRN_R_DETECTION_LENGTH =
				    0x60 + (int)len * 7 + pat;
				ARENA_PARAMS->
				    SD_DETECTOR_DETECTION_COUNTER_THRESHOLD =
				    0x1b30 + (int)tag;

				seed_p4d_pair(tag);
				OURS.phase4Demod = (V90Phase4Demodulator *)
				    edp_p4d[0];
				THEIRS.phase4Demod = (V90Phase4Demodulator *)
				    edp_p4d[1];

				arena_snapshot();
				dsplib_debug_capture_reset();
				got = OURS.enterDataPhase();
				arena_switch();
				want = ref_equ_enterDataPhase(&THEIRS);

				/*
				 * The one field the two sides are MEANT to
				 * differ in: each points at its own peer.
				 * Checked for having been left alone, then
				 * normalised, so the object comparison below
				 * covers the other 0x14c bytes exactly.
				 */
				diff_eq_int("phase4Demod is not written (%ld)",
					    (long)(OURS.phase4Demod
						   == (V90Phase4Demodulator *)
						      edp_p4d[0]
						   && THEIRS.phase4Demod
						   == (V90Phase4Demodulator *)
						      edp_p4d[1]), 1, tag);
				OURS.phase4Demod = THEIRS.phase4Demod =
				    (V90Phase4Demodulator *)edp_p4d[0];

				diff_eq_obj("after enterDataPhase", V90Equalizer,
					    &OURS, &THEIRS, tag);
				arena_compare("the arena after enterDataPhase",
					      tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "the phase 4 demodulator",
					     "V90Phase4Demodulator",
					     edp_p4d[0], edp_p4d[1],
					     sizeof(V90Phase4Demodulator), tag);
				guardlen = EDP_P4D_SLOT
				    - (unsigned)sizeof(V90Phase4Demodulator);
				diff_eq_int("no store past the peer, ours "
					    "(%ld)",
					    memcmp(edp_p4d[0]
						   + sizeof(V90Phase4Demodulator),
						   edp_p4d_seed
						   + sizeof(V90Phase4Demodulator),
						   guardlen) == 0, 1, tag);
				diff_eq_int("no store past the peer, the blob "
					    "(%ld)",
					    memcmp(edp_p4d[1]
						   + sizeof(V90Phase4Demodulator),
						   edp_p4d_seed
						   + sizeof(V90Phase4Demodulator),
						   guardlen) == 0, 1, tag);
				diff_eq_int("no store past the object (%ld)",
					    guard_equal(), 1, tag);
				diff_eq_int("the return (%ld)", (long)got,
					    (long)want, tag);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				lines = (long)dsplib_debug_capture_lines(1);
				if (prev_ret >= 0 && (long)want != prev_ret)
					sep_ret++;
				if (prev_lines >= 0 && lines != prev_lines)
					sep_lines++;
				prev_ret = want;
				prev_lines = lines;

				if (early) {
					saw_early = 1;
					diff_eq_int("the early out printed "
						    "nothing (%ld)", lines, 0,
						    tag);
					diff_eq_int("and returned zero (%ld)",
						    (long)want, 0, tag);
					diff_eq_int("and left the peer alone "
						    "(%ld)",
						    memcmp(edp_p4d[1],
							   edp_p4d_seed,
							   sizeof(V90Phase4Demodulator))
						    == 0, 1, tag);
					continue;
				}

				diff_eq_int("the state was entered (%ld)",
					    (long)THEIRS.state,
					    V90EQU_STATE_DATA, tag);
				diff_eq_int("stateCount is NOT reset (%ld)",
					    (long)THEIRS.stateCount,
					    (long)(0x3c3c0000 + (int)len), tag);
				/*
				 * The detector is re-armed on EVERY non-early
				 * path, including the one that prints nothing.
				 */
				diff_eq_int("the RRN detector was re-armed "
					    "(%ld)",
					    (long)((V90Phase4Demodulator *)
						   edp_p4d[1])->rDetector2
						  .rLimit, 0xb4, tag);

				if (mmxin) {
					saw_mmxin = 1;
					diff_eq_int("the fixed-point entry "
						    "printed nothing (%ld)",
						    lines, 1, tag);
					diff_eq_int("and returned zero (%ld)",
						    (long)want, 0, tag);
					continue;
				}

				saw_printed = 1;
				if (len == 0)
					saw_zero_len = 1;
				if (want)
					saw_ret1 = 1;
				else
					saw_ret0 = 1;

				/*
				 * The two extremes ARE in the object, so they
				 * are recomputed rather than left to the
				 * transcript: over the MAGNITUDE, seeded from
				 * tap 0 and including it (D325).
				 */
				{
					unsigned int k;
					float mx = arena_save.lecoefs[0] < 0.0f
					    ? -arena_save.lecoefs[0]
					    : arena_save.lecoefs[0];
					float mn = mx;

					for (k = 1; k < len; k++) {
						float a = arena_save.lecoefs[k];

						if (a < 0.0f)
							a = -a;
						if (a > mx)
							mx = a;
						if (a < mn)
							mn = a;
					}
					diff_eq_float("maxLeCoefValue",
						      THEIRS.maxLeCoefValue,
						      mx, tag);
					diff_eq_float("minLeCoefValue",
						      THEIRS.minLeCoefValue,
						      mn, tag);
				}
			}

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the early out was taken", saw_early, 1, 0);
	diff_eq_int("the fixed-point entry guard was taken", saw_mmxin, 1, 0);
	diff_eq_int("the dump ran", saw_printed, 1, 0);
	diff_eq_int("a zero-length filter was summarised", saw_zero_len, 1, 0);
	diff_eq_int("the conversion took and returned 1", saw_ret1, 1, 0);
	diff_eq_int("the conversion bailed and returned 0", saw_ret0, 1, 0);
	diff_eq_int("the return value separated trials",
		    sep_ret > 8 ? 1 : 0, 1, 0);
	diff_eq_int("the printed line count separated trials",
		    sep_lines > 8 ? 1 : 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	return run_enterdataphase();
}
