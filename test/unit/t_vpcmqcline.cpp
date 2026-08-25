/*
 * t_vpcmqcline.cpp -- `VPcmFloModem::qcLineVerification` (.text+0xf750, 779
 * bytes) and `VPcmFloModem::vPcmResetPhase3Modem` (.text+0xf200, 149 bytes)
 * against the blob.
 *
 * ===========================================================================
 * WHY THESE TWO SHARE A BINARY, AND WHY IT IS A NEW ONE
 * ===========================================================================
 *
 * They are the last two of `VPcmV34Progress`'s four entry points and they
 * need the same thing nothing else in this file's neighbourhood has: a WHOLE
 * `VPcmFloModem`, built by `VPCMXF_Create` on both sides.
 *
 * `t_vpcmep3.cpp` was the obvious home for `vPcmResetPhase3Modem` and it
 * cannot be one.  Its fixture hand-builds a `V90Demodulator` graph over
 * static byte arrays and wires the modem slot by hand, which is enough for
 * `enterPhase3`; `vPcmResetPhase3Modem` also calls `V92Modem::reset` and
 * `V92EchoCanceller::reset`, and the second of those loops over `filterLength`
 * zeroing `echoCoeff`, derives `echoLength` from `params->
 * V92_ECHO_DELAY_OFFSET` and calls `arma->reset()` -- three heap blocks and a
 * parameter block that fixture does not stand up, whose seeded lengths would
 * be a two-billion-iteration clear rather than a test.  Adding
 * `VPCMXF_Create` to `t_vpcmep3.cpp` would have meant a THIRD copy of the
 * apparatus below; putting both functions here means one.
 *
 * The measuring half -- both sides built by `VPCMXF_Create`, the comparison
 * surface discovered transitively by walking every word that is a live
 * allocation base on BOTH sides, and the three-way rule for a differing word
 * -- is `t_vpcmrunpcm.cpp`'s and `t_v90rundemod.cpp`'s, and their headers are
 * where the argument for each of those lives.
 *
 * A BINARY OF ITS OWN, not more trials in one of those two.  All four entry
 * points share `VPcmFloModem.cpp`, so a mutation set over that file scored by
 * ONE binary could not tell which function a row belongs to;
 * `test/mutations/suites.json` already maps five sets over this file to five
 * binaries for exactly that reason and `vpcmqcline` is the sixth.
 *
 * ===========================================================================
 * WHAT IS DELIBERATELY NOT DRIVEN, AND WHAT THAT COSTS
 * ===========================================================================
 *
 * `V90Modem::side` is held at 2 for the `qcLineVerification` group, outside
 * {0, 1}, so `V90Modem::progress` fans out to neither half and
 * `V90Demodulator::progress` never runs.  `t_vpcmrunpcm.cpp`'s header carries
 * the argument at length and the first reason decides: `V90Equalizer::process`
 * is a `tools/gccdiverge.json` entry, a declared binary cannot carry a
 * mutation suite (findings 2157 and 3002), and driving the real demodulator
 * would drag that divergence into this binary -- which is required to carry
 * `vpcmqcline`.
 *
 * WHAT THAT COSTS, stated rather than hidden.  `modem.progress(rxbits, *nrx,
 * in, n)` reaches a callee that reads none of its four arguments, so their
 * ORDER is not proved here.  Worse, and named because it is the trap:
 * `*nrx` and `*nbits` are zeroed on EVERY path of `qcLineVerification`, so
 * neither can witness that `progress` ran at all -- a test watching `*nrx`
 * for evidence would be watching the one word the function guarantees is
 * zero.  At level 2 the illegal-side report is the only witness there is, and
 * the transcript comparison is what carries it.  What IS proved is that none
 * of the four buffers is written by anything else in the function, asserted
 * element by element.
 *
 * `vPcmResetPhase3Modem` has no such constraint and is driven on the
 * CONSTRUCTED configuration: `VPCMXF_Create(0, ...)` sets `side` to
 * `V90_MODEM_SIDE_ANALOG` and the V92Modem to analog with it, so
 * `V90Modem::reset` really resets the demodulator, `V92Modem::reset` really
 * packs the DIL and resets the V.92 modulator, and `V92EchoCanceller::reset`
 * really clears its filter.  Side 2 is swept as well, for the illegal arm.
 *
 * `txbits` IS DEAD.  There is no reference to `0x48(%esp)` anywhere in
 * `qcLineVerification`'s 779 bytes (finding 7604), so no mutation can ever be
 * caught on it.  The buffer is seeded and asserted untouched, and this is
 * said here rather than left as a fixture that looks incomplete.
 *
 * ===========================================================================
 * THE AXES THIS SET EXISTS TO SEPARATE
 * ===========================================================================
 *
 *   1. `qcVerifyState` REACHES ALL THREE OF 0, 1 AND 2.  The common tail
 *      branches on `== 1` and the silence half on `== 2`, so a two-valued
 *      sweep leaves one of the two tests unmeasured.
 *   2. `qcSampleCount` STRADDLES 480 IN BOTH DIRECTIONS IN THE 0x3b ARM, and
 *      the seeds are chosen for `seed + n` rather than for `seed`, because
 *      `qcSampleCount += (int)n` runs BEFORE the switch.  480 exactly is
 *      driven too: `jle` against `jl` parts there and nowhere else.
 *   3. THE SILENCE COUNT IS SIGNED AND COUNTS UP THROUGH ZERO.  `-384` is the
 *      constant; the `>= 0` test is driven at -1, 0 and +1 after the add.
 *   4. THE DEBUG SWEEP IS {0, 1, 2}.  Every one of the eight gates is `> 1`,
 *      and a {0, 2} sweep cannot separate `> 1` from `> 0`.
 *   5. `verificationStatus` IS TAKEN WITH A `movzwl`, AND THE HIGH HALF IS
 *      NOT DRIVEN TWO-SIDEDLY TODAY.  See the QC_WIDE_STATUS block below --
 *      the axis is parked against a defect in `src/`, not omitted, and the
 *      width is still measured on the blob alone by `run_status_width`.
 *   6. `enterWaitForANSpcmDrop` IS IDEMPOTENT.  It returns at once when the
 *      state already holds `P3D_STATE_WAIT_FOR_ANS_PCM_DROP`, so both the
 *      latched and the unlatched entry are driven and each is asserted to
 *      leave a DIFFERENT phase 3 demodulator.
 *   7. `vPcmResetPhase3Modem`'s `pcmSessionType` IS NON-ZERO IN HALF THE
 *      TRIALS.  With it zero, "passes the session type to `setSessionFlag`"
 *      and "passes 0" are the same program (findings 7458 and 7105) -- and
 *      the same seeding rule puts `sweepCounter`, `sineWave.phase`,
 *      `word_7f60`, `word_7f64`, `byte_6118` and the retrain bit away from
 *      the values the function writes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34fsk.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void *VPCMXF_Create(int digitalSide, void *v34Object,
		    struct _tagModemParameters *dpRuntime,
		    unsigned int durationMs, int mode);
void VPCMXF_Delete(void *self);
void *ref_VPCMXF_Create(int digitalSide, void *v34Object,
			struct _tagModemParameters *dpRuntime,
			unsigned int durationMs, int mode);
void ref_VPCMXF_Delete(void *self);

int ref_qcLineVerification(void *self, float *in, float *out, unsigned int n,
			   int *rxbits, int *nrx, int *txbits, int *nbits)
	asm("ref__ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_");

void ref_vPcmResetPhase3Modem(void *self)
	asm("ref__ZN12VPcmFloModem20vPcmResetPhase3ModemEv");

/*
 * NOT UNDER TEST HERE, AND CALLED AS FIXTURE, for t_v90rundemod.cpp's reason:
 * a freshly constructed Phase 2 record holds a null `L2`, and this is the
 * object's own way of filling it.  Each side calls its own.
 */
void ref_setPhaseIIinfo(void *self, int *info0, int rtd)
	asm("ref__ZN12VPcmFloModem14setPhaseIIinfoEPii");
}

/* ============================================================== the fixture */

#define FLO_SIZE	0x7f68

typedef char vpcmqc_is_0x7f68[(sizeof(VPcmFloModem) == FLO_SIZE) ? 1 : -1];

#define NSAMP		48		/* durationMs 5 -> 5 * 9.6 + 0.5 */
#define DURATION_MS	5

#define MP_SLOT		(sizeof(struct _tagModemParameters))
#define MPARAMS(s)	((struct _tagModemParameters *)mparams[s])

/*
 * `V90Phase3Demodulator::state`'s latch value, spelled as the enumerator so
 * that the number stays in the one header that owns it.
 */
#define P3D_WAIT_FOR_ANS_PCM_DROP	((unsigned)P3D_STATE_WAIT_FOR_ANS_PCM_DROP)

static struct v34_object v34obj[2];
static unsigned char mparams[2][MP_SLOT] __attribute__((aligned(8)));
static unsigned char *base[2];

static float sig_in[2][NSAMP];
static float sig_out[2][NSAMP];
static int rxbits[2][NSAMP];
static int txbits[2][NSAMP];
static int nrx[2];
static int nbits[2];

static VPcmFloModem *
V(int side)
{
	return (VPcmFloModem *)base[side];
}

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

/* ==================================================== the discovered regions */

#define MAXREG		512
#define MAXLIVE		4096

struct region {
	unsigned char	*a;
	unsigned char	*b;
	unsigned	size;
	unsigned char	*pre_a;
	unsigned char	*pre_b;
};

static struct region reg[MAXREG];
static int nreg;
static void *live[MAXLIVE];
static int nlive;
static int region_overflow;
static int lopsided;

static int
is_live_base(const void *p)
{
	int i;

	if (p == 0)
		return 0;
	for (i = 0; i < nlive; i++)
		if (live[i] == p)
			return 1;
	return 0;
}

static int
already(const void *p)
{
	int i;

	for (i = 0; i < nreg; i++)
		if (reg[i].a == p || reg[i].b == p)
			return 1;
	return 0;
}

static void
add_region(void *a, void *b, unsigned size)
{
	struct region *r;

	if (nreg >= MAXREG) {
		region_overflow = 1;
		return;
	}
	r = &reg[nreg++];
	r->a = (unsigned char *)a;
	r->b = (unsigned char *)b;
	r->size = size;
	r->pre_a = (unsigned char *)malloc(size);
	r->pre_b = (unsigned char *)malloc(size);
}

static void
free_regions(void)
{
	int i;

	for (i = 0; i < nreg; i++) {
		free(reg[i].pre_a);
		free(reg[i].pre_b);
	}
	nreg = 0;
}

static void
discover(void)
{
	int r;
	unsigned off;

	region_overflow = 0;
	lopsided = 0;
	nlive = harness_alloc_live_set(live, MAXLIVE);

	add_region(base[0], base[1], FLO_SIZE);
	add_region(&v34obj[0], &v34obj[1], sizeof(struct v34_object));
	add_region(mparams[0], mparams[1], (unsigned)MP_SLOT);

	for (r = 0; r < nreg; r++) {
		for (off = 0; off + 4 <= reg[r].size; off += 4) {
			void *pa, *pb;
			unsigned sa, sb;

			memcpy(&pa, reg[r].a + off, sizeof pa);
			memcpy(&pb, reg[r].b + off, sizeof pb);

			if (is_live_base(pa) != is_live_base(pb)) {
				lopsided++;
				continue;
			}
			if (!is_live_base(pa))
				continue;
			if (already(pa) || already(pb))
				continue;

			sa = harness_alloc_reqsize(pa);
			sb = harness_alloc_reqsize(pb);
			add_region(pa, pb, sa < sb ? sa : sb);
		}
	}
}

static void
snapshot(void)
{
	int r;

	for (r = 0; r < nreg; r++) {
		memcpy(reg[r].pre_a, reg[r].a, reg[r].size);
		memcpy(reg[r].pre_b, reg[r].b, reg[r].size);
	}
}

static int
locate(const void *p, int side, unsigned *off)
{
	unsigned long v = (unsigned long)p;
	int k;

	for (k = 0; k < nreg; k++) {
		unsigned char *b = side == 0 ? reg[k].a : reg[k].b;

		if (v >= (unsigned long)b
		    && v < (unsigned long)b + reg[k].size) {
			*off = (unsigned)(v - (unsigned long)b);
			return k;
		}
	}
	return -1;
}

static long words_equal;
static long words_corresponded;
static long words_static;
static int nregion_last;

/*
 * Which trial is being compared, in the trial's own words.  A failure report
 * that names only an index makes the reader count rows to find out what was
 * being driven, which is exactly the step that gets skipped.
 */
static const char *cur_what = "";

/*
 * THE THREE CASES, and t_vpcmrunpcm.cpp's argument for each.  This file meets
 * only two of them: nothing either function reaches installs a static table
 * into the compared graph, so a differing word that neither side can resolve
 * AND that either side wrote is a failure with no exemption at all.  If a
 * third case ever appears here it must be NAMED, for finding 7521's reason --
 * a general rule silently swallowed seven real mutations in t_vpcmrunpcm.cpp,
 * because a pair of wrong small integers is indistinguishable from a pair of
 * addresses and no property of a WORD separates them.
 */
static void
compare_regions(long trial)
{
	int r;
	int reported = 0;

	for (r = 0; r < nreg; r++) {
		unsigned i;

		for (i = 0; i + 4 <= reg[r].size; i += 4) {
			unsigned int va, vb, pva, pvb;
			void *pa, *pb;
			unsigned oa = 0, ob = 0;
			int ka, kb;
			int bad;

			memcpy(&va, reg[r].a + i, 4);
			memcpy(&vb, reg[r].b + i, 4);
			if (va == vb) {
				words_equal++;
				continue;
			}
			memcpy(&pva, reg[r].pre_a + i, 4);
			memcpy(&pvb, reg[r].pre_b + i, 4);

			memcpy(&pa, reg[r].a + i, sizeof pa);
			memcpy(&pb, reg[r].b + i, sizeof pb);
			ka = locate(pa, 0, &oa);
			kb = locate(pb, 1, &ob);

			if (ka >= 0 && kb >= 0) {
				bad = ka != kb || oa != ob;
				if (!bad)
					words_corresponded++;
			} else if (pva != pvb && va == pva && vb == pvb) {
				bad = 0;
				words_static++;
			} else {
				bad = 1;
			}

			diff_eq_int("region %ld word", bad, 0, (long)r);
			if (bad && reported < 8) {
				reported++;
				printf("    trial %ld (%s) region %d offset "
				       "%u: ours 0x%08x (was 0x%08x) blob "
				       "0x%08x (was 0x%08x) ka %d/%u kb %d/%u\n",
				       trial, cur_what, r, i, va, pva, vb, pvb,
				       ka, oa, kb, ob);
				fflush(stdout);
			}
		}

		for (; i < reg[r].size; i++)
			diff_eq_int("region %ld tail byte",
				    reg[r].a[i] != reg[r].b[i], 0, (long)r);
	}
}

/* ============================================================ the two sides */

static int info0_bits[48];

/*
 * `L2` is filled by the object's own `setPhaseIIinfo`, each side calling its
 * own.  `V90PreFilter::selectFilter` reaches `autoSelection` on the default
 * `PRE_FILTER_GAIN`, and that reads `L2` through a pointer a freshly
 * constructed Phase 2 record leaves null.
 */
static void
fill_phase2(int s)
{
	int i;

	for (i = 0; i < 48; i++)
		info0_bits[i] = (i * 7 + 3) & 1;
	if (s == 0)
		V(0)->setPhaseIIinfo(info0_bits, 37);
	else
		ref_setPhaseIIinfo(base[1], info0_bits, 37);
}

/*
 * `digitalSide` PICKS WHICH MODEM IS BUILT, and it is not a knob but the only
 * way to reach the digital arm.  `VPCMXF_Create` sets `side` to
 * `(digitalSide == 0)`, so an analogue object has a `V90Demodulator` and a
 * NULL `modulator` -- and `V90Modem::setSessionFlag` dereferences whichever
 * one `side` names.  Poking `side` to 0 on an analogue object is therefore a
 * null dereference in the fixture and not a test of anything; the digital arm
 * needs a digitally built object, which is what this argument is for.  Side 2
 * is safe on either, because neither `setSessionFlag` nor `reset` touches a
 * pointer on the illegal arm.
 */
static int
build(long trial, int digitalSide)
{
	memset(&v34obj[0], 0, sizeof v34obj[0]);
	memset(&v34obj[1], 0, sizeof v34obj[1]);
	memset(mparams[0], 0, MP_SLOT);
	memset(mparams[1], 0, MP_SLOT);

	harness_alloc_reset();
	base[0] = (unsigned char *)VPCMXF_Create(digitalSide, &v34obj[0],
						 MPARAMS(0), DURATION_MS, 3);
	base[1] = (unsigned char *)ref_VPCMXF_Create(digitalSide, &v34obj[1],
						     MPARAMS(1), DURATION_MS,
						     3);

	diff_eq_int("both sides constructed (%ld)",
		    base[0] != 0 && base[1] != 0, 1, trial);
	if (base[0] == 0 || base[1] == 0)
		return 0;

	fill_phase2(0);
	fill_phase2(1);
	return 1;
}

static void
demolish(void)
{
	free_regions();
	VPCMXF_Delete(base[0]);
	ref_VPCMXF_Delete(base[1]);
	base[0] = 0;
	base[1] = 0;
}

static void
seed_buffers(long trial)
{
	int i;

	lfsr = 0x51a7u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < NSAMP; i++) {
		float a = (float)((int)nextb() - 128) * 0.013671875f
			  + (float)i * 0.0009765625f;
		float b = (float)((int)nextb() - 128) * 0.005859375f;

		sig_in[0][i] = sig_in[1][i] = a;
		/*
		 * THE OUTPUT BUFFER IS SEEDED AWAY FROM ZERO, which is what
		 * makes the silence half's clear loop visible: a zero-filled
		 * buffer cannot tell "written with zeros" from "not written",
		 * and the tone half's `generate` from a clear that stopped
		 * one sample short.  Finding 7105's rule.
		 */
		sig_out[0][i] = sig_out[1][i] = b != 0.0f ? b : 0.5f;
		rxbits[0][i] = rxbits[1][i] = (int)nextb();
		txbits[0][i] = txbits[1][i] = (int)nextb() + 0x1000;
	}
	nrx[0] = nrx[1] = 7;
	nbits[0] = nbits[1] = 11;
}

/* ==================================================== qcLineVerification */

struct qc_trial {
	const char	*what;
	unsigned	w3c;		/* the demodulator's +0x3c        */
	unsigned	verifyState;	/* qcVerifyState                  */
	int		sampleCount;	/* qcSampleCount BEFORE the add   */
	unsigned	terminate;	/* qcTerminateRequested           */
	unsigned	p3dStatus;	/* the phase 3 demodulator's      */
	unsigned	p3dState;	/* ...and its state               */
	float		phase;		/* sineWave.phase                 */
	unsigned	n;		/* the sample count argument      */
	unsigned	level;
};

/*
 * ===========================================================================
 * ONE AXIS IS PARKED, AND IT IS PARKED AGAINST A DEFECT IN `src/`
 * ===========================================================================
 *
 * The object takes the phase 3 demodulator's verification status with a
 * `movzwl` -- `movzwl 0x41c(%ecx),%eax` at .text+0xf842 and again at +0xf8ea,
 * then `mov %eax,0x6fb4(%esi)` -- so the SOURCE is sixteen bits wide and the
 * 32-bit result is stored.  include/dsplib/VPcmFloModem.h records exactly
 * that and calls it CLAUDE.md's forced column;
 * src/pump/v90/VPcmFloModem.cpp copies the whole 32-bit field at both sites
 * and does not narrow.
 *
 * `V90Phase3Demodulator::verificationStatus` is a full 32-bit field -- its
 * writers are `movl $0x0` and `movl $0x1` -- so no in-object path can put a
 * value with a non-zero HIGH half there, and the two readings agree over
 * every value the field actually holds.  That is finding 613's shape exactly:
 * a difference no differential test can see unless the fixture puts a value
 * there that the object's own writers cannot.
 *
 * A seed of 0x1234abcd DOES make it visible, and this file measured it: ours
 * stores 0x1234abcd and the blob stores 0x0000abcd, at VPcmFloModem +0x6fb4,
 * on eight trials and in the level-2 transcript beside them.  The two-sided
 * seeds below are therefore held inside sixteen bits until the source is
 * corrected -- `QC_STATUS` and the four rows around it -- and the width is
 * measured on the BLOB ALONE by `run_status_width`, which is a claim about
 * the object and passes today.  Restore `QC_STATUS` to 0x1234abcd the moment
 * `src/` narrows, and the axis is two-sided again.
 *
 * ===========================================================================
 *
 * The sample-count seeds are chosen for `sampleCount + n`, not for
 * `sampleCount`: the add happens before the switch.  With n = 48 the four rows below land on 432,
 * 480, 481 and 528 -- one under, one exactly at, and two over the 480-sample
 * threshold the object compares against.
 */
/*
 * The two-sided verification status.  SIXTEEN BITS AND NO MORE; see the block
 * above for why, and restore it to 0x1234abcd when src/ narrows the copy.
 */
#define QC_STATUS	0xabcdu

#define QC_BELOW	384	/* + 48 = 432 */
#define QC_EQUAL	432	/* + 48 = 480 */
#define QC_JUST_OVER	433	/* + 48 = 481 */
#define QC_OVER		480	/* + 48 = 528 */

static const struct qc_trial qc_v[] = {
	/*
	 * ARM 0x3a.  Every entry state, because the arm overwrites all four
	 * words and the tail then reads two of them back.
	 */
	{ "0x3a, state 0", 0x3a, 0, 100, 0, QC_STATUS, 0, 0.0f, NSAMP, 0 },
	{ "0x3a, state 1", 0x3a, 1, 100, 1, QC_STATUS, 0, 0.25f, NSAMP, 0 },
	{ "0x3a, state 2", 0x3a, 2, -20, 0, QC_STATUS, 0, -0.5f, NSAMP, 0 },
	{ "0x3a, already waiting", 0x3a, 0, 100, 0, 0x0000beefu,
	  P3D_WAIT_FOR_ANS_PCM_DROP, 1.5f, NSAMP, 0 },
	{ "0x3a, status 0", 0x3a, 0, 100, 0, 0u, 0, 0.0f, NSAMP, 0 },
	{ "0x3a, status 1", 0x3a, 0, 100, 0, 1u, 0, 0.0f, NSAMP, 0 },
	{ "0x3a, status all sixteen bits", 0x3a, 0, 100, 0, 0xffffu, 0,
	  0.0f, NSAMP, 0 },
	{ "0x3a, status top bit of the half", 0x3a, 0, 100, 0, 0x8000u, 0,
	  0.0f, NSAMP, 0 },

	/* ARM 0x3b, TONEq RUNNING, on both sides of 480 and exactly at it. */
	{ "0x3b, running, below", 0x3b, 1, QC_BELOW, 0, QC_STATUS, 0,
	  0.125f, NSAMP, 0 },
	{ "0x3b, running, equal", 0x3b, 1, QC_EQUAL, 0, QC_STATUS, 0,
	  0.125f, NSAMP, 0 },
	{ "0x3b, running, just over", 0x3b, 1, QC_JUST_OVER, 0, QC_STATUS,
	  0, 0.125f, NSAMP, 0 },
	{ "0x3b, running, over", 0x3b, 1, QC_OVER, 0, QC_STATUS, 0, 0.125f,
	  NSAMP, 0 },
	{ "0x3b, running, below, latched", 0x3b, 1, QC_BELOW, 1, QC_STATUS,
	  0, 0.125f, NSAMP, 0 },
	{ "0x3b, running, over, latched", 0x3b, 1, QC_OVER, 1, QC_STATUS, 0,
	  0.125f, NSAMP, 0 },
	{ "0x3b, running, negative count", 0x3b, 1, -400, 0, QC_STATUS, 0,
	  0.125f, NSAMP, 0 },

	/* ARM 0x3b, TONEq NOT RUNNING -- the "no completion status" half. */
	{ "0x3b, not running, state 0", 0x3b, 0, 200, 0, QC_STATUS, 0,
	  0.75f, NSAMP, 0 },
	{ "0x3b, not running, state 2", 0x3b, 2, 200, 1, QC_STATUS, 0,
	  0.75f, NSAMP, 0 },
	{ "0x3b, not running, state 3", 0x3b, 3, 200, 0, QC_STATUS, 0,
	  0.75f, NSAMP, 0 },

	/*
	 * NO ARM AT ALL: the tail on its own, which is where the silence
	 * half's `>= 0` test lives.  -49, -48 and -47 land on -1, 0 and +1.
	 */
	{ "default, state 0", 0x00, 0, 0, 0, QC_STATUS, 0, 0.0f, NSAMP, 0 },
	{ "default, state 1, latched, over", 0x11, 1, QC_OVER, 1, QC_STATUS,
	  0, 0.375f, NSAMP, 0 },
	{ "default, state 1, latched, equal", 0x11, 1, QC_EQUAL, 1,
	  QC_STATUS, 0, 0.375f, NSAMP, 0 },
	{ "default, state 1, latched, below", 0x11, 1, QC_BELOW, 1,
	  QC_STATUS, 0, 0.375f, NSAMP, 0 },
	{ "default, state 1, unlatched, over", 0x11, 1, QC_OVER, 0,
	  QC_STATUS, 0, 0.375f, NSAMP, 0 },
	{ "default, state 2, count -1", 0x39, 2, -49, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 0 },
	{ "default, state 2, count 0", 0x39, 2, -48, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 0 },
	{ "default, state 2, count +1", 0x39, 2, -47, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 0 },
	{ "default, state 2, count large", 0x3c, 2, 1000, 1, QC_STATUS, 0,
	  0.0f, NSAMP, 0 },
	{ "default, state 3, count 0", 0x2b, 3, -48, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 0 },

	/* n = 0 and n = 1: the clear loop and `generate` at their extremes. */
	{ "n = 0, state 1", 0x11, 1, QC_OVER, 0, QC_STATUS, 0, 0.5f, 0, 0 },
	{ "n = 0, state 2", 0x11, 2, 0, 0, QC_STATUS, 0, 0.5f, 0, 0 },
	{ "n = 1, state 1", 0x11, 1, 479, 1, QC_STATUS, 0, 0.5f, 1, 0 },
	{ "n = 1, state 2", 0x11, 2, -1, 0, QC_STATUS, 0, 0.5f, 1, 0 },
	{ "n = 1, 0x3b running", 0x3b, 1, 479, 0, QC_STATUS, 0, 0.5f, 1, 0 },

	/* And the eight gates, at level 1 and at level 2. */
	{ "level 1, 0x3a", 0x3a, 0, 100, 0, QC_STATUS, 0, 0.0f, NSAMP, 1 },
	{ "level 1, 0x3b running over", 0x3b, 1, QC_OVER, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 1 },
	{ "level 1, 0x3b running below", 0x3b, 1, QC_BELOW, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 1 },
	{ "level 1, 0x3b not running", 0x3b, 0, 200, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 1 },
	{ "level 1, tail latched over", 0x11, 1, QC_OVER, 1, QC_STATUS, 0,
	  0.0f, NSAMP, 1 },
	/*
	 * LEVEL 1 ON THE SILENCE-OVER GATE, and it is here because the
	 * mutation set asked for it: `the silence-over report is made at level
	 * 1 as well` was the one uncaught row of 58, because no level-1 trial
	 * reached `qcVerifyState == 2 && qcSampleCount >= 0`.  Every gate in
	 * this function is `> 1` and each needs its own level-1 witness.
	 */
	{ "level 1, silence over", 0x11, 2, -48, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 1 },
	{ "level 2, 0x3a", 0x3a, 0, 100, 0, QC_STATUS, 0, 0.0f, NSAMP, 2 },
	{ "level 2, 0x3a already waiting", 0x3a, 0, 100, 0, 0x0000beefu,
	  P3D_WAIT_FOR_ANS_PCM_DROP, 0.0f, NSAMP, 2 },
	{ "level 2, 0x3b running below", 0x3b, 1, QC_BELOW, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 2 },
	{ "level 2, 0x3b running over", 0x3b, 1, QC_OVER, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 2 },
	{ "level 2, 0x3b not running", 0x3b, 0, 200, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 2 },
	{ "level 2, tail latched over", 0x11, 1, QC_OVER, 1, QC_STATUS, 0,
	  0.0f, NSAMP, 2 },
	{ "level 2, silence over", 0x11, 2, -48, 0, QC_STATUS, 0, 0.0f,
	  NSAMP, 2 },
	{ "level 2, silence still running", 0x11, 2, -400, 0, QC_STATUS, 0,
	  0.0f, NSAMP, 2 }
};

#define NQC	((int)(sizeof(qc_v) / sizeof(qc_v[0])))

static void
qc_poke(int s, const struct qc_trial *t)
{
	VPcmFloModem *m = V(s);
	V90Demodulator *d = m->modem.demodulator;

	/*
	 * Outside {0, 1}, so `V90Modem::progress` fans out to neither half.
	 * See the file comment for what that costs.
	 */
	m->modem.side = (V90ModemSide)2;

	d->word_3c = t->w3c;
	d->phase3Demodulator->verificationStatus = t->p3dStatus;
	d->phase3Demodulator->state = (Phase3DemodulatorState)t->p3dState;

	m->qcVerifyState = t->verifyState;
	m->qcSampleCount = t->sampleCount;
	m->qcTerminateRequested = t->terminate;
	/*
	 * AWAY FROM EVERY VALUE THE FUNCTION CAN WRITE, so that the 0x3a arm's
	 * copy is visible whatever the phase 3 demodulator holds.
	 */
	m->verificationStatus = 0xc0de0000u;
	m->sineWave.phase = t->phase;
}

static int
run_qcline(void)
{
	int trial, i;
	int sawRet1 = 0, sawRet0 = 0;
	int sawTone = 0, sawSilence = 0;
	int sawState[4];
	int sawOver = 0, sawUnder = 0, sawEqual = 0;
	int sawLatched = 0, sawUnlatched = 0;
	int sawEnter = 0, sawAlready = 0;
	int quiet = 0, loud = 0;

	diff_begin("VPcmFloModem::qcLineVerification against the blob, over "
		   "both event codes, all three verify states and the "
		   "480-sample threshold from both sides");

	for (i = 0; i < 4; i++)
		sawState[i] = 0;

	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < NQC; trial++) {
		const struct qc_trial *t = &qc_v[trial];
		int ra, rb;
		int allocs;
		unsigned prevState[2];
		int allZero, anyNonZero;

		if (!build(trial, 0))
			continue;

		qc_poke(0, t);
		qc_poke(1, t);
		seed_buffers(trial);

		prevState[0] = V(0)->modem.demodulator->phase3Demodulator
		    ->state;
		prevState[1] = V(1)->modem.demodulator->phase3Demodulator
		    ->state;

		discover();
		diff_eq_int("no region overflow (%ld)", region_overflow, 0,
			    trial);
		diff_eq_int("no lopsided pointer word (%ld)", lopsided, 0,
			    trial);
		snapshot();
		nregion_last = nreg;
		cur_what = t->what;

		allocs = harness_alloc.allocs;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = t->level;
		ref_dsplibs_debug_level = t->level;

		ra = V(0)->qcLineVerification(sig_in[0], sig_out[0], t->n,
					      rxbits[0], &nrx[0], txbits[0],
					      &nbits[0]);
		rb = ref_qcLineVerification(base[1], sig_in[1], sig_out[1],
					    t->n, rxbits[1], &nrx[1],
					    txbits[1], &nbits[1]);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("return value (%ld)", ra, rb, trial);
		diff_eq_int("the return is 0 or 1 (%ld)", ra == 0 || ra == 1,
			    1, trial);
		diff_eq_int("nothing was allocated (%ld)",
			    harness_alloc.allocs - allocs, 0, trial);

		for (i = 0; i < NSAMP; i++)
			diff_eq_float("in[%ld] is untouched", sig_in[0][i],
				      sig_in[1][i], (long)i);
		for (i = 0; i < NSAMP; i++)
			diff_eq_float("out[%ld]", sig_out[0][i],
				      sig_out[1][i], (long)i);
		for (i = 0; i < NSAMP; i++)
			diff_eq_int("rxbits[%ld]", rxbits[0][i], rxbits[1][i],
				    (long)i);
		/* Dead, and asserted rather than assumed; finding 7604. */
		for (i = 0; i < NSAMP; i++)
			diff_eq_int("txbits[%ld] is untouched", txbits[0][i],
				    txbits[1][i], (long)i);
		diff_eq_int("*nrx (%ld)", nrx[0], nrx[1], trial);
		diff_eq_int("*nbits (%ld)", nbits[0], nbits[1], trial);
		diff_eq_int("*nrx is zeroed on every path (%ld)", nrx[1], 0,
			    trial);
		diff_eq_int("*nbits is zeroed on every path (%ld)", nbits[1],
			    0, trial);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		diff_eq_int("the transcript line counts agree (%ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		compare_regions(trial);

		/*
		 * THE 0x3a ARM'S COPY IS SIXTEEN BITS WIDE.  `movzwl` at the
		 * source, so a 32-bit copy would carry the high half across
		 * and the seed above is chosen to have one.
		 */
		if (t->w3c == 0x3a || (t->w3c == 0x3b && t->verifyState != 1))
			diff_eq_int("verificationStatus is the demodulator's "
				    "(%ld)", (long)V(1)->verificationStatus,
				    (long)(t->p3dStatus & 0xffffu), trial);
		else
			diff_eq_int("verificationStatus is untouched (%ld)",
				    (long)V(1)->verificationStatus,
				    (long)0xc0de0000u, trial);

		/* Which state the trial LEFT, on the blob's side. */
		if (V(1)->qcVerifyState < 4)
			sawState[V(1)->qcVerifyState] = 1;
		if (t->verifyState < 4)
			sawState[t->verifyState] = 1;

		if (rb)
			sawRet1 = 1;
		else
			sawRet0 = 1;

		allZero = 1;
		anyNonZero = 0;
		for (i = 0; i < (int)t->n; i++) {
			if (sig_out[1][i] != 0.0f) {
				allZero = 0;
				anyNonZero = 1;
			}
		}
		if (t->n > 0) {
			if (anyNonZero)
				sawTone = 1;
			if (allZero)
				sawSilence = 1;
		}

		if (t->w3c == 0x3b && t->verifyState == 1) {
			int after = t->sampleCount + (int)t->n;

			if (after > 480)
				sawOver = 1;
			else if (after == 480)
				sawEqual = 1;
			else
				sawUnder = 1;
		}
		if (t->terminate)
			sawLatched = 1;
		else
			sawUnlatched = 1;

		if (t->w3c == 0x3a) {
			if (prevState[1] != P3D_WAIT_FOR_ANS_PCM_DROP)
				sawEnter = 1;
			else
				sawAlready = 1;
			diff_eq_int("enterWaitForANSpcmDrop latched the "
				    "state (%ld)",
				    (long)V(1)->modem.demodulator
				    ->phase3Demodulator->state,
				    (long)P3D_WAIT_FOR_ANS_PCM_DROP, trial);
		}

		if (dsplib_debug_capture_lines(1) == 0)
			quiet++;
		else
			loud++;

		demolish();
	}

	dsplib_debug_capture_on = 0;

	printf("    surface: %d regions, %ld words equal, %ld corresponding "
	       "pointer pairs, %ld borrowed tables untouched\n",
	       nregion_last, words_equal, words_corresponded, words_static);
	diff_eq_int("the exempt classes are a minority of the surface",
		    (words_corresponded + words_static) * 20 < words_equal, 1,
		    0);

	diff_eq_int("the period ends at least once", sawRet1, 1, 0);
	diff_eq_int("...and does not end most of the time", sawRet0, 1, 0);
	diff_eq_int("the tone half filled the buffer", sawTone, 1, 0);
	diff_eq_int("the silence half zeroed it", sawSilence, 1, 0);
	diff_eq_int("qcVerifyState 0 was reached", sawState[0], 1, 0);
	diff_eq_int("qcVerifyState 1 was reached", sawState[1], 1, 0);
	diff_eq_int("qcVerifyState 2 was reached", sawState[2], 1, 0);
	diff_eq_int("the 480-sample threshold was crossed from above",
		    sawOver, 1, 0);
	diff_eq_int("...and from below", sawUnder, 1, 0);
	diff_eq_int("...and landed on it exactly", sawEqual, 1, 0);
	diff_eq_int("the termination latch was set", sawLatched, 1, 0);
	diff_eq_int("...and clear", sawUnlatched, 1, 0);
	diff_eq_int("enterWaitForANSpcmDrop really entered", sawEnter, 1, 0);
	diff_eq_int("...and was called when already there", sawAlready, 1, 0);
	diff_eq_int("both arms of the debug gate were taken",
		    quiet > 0 && loud > 0, 1, 0);

	return diff_end();
}

/* ================================================ vPcmResetPhase3Modem */

/*
 * THREE LINES OF THE LEVEL-2 TRANSCRIPT HOLD A HEAP ADDRESS, and no fixture
 * can make them agree: `V92Modem::reset` reaches `V92DILdescriptorPacker`,
 * which prints "## Debug: pParamObj address = %X" and two more like it, and
 * the two sides allocate separately.  t_v92modem.cpp drops the whole text
 * comparison on the analogue side for exactly these three lines; this
 * REWRITES them instead and keeps the other ninety, because a transcript of
 * three thousand characters is most of what this function is observable
 * through.
 *
 * `addr_lines_seen` is what says the rewrite is not a hole for nothing: if no
 * trial ever contained one of these lines, the normalisation would be
 * excusing nothing and the assertion below fails.
 */
static int addr_lines_seen;

static const char *
normalise(const char *src, char *dst, size_t n)
{
	static const char key[] = "address = ";
	size_t klen = sizeof key - 1;
	size_t i = 0, o = 0;

	while (src[i] != '\0' && o + 2 < n) {
		if (strncmp(src + i, key, klen) == 0) {
			memcpy(dst + o, key, klen);
			o += klen;
			i += klen;
			while (src[i] != '\0' && src[i] != '\r'
			       && src[i] != '\n')
				i++;
			dst[o++] = '#';
			addr_lines_seen++;
			continue;
		}
		dst[o++] = src[i++];
	}
	dst[o] = '\0';
	return dst;
}

#define TRANSCRIPT_MAX	65536

struct rp3_trial {
	const char	*what;
	int		digital;	/* which modem VPCMXF_Create builds */
	int		sessionType;	/* pcmSessionType, +0x611c        */
	unsigned	side;		/* V90Modem::side, poked          */
	unsigned char	cfg3;		/* modemParams->unnamed_0003      */
	unsigned	level;
	int		sweepCounter;
	float		phase;
};

static const struct rp3_trial rp3_v[] = {
	/*
	 * `pcmSessionType` NON-ZERO IN HALF OF THEM.  With it zero, "passes
	 * the session type to setSessionFlag" and "passes 0" are the same
	 * program, and `V90Modem::reset`'s own argument is 0 either way --
	 * which is what findings 7458 and 7105 are about.
	 */
	{ "V.90 session, analog",	0, 0, 1, 0x8b, 0,  3,  0.75f },
	{ "V.92 session, analog",	0, 1, 1, 0x8b, 0,  3,  0.75f },
	{ "V.92 session, retrain set",	0, 1, 1, 0xff, 0,  7, -1.25f },
	{ "V.90 session, retrain clear",0, 0, 1, 0xfb, 0,  7, -1.25f },
	{ "V.92 session, all bits set",	0, 1, 1, 0xff, 0, -9,  2.5f },
	{ "V.92 session, illegal side",	0, 1, 2, 0x8b, 0,  5,  0.5f },
	{ "V.90 session, illegal side",	0, 0, 2, 0xff, 0,  5,  0.5f },

	/* The digital modem, whose `side` the constructor sets to 0. */
	{ "V.92 session, digital",	1, 1, 0, 0x8b, 0,  5,  0.5f },
	{ "V.90 session, digital",	1, 0, 0, 0xff, 0,  5,  0.5f },
	{ "V.92 session, digital, illegal side", 1, 1, 2, 0x8b, 0, 5, 0.5f },

	{ "level 1, V.92 analog",	0, 1, 1, 0x8b, 1,  3,  0.75f },
	{ "level 1, V.90 analog",	0, 0, 1, 0x8b, 1,  3,  0.75f },
	{ "level 2, V.92 analog",	0, 1, 1, 0xff, 2,  3,  0.75f },
	{ "level 2, V.90 analog",	0, 0, 1, 0x8b, 2,  3,  0.75f },
	{ "level 2, V.92 illegal side",	0, 1, 2, 0xff, 2,  3,  0.75f },
	{ "level 2, V.92 digital",	1, 1, 0, 0xff, 2,  3,  0.75f }
};

#define NRP3	((int)(sizeof(rp3_v) / sizeof(rp3_v[0])))

static void
rp3_poke(int s, const struct rp3_trial *t)
{
	VPcmFloModem *m = V(s);

	m->modem.side = (V90ModemSide)t->side;
	m->pcmSessionType = t->sessionType;

	/*
	 * EVERY DESTINATION AWAY FROM THE VALUE THE FUNCTION WRITES.  A store
	 * whose value is already there cannot fail its own mutation, which is
	 * finding 7105 and what made three of t_v90rundemod's rows uncatchable
	 * until its poke() moved them.
	 */
	m->sweepCounter = t->sweepCounter;
	m->word_7f60 = 0xa5a5a5a5u;
	m->word_7f64 = 0x5a5a5a5au;
	m->byte_6118 = 0x37;
	m->sineWave.phase = t->phase;
	m->modem.sessionFlag = 0xdeadbeefu;
	MPARAMS(s)->unnamed_0003 = t->cfg3;
}

static int
run_rp3(void)
{
	static char tsa[TRANSCRIPT_MAX], tsb[TRANSCRIPT_MAX];
	int trial;
	int sawAnalog = 0, sawDigital = 0, sawIllegal = 0;
	int sawV92 = 0, sawV90 = 0;
	int sawRetrainSet = 0, sawRetrainClear = 0;
	int quiet = 0, loud = 0;
	int phaseMoved = 0;

	diff_begin("VPcmFloModem::vPcmResetPhase3Modem against the blob");

	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < NRP3; trial++) {
		const struct rp3_trial *t = &rp3_v[trial];
		int allocs, ourAllocs;

		if (!build(trial + 500, t->digital))
			continue;

		rp3_poke(0, t);
		rp3_poke(1, t);
		seed_buffers(trial + 500);

		discover();
		diff_eq_int("no region overflow (%ld)", region_overflow, 0,
			    trial);
		diff_eq_int("no lopsided pointer word (%ld)", lopsided, 0,
			    trial);
		snapshot();
		nregion_last = nreg;
		cur_what = t->what;

		allocs = harness_alloc.allocs;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = t->level;
		ref_dsplibs_debug_level = t->level;

		V(0)->vPcmResetPhase3Modem();
		ourAllocs = harness_alloc.allocs - allocs;
		ref_vPcmResetPhase3Modem(base[1]);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("the two sides allocated alike (%ld)", ourAllocs,
			    harness_alloc.allocs - allocs - ourAllocs, trial);
		diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0,
			    trial);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(normalise(dsplib_debug_capture_text(0),
					     tsa, TRANSCRIPT_MAX),
				   normalise(dsplib_debug_capture_text(1),
					     tsb, TRANSCRIPT_MAX)) == 0, 1,
			    trial);
		diff_eq_int("the transcript line counts agree (%ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);

		compare_regions(trial);

		/*
		 * THE SIX STORES AND THE SESSION FLAG, BY VALUE ON THE BLOB'S
		 * SIDE, so that each is a claim about the object.
		 */
		diff_eq_int("sweepCounter (%ld)", (long)V(1)->sweepCounter, 0,
			    trial);
		diff_eq_int("word_7f64 (%ld)", (long)V(1)->word_7f64, 0,
			    trial);
		diff_eq_int("word_7f60 (%ld)", (long)V(1)->word_7f60, 0,
			    trial);
		diff_eq_int("byte_6118 (%ld)", (long)V(1)->byte_6118, 1,
			    trial);
		diff_eq_int("sineWave.phase is restarted (%ld)",
			    V(1)->sineWave.phase == 0.0f, 1, trial);
		diff_eq_int("the retrain bit is withdrawn (%ld)",
			    (long)(MPARAMS(1)->unnamed_0003 & 0x04u), 0,
			    trial);
		/*
		 * AND NOTHING ELSE IN THAT BYTE MOVED.  `andb $0xfb` clears one
		 * bit; a store of zero would pass the check above and fail
		 * this one.
		 */
		diff_eq_int("...and only that bit (%ld)",
			    (long)MPARAMS(1)->unnamed_0003,
			    (long)(t->cfg3 & ~0x04u), trial);
		/*
		 * THE SESSION TYPE REACHES `setSessionFlag`, which stores it
		 * unconditionally whatever the side.  This is the whole of
		 * trap 7458: at a session type of 0 this assertion holds for a
		 * reconstruction that passes 0.
		 */
		diff_eq_int("setSessionFlag got the session type (%ld)",
			    (long)V(1)->modem.sessionFlag,
			    (long)t->sessionType, trial);

		if (t->phase != 0.0f)
			phaseMoved = 1;
		if (t->sessionType)
			sawV92 = 1;
		else
			sawV90 = 1;
		if (t->cfg3 & 0x04u)
			sawRetrainSet = 1;
		else
			sawRetrainClear = 1;
		if (t->side == 0)
			sawDigital = 1;
		else if (t->side == 1)
			sawAnalog = 1;
		else
			sawIllegal = 1;

		if (dsplib_debug_capture_lines(1) == 0)
			quiet++;
		else
			loud++;

		demolish();
	}

	dsplib_debug_capture_on = 0;

	diff_eq_int("a V.92 session type was driven", sawV92, 1, 0);
	diff_eq_int("...and a V.90 one", sawV90, 1, 0);
	diff_eq_int("the retrain bit was set going in", sawRetrainSet, 1, 0);
	diff_eq_int("...and clear", sawRetrainClear, 1, 0);
	diff_eq_int("the analogue side was driven", sawAnalog, 1, 0);
	diff_eq_int("the digital side was driven", sawDigital, 1, 0);
	diff_eq_int("the illegal side was driven", sawIllegal, 1, 0);
	diff_eq_int("the oscillator was not already at phase zero",
		    phaseMoved, 1, 0);
	diff_eq_int("both arms of the debug gate were taken",
		    quiet > 0 && loud > 0, 1, 0);
	diff_eq_int("the address-bearing lines the rewrite excuses were "
		    "really printed", addr_lines_seen > 0, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE WIDTH OF THE COPY, MEASURED ON THE BLOB ALONE
 * ===========================================================================
 *
 * ONE SIDE, ON PURPOSE, and it is not a two-sided comparison withheld out of
 * convenience: `src/` does not narrow (see the QC_WIDE_STATUS block above),
 * so a two-sided trial here would be red rather than informative.  What this
 * measures is what the OBJECT does, which is the claim
 * include/dsplib/VPcmFloModem.h makes and which nothing had ever run.
 *
 * IT FIRES: the assertion is that 0x1234abcd comes back as 0xabcd, so a blob
 * that copied the whole word would fail it.  Both of the two copy sites are
 * driven -- the 0x3a arm and the 0x3b arm's "no completion status" half.
 */
static int
run_status_width(void)
{
	static const unsigned int status_v[] = {
		0x1234abcdu, 0x00010000u, 0xffffffffu, 0xdead0000u
	};
	static const unsigned int w3c_v[] = { 0x3au, 0x3bu };
	unsigned si, wi;
	long tag = 0;

	diff_begin("the object narrows the verification status to sixteen "
		   "bits (BLOB ONLY -- src/ does not; see the file comment)");

	for (wi = 0; wi < 2; wi++)
		for (si = 0; si < sizeof status_v / sizeof status_v[0]; si++) {
			struct qc_trial t;

			t.what = "status width";
			t.w3c = w3c_v[wi];
			t.verifyState = 0;	/* 0x3b's not-running half */
			t.sampleCount = 100;
			t.terminate = 0;
			t.p3dStatus = status_v[si];
			t.p3dState = 0;
			t.phase = 0.0f;
			t.n = NSAMP;
			t.level = 0;

			if (!build(1000 + (long)tag, 0))
				continue;
			qc_poke(1, &t);
			seed_buffers(1000 + tag);

			ref_qcLineVerification(base[1], sig_in[1], sig_out[1],
					       t.n, rxbits[1], &nrx[1],
					       txbits[1], &nbits[1]);

			diff_eq_int("the blob stored the LOW HALF (%ld)",
				    (long)V(1)->verificationStatus,
				    (long)(status_v[si] & 0xffffu), tag);
			diff_eq_int("...which is not the whole word (%ld)",
				    (long)(status_v[si] & 0xffffu)
				    != (long)status_v[si], 1, tag);

			demolish();
			tag++;
		}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	rc |= run_qcline();
	rc |= run_status_width();
	rc |= run_rp3();

	return rc;
}
