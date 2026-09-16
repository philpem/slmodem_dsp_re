/*
 * t_v90rundemod.cpp -- `VPcmFloModem::v90RunDemodulator` (.text+0xd860,
 * 3,013 bytes) against the blob.
 *
 * ===========================================================================
 * IT IS `t_vpcmrunpcm.cpp`'S APPARATUS AND ITS OWN AXES
 * ===========================================================================
 *
 * The measuring half -- both sides built by `VPCMXF_Create`, the comparison
 * surface discovered transitively by walking every word that is a live
 * allocation base on BOTH sides, and the three-way rule for a differing word
 * -- is that file's, and its header is where the argument for each of those
 * lives.  It is duplicated rather than shared because a header of test
 * apparatus shared between two binaries is a second thing to keep right, and
 * because the two files' axes have almost nothing in common: this function
 * has no transmit half, no echo canceller, no V.92 modulator and no third
 * dispatch.
 *
 * WHY A BINARY OF ITS OWN AND NOT MORE TRIALS IN `t_vpcmrunpcm`.  The two
 * functions share a source file, so a mutation set over `VpcmFloModem.cpp`
 * that is scored by ONE binary cannot tell which function a row belongs to;
 * `test/mutations/suites.json` already maps four sets over this file to four
 * binaries for that reason, and `v90rundemod` is the fifth.
 *
 * ===========================================================================
 * WHAT IS DELIBERATELY NOT DRIVEN
 * ===========================================================================
 *
 * `V90Modem::side` is held at 2, outside {0, 1}, so `V90Modem::progress` --
 * 188 bytes of side switch and nothing else -- fans out to neither half and
 * `V90Demodulator::progress` never runs.  t_vpcmrunpcm.cpp's header carries
 * the argument at length and the first reason is the one that decides:
 * `V90Equalizer::process` is a `tools/gccdiverge.json` entry, a declared
 * binary cannot carry a mutation suite (findings F2157 and F3002), and driving
 * the real demodulator would drag that divergence into this binary.
 *
 * WHAT THAT COSTS, stated rather than hidden: the four arguments this
 * function forwards to `V90Modem::progress` (`rxbits`, `*nrx`, `in`, `n`) are
 * not proved to be in that order here, because the callee reads none of them.
 * What IS proved is that none of the four buffers is written by anything else
 * in the function, which is asserted element by element.
 *
 * ===========================================================================
 * THE FOUR THINGS THIS SET EXISTS TO SEPARATE
 * ===========================================================================
 *
 * Each is a place where the object's own near-twin would pass a weaker suite:
 *
 *   1. DISPATCH 1 CASE 3's POLARITY.  `runPcmModem` returns 2 when
 *      `info0Layout` is zero and this function latches `progressState = 4` and
 *      returns 3.  All four corners of (layout, inPhase3, ENABLE_ERROR_
 *      CORRECTION_RRN) are trialled, because three of them agree between the
 *      two readings and only `layout == 0` parts them.
 *   2. ARMS 0x1a AND 0x1b, MP AGAINST MPnot.  Four one-token differences over
 *      two otherwise identical blocks; the clear flag is an axis, the
 *      `SENSITIVE_ISP_DETECTED` tail is an axis, and the else arm's
 *      three-way conjunction is swept over all its corners.
 *   3. WHICH MAPPING BLOCK EACH `V90CPPacker` CALL READS.  `mappingParams`
 *      and `mappingParamsAlt` are seeded with different values in every
 *      field the packer reads, so a swap changes the bit vector it writes.
 *      V90Modem.h records findings F1301 and F1307 as two batches that shipped
 *      that pair the wrong way round.
 *   4. `copyMpInfoForInterface`'s THIRTEEN FIELDS.  Every source field in
 *      `modem.mp` is seeded to a DISTINCT value, so copying the wrong one is
 *      visible, and `rateMask` is seeded non-zero so its doubling is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34fsk.h"
#include "dsplib/V90ConnectionEvaluator.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Resampler.h"
#include "dsplib/tagV90AdditionalCPinfo.h"
/* `V34_SHELL_TX`, the two shell contexts' spacing; see is_static_install. */
#include "dsplib/v34shell.h"
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

int ref_v90RunDemodulator(void *self, float *in, unsigned int n, int *rxbits,
			  int *nrx)
	asm("ref__ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_");

/*
 * NOT UNDER TEST HERE, AND CALLED AS FIXTURE, for t_vpcmrunpcm.cpp's reason:
 * a freshly constructed Phase 2 record holds a null `L2`, and this is the
 * object's own way of filling it.  Each side calls its own.
 */
void ref_setPhaseIIinfo(void *self, int *info0, int rtd)
	asm("ref__ZN12VPcmFloModem14setPhaseIIinfoEPii");
}

/* ================================================================ the fixture */

#define FLO_SIZE	0x7f68

typedef char v90rd_is_0x7f68[(sizeof(VPcmFloModem) == FLO_SIZE) ? 1 : -1];

/*
 * The offsets the trials poke.  Several are inside heap blocks this file has
 * no complete declaration for, so they are spelled rather than reached
 * through the class; each carries the disassembly site that reads it.
 */
#define OFF_NOFBITS	0x1736		/* mov %ax,0x1736, 0xe104    */
#define OFF_FLAGS173A	0x173a		/* getConstelationSize, 0xdcab */
#define OFF_FLAG173D	0x173d		/* movb $0x1,0x173d, 0xdd56  */
#define OFF_FLAG173E	0x173e		/* movzbl 0x173e, 0xdf74     */
#define OFF_MPINFO	0x1744		/* copyMpInfoForInterface    */
#define OFF_MODEM	0x1758
#define OFF_DEMOD	0x175c		/* V90Modem::demodulator     */
#define OFF_PHASE2INFO	0x1760		/* V90Modem::phase2Info      */
#define OFF_JD		0x1764		/* V90Modem::jd, 0xdcbf      */
#define OFF_MAPPING	0x1770		/* V90Modem::mappingParams   */
#define OFF_MAPPINGALT	0x1dc0		/* V90Modem::mappingParamsAlt */
#define OFF_CPINFO	0x2410		/* V90Modem::additionalCPinfo */
#define OFF_MP		0x2428		/* V90Modem::mp, 0xd8d7      */
#define OFF_V90PARAMS	0x610c		/* V90Modem::params        */
#define OFF_SIDE	0x6114		/* V90Modem::side, +0x49bc   */
#define OFF_BYTE6118	0x6118
#define OFF_BYTE6119	0x6119		/* cmpb $0x0,0x6119, 0xde1a  */
#define OFF_LAYOUT	0x6120		/* info0Layout, 0xda18       */
#define OFF_V92MODEM	0x6124
#define OFF_V92SIDE	(OFF_V92MODEM + 0xaa8)
#define OFF_CPNOFBITS	0x7dcc		/* mov %ax,0x7dcc, 0xdc39    */
#define OFF_TERMJA	0x7dce
#define OFF_TERMCP	0x7dcf		/* cmpb $0x0,0x7dcf, 0xd951  */
#define OFF_TERMCPNOT	0x7dd0		/* cmpb $0x0,0x7dd0, 0xd99f  */
#define OFF_CPNOTLOADED	0x7dd1		/* cmpb $0x0,0x7dd1, 0xd996  */
#define OFF_BITPOINTER	0x1738		/* resetBitPointer, 0xdc58   */
#define OFF_NTXSEQ	0x7dd4		/* resetBitPointer, 0xdc74   */
#define OFF_MINTXSEQ	0x7dd6		/* setMinNofTransmitSequences */

/* Inside V90Demodulator; include/dsplib/V90Demodulator.h. */
#define DEM_INPHASE3	0x34
#define DEM_WORD3C	0x3c

/*
 * Inside V90Parameters, and all four names are the object's own out of
 * include/dsplib/V90Parameters.h.
 */
#define PARAM_SILENCE_SCR	0x364
#define PARAM_MIN_RTD		0x368
#define PARAM_EC_RRN		0x480
#define PARAM_SENSITIVE_ISP	0x4f8

/*
 * Two more of the parameter block, held off their defaults for
 * t_vpcmrunpcm.cpp's reason: `PRE_FILTER_GAIN` at -1 sends
 * `V90PreFilter::selectFilter` into `autoSelection`, which reads the Phase 2
 * record's `L2` through a pointer a freshly constructed demodulator has not
 * filled.  Nothing this function reads is affected (D561).
 */
#define PARAM_CONNECTION_TYPE	0x0c
#define PARAM_PRE_FILTER_GAIN	0x4c

/* `V90Phase2Info::rtd`, +0x04, compared at 0xdcf8. */
#define P2I_RTD			0x04

#define NSAMP		48		/* durationMs 5 -> 5 * 9.6 + 0.5 */
#define DURATION_MS	5

#define MP_SLOT		(sizeof(struct _tagModemParameters))

static struct v34_object v34obj[2];
static unsigned char mparams[2][MP_SLOT] __attribute__((aligned(8)));
static unsigned char *base[2];

#define MPARAMS(s)	((struct _tagModemParameters *)mparams[s])

static float sig_in[2][NSAMP];
static int rxbits[2][NSAMP];
static int nrx[2];

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
 * Which arm the trial being compared is driving.  A failure report that names
 * only a trial index makes the reader count rows to find out what was being
 * driven, which is exactly the step that gets skipped.
 */
static unsigned cur_w3c;
static const char *cur_what = "";
static long words_unresolved;

/*
 * THE FOUR WORDS THAT ARE EXCUSED BY NAME, AND WHY BY NAME.
 *
 * Arms 0x22 and 0x23 call `VPcmV34SetV90RateReneg`, which calls
 * `preinitdigital`, which installs FOUR STATIC ADDRESSES into the V.34
 * object -- one `conv` table and one `scramble` function pointer in each of
 * the two shell contexts, at +0xa28 and +0xe48 and again `V34_SHELL_TX`
 * (0x1be0) further on.  Our side holds an address in our build and the blob's
 * side holds one in its own copy, and nothing can pair those up: they are not
 * inside any discovered allocation, so `locate` answers -1 for both.
 *
 * The claim that is left is that BOTH sides wrote the word or NEITHER did,
 * which is what these four assert.
 *
 * THEY ARE NAMED RATHER THAN INFERRED, and t_vpcmrunpcm.cpp's header is the
 * reason: a general rule -- "a differing word neither side can resolve is a
 * static pointer" -- silently swallowed seven real mutations there, because a
 * pair of wrong small integers is indistinguishable from a pair of addresses.
 * There is no property of a WORD that separates the two cases; only knowing
 * which field it is does.  Finding F7521's shape.
 */
#define V34_SHELL_CONV		0x0a28
#define V34_SHELL_SCRAMBLE	0x0e48
#define V34OBJ_REGION		1	/* add_region's second call, below */

static int
is_static_install(int r, unsigned off)
{
	if (r != V34OBJ_REGION)
		return 0;
	return off == V34_SHELL_CONV
	    || off == V34_SHELL_SCRAMBLE
	    || off == V34_SHELL_CONV + V34_SHELL_TX
	    || off == V34_SHELL_SCRAMBLE + V34_SHELL_TX;
}

/*
 * The three cases, and t_vpcmrunpcm.cpp's argument for each.  This file meets
 * only two of them: there is no `V92Phase4Modulator::pattern` here, because
 * nothing this function reaches installs a static table, so a differing word
 * that neither side can resolve AND that either side wrote is a failure with
 * no exemption at all.
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
			} else if (is_static_install(r, i)) {
				bad = (va != pva) != (vb != pvb);
				if (!bad)
					words_unresolved++;
			} else {
				bad = 1;
			}

			diff_eq_int("region %ld word", bad, 0, (long)r);
			if (bad && reported < 8) {
				reported++;
				printf("    trial %ld (%s, word_3c 0x%02x) "
				       "region %d offset %u: "
				       "ours 0x%08x (was 0x%08x) blob 0x%08x "
				       "(was 0x%08x) ka %d/%u kb %d/%u\n",
				       trial, cur_what, cur_w3c, r, i,
				       va, pva, vb, pvb, ka, oa, kb, ob);
				fflush(stdout);
			}
		}

		for (; i < reg[r].size; i++)
			diff_eq_int("region %ld tail byte",
				    reg[r].a[i] != reg[r].b[i], 0, (long)r);
	}
}

/* ======================================================== the trial matrix */

struct trial {
	const char	*what;
	unsigned char	b6118;
	int		layout;
	unsigned	inPhase3;
	int		ecRRN;
	unsigned	w3c;
	unsigned char	b6119;
	unsigned char	termJa;
	unsigned char	termCp;
	unsigned char	termCpNot;
	unsigned char	cpNotLoaded;
	unsigned int	ceWord90;
	int		p4d3c;
	int		p4d38;
	int		sensitiveIsp;
	int		silenceScr;
	int		minRtd;
	int		rtd;
	unsigned char	f173a2;
	unsigned char	f173e;
	unsigned char	cfg3;
	unsigned	level;
	/*
	 * The recovered timing offset, which five arms log.  Zero is the
	 * constructor's, and at zero the scale factor and the width of the
	 * conversion are both invisible; t_vpcmrunpcm.cpp's p4 presets carry
	 * the same reasoning and the same geometric spread.
	 */
	float		timingOffset;
	/* Which set of thirteen values `modem.mp` carries; see seed_mp. */
	int		mpTag;
};

/*
 * The defaults: dispatch 1 seeded with 0, the master flags clear, the
 * three-way conjunction FALSE (so the MPnot else arm terminates), the round
 * trip below the threshold, and the timing offset small enough that
 * `ppm * 10` stays inside a short.
 */
/*
 * `f173e`, `cfg3`, `level`, `timingOffset`, `mpTag`.  0x89 has BOTH of
 * `CFG_FLAG3_PHASE2` and `CFG_FLAG3_RETRAIN` clear, so the three arms that
 * raise the retrain bit move it from 0 to 1 and are visible; the two rows
 * that drive arm 0x08's clear supply 0xff and 0xfd instead.
 */
#define TAIL	, 0, 0x89, 0, 0.0015f, 1
#define D(w)	{ "word_3c", 0, 1, 4, 1, (w), 1, 0, 0, 0, 1, \
		  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL }

static const struct trial trial_v[] = {
	/*
	 * DISPATCH 1, AND ITS CASE 3 IN ALL FOUR CORNERS.  `b6118` 5 and 6
	 * are the `ja` at 0xd875, which falls through with the return still
	 * at its initial 0.
	 */
	{ "b6118 0", 0, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118 1", 1, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118 2", 2, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118 4", 4, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118 5", 5, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118 6", 6, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },

	/*
	 * `layout` 0 IS THE ONE THAT PARTS THIS FUNCTION FROM `runPcmModem`,
	 * whose same arm returns 2 where this one latches 4 and returns 3.
	 * The other three corners agree between the two readings.
	 */
	{ "b6118=3 layout 0",	3, 0, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118=3 layout 0, not p4", 3, 0, 3, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118=3 layout 0, no param", 3, 0, 4, 0, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118=3 retrain",	3, 1, 4, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118=3 not p4",	3, 1, 3, 1, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "b6118=3 no param",	3, 1, 4, 0, 0, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },

	/* Dispatch 2, every case label and one past the end of the table. */
	D(0x00), D(0x01), D(0x02), D(0x03), D(0x04), D(0x05), D(0x06),
	D(0x07), D(0x08), D(0x09), D(0x0a), D(0x0b), D(0x0c), D(0x0d),
	D(0x0e), D(0x0f), D(0x10), D(0x11), D(0x12), D(0x13), D(0x14),
	D(0x15), D(0x16), D(0x17), D(0x18), D(0x19), D(0x1a), D(0x1b),
	D(0x1c), D(0x1d), D(0x1e), D(0x1f), D(0x20), D(0x21), D(0x22),
	D(0x23), D(0x24), D(0x25), D(0x26), D(0x27), D(0x28), D(0x29),
	D(0x2a), D(0x2b), D(0x2c),

	/* Arm 0x03 and arm 0x17, both gated on `retrainLatch`. */
	{ "0x03, no 6119", 0, 1, 4, 1, 0x03, 0, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "0x17, no 6119", 0, 1, 4, 1, 0x17, 0, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },

	/*
	 * ARM 0x06's SILENCE-SCR OVERRIDE, all four corners of the two tests
	 * and both constellation codes.  The round trip is compared UNSIGNED
	 * and 0x7fffffff against a threshold of 0x80000000 is what would part
	 * a `ja` from a `jg`.
	 */
	{ "0x06, no silence",	0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 0, 0x40, 0x20, 0 TAIL },
	{ "0x06, rtd below",	0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "0x06, rtd above",	0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x20, 0x40, 0 TAIL },
	{ "0x06, rtd equal",	0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x40, 0 TAIL },
	{ "0x06, rtd unsigned",	0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, (int)0x80000000, 0x7fffffff, 0 TAIL },
	{ "0x06, rtd unsigned other way", 0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x7fffffff, (int)0x80000000, 0 TAIL },

	/* Arm 0x08's phase-2 guard, both ways. */
	{ "0x08, phase2 set",	0, 1, 4, 1, 0x08, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0xff, 0, 0.0015f, 1 },
	{ "0x08, phase2 clear",	0, 1, 4, 1, 0x08, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0xfd, 0, 0.0015f, 1 },

	/*
	 * ARMS 0x1a AND 0x1b.  The clear flag, the `terminateCp` gate, the
	 * `SENSITIVE_ISP_DETECTED` tail and every corner of the else arm's
	 * three-way conjunction -- which is where a `&&` written for a `||`
	 * shows up.
	 */
	{ "0x1a, clear 0",	0, 1, 4, 1, 0x1a, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 2 },
	{ "0x1a, cp set",	0, 1, 4, 1, 0x1a, 1, 0, 1, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, clear 0",	0, 1, 4, 1, 0x1b, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 2 },
	{ "0x1b, isp",		0, 1, 4, 1, 0x1b, 1, 0, 0, 0, 1,
	  0, 0, 0, 1, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, cp set, terminate", 0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, cp set, not loaded", 0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 0,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, cp set, already terminated", 0, 1, 4, 1, 0x1b, 1, 0, 1, 1, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, conj 1,1,1",	0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  9, 1, 1, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, conj 0,1,1",	0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  0, 1, 1, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, conj 1,0,1",	0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  9, 0, 1, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },
	{ "0x1b, conj 1,1,0",	0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  9, 1, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 0, 0.0015f, 3 },

	/* Arms 0x19 and 0x2a, and the clear flag they hand the packer. */
	{ "0x19, clear 0",	0, 1, 4, 1, 0x19, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 1 },
	{ "0x2a, clear 0",	0, 1, 4, 1, 0x2a, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 1 },

	/* Arm 0x12, whose packer call is the one that reads `mappingParams`. */
	{ "0x12, clear irrelevant", 0, 1, 4, 1, 0x12, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 1 },

	/*
	 * THE FOUR AXES THE MUTATION SET ASKED FOR, each named with the row
	 * that was uncaught without it.
	 */
	/* `the data-phase arm does not raise retrainLatch`. */
	{ "0x1e, no 6119",	0, 1, 4, 1, 0x1e, 0, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	/*
	 * `the Jd arm takes the silence flag as a truth value rather than a
	 * byte`.  0x100 is non-zero and its LOW BYTE is zero, which is the
	 * only shape that parts a truncation from a test against zero.
	 */
	{ "0x06, silence 0x100", 0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 0x100, 0x40, 0x20, 0 TAIL },
	/*
	 * `the rate-reneg arms take the evaluator's counter whole rather than
	 * as a short`.  0x10000 is non-zero and its low SIXTEEN bits are
	 * zero, so `VPcmV34SetV90RateReneg` sees 0 through the narrowing the
	 * object does and 0x10000 through any wider one -- and it assigns
	 * `v90_receiver` 11 or 15 on exactly that test.
	 */
	{ "0x22, word_90 low half zero", 0, 1, 4, 1, 0x22, 1, 0, 0, 0, 1,
	  0x10000, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "0x23, word_90 low half zero", 0, 1, 4, 1, 0x23, 1, 0, 0, 0, 1,
	  0x10000, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },

	/* Arm 0x1c's cleardown report, both ways. */
	{ "0x1c, no cleardown",	0, 1, 4, 1, 0x1c, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 0, 0.0015f, 1 },

	/* Arms 0x22 and 0x23, and the latch between them. */
	{ "0x22, latch clear",	0, 1, 4, 1, 0x22, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "0x22, latch set",	0, 1, 4, 1, 0x22, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 1 TAIL },
	{ "0x23, latch clear",	0, 1, 4, 1, 0x23, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },
	{ "0x23, latch set",	0, 1, 4, 1, 0x23, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 1 TAIL },
	/*
	 * `word_90` WITH BIT 15 SET.  The two calls to
	 * `VPcmV34SetV90RateReneg` narrow it to a `short`, and the callee
	 * tests only for zero, so what a bit-15 value drives is the narrowing
	 * having happened at all rather than its sign; the "low half zero"
	 * row below is the one that separates the two widths.
	 */
	{ "0x23, word_90 negative", 0, 1, 4, 1, 0x23, 1, 0, 0, 0, 1,
	  0x1298a3, 0, 0, 0, 1, 0x40, 0x20, 0 TAIL },

	/*
	 * THE NINETEEN GATED DIAGNOSTICS, driven at level 2 so every print
	 * site executes.  What comes out is not compared -- the transcript
	 * tier is not this file's -- but a gate nobody enters is a gate
	 * nobody has measured (docs/method/gates.md).
	 */
	{ "level 2, 0x06", 0, 1, 4, 1, 0x06, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x20, 0x40, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x12", 0, 1, 4, 1, 0x12, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x15", 0, 1, 4, 1, 0x15, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x17", 0, 1, 4, 1, 0x17, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x19", 0, 1, 4, 1, 0x19, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x1a", 0, 1, 4, 1, 0x1a, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 2 },
	{ "level 2, 0x1b", 0, 1, 4, 1, 0x1b, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 2 },
	{ "level 2, 0x1b else", 0, 1, 4, 1, 0x1b, 1, 0, 1, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 3 },
	{ "level 2, 0x1c", 0, 1, 4, 1, 0x1c, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x1d", 0, 1, 4, 1, 0x1d, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x1e", 0, 1, 4, 1, 0x1e, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x1f", 0, 1, 4, 1, 0x1f, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x21", 0, 1, 4, 1, 0x21, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x22", 0, 1, 4, 1, 0x22, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x23", 0, 1, 4, 1, 0x23, 1, 0, 0, 0, 1,
	  0x5a3c1, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x26", 0, 1, 4, 1, 0x26, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x2a", 0, 1, 4, 1, 0x2a, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 1, 0x8b, 2, 0.0015f, 1 },
	{ "level 2, 0x2b", 0, 1, 4, 1, 0x2b, 1, 0, 0, 0, 1,
	  0, 0, 0, 0, 1, 0x40, 0x20, 0, 0, 0x8b, 2, 0.0015f, 1 }
};

#define NTRIAL	((int)(sizeof(trial_v) / sizeof(trial_v[0])))

/*
 * AND THE 45-VALUE SWEEP AGAIN, THREE TIMES, WITH THE TIMING OFFSET AND THE
 * MP MESSAGE SOMEWHERE ELSE.
 *
 * `getTimingOffsetPPM` is `1e6f * timingOffset / ppmScale`, so a whole-number
 * offset puts the result in the millions and `fistps` answers the x87
 * indefinite for `ppm * 10` and for `ppm * 1` alike -- the two compare equal
 * and the scale factor is unmeasurable.  These three span six decades so that
 * at least one lands with `ppm * 10` inside a short.
 */
struct preset {
	float		timingOffset;
	int		mpTag;
	unsigned char	f173e;
};

static const struct preset preset_v[] = {
	{ 0.0015f, 1, 0 },
	{ 5000.0f, 2, 1 },
	{ -0.0005f, 3, 1 }
};

#define NPRE	((int)(sizeof(preset_v) / sizeof(preset_v[0])))
#define NW3C	0x2d
#define NSWEEP	(NPRE * NW3C)
#define NALL	(NTRIAL + NSWEEP)

static void
sweep_trial(struct trial *t, int k)
{
	const struct preset *q = &preset_v[k / NW3C];
	static const struct trial base_trial = D(0);

	*t = base_trial;
	t->what = "sweep";
	t->w3c = (unsigned)(k % NW3C);
	t->timingOffset = q->timingOffset;
	t->mpTag = q->mpTag;
	t->f173e = q->f173e;
}

/* ================================================================== the run */

/*
 * THE TWO MAPPING BLOCKS ARE SEEDED DIFFERENTLY, AND THAT IS THE POINT.
 *
 * Arm 0x12 packs the CP message out of `V90Modem::mappingParams` (+0x1770)
 * and arms 0x19, 0x1a, 0x1b and 0x2a out of `mappingParamsAlt` (+0x1dc0).
 * Every field `V90CPPacker` reads is given a different value in the two
 * blocks, and the bit vector it writes is inside the compared object, so a
 * swap changes what comes out.
 *
 * The lengths are held at 4 and the table bytes below 0x80 for
 * t_vpcmrunpcm.cpp's reason: `getConstellationMask` shifts a table byte right
 * by four IN AN 8-BIT REGISTER and indexes an eight-entry mask with the
 * result, so a byte of 0x80 or more writes past the mask.  That is the
 * object's own behaviour and is D-noted where it is written, but a trial that
 * reaches it is not a trial (D561).
 */
#define SEED_CONSTEL_LEN	4

static void
seed_mapping(unsigned char *blk, int tag)
{
	V90MappingParams *p = (V90MappingParams *)blk;
	int c, j;

	p->word_0 = (unsigned int)(0x30 + tag);
	p->word_61c = 1;
	p->shaperSR = 0x2c00 + tag;
	p->shaperId = (unsigned int)(5 + tag);
	p->shaperA1 = 0.125f * (float)(tag + 1);
	p->shaperA2 = 0.25f * (float)(tag + 2);
	p->shaperB1 = 0.5f * (float)(tag + 3);
	p->shaperB2 = 0.0625f * (float)(tag + 4);

	for (c = 0; c < V90_CONSTELLATIONS; c++) {
		p->constellationSize[c] = SEED_CONSTEL_LEN;
		p->distinctIndex[c] = 0;
		for (j = 0; j < V90_CONSTELLATION_MAX; j++) {
			p->constellation[c][j] = (unsigned char)
			    ((j * 13 + c * 7 + tag * 29) & 0x7f);
			p->codecConstellation[c][j] = (unsigned char)
			    ((j * 5 + c * 11 + tag * 17) & 0x7f);
		}
	}
}

static void
seed_cpinfo(unsigned char *blk)
{
	tagV90AdditionalCPinfo *info = (tagV90AdditionalCPinfo *)blk;

	info->word_00 = 0x51;
	info->word_04 = 0x03;
	info->float_08 = 1.75f;
	info->word_0c = 0x27;
	info->word_10 = 0x1234;
	info->short_14 = 0x33;
}

/*
 * THIRTEEN DISTINCT VALUES, and distinctness is what makes
 * `copyMpInfoForInterface` measurable: with two source fields equal, a copy
 * that read the wrong one would land on the right value anyway.  `rateMask`
 * is non-zero and not a palindrome under doubling, so the `add %ecx,%ecx` is
 * visible too.
 *
 * ONE TAG'S RATE MASK IS NEGATIVE AND IT PROVES NOTHING ABOUT THE LOAD.  It
 * was seeded that way to separate the object's `movswl` from a `movzwl`, and
 * the mutation set answered that no seeding can: the store is sixteen bits,
 * so only the low sixteen bits of the source can reach it and the extension
 * is discarded.  The value is kept because a negative one is still a
 * different value from the other two tags', and finding F7585 carries the
 * retraction.
 */
static void
seed_mp(unsigned char *blk, int tag)
{
	V90MP *mp = (V90MP *)blk;

	mp->Type = (char)(0x11 + tag);
	mp->Rate = (char)(0x22 + tag);
	mp->Trellis = (char)(0x33 + tag);
	mp->NonLin = (char)(0x44 + tag);
	mp->Shaping = (char)(0x55 + tag);
	mp->CPack = (char)(0x66 + tag);
	mp->rateMask = (short)(tag == 3 ? -0x2ac9 : 0x1357 + tag);
	mp->h1Real = (short)(0x0102 + tag);
	mp->h1Imag = (short)(0x0304 + tag);
	mp->h2Real = (short)(0x0506 + tag);
	mp->h2Imag = (short)(0x0708 + tag);
	mp->h3Real = (short)(0x090a + tag);
	mp->h3Imag = (short)(0x0b0c + tag);
}

static int info0_bits[48];

static void
poke(int s, const struct trial *t)
{
	unsigned char *o = base[s];
	unsigned char *dem;
	unsigned char *par;
	unsigned char *p2i;
	int i;

	for (i = 0; i < 48; i++)
		info0_bits[i] = (i * 7 + 3) & 1;
	if (s == 0)
		((VPcmFloModem *)o)->setPhaseIIinfo(info0_bits, 37);
	else
		ref_setPhaseIIinfo(o, info0_bits, 37);

	memcpy(&dem, o + OFF_DEMOD, sizeof dem);
	memcpy(&par, o + OFF_V90PARAMS, sizeof par);
	memcpy(&p2i, o + OFF_PHASE2INFO, sizeof p2i);

	/*
	 * Both fan-outs off.  2 is outside {0, 1} for the V.90 side and
	 * outside {DIGITAL, ANALOG} for the V.92 one; the second is held
	 * only so that a stray call could not reach a live transmitter.
	 */
	*(unsigned int *)(o + OFF_SIDE) = 2;
	*(unsigned int *)(o + OFF_V92SIDE) = 2;

	o[OFF_BYTE6118] = t->b6118;
	o[OFF_BYTE6119] = t->b6119;
	*(int *)(o + OFF_LAYOUT) = t->layout;
	o[OFF_TERMJA] = t->termJa;
	o[OFF_TERMCP] = t->termCp;
	o[OFF_TERMCPNOT] = t->termCpNot;
	o[OFF_CPNOTLOADED] = t->cpNotLoaded;
	o[OFF_FLAGS173A + 0] = 1;	/* trainConstel, and non-zero    */
	o[OFF_FLAGS173A + 1] = 0;	/* rrnConstel                    */
	/*
	 * NONE OF THESE THREE IS AT THE VALUE ITS WRITER WRITES.  A store of
	 * 0 over a 0 is a store no comparison can see, and the mutation set
	 * said so: `resetBitPointer does not reset the bit pointer`,
	 * `setMinNofTransmitSequences does not zero the counter` and the two
	 * arms that require one transmission were all uncaught until these
	 * three were seeded away from 0, 0 and 1.
	 */
	*(unsigned short *)(o + OFF_BITPOINTER) = 0x1d9;
	*(unsigned short *)(o + OFF_NTXSEQ) = 0x2b;
	*(unsigned short *)(o + OFF_MINTXSEQ) = 0x37;
	o[OFF_FLAGS173A + 2] = t->f173a2;
	o[OFF_FLAG173D] = 0;
	o[OFF_FLAG173E] = t->f173e;
	MPARAMS(s)->unnamed_0003 = t->cfg3;

	/*
	 * `V90Jd::getConstelationSize` returns `bits[28]` and `bits[29]`, and
	 * arm 0x06 sends the first upward twice -- once to
	 * `setNofBitsPhase4` and once to `V34XF_IndicateJdReceived`.  With
	 * the two EQUAL, swapping them and choosing the wrong one of them are
	 * both invisible, which is what the mutation set reported.  They are
	 * driven off `mpTag` rather than off an axis of their own because
	 * every trial already carries one.
	 */
	{
		V90Jd *jd;

		memcpy(&jd, o + OFF_JD, sizeof jd);
		jd->bits[28] = (unsigned char)(t->mpTag != 2);
		jd->bits[29] = (unsigned char)(t->mpTag != 1);
	}

	seed_mapping(o + OFF_MAPPING, 1);
	seed_mapping(o + OFF_MAPPINGALT, 2);
	seed_cpinfo(o + OFF_CPINFO);
	seed_mp(o + OFF_MP, t->mpTag);

	*(unsigned int *)(dem + DEM_WORD3C) = t->w3c;
	*(unsigned int *)(dem + DEM_INPHASE3) = t->inPhase3;
	*(int *)(par + PARAM_EC_RRN) = t->ecRRN;
	*(int *)(par + PARAM_SILENCE_SCR) = t->silenceScr;
	*(int *)(par + PARAM_MIN_RTD) = t->minRtd;
	*(int *)(par + PARAM_SENSITIVE_ISP) = t->sensitiveIsp;
	*(int *)(p2i + P2I_RTD) = t->rtd;

	{
		V90Demodulator *d = (V90Demodulator *)dem;
		V90Phase4Demodulator *p4 = d->phase4Demodulator;

		d->resampler.timingOffset = t->timingOffset;
		d->connectionEvaluator->silenceRrnRequest = t->ceWord90;
		p4->int_003c = t->p4d3c;
		p4->int_0038 = t->p4d38;
	}

	*(int *)(par + PARAM_CONNECTION_TYPE) = 0;
	*(int *)(par + PARAM_PRE_FILTER_GAIN) = 0;
}

/*
 * The two buffers.  Both `progress` fan-outs are off, so nothing this
 * function calls reads or writes either -- which is the claim, asserted
 * element by element rather than assumed.
 */
static void
seed_buffers(long trial)
{
	int i;

	lfsr = 0x51a7u + 0x9e37u * (unsigned)trial;
	for (i = 0; i < NSAMP; i++) {
		float a = (float)((int)nextb() - 128) * 0.013671875f
			  + (float)i * 0.0009765625f;

		sig_in[0][i] = sig_in[1][i] = a;
		rxbits[0][i] = rxbits[1][i] = (int)nextb();
	}
	nrx[0] = nrx[1] = 7;
}

static int
run(void)
{
	long trial;
	int rc;

	diff_begin("VPcmFloModem::v90RunDemodulator against the blob, over "
		   "both jump tables and every inner branch below them");

	for (trial = 0; trial < NALL; trial++) {
		struct trial synth;
		const struct trial *t;
		int ra, rb;
		int i;
		int allocs;

		if (trial < NTRIAL) {
			t = &trial_v[trial];
		} else {
			sweep_trial(&synth, (int)(trial - NTRIAL));
			t = &synth;
		}

		memset(&v34obj[0], 0, sizeof v34obj[0]);
		memset(&v34obj[1], 0, sizeof v34obj[1]);
		memset(mparams[0], 0, MP_SLOT);
		memset(mparams[1], 0, MP_SLOT);

		harness_alloc_reset();
		base[0] = (unsigned char *)VPCMXF_Create(0, &v34obj[0],
							 MPARAMS(0),
							 DURATION_MS, 3);
		base[1] = (unsigned char *)ref_VPCMXF_Create(0, &v34obj[1],
							     MPARAMS(1),
							     DURATION_MS, 3);

		diff_eq_int("both sides constructed (%ld)",
			    base[0] != 0 && base[1] != 0, 1, trial);
		if (base[0] == 0 || base[1] == 0)
			continue;

		poke(0, t);
		poke(1, t);
		seed_buffers(trial);

		discover();
		diff_eq_int("no region overflow (%ld)", region_overflow, 0,
			    trial);
		diff_eq_int("no lopsided pointer word (%ld)", lopsided, 0,
			    trial);
		snapshot();
		nregion_last = nreg;
		cur_w3c = t->w3c;
		cur_what = t->what;

		allocs = harness_alloc.allocs;

		dsplibs_debug_level = t->level;
		ref_dsplibs_debug_level = t->level;

		ra = ((VPcmFloModem *)base[0])->v90RunDemodulator(sig_in[0],
		    NSAMP, rxbits[0], &nrx[0]);
		rb = ref_v90RunDemodulator(base[1], sig_in[1], NSAMP,
					   rxbits[1], &nrx[1]);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("return value (%ld)", ra, rb, trial);
		diff_eq_int("nothing was allocated (%ld)",
			    harness_alloc.allocs - allocs, 0, trial);

		for (i = 0; i < NSAMP; i++)
			diff_eq_float("in[%ld] is untouched", sig_in[0][i],
				      sig_in[1][i], (long)i);
		for (i = 0; i < NSAMP; i++)
			diff_eq_int("rxbits[%ld]", rxbits[0][i], rxbits[1][i],
				    (long)i);
		diff_eq_int("*nrx (%ld)", nrx[0], nrx[1], trial);

		compare_regions(trial);

		free_regions();
		VPCMXF_Delete(base[0]);
		ref_VPCMXF_Delete(base[1]);
		base[0] = 0;
		base[1] = 0;
	}

	printf("    surface: %d regions, %ld words equal, %ld corresponding "
	       "pointer pairs, %ld borrowed tables untouched, %ld static "
	       "installs\n",
	       nregion_last, words_equal, words_corresponded, words_static,
	       words_unresolved);
	diff_eq_int("the exempt classes are a minority of the surface",
		    (words_corresponded + words_static + words_unresolved) * 20
		    < words_equal, 1, 0);
	/*
	 * AND THE FOUR NAMED WORDS MUST ACTUALLY FIRE.  An exemption nobody
	 * reaches is an exemption nobody has measured, and it would go on
	 * excusing whatever landed on those offsets later.  Arms 0x22 and
	 * 0x23 are trialled seven times between the explicit rows and the
	 * sweep, so the count is four per such trial and never zero.
	 */
	diff_eq_int("the named static installs were reached",
		    words_unresolved > 0, 1, 0);

	rc = diff_end();
	return rc;
}

/*
 * THE RETURN SET, AND IT IS `runPcmModem`'S MINUS 6.  Seven codes reach
 * `VPcmV34Progress` from here; 4 is in neither function's set and 6 is the
 * V.90 fallback report out of `runPcmModem`'s third dispatch, which this
 * function does not have.  Asserted rather than left implicit, because a
 * mutation that turned one arm's 5 into a 6 would be caught by the comparison
 * above and this is what says WHAT the set is.
 */
static int
run_return_set(void)
{
	static const int expect[] = { 0, 1, 2, 3, 5, 7, 8 };
	int seen[16];
	int i;
	long trial;
	int rc;

	diff_begin("the codes v90RunDemodulator can return");
	memset(seen, 0, sizeof seen);

	for (trial = 0; trial < NALL; trial++) {
		struct trial synth;
		const struct trial *t;
		int r;

		if (trial < NTRIAL) {
			t = &trial_v[trial];
		} else {
			sweep_trial(&synth, (int)(trial - NTRIAL));
			t = &synth;
		}

		memset(&v34obj[1], 0, sizeof v34obj[1]);
		memset(mparams[1], 0, MP_SLOT);
		base[1] = (unsigned char *)ref_VPCMXF_Create(0, &v34obj[1],
							     MPARAMS(1),
							     DURATION_MS, 3);
		if (base[1] == 0)
			continue;
		poke(1, t);
		seed_buffers(trial);
		r = ref_v90RunDemodulator(base[1], sig_in[1], NSAMP,
					  rxbits[1], &nrx[1]);
		if (r >= 0 && r < 16)
			seen[r] = 1;
		ref_VPCMXF_Delete(base[1]);
		base[1] = 0;
	}

	for (i = 0; i < 16; i++) {
		int want = 0;
		unsigned k;

		for (k = 0; k < sizeof expect / sizeof expect[0]; k++)
			if (expect[k] == i)
				want = 1;
		diff_eq_int("code %ld reachable", seen[i], want, (long)i);
	}

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	rc |= run();
	rc |= run_return_set();

	return rc;
}
