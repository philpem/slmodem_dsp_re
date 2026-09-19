/*
 * t_v90connevalnan.cpp -- ONE check, and it is in its own binary on purpose.
 *
 * `V90ConnectionEvaluator::evaluatePhase4` prints its replacement threshold
 * through the object's branchless sign select:
 *
 *     3fdb7:  d9 ee            fldz                 ; the zero goes in first
 *             d8 d9            fcom  %st(1)         ; against t, ONE ordered
 *                                                   ; compare, no parity test
 *             df e0            fnstsw %ax
 *             9e               sahf
 *             19 f6            sbb   %esi,%esi      ; 0x2d - 2*CF
 *             83 e6 fe         and   $0xfffffffe,%esi
 *             83 c6 2d         add   $0x2d,%esi
 *
 * FCOM sets CF for an UNORDERED result exactly as it does for an ordered
 * `0.0f < t`, so a NaN threshold prints '+'.  The source spelling is therefore
 * `!(0.0f >= t) ? '+' : '-'` and not `(0.0f < t) ? '+' : '-'`; the tree has
 * had the correct spelling since the sweep in finding F2410, where the
 * `t_v90conneval` block at tag 7500 was the one site a NaN demonstrably
 * reaches.
 *
 * WHY IT IS NOT IN `t_v90conneval`, and this is the whole reason the file
 * exists.  The modern build compiles `src/` with the recovered
 * `-ffast-math` source profile (`CXXMATHFLAGS`), which implies
 * `-ffinite-math-only`, so GCC 14 is entitled to assume no NaN and folds the
 * ternary to
 *
 *             dd d8            fstp  %st(0)
 *             0f 96 c2         setbe %dl
 *             8d 54 12 2b      lea   0x2b(%edx,%edx,1),%edx
 *
 * -- `setbe` is CF or ZF, both set for an unordered compare, so the modern
 * build prints '-' where the object prints '+'.  There is no source spelling
 * that serves both compilers: `!(0.0f >= t)` is the object's (finding F2410),
 * and `(0.0f < t)` gives '-' on the object too.  `make period` has no
 * allow-list and reproduces the object here with the object's own compiler and
 * flags, which is the tier that decides; the modern build declares the check
 * in `tools/gccdiverge.json` instead of the source being bent to it.
 *
 * An excused binary EXITS NON-ZERO, and `tools/mutate.py` judges a mutant
 * caught by a non-zero exit -- so the excused binary cannot score a mutation
 * set and refuses rather than scoring one wrongly.  `t_v90conneval` carries
 * the `v90conneval` suite (213 mutations), so the divergent transcript check
 * is lifted HERE, leaving that binary green under both compilers and
 * mutation-testable.  `t_v90p4dnan`, `t_v90adidnan` and `t_v92ecnan` are the
 * worked precedents (findings F2157, F3002, F6000 and F6001); issue #143.
 *
 * THE FIXTURE IS THE ONE BLOCK THAT DRIVES IT.  The parameters, counters and
 * average are the `t_v90conneval` tag-7500 setup verbatim: the retrain arm
 * fires (100.0f is over `PDSNR_THRESHOLD_IN_PHASE4` and +0x18 reaches 90), the
 * threshold `t` is a quiet NaN, and the blob's debug line prints it.  The
 * argument is 0.0f and NOT a NaN on purpose: `evaluatePhase4`'s own argument
 * compare is `flds 0x438(%ecx); fcomp %st(1); jae`, a second divergence the
 * parent file declines to drive, and it is not this check.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConnectionEvaluator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_ce_phase4(void *, float)
	asm("ref__ZN22V90ConnectionEvaluator14evaluatePhase4Ef");
int our_ce_phase4(void *, float)
	asm("_ZN22V90ConnectionEvaluator14evaluatePhase4Ef");
}

#define CE_SLOT		((unsigned)sizeof(V90ConnectionEvaluator) + 68u)
#define PARM_SLOT	((unsigned)sizeof(V90Parameters) + 64u)

static unsigned lfsr;
static unsigned char ce_a[CE_SLOT] __attribute__((aligned(8)));
static unsigned char ce_b[CE_SLOT] __attribute__((aligned(8)));
static unsigned char parm_a[PARM_SLOT] __attribute__((aligned(8)));
static unsigned char parm_b[PARM_SLOT] __attribute__((aligned(8)));

#define CEA	((V90ConnectionEvaluator *)ce_a)
#define CEB	((V90ConnectionEvaluator *)ce_b)
#define PA	((V90Parameters *)parm_a)
#define PB	((V90Parameters *)parm_b)

static unsigned char
next_byte(int mode, unsigned i)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0xa5;
	case 2:
		return 0xff;
	case 3:
		return (unsigned char)((lfsr & 0xfe) | (unsigned)(i & 1u));
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

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

static float
as_float(unsigned int bits)
{
	float f;

	memcpy(&f, &bits, 4);
	return f;
}

#define SET_P(f, v)	do { PA->f = (v); PB->f = (v); } while (0)
#define SET_PF(f, bits)	do { unsigned int b_ = (bits); \
			     memcpy(&PA->f, &b_, 4); \
			     memcpy(&PB->f, &b_, 4); } while (0)
#define SET_CE(f, v)	do { CEA->f = (v); CEB->f = (v); } while (0)
#define SET_CEF(f, bits) do { unsigned int b_ = (bits); \
			      memcpy(&CEA->f, &b_, 4); \
			      memcpy(&CEB->f, &b_, 4); } while (0)

/*
 * Every threshold the retrain arm reads during this call, from the parent
 * file's `p34_params`.  The slots it must NOT read stay at their FLT_MAX
 * so a reconstruction that read one diverges in state rather than merely in
 * text.
 */
static void
p34_params(void)
{
	SET_PF(TRN1D_ERROR_FOR_V34_FALLBACK,		0x41200000u);
	SET_PF(PHASE3_ERROR_FOR_V34_FALLBACK,		0x41300000u);
	SET_PF(PDSNR_THRESHOLD_IN_PHASE3,		0x41400000u);
	SET_PF(PDSNR_THRESHOLD_IN_PHASE4,		0x41500000u);
	SET_PF(PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH,
							0x41600000u);
	SET_PF(PDSNR_CURRENT_V34_DROP_THRESH_PHASE4,				0x437a0000u);

	SET_PF(PHASE4_ERROR_FOR_V34_FALLBACK,		0x7f7fffffu);
	SET_PF(QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH,
							0x7f7fffffu);
	SET_PF(EIA6_PDSNR_THRESHOLD_IN_PHASE3,		0x7f7fffffu);
	SET_PF(EIA6_PDSNR_THRESHOLD_IN_PHASE4,		0x7f7fffffu);
	SET_PF(EIA6_TRN1D_ERROR_FOR_V34_FALLBACK,	0x7f7fffffu);
	SET_PF(TRN1D_MAX_MEAN_ERROR_STD_IN_PHASE3,	0x7f7fffffu);
	SET_PF(TRN2D_MAX_MEAN_ERROR_STD_IN_PHASE4,	0x7f7fffffu);

	SET_P(RETRAIN_DETECT_DURATION, 1);
	SET_P(NOF_REMOTE_RATE_RENEG_BEFORE_RETRAIN, 1);
	SET_P(MAX_NOF_RATES_DIFF_BEFORE_RETRAIN, 1);
}

int
main(void)
{
	long tag = 7500;
	int va, vb;

	diff_begin("V90ConnectionEvaluator: evaluatePhase4's "
		   "replacement-threshold sign on an unordered "
		   "PDSNR_CURRENT_V34_DROP_THRESH_PHASE4");

	dsplibs_debug_level = 2;
	ref_dsplibs_debug_level = 2;

	/* The parent file's tag-7500 seeds, so the frame is the same shape. */
	fill_pair(ce_a, ce_b, CE_SLOT, 3450, 0);
	fill_pair(parm_a, parm_b, PARM_SLOT, 3451, 0);
	CEA->params = PA;
	CEB->params = PA;

	p34_params();
	SET_PF(PDSNR_CURRENT_V34_DROP_THRESH_PHASE4, 0x7fc00000u); /* quiet NaN */
	SET_P(MAX_NOF_V90_RETRAINS, 4);
	SET_P(MAX_NOF_REMOTE_RETRAINS, -5);
	SET_P(unnamed_45c, -5);
	SET_CE(nofV90Retrains, 0u);
	SET_CE(meanErrorCheckArmed, (short)0);
	SET_CE(delayedRetrainRequest, 0u);
	SET_CE(delayedRetrainArmed, 0u);
	SET_CE(rateUpCounter, 0u);
	SET_CE(retrainCounter, 0u);
	SET_CE(phase3FallbackDuration, 7u);
	SET_CE(phase4FallbackDuration, 100000u);
	SET_CE(retrainDetectDuration, 90);
	SET_CEF(phase4ErrorForV34Fallback, 0x7f7fffffu);
	SET_CE(avePdsnrNofSymbols, 100u);
	SET_CEF(avePdsnr, 0x42c80000u);		/* 100.0f */

	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;

	va = CEA->evaluatePhase4(as_float(0x00000000u));
	vb = ref_ce_phase4(ce_b, as_float(0x00000000u));

	dsplib_debug_capture_on = 0;

	diff_eq_int("the verdict matches (%ld)", va, vb, tag);
	diff_eq_int("the retrain fired on an unordered threshold (%ld)", vb, 4,
		    tag);
	/*
	 * ANTI-VACUITY, and it is an OBSERVABLE rather than a path: the line
	 * can only differ if the blob printed one.  Both transcripts being
	 * empty would make the comparison below pass for the wrong reason.
	 */
	diff_eq_int("the blob printed (%ld)",
		    (int)dsplib_debug_capture_lines(1) > 0, 1, tag);
	diff_eq_int("the transcripts agreed (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);

	return diff_end();
}