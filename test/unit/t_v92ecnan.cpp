/*
 * t_v92ecnan.cpp -- ONE check, and it is in its own binary on purpose.
 *
 * `V92EchoCanceller::process` opens with a sentinel test on the caller's
 * output block:
 *
 *     10f4e:  d9 00              flds  (%eax)          ; out[0]
 *     10f50:  d8 1d .. .. .. ..  fcomps <177.0f>
 *     10f56:  df e0              fnstsw %ax
 *     10f58:  9e                 sahf
 *     10f59:  75 ..              jne   <filter>
 *
 * -- ONE ordered compare and NO parity test.  FCOM sets C3 for an UNORDERED
 * result exactly as it does for an equal one, so `sahf` puts ZF up either way
 * and the sentinel arm is taken for a NaN as well as for 177.0f: the block is
 * copied straight through, the read cursor is stepped, and the adaption is
 * skipped.
 *
 * WHY IT IS NOT IN `t_v90leaves`, and this is the whole reason the file
 * exists.  IEEE C requires `==` to be false for a NaN and GCC 13 emits the
 * parity test whatever it is told -- `-mno-ieee-fp` is accepted and does
 * nothing, and `-ffinite-math-only` withdraws NaN semantics from the whole
 * translation unit and breaks eleven sites that depend on them (finding F2304).
 * So the modern build runs the FILTER on a NaN block, which moves `echoCoeff`,
 * `state` and `historyIndex`, and every later block of that trial diverges
 * with it.
 *
 * In `t_v90leaves` that was 273 of `V92EchoCanceller::process`'s 4,570 checks,
 * all downstream of one block -- and it made the whole BINARY exit non-zero on
 * the modern build.  `tools/mutate.py` judges a mutant caught by a non-zero
 * exit, so it cannot score a mutation set against a baseline that is already
 * red, and it refuses.  FIVE mutation suites are pinned to `t_v90leaves` --
 * `v90cd`, `v90demapper`, `v90rto`, `v90sbe` and `v92ec`, 122 mutations -- and
 * all five were unscoreable for this one arm.  Findings F2157 and F3002.
 *
 * Splitting the divergent check into its own binary is what that refusal asks
 * for; `t_v90p4dnan` is the worked precedent and `t_v92ecparams` the second.
 * Here only the NaN VALUE moves: `t_v90leaves` still drives every block of
 * every shape and still takes the sentinel arm every seventh block, because
 * the blob's path for 177.0f and its path for a NaN are the same path.  What
 * it can no longer observe is that the second value reaches it, and that is
 * this file.
 *
 * `make period` has no allow-list and passes this file with the object's own
 * compiler, which is the tier that decides.  Findings F6000, F2300 and F2304.
 *
 * THE FIXTURE IS SMALL BECAUSE THE ARM IS.  The sentinel arm reads `params`
 * not at all, calls nothing, and touches four fields: it copies `count`
 * samples, steps `historyIndex` modulo `historyAlloc - (filterLength - 1)`,
 * and leaves `state`, `echoCoeff` and `echoHistory` alone.  So one object per
 * side, one coefficient array, one history and one output block is the whole
 * of it -- but every buffer still carries a compared GUARD past its end
 * (finding F230's argument: the arm that must write nothing is proved by
 * comparing what it did not write).
 *
 * ANTI-VACUITY IS AN OBSERVABLE, NOT A PATH.  A NaN block that came out
 * FILTERED would agree with a filtered reference for ever if the reference
 * were ours, so what is asserted is a property only the unordered arm can
 * produce in the BLOB: the blob's output block is the blob's INPUT block,
 * sample for sample, on a state that would otherwise have adapted.  The
 * ordered reading of the same two numbers cannot produce that -- 177.0f is
 * not equal to a NaN under IEEE, and `out[0]` is not 177.0f.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V92EchoCanceller.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_ec_process(void *self, float *in, float *out, unsigned int count)
	asm("ref__ZN16V92EchoCanceller7processEPfS0_j");
}

#define EC_SLOT		96	/* 60 bytes of object and a guard past it   */
#define ECN_COEFF	40	/* declared floats in echoCoeff             */
#define ECN_HIST	320	/* declared floats in echoHistory           */
#define ECN_GUARD	8	/* compared floats past every buffer        */
#define ECN_LEAD	8	/* and BEFORE the history                   */
#define ECN_BLK		40	/* samples in a driven block                */
#define ECN_PARM	0xdc	/* sizeof(V92Parameters)                    */

/*
 * Written out rather than a union, for `t_v90leaves`' reason: the class
 * declares a destructor, and a union member with a non-trivial one deletes the
 * union's.  Nothing here constructs or destroys the slot; the raw bytes are
 * seeded and the member is called on them.
 */
struct ecn_slot {
	unsigned char raw[EC_SLOT] __attribute__((aligned(8)));
	V92EchoCanceller &o;

	ecn_slot() : o(*(V92EchoCanceller *)raw) { }
};

static struct ecn_slot ec_a, ec_b;

static float ecn_coeff[2][ECN_COEFF + ECN_GUARD];
/*
 * A GUARD AT BOTH ENDS OF THE HISTORY, as in `t_v90leaves`: the class's other
 * writer copies DOWNWARD, so a defect that walked off the FRONT would be
 * invisible to a trailing guard alone.  `ECN_H` is the pointer the object is
 * given; the comparisons cover the whole array.
 */
static float ecn_hist[2][ECN_LEAD + ECN_HIST + ECN_GUARD];
#define ECN_H(side)	(&ecn_hist[side][ECN_LEAD])
static float ecn_out[2][ECN_BLK + ECN_GUARD];
static float ecn_in[ECN_BLK];
static unsigned char ecn_parm[2][ECN_PARM];

static unsigned int lfsr;

/* Varied, and never zero -- findings F223, F224, F230. */
static void
fill_pair(void *a, void *b, unsigned int n, int trial)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	unsigned int i;

	lfsr = 0x1234u + 0x9e37u * (unsigned int)trial + 1u;
	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1)
		       ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
		pa[i] = pb[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
}

static float
ecn_bits(unsigned int u)
{
	float f;

	memcpy(&f, &u, sizeof f);
	return f;
}

/*
 * The two heap pointers hold two different addresses and always will, and the
 * ARMA pointer at +0x04 is null on both sides here, so the compared image
 * blanks the pointer words.  `t_v90leaves`' `ecx_cmp_slot`, offsets included.
 */
static void
ecn_cmp_slot(const char *what, long tag)
{
	unsigned char sa[EC_SLOT], sb[EC_SLOT];

	memcpy(sa, ec_a.raw, EC_SLOT);
	memcpy(sb, ec_b.raw, EC_SLOT);
	memset(sa + 0x00, 0, 8);
	memset(sb + 0x00, 0, 8);
	memset(sa + 0x20, 0, 8);
	memset(sb + 0x20, 0, 8);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92EchoCanceller slot", sa, sb,
		     EC_SLOT, tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * The states worth driving.  The sentinel arm is taken BEFORE the dispatch, so
 * a state that would have adapted (2 and 3), one that only filters (0), one
 * that is counting down to a transition (1) and one that is not an enumerator
 * at all (7) must all come out untouched -- which is the strongest form of
 * "the arm was taken": five different things that would each have happened.
 */
static const int ecn_states[] = { 0, 1, 2, 3, 7 };
#define ECN_NSTATE	((int)(sizeof(ecn_states) / sizeof(ecn_states[0])))

/* The block lengths: the empty one, one sample, and a full block. */
static const unsigned int ecn_counts[] = { 0u, 1u, 8u, ECN_BLK };
#define ECN_NCOUNT	((int)(sizeof(ecn_counts) / sizeof(ecn_counts[0])))

/*
 * The quiet NaN, the signalling NaN, and a NaN with the sign bit set.  All
 * three set C3 on `fcom` and all three are false under IEEE `==`, so the arm
 * has to be taken for every one of them or the reading is right about quiet
 * NaNs and wrong about the encoding.
 */
static const unsigned int ecn_nans[] = {
	0x7fc00000u, 0x7f800001u, 0xffc00000u, 0xffffffffu
};
#define ECN_NNAN	((int)(sizeof(ecn_nans) / sizeof(ecn_nans[0])))

int
main(void)
{
	int st, ci, ni;
	unsigned int lvl;
	int sawCopied = 0, sawStepped = 0, sawWouldAdapt = 0;
	long trial = 0;

	diff_begin("V92EchoCanceller::process: the sentinel on an unordered "
		   "out[0]");

	for (lvl = 0; lvl <= 2u; lvl++) {
		set_level(lvl);

		for (st = 0; st < ECN_NSTATE; st++) {
			for (ci = 0; ci < ECN_NCOUNT; ci++) {
				for (ni = 0; ni < ECN_NNAN; ni++) {
					unsigned int count = ecn_counts[ci];
					unsigned int fl = 16u;
					unsigned int mod = ECN_HIST
							   - (fl - 1u);
					unsigned char before[EC_SLOT];
					float cbefore[ECN_COEFF + ECN_GUARD];
					float hbefore[ECN_LEAD + ECN_HIST
						      + ECN_GUARD];
					unsigned int hi0 = 37u;
					int side, i;

					trial++;

					fill_pair(ec_a.raw, ec_b.raw, EC_SLOT,
						  (int)trial);
					fill_pair(ecn_coeff[0], ecn_coeff[1],
						  (unsigned int)
						  sizeof(ecn_coeff[0]),
						  (int)trial + 1);
					fill_pair(ecn_hist[0], ecn_hist[1],
						  (unsigned int)
						  sizeof(ecn_hist[0]),
						  (int)trial + 2);
					fill_pair(ecn_parm[0], ecn_parm[1],
						  ECN_PARM, (int)trial + 3);

					/*
					 * A REAL echo to adapt to, so that a
					 * side which took the FILTER instead
					 * would move the coefficients rather
					 * than happen to leave them.
					 */
					for (i = 0; i < ECN_HIST; i++)
						ECN_H(0)[i] = ECN_H(1)[i] =
							(i & 1) ? 0.5f : -0.5f;
					for (i = 0; i < (int)fl; i++) {
						float c = (float)((i % 5) - 2)
							  * 0.03125f
							  + 0.015625f;

						ecn_coeff[0][i] =
						    ecn_coeff[1][i] = c;
					}
					for (i = 0; i < ECN_BLK; i++)
						ecn_in[i] = 0.05f
							    * (float)((i % 9)
								      - 4)
							    + 0.125f;

					for (side = 0; side < 2; side++) {
						V92EchoCanceller *e =
						    side == 0 ? &ec_a.o
							      : &ec_b.o;

						e->params = (V92Parameters *)
							    ecn_parm[side];
						e->arma = 0;
						e->echoCoeff = ecn_coeff[side];
						e->echoHistory = ECN_H(side);
						e->filterLength = fl;
						e->filterLengthMinusOne = fl - 1u;
						e->historyAlloc = ECN_HIST;
						e->echoLength = 0x33333333u;
						e->historyIndex = hi0;
						e->state =
						    (V92EchoCancellerState)
						    ecn_states[st];
						e->echoDelay = 120u;
						e->updateDuration = 1u;
						e->updateSampleCount = 0u;
						e->echoBeta =
						    ecn_bits(0x3ca3d70au);
						e->echoBetaDecay =
						    ecn_bits(0x3f7ff972u);
					}

					fill_pair(ecn_out[0], ecn_out[1],
						  (unsigned int)
						  sizeof(ecn_out[0]),
						  (int)trial + 4);
					ecn_out[0][0] = ecn_out[1][0] =
						ecn_bits(ecn_nans[ni]);

					memcpy(before, ec_b.raw, EC_SLOT);
					memcpy(cbefore, ecn_coeff[1],
					       sizeof cbefore);
					memcpy(hbefore, ecn_hist[1],
					       sizeof hbefore);

					dsplib_debug_capture_on = 1;
					dsplib_debug_capture_reset();

					ec_a.o.process(ecn_in, ecn_out[0],
						       count);
					ref_ec_process(&ec_b.o, ecn_in,
						       ecn_out[1], count);

					dsplib_debug_capture_on = 0;

					ecn_cmp_slot("after process", trial);
					diff_eq_obj_(__FILE__, __LINE__,
						     "after process",
						     "the output block and its "
						     "guard", ecn_out[0],
						     ecn_out[1],
						     sizeof(ecn_out[0]), trial);
					diff_eq_obj_(__FILE__, __LINE__,
						     "after process",
						     "echoCoeff and its guard",
						     ecn_coeff[0], ecn_coeff[1],
						     sizeof(ecn_coeff[0]),
						     trial);
					diff_eq_obj_(__FILE__, __LINE__,
						     "after process",
						     "echoHistory and its "
						     "guard", ecn_hist[0],
						     ecn_hist[1],
						     sizeof(ecn_hist[0]),
						     trial);
					diff_eq_int("no store past the object "
						    "(%ld)",
						    memcmp(ec_a.raw
							   + sizeof(ec_a.o),
							   ec_b.raw
							   + sizeof(ec_b.o),
							   EC_SLOT
							   - sizeof(ec_a.o))
						    == 0, 1, trial);
					diff_eq_int("transcript matches (%ld)",
						    strcmp(dsplib_debug_capture_text(0),
							   dsplib_debug_capture_text(1))
						    == 0, 1, trial);
					diff_eq_int("line counts match (%ld)",
						    (int)dsplib_debug_capture_lines(0),
						    (int)dsplib_debug_capture_lines(1),
						    trial);

					/*
					 * THE BLOB'S OWN ANSWER, spelled
					 * without reference to our source.
					 * These are the checks that say the
					 * unordered arm was TAKEN rather than
					 * that the two sides agree.
					 */
					diff_eq_int("the blob left the state "
						    "alone (%ld)",
						    (long)(unsigned int)
						    ec_b.o.state,
						    (long)ecn_states[st],
						    trial);
					diff_eq_int("the blob left echoCoeff "
						    "alone (%ld)",
						    memcmp(cbefore,
							   ecn_coeff[1],
							   sizeof cbefore)
						    == 0, 1, trial);
					diff_eq_int("the blob left echoHistory "
						    "alone (%ld)",
						    memcmp(hbefore,
							   ecn_hist[1],
							   sizeof hbefore)
						    == 0, 1, trial);
					if (count > 1u) {
						diff_eq_int("the blob copied "
							    "the block through "
							    "(%ld)",
							    memcmp(ecn_in + 1,
								   &ecn_out[1][1],
								   (count - 1u)
								   * 4) == 0, 1,
							    trial);
						sawCopied = 1;
					}
					if (count != 0u) {
						diff_eq_int("and stepped the "
							    "read cursor "
							    "(%ld)",
							    (long)
							    ec_b.o.historyIndex,
							    (long)((hi0 + count)
								   % mod),
							    trial);
						sawStepped = 1;
					}
					if (ecn_states[st] == 2
					    || ecn_states[st] == 3)
						sawWouldAdapt = 1;
				}
			}
		}
	}

	set_level(0);

	/*
	 * The arms this file claims to have reached.  Without them a change
	 * that stopped driving the NaN -- by a constant moving, not by anyone
	 * deciding to -- would leave every comparison above passing on nothing.
	 */
	diff_eq_int("a block was copied through", sawCopied, 1, 0);
	diff_eq_int("the read cursor was stepped", sawStepped, 1, 0);
	diff_eq_int("an adapting state was among them", sawWouldAdapt, 1, 0);

	return diff_end();
}
