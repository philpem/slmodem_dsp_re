/*
 * t_v34diag.cpp -- the differential suite for
 *
 *     VPcmV34GetDiagnostics                                          821
 *
 * WHAT IS COMPARED.  The record, the guard past its declared tail, the whole
 * `v34_object` (this function must not write a byte of it), the whole
 * `VPcmFloModem` and the whole `V90Demodulator` graph `getAT_UD` reaches --
 * and the TRANSCRIPT, which is the only thing that can see the six
 * `dsplibs_debug_printf` sites at all.  Six of the eight strings in this
 * function land in no object any comparison can reach; finding F3403's batch
 * measured 23 of 31 diagnostics in that state and then caught a real defect
 * with the transcript group, so it is not decoration here either.
 *
 * THE RECORD'S LENGTH IS A LOWER BOUND, so the 64-byte guard past +0x22c is
 * the only thing that would catch a store this batch mis-read.  It is checked
 * against the SEED rather than against the other side, which is the sharp
 * form: two identical mis-reads would agree with each other.
 *
 * ===========================================================================
 * THE GRID, AND WHY EACH AXIS IS IN IT
 * ===========================================================================
 *
 * Finding F4756 is the standing warning: a suite green over 100,000 checks per
 * member had nine of 26 mutations survive, because grid axes made whole states
 * unreachable.  Every axis below is here because a REGION OF CODE is
 * unreachable without it.
 *
 *   status          0 / 1 / 2 / 3      picks the V.34, V.90, V.92 and "other"
 *                                      arms.  3 exists to prove the test is
 *                                      `== 2` and not `>= 2`.
 *   info0Layout     0 / non-zero       whether `getAT_UD` runs at all.  With
 *                                      it zero the whole receive half of the
 *                                      record is left as seeded, which is a
 *                                      different observable from filling it.
 *   phase           3 / not 3          whether the V.92 upstream rate is
 *                                      computed or reported as zero.  3 is
 *                                      `V92MOD_PHASE_DATA`; the arm below
 *                                      uses 7, which is no phase at all, to
 *                                      say "definitely not the data phase".
 *   equerr, sig_energy a table            the two dB loops.  `equerr <= 0` skips
 *                                      both; the pairs below make the -6 dB
 *                                      loop run 0, 1 and many times and the
 *                                      -1 dB loop likewise, which is the only
 *                                      way the `+= 6` and the `+= 1` are
 *                                      separately observable.
 *   v90_timing_offset           0 / +ve / -ve      +0x228's zero test and its sign
 *                                      extension.
 *   tx_pwr_reduction           a table            the transmit level's subtrahend, both
 *                                      signs, because `-12 - x` and `-12 + x`
 *                                      are the same instruction.
 *   K               a table            the V.92 upstream rate, including
 *                                      values whose 8000-times product passes
 *                                      2^31 -- that product is converted to
 *                                      float as UNSIGNED and a signed reading
 *                                      differs only there.
 *   debug level     0 / 1 / 2          the gate is `> 1`, so 1 must be swept
 *                                      or a site at the wrong threshold is
 *                                      invisible (the debug.h note, and 150).
 *
 * ===========================================================================
 * SEPARATING TRIALS
 * ===========================================================================
 *
 * Counters here count trials that differ in an OBSERVABLE result -- a byte of
 * the record, a byte of transcript -- never "a branch was taken", which
 * finding F3509 rules worthless.  `run_separation` holds every axis fixed but
 * one, runs the BLOB twice, and requires the two records to differ; that is a
 * claim about the object and a seeded slot cannot fake it.  The mutations in
 * test/mutations/v34diag.json are what adjudicate.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Equalizer.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/int_complex.h"
#include "dsplib/K56FlexFloModem.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/VPcmFloModem.h"
#include "dsplib/dp.h"
extern "C" {
#include "dsplib/dp_param.h"
}
#include "dsplib/modem_params.h"

extern "C" {

void ref_VPcmV34GetDiagnostics(void *obj, void *results)
    asm("ref_VPcmV34GetDiagnostics");

unsigned long ref_VPcmV34GetVisualDiagnostics(void *obj, int what,
					      void *points,
					      unsigned long maxCount)
    asm("ref_VPcmV34GetVisualDiagnostics");

extern unsigned int ref_dsplibs_debug_level;

void dp_vpcm_init(void);
void ref_dp_vpcm_init(void);
void *ref_dp_runtime_create(void *modem);
void ref_dp_runtime_delete(void *runtime);
void ref_V34EchoCleanUp(struct v34_echo *e);
void ref_V34EchoUpdateDelayLine(struct v34_echo *e, short sample);
int ref_V34EchoFilter(struct v34_echo *e, short lag);
void ref_V34EchoAdapt(struct v34_echo *e, short err);
void ref_V34EqualizerCleanUp(struct v34_equalizer *q);
void ref_V34EqualizerUpdateDelayLine(struct v34_equalizer *q, short re,
                                    short im);
void ref_V34EqualizerAdapt(struct v34_equalizer *q, short re, short im);

/*
 * THE ELEVEN ACCESSORS, CALLED BY NAME RATHER THAN THROUGH THE DISPATCHER.
 * See `run_members` below for why the section exists.
 *
 * `this` IS AN ORDINARY FIRST STACK ARGUMENT, and that was read off the object
 * rather than assumed: `_ZN12VPcmFloModem16getConstellationEP11int_complexm`
 * opens `push %ebp; push %edi; push %esi; push %ebx; sub $0x1c,%esp` and then
 * `mov 0x30(%esp),%edi` -- 16 bytes of saves plus 28 of frame plus the return
 * address is 0x30, so that load IS the first argument. No `regparm` here;
 * these are `T` symbols and keep the C convention. `t_v34mp.cpp`'s
 * `getMPrecvdBits` is the other case and needed `regparm(1)` because the object
 * keeps it LOCAL -- and its header comment says why getting this wrong is the
 * worse failure: it links and passes garbage.
 *
 * `extern "C"` for F225's reason: `symmap.py` prepends `ref_` to the raw symbol
 * string and never demangles, so a C++ declaration would mangle the already
 * mangled name a second time.
 */
unsigned long ref_VPcmFloModem_getConstellation(void *self, void *points,
						unsigned long maxCount)
    asm("ref__ZN12VPcmFloModem16getConstellationEP11int_complexm");
unsigned long ref_VPcmFloModem_getLinearEqualizer(void *self, void *points,
						  unsigned long maxCount)
    asm("ref__ZN12VPcmFloModem18getLinearEqualizerEP11int_complexm");
unsigned long ref_VPcmFloModem_getDFE(void *self, void *points,
				      unsigned long maxCount)
    asm("ref__ZN12VPcmFloModem6getDFEEP11int_complexm");

int ref_K56Flex_getConstellation(void *self, void *points,
				 unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem16getConstellationEP11int_complexm");
int ref_K56Flex_getLinearEqualizer(void *self, void *points,
				   unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem18getLinearEqualizerEP11int_complexm");
int ref_K56Flex_getDFE(void *self, void *points, unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem6getDFEEP11int_complexm");
int ref_K56Flex_getDecisionErrors(void *self, void *points,
				  unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem17getDecisionErrorsEP11int_complexm");
int ref_K56Flex_getResamplerPhase(void *self, void *points,
				  unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem17getResamplerPhaseEP11int_complexm");
int ref_K56Flex_getResamplerOffset(void *self, void *points,
				   unsigned long maxCount)
    asm("ref__ZN15K56FlexFloModem18getResamplerOffsetEP11int_complexm");
int ref_K56Flex_getK56FlexMpBits(void *self, short *bits)
    asm("ref__ZN15K56FlexFloModem16getK56FlexMpBitsEPs");
int ref_K56Flex_getK56FlexJaBits(void *self, short *bits)
    asm("ref__ZN15K56FlexFloModem16getK56FlexJaBitsEPs");
void ref_K56Flex_setMinMaxRates(void *self, int lo, int hi)
    asm("ref__ZN15K56FlexFloModem14setMinMaxRatesEii");
void ref_K56Flex_enterPhase3FullDuplex(void *self)
    asm("ref__ZN15K56FlexFloModem21enterPhase3FullDuplexEv");

}

/*
 * ===========================================================================
 * THE FIXTURE
 * ===========================================================================
 *
 * Eleven slots, every one oversized by a guard nothing under test may reach,
 * and both sides seeded from one LFSR so a byte that differs is a difference
 * the code made.
 *
 * `V34_OBJECT_BYTES` is v34fsk.h's own bound rather than `sizeof`: the struct
 * there is a partial map whose declared tail has moved twice, and the object
 * this function is handed is `vpcm.h`'s 0xac4c-byte block whatever the map
 * currently reaches.
 */
#define V34_OBJECT_BYTES	0xac4c
#define OBJ_GUARD		64
#define OBJ_SLOT		(V34_OBJECT_BYTES + OBJ_GUARD)

#define XF_SLOT			(sizeof(VPcmFloModem) + 64)
#define DEM_SLOT		(0x298 + 64)
#define EQU_SLOT		0x150
#define PH2_SLOT		(sizeof(V90Phase2Info) + 32)
#define MPAR_SLOT		(sizeof(V90MappingParams) + 32)
#define ADID_SLOT		(sizeof(V90AutoDigitalImpDetector))
#define V92MOD_SLOT		(sizeof(V92Modulator) + 32)
#define B2S_SLOT		(sizeof(V92BitsToSymbol) + 32)
#define TX_SLOT			(sizeof(V92Transmitter) + 32)

/* The diagnostics record, plus the guard that keeps "at least 0x22c" honest. */
#define DR_BOUND		0x22c
#define DR_GUARD		64
#define DR_SLOT			(DR_BOUND + DR_GUARD)

static unsigned char obj_[2][OBJ_SLOT] __attribute__((aligned(8)));
static unsigned char xf_[2][XF_SLOT] __attribute__((aligned(8)));
static unsigned char dem_[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char equ_[2][EQU_SLOT] __attribute__((aligned(8)));
static unsigned char ph2_[2][PH2_SLOT] __attribute__((aligned(8)));
static unsigned char mpar_[2][MPAR_SLOT] __attribute__((aligned(8)));
static unsigned char adid_[2][ADID_SLOT] __attribute__((aligned(8)));
static unsigned char v92mod_[2][V92MOD_SLOT] __attribute__((aligned(8)));
static unsigned char b2s_[2][B2S_SLOT] __attribute__((aligned(8)));
static unsigned char tx_[2][TX_SLOT] __attribute__((aligned(8)));
static unsigned char drr[2][DR_SLOT] __attribute__((aligned(8)));

static struct v34_object *
O(int side)
{
	return (struct v34_object *)obj_[side];
}

static struct v34_receiver *
RX(int side)
{
	return (struct v34_receiver *)(obj_[side] + 0x264);
}

static VPcmFloModem *
XF(int side)
{
	return (VPcmFloModem *)xf_[side];
}

static V90Demodulator *
D(int side)
{
	return (V90Demodulator *)dem_[side];
}

static unsigned lfsr_state;

static unsigned char
lfsr(void)
{
	lfsr_state = lfsr_state * 1103515245u + 12345u;
	return (unsigned char)(lfsr_state >> 17);
}

static void
fill_pair(void *a, void *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		unsigned char v = lfsr();

		((unsigned char *)a)[i] = v;
		((unsigned char *)b)[i] = v;
	}
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * Finite, POSITIVE values for the two fields `getAT_UD` takes the logarithm
 * of.  A seeded 32-bit pattern is a NaN or a negative often enough that this
 * would otherwise be measuring the two compilations' `log10` of a domain
 * error rather than this reconstruction; t_v90dataph.cpp makes the same
 * argument for the same two fields.
 */
static const float lvl_v[] = {
	1.0f, 0.5f, 2.0f, 1.0e-6f, 1.0e6f, 0.125f, 37.5f, 1.0f / 3.0f
};
#define NLVL		((int)(sizeof lvl_v / sizeof lvl_v[0]))

/*
 * THE dB LADDER.  `sig_energy / equerr` is the ratio and the two loops step it down
 * by 0.2511597 (a -6 dB step, `+= 6`) and then by 0.79418945 (a -1 dB step,
 * `+= 1`), both as `(x * k) >> 14`, until it reaches zero.
 *
 * The pairs below are chosen so that BOTH loop trip counts take the values 0,
 * 1 and many across the table, and so that `equerr <= 0` skips the pair
 * entirely.  Without the zero-trip rows a mutation that deletes either `+=`
 * survives on every row where the other loop dominates the answer.
 */
static const struct {
	int	num;	/* v34_receiver::sig_energy */
	short	den;	/* v34_receiver::equerr */
} snr_v[] = {
	{	   0,	  1 },	/* ratio 0: neither loop runs           */
	{	   1,	  1 },	/* ratio 1: no -6 dB step, no -1 dB step */
	{	   2,	  1 },	/* one -1 dB step                        */
	{	   4,	  1 },	/* one -6 dB step, then -1 dB steps      */
	{	 100,	  1 },
	{  1000000,	  1 },	/* many -6 dB steps                      */
	{ 123456789,	  7 },
	{	 999,	999 },	/* ratio exactly 1                       */
	{	1000,	  3 },
	{	  -5,	  1 },	/* a negative ratio: neither loop runs   */
	{  1000000,	  0 },	/* equerr == 0: the outer test fails       */
	{  1000000,	 -3 }	/* equerr < 0                              */
};
#define NSNR		((int)(sizeof snr_v / sizeof snr_v[0]))

/* +0x228's zero test, and the sign extension on the value that is not zero. */
static const short fac_v[] = { 0, 1, -1, 0x7fff, (short)0x8000, 0x1234 };
#define NFAC		((int)(sizeof fac_v / sizeof fac_v[0]))

/* `-12.0f - tx_pwr_reduction`: both signs, because the instruction cannot tell them. */
static const short pr_v[] = { 0, 1, 3, -1, -6, 0x7fff, (short)0x8000 };
#define NPR		((int)(sizeof pr_v / sizeof pr_v[0]))

/*
 * `V92Transmitter::K`.  8000 * K is converted to float as UNSIGNED, so the
 * rows past 268435 are the ones a signed reading gets wrong; they are here
 * for exactly that reason.
 */
static const int k_v[] = { 0, 1, 12, 33, 268435, 300000, 1000000, -1 };
#define NK		((int)(sizeof k_v / sizeof k_v[0]))

struct trial {
	int	status;		/* v34_object::status                   */
	int	analog;		/* VPcmFloModem::info0Layout            */
	int	upstream;	/* V92Modulator::phase == PHASE_DATA    */
	int	si;		/* index into snr_v                     */
	int	fi;		/* index into fac_v                     */
	int	pi;		/* index into pr_v                      */
	int	ki;		/* index into k_v                       */
	int	li;		/* index into lvl_v                     */
};

/*
 * Plant one trial into both sides.  Everything not named here is the LFSR's,
 * so the rate config, the two latched bit rates, the renegotiation counters
 * and the two unnamed shorts at +0xac12 and +0xac14 all vary per trial
 * without being enumerated.
 */
static void
setup(int n, const struct trial *t)
{
	int side;

	lfsr_state = 0x7a19u + 0x9e37u * (unsigned)n;

	fill_pair(obj_[0], obj_[1], OBJ_SLOT);
	fill_pair(xf_[0], xf_[1], XF_SLOT);
	fill_pair(dem_[0], dem_[1], DEM_SLOT);
	fill_pair(equ_[0], equ_[1], EQU_SLOT);
	fill_pair(ph2_[0], ph2_[1], PH2_SLOT);
	fill_pair(mpar_[0], mpar_[1], MPAR_SLOT);
	fill_pair(adid_[0], adid_[1], ADID_SLOT);
	fill_pair(v92mod_[0], v92mod_[1], V92MOD_SLOT);
	fill_pair(b2s_[0], b2s_[1], B2S_SLOT);
	fill_pair(tx_[0], tx_[1], TX_SLOT);
	fill_pair(drr[0], drr[1], DR_SLOT);

	for (side = 0; side < 2; side++) {
		struct v34_object *o = O(side);
		VPcmFloModem *x = XF(side);
		V90Demodulator *d = D(side);

		o->status = t->status;
		o->p3548 = x;
		o->tx.tx_pwr_reduction = pr_v[t->pi];
		o->v90_timing_offset = fac_v[t->fi];

		RX(side)->sig_energy = snr_v[t->si].num;
		RX(side)->equerr = snr_v[t->si].den;

		x->info0Layout = t->analog;
		x->modem.demodulator = d;
		x->v92modem.modulator = (V92Modulator *)v92mod_[side];

		((V92Modulator *)v92mod_[side])->phase =
		    t->upstream ? 3u : 7u;
		((V92Modulator *)v92mod_[side])->bitsToSymbol =
		    (V92BitsToSymbol *)b2s_[side];
		((V92BitsToSymbol *)b2s_[side])->transmitter =
		    (V92Transmitter *)tx_[side];
		((V92Transmitter *)tx_[side])->K = k_v[t->ki];

		/* Everything `getAT_UD` dereferences. */
		d->phase2Info = (V90Phase2Info *)ph2_[side];
		d->equalizer = (V90Equalizer *)equ_[side];
		d->mappingParamsAlt = (V90MappingParams *)mpar_[side];
		d->autoDigitalImpDetector =
		    (V90AutoDigitalImpDetector *)adid_[side];

		((V90Equalizer *)equ_[side])->meanErrorEnergyCurrent =
		    lvl_v[t->li];
		d->agc.level = lvl_v[(t->li + 3) % NLVL];
		d->rateValid = 1;
		((V90MappingParams *)mpar_[side])->word_0 =
		    (unsigned int)(t->li + 1) * 6u;
		((V90Phase2Info *)ph2_[side])->rtd = 0x1234 + t->li;
	}
}

static void
call_both(void)
{
	VPcmV34GetDiagnostics(obj_[0], (struct TAG_DiagnosticResults *)drr[0]);
	ref_VPcmV34GetDiagnostics(obj_[1], drr[1]);
}

/*
 * TWO HEAP POINTERS HOLD TWO DIFFERENT ADDRESSES AND ALWAYS WILL, so the
 * wired-up words are blanked in a copy before the two slots are compared --
 * CLAUDE.md's "a loop is still right where some region must be skipped",
 * spelled as a snapshot so `diff_eq_obj` still does the coalescing and the
 * counting.
 *
 * The list is exhaustive by construction: it is exactly the set `setup`
 * writes a slot address into.  Anything else that differs is a difference the
 * code made.
 */
static void
snap(unsigned char *dst, const unsigned char *src, size_t n,
     const unsigned int *ptr, int nptr)
{
	int i;

	memcpy(dst, src, n);
	for (i = 0; i < nptr; i++)
		memset(dst + ptr[i], 0, 4);
}

static const unsigned int obj_ptrs[] = { 0x3548 };	/* p3548        */
static const unsigned int xf_ptrs[] = { 0x175c, 0x6124 };
static const unsigned int dem_ptrs[] = { 0x004, 0x018, 0x1d8, 0x23c };
static const unsigned int v92mod_ptrs[] = { 0x4c };	/* bitsToSymbol */
static const unsigned int b2s_ptrs[] = { 0x00 };	/* transmitter  */

#define NPTR(a)		((int)(sizeof (a) / sizeof (a)[0]))

#define CMP_MASKED(what, name, slot, bytes, ptrs, tag)			\
	do {								\
		static unsigned char sa[bytes], sb[bytes];		\
									\
		snap(sa, slot[0], (bytes), (ptrs), NPTR(ptrs));		\
		snap(sb, slot[1], (bytes), (ptrs), NPTR(ptrs));		\
		diff_eq_obj_(__FILE__, __LINE__, what, name, sa, sb,	\
			     (bytes), tag);				\
	} while (0)

/* Everything that must come out the same, in one place. */
static void
compare_all(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "the diagnostics record",
		     drr[0], drr[1], DR_BOUND, tag);
	CMP_MASKED(what, "the V.34 object", obj_, OBJ_SLOT, obj_ptrs, tag);
	CMP_MASKED(what, "the VPcmFloModem", xf_, XF_SLOT, xf_ptrs, tag);
	CMP_MASKED(what, "the demodulator", dem_, DEM_SLOT, dem_ptrs, tag);
	CMP_MASKED(what, "the V.92 modulator", v92mod_, V92MOD_SLOT,
		   v92mod_ptrs, tag);
	CMP_MASKED(what, "the V.92 bits-to-symbol", b2s_, B2S_SLOT,
		   b2s_ptrs, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the equaliser",
		     equ_[0], equ_[1], EQU_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the Phase 2 record",
		     ph2_[0], ph2_[1], PH2_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the impairment detector",
		     adid_[0], adid_[1], ADID_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the V.92 transmitter",
		     tx_[0], tx_[1], TX_SLOT, tag);

	/*
	 * THE GUARD PAST THE DECLARED TAIL, checked against the SEED and not
	 * against the other side: two identical mis-reads would agree with
	 * each other and this is the only form of the claim that a wrong
	 * bound cannot pass.  The record's length is a lower bound
	 * (TAG_DiagnosticResults.h), so nothing else bounds it from above.
	 */
	{
		unsigned int k;
		int touched = 0;

		for (k = 0; k < DR_GUARD; k++)
			if (drr[1][DR_BOUND + k] != drr[0][DR_BOUND + k])
				touched = 1;
		diff_eq_int("nothing was written past the declared tail (%ld)",
			    touched, 0, tag);
	}
}

/*
 * ===========================================================================
 * THE SWEEP
 * ===========================================================================
 */
static int
run_sweep(void)
{
	int status, analog, upstream, si, fi, pi, ki, li, level;
	int n = 0;
	int sawAtUd = 0, sawNoAtUd = 0, sawMinusOne = 0, sawTiming = 0;
	int sawZeroRate = 0, sawSomeRate = 0, sawDb6 = 0, sawDb1 = 0;

	diff_begin("VPcmV34GetDiagnostics");

	dsplib_debug_capture_on = 1;

	for (level = 0; level <= 2; level++) {
		set_level((unsigned int)level);

		for (status = 0; status <= 3; status++)
		for (analog = 0; analog <= 1; analog++)
		for (upstream = 0; upstream <= 1; upstream++)
		for (si = 0; si < NSNR; si++) {
			struct trial t;
			long tag;
			const struct TAG_DiagnosticResults *r;

			fi = si % NFAC;
			pi = si % NPR;
			ki = si % NK;
			li = si % NLVL;

			t.status = status;
			t.analog = analog;
			t.upstream = upstream;
			t.si = si;
			t.fi = fi;
			t.pi = pi;
			t.ki = ki;
			t.li = li;

			tag = (long)level * 1000000 + status * 100000
			    + analog * 10000 + upstream * 1000 + si;

			setup(n++, &t);
			dsplib_debug_capture_reset();
			call_both();

			compare_all("VPcmV34GetDiagnostics", tag);

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			r = (const struct TAG_DiagnosticResults *)drr[1];

			/*
			 * Observable-result counters.  Each names a value in
			 * the record that only one arm can produce, so none of
			 * them is the "a branch ran" claim finding F3509 rules
			 * worthless.
			 */
			if ((status == 1 || status == 2) && analog != 0) {
				sawAtUd = 1;
				if (r->rxBaudRate != 8000u)
					diff_eq_int("getAT_UD ran (%ld)",
						    (long)r->rxBaudRate,
						    8000L, tag);
			} else if (status == 1 || status == 2) {
				sawNoAtUd = 1;
			}

			if (status == 2 && analog != 0) {
				if (upstream && k_v[ki] > 0)
					sawSomeRate |= (r->txDataRate != 0u);
				if (!upstream)
					sawZeroRate |= (r->txDataRate == 0u);
			}

			if (r->word_228 == 0xffffffffu)
				sawMinusOne = 1;
			else
				sawTiming = 1;

			if (status != 1 && status != 2) {
				if (r->float_070 >= 6.0f)
					sawDb6 = 1;
				if (r->float_070 > 0.0f && r->float_070 < 6.0f)
					sawDb1 = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the analog arm called getAT_UD", sawAtUd, 1, 0);
	diff_eq_int("the digital arm did not", sawNoAtUd, 1, 0);
	diff_eq_int("+0x228 reported -1 for a zero timing offset",
		    sawMinusOne, 1, 0);
	diff_eq_int("+0x228 reported a timing offset", sawTiming, 1, 0);
	diff_eq_int("the V.92 upstream rate was computed", sawSomeRate, 1, 0);
	diff_eq_int("the V.92 upstream rate was reported as zero",
		    sawZeroRate, 1, 0);
	diff_eq_int("the -6 dB loop contributed", sawDb6, 1, 0);
	diff_eq_int("the -1 dB loop contributed alone", sawDb1, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * SEPARATION
 * ===========================================================================
 *
 * One axis moves, everything else is held, and the BLOB is run twice.  The
 * claim is that the two runs leave DIFFERENT records behind -- an observable
 * difference in the object under test, which a seeded slot cannot fake and
 * which no path counter can stand in for.
 */
static int
saw_records_differ(const struct trial *a, const struct trial *b, int n)
{
	static unsigned char first[DR_SLOT];

	setup(n, a);
	ref_VPcmV34GetDiagnostics(obj_[1], drr[1]);
	memcpy(first, drr[1], DR_SLOT);

	setup(n, b);
	ref_VPcmV34GetDiagnostics(obj_[1], drr[1]);

	return memcmp(first, drr[1], DR_SLOT) != 0;
}

static int
run_separation(void)
{
	struct trial base, other;
	int n = 90000;

	diff_begin("VPcmV34GetDiagnostics: the axes separate");

	set_level(0);

	base.status = 2;
	base.analog = 1;
	base.upstream = 1;
	base.si = 5;
	base.fi = 1;
	base.pi = 1;
	base.ki = 3;
	base.li = 1;

	other = base;
	other.status = 0;
	diff_eq_int("status separates V.92 from V.34",
		    saw_records_differ(&base, &other, n++), 1, 0);

	other = base;
	other.status = 1;
	diff_eq_int("status separates V.92 from V.90",
		    saw_records_differ(&base, &other, n++), 1, 0);

	other = base;
	other.analog = 0;
	diff_eq_int("info0Layout separates analog from digital",
		    saw_records_differ(&base, &other, n++), 1, 0);

	other = base;
	other.upstream = 0;
	diff_eq_int("the upstream mode separates",
		    saw_records_differ(&base, &other, n++), 1, 0);

	other = base;
	other.ki = 4;
	diff_eq_int("K separates", saw_records_differ(&base, &other, n++),
		    1, 0);

	other = base;
	other.fi = 0;
	diff_eq_int("v90_timing_offset separates", saw_records_differ(&base, &other, n++),
		    1, 0);

	/* The V.34 arm's own axes, on the V.34 arm. */
	base.status = 0;

	other = base;
	other.si = 1;
	diff_eq_int("the SNR ratio separates",
		    saw_records_differ(&base, &other, n++), 1, 0);

	other = base;
	other.pi = 3;
	diff_eq_int("the power reduction separates",
		    saw_records_differ(&base, &other, n++), 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE TRANSCRIPT
 * ===========================================================================
 *
 * Six of the eight diagnostics in this function print a fixed string with no
 * conversion, so what the transcript discriminates is WHICH strings fire in
 * WHAT ORDER -- the arm dispatch and the level gate -- and nothing else can
 * see them at all.  The level is swept because the gate is `> 1`: a site
 * written at the wrong threshold produces a byte-identical transcript at
 * level 2 and differs at level 1 (debug.h's note, finding F150).
 */
static int
run_transcript(void)
{
	struct trial t;
	int status, level, n = 80000;
	int quiet = 0, loud = 0, differed = 0;
	static char seen[4][512];

	diff_begin("VPcmV34GetDiagnostics: the transcript");

	dsplib_debug_capture_on = 1;

	t.analog = 1;
	t.upstream = 1;
	t.si = 4;
	t.fi = 2;
	t.pi = 2;
	t.ki = 2;
	t.li = 2;

	for (level = 0; level <= 2; level++) {
		set_level((unsigned int)level);

		for (status = 0; status <= 3; status++) {
			long tag = (long)level * 10 + status;

			t.status = status;
			setup(n++, &t);
			dsplib_debug_capture_reset();
			call_both();

			diff_eq_int("the transcript (%ld)",
				    (long)strcmp(dsplib_debug_capture_text(0),
						 dsplib_debug_capture_text(1)),
				    0, tag);

			if (level <= 1) {
				if (dsplib_debug_capture_lines(1) == 0u)
					quiet = 1;
				else
					diff_eq_int("the gate is > 1 (%ld)",
						    (long)
						    dsplib_debug_capture_lines(
							1), 0L, tag);
			} else {
				if (dsplib_debug_capture_lines(1) > 0u)
					loud = 1;
				strncpy(seen[status],
					dsplib_debug_capture_text(1),
					sizeof seen[0] - 1);
				seen[status][sizeof seen[0] - 1] = '\0';
			}
		}
	}

	/*
	 * THREE ARMS, THREE TRANSCRIPTS, AND THE FOURTH IS THE CONTROL.
	 * `status` 0, 1 and 2 must print three different things -- that is
	 * the observable saying the dispatch reached three different places,
	 * and it is a transcript claim because nothing in the record
	 * distinguishes a V.90 digital call from a V.92 digital one.
	 *
	 * `status` 3 must print what 0 printed, which is the sharper half:
	 * it says the two PCM tests are `== 1` and `== 2` and not `>= 1` and
	 * `>= 2`.  A reconstruction with the wrong comparison passes every
	 * record check and fails exactly here.
	 */
	if (strcmp(seen[0], seen[1]) != 0 && strcmp(seen[1], seen[2]) != 0 &&
	    strcmp(seen[0], seen[2]) != 0)
		differed = 1;
	diff_eq_int("status 3 took the V.34 arm, so the tests are equalities",
		    strcmp(seen[3], seen[0]), 0, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the gate kept levels 0 and 1 quiet", quiet, 1, 0);
	diff_eq_int("level 2 printed", loud, 1, 0);
	diff_eq_int("the three arms printed three different transcripts",
		    differed, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * VPcmV34GetVisualDiagnostics
 * ===========================================================================
 *
 * NINE SELECTORS, FOUR SOURCES EACH, AND A TENTH SELECTOR THAT MUST DO
 * NOTHING.  The grid sweeps `what` 0..9 -- one past the jump table -- against
 * `status` 0..3 and `role` over both roles and a third value that is
 * neither, because three of the arms test the role and three do not.
 *
 * THE OUTPUT ARRAY IS SEEDED AND HAS A GUARD PAST `maxCount`.  Both matter:
 *
 *   - seeding is the only way to see a PARTIAL fill, and selectors 3 and 4
 *     write only the imaginary half on the two PCM arms;
 *   - the guard is the only way to see an OVERRUN, and selectors 3 and 4
 *     overrun by construction (D710) -- they ignore `maxCount` entirely, so
 *     `maxCount == 0` is in the grid and the eight bytes they write past the
 *     caller's bound are compared rather than assumed.
 *
 * EVERY LENGTH FIELD IS PLANTED AND NEVER SEEDED.  A seeded `unsigned int`
 * length leaves `maxCount` as the only bound and the loop reads a long way
 * off the end of a small array; the six are `V90Equalizer::linearEquLength`
 * and `::dfeLength`, `V90Demodulator::nofSymbols`,
 * `V92EchoCanceller::filterLength` and both `v34_echo::taps`.
 *
 * THE THREE MUTATING ARMS ARE WHY THE OBJECTS ARE COMPARED AND NOT JUST THE
 * POINTS.  `VPcmFloModem::sweepCounter` moves once per point in both
 * constellation arms and not at all when none come out, and selector 0's V.34
 * arm clears `v34_object::hist1_idx` unconditionally -- reading the residual ring
 * EMPTIES it, even when the caller asked for no points.
 */

#define PTS_N		192		/* points the array can hold        */
#define PTS_GUARD	32		/* points past it nothing may reach */
#define PTS_SLOT	(PTS_N + PTS_GUARD)

#define COEF_N		256

#define K56_SLOT	64

static struct int_complex pts[2][PTS_SLOT];
/* What both sides were seeded with, kept so the guard can be checked against
 * it: comparing the two sides against each other cannot see an overrun both
 * of them make. */
static struct int_complex seed_pts[PTS_SLOT];
static float lin_[2][COEF_N];
static float dfe_[2][COEF_N];
static float con_[2][COEF_N];
static float ecc_[2][COEF_N];
/*
 * The two V.34 cancellers' coefficients live OUTSIDE the object even though
 * the real ones are inside it: `v34_object::echo0_coeff` is 144 shorts and is
 * followed by `echo1`, whose `coeff` and `dline` are POINTERS -- so a trial
 * whose tap count runs past 144 would compare two different addresses and
 * report a difference the fixture made.  The two pointer words are masked in
 * `obj_ptrs_v` either way.
 */
static short scoef_[2][2][COEF_N];
static unsigned char k56_[2][K56_SLOT] __attribute__((aligned(8)));

/*
 * The pointers `setup_visual` wires, per slot.  Same argument as `obj_ptrs`
 * above: exhaustive by construction, because it is exactly the set of words
 * the fixture writes an address into.
 */
static const unsigned int obj_ptrs_v[] = {
	0x3548,		/* p3548          -> the VPcmFloModem   */
	0x80c0,		/* echo0.coeff    -> inside the object   */
	0x9140,		/* echo1.coeff    -> inside the object   */
	0xac18		/* pac18          -> the K56FlexFloModem */
};
static const unsigned int xf_ptrs_v[] = {
	0x175c,		/* modem.demodulator                     */
	0x6124,		/* v92modem.modulator                    */
	0x6bf0		/* echoCanceller.echoCoeff               */
};
static const unsigned int dem_ptrs_v[] = {
	0x004, 0x018, 0x1d8, 0x23c,
	0x254		/* array_254 -- the constellation floats */
};
static const unsigned int equ_ptrs_v[] = {
	0x14,		/* linearEquCoefs                        */
	0x40		/* dfeCoefs                              */
};

/*
 * Finite coefficient values.  A seeded 32-bit pattern is a NaN or an infinity
 * often enough that the run would be measuring what two compilations do to
 * `fistpl` of one, which is the x87's answer and not this reconstruction's.
 * The extremes are here on purpose: 1e30 times the scale overflows the 32-bit
 * conversion and both sides must produce the same integer indefinite.
 */
static const float coef_v[] = {
	0.0f, 1.0f, -1.0f, 0.5f, -0.25f, 3.0f, 1.0e-8f, 1.0e30f, -1.0e30f,
	0.9999f, -0.9999f, 12345.6f,
	/*
	 * THESE THREE SEPARATE THE CONSTELLATION SCALE'S TYPE, and nothing
	 * else in the table does.  1.7 as a `double` is 1.6999999999999999
	 * and as a `float` is 1.7000000476837158; the two truncate to the
	 * same `int` for every small value, and differ by one for a value
	 * whose product sits just below an integer.  1e7 is the worked case:
	 * 1.7 * 1e7 truncates to 16999999 and 1.7f * 1e7 to 17000000.
	 */
	1.0e7f, 2.0e7f, 1.0e8f
};
#define NCOEF	((int)(sizeof coef_v / sizeof coef_v[0]))

/* Length fields, against the `maxCount` table below. */
static const unsigned int len_v[] = { 0u, 1u, 7u, 80u, 200u, 0xffffffffu };
#define NLEN	((int)(sizeof len_v / sizeof len_v[0]))

static const unsigned long max_v[] = { 0ul, 1ul, 7ul, 80ul, 192ul };
#define NMAX	((int)(sizeof max_v / sizeof max_v[0]))

/*
 * `sweepCounter` values.  The two arms wrap at `n / 5 == 750` and at
 * `n / 15 == 100`, and the field is signed and never reset, so the negative
 * rows are the ones a wrong declaration gets wrong.
 */
static const int sweep_v[] = {
	0, 1, 14, 15, 74, 1499, 1500, 3749, 3750, 22499, -1, -16, -3751
};
#define NSWEEP	((int)(sizeof sweep_v / sizeof sweep_v[0]))

/* `v34_object::role`: both roles the object tests for, and a third value. */
static const short role_v[] = { 0x65, 0x66, 0x12 };
#define NROLE	((int)(sizeof role_v / sizeof role_v[0]))

struct vtrial {
	int		status;
	int		ri;		/* index into role_v      */
	int		li;		/* index into len_v       */
	int		mi;		/* index into max_v       */
	int		si;		/* index into sweep_v     */
	int		phase3;		/* V90Demodulator::inPhase3 */
	int		analog;		/* VPcmFloModem::info0Layout */
	int		session;	/* VPcmFloModem::pcmSessionType */
};

static void
setup_visual(int n, const struct vtrial *t)
{
	int side, k;

	lfsr_state = 0x31c7u + 0x9e37u * (unsigned)n;

	fill_pair(obj_[0], obj_[1], OBJ_SLOT);
	fill_pair(xf_[0], xf_[1], XF_SLOT);
	fill_pair(dem_[0], dem_[1], DEM_SLOT);
	fill_pair(equ_[0], equ_[1], EQU_SLOT);
	fill_pair(v92mod_[0], v92mod_[1], V92MOD_SLOT);
	fill_pair(k56_[0], k56_[1], K56_SLOT);
	fill_pair(pts[0], pts[1], sizeof pts[0]);
	memcpy(seed_pts, pts[0], sizeof seed_pts);

	for (k = 0; k < COEF_N; k++) {
		float v = coef_v[(k + n) % NCOEF];

		lin_[0][k] = lin_[1][k] = v;
		dfe_[0][k] = dfe_[1][k] = coef_v[(k + n + 3) % NCOEF];
		con_[0][k] = con_[1][k] = coef_v[(k + n + 5) % NCOEF];
		ecc_[0][k] = ecc_[1][k] = coef_v[(k + n + 7) % NCOEF];
		scoef_[0][0][k] = scoef_[1][0][k] =
		    (short)((k * 977 + n * 31) & 0xffff);
		scoef_[0][1][k] = scoef_[1][1][k] =
		    (short)((k * 1543 + n * 17) & 0xffff);
	}

	for (side = 0; side < 2; side++) {
		struct v34_object *o = O(side);
		VPcmFloModem *x = XF(side);
		V90Demodulator *d = D(side);
		V90Equalizer *eq = (V90Equalizer *)equ_[side];

		o->status = t->status;
		o->p3548 = x;
		o->pac18 = k56_[side];
		o->role = role_v[t->ri];

		/* Selector 0's V.34 arm: the ring's write cursor. */
		o->hist1_idx = (short)len_v[t->li];

		o->echo0.taps = len_v[t->li];
		o->echo0.coeff = scoef_[side][0];
		o->echo1.taps = len_v[t->li];
		o->echo1.coeff = scoef_[side][1];

		x->info0Layout = t->analog;
		x->pcmSessionType = t->session;
		x->sweepCounter = sweep_v[t->si];
		x->modem.demodulator = d;
		x->v92modem.modulator = (V92Modulator *)v92mod_[side];
		x->echoCanceller.filterLength = len_v[t->li];
		x->echoCanceller.echoCoeff = ecc_[side];

		d->equalizer = eq;
		d->phase2Info = (V90Phase2Info *)ph2_[side];
		d->mappingParamsAlt = (V90MappingParams *)mpar_[side];
		d->autoDigitalImpDetector =
		    (V90AutoDigitalImpDetector *)adid_[side];
		d->inPhase3 = (unsigned int)t->phase3;
		d->nofSymbols = len_v[t->li];
		d->word_260 = (unsigned int)(n * 7);
		d->array_254 = con_[side];

		eq->linearEquLength = len_v[t->li];
		eq->linearEquCoefs = lin_[side];
		eq->dfeLength = len_v[t->li];
		eq->dfeCoefs = dfe_[side];
	}
}

static void
compare_visual(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "the points",
		     pts[0], pts[1], sizeof pts[0], tag);
	CMP_MASKED(what, "the V.34 object", obj_, OBJ_SLOT, obj_ptrs_v, tag);
	CMP_MASKED(what, "the VPcmFloModem", xf_, XF_SLOT, xf_ptrs_v, tag);
	CMP_MASKED(what, "the demodulator", dem_, DEM_SLOT, dem_ptrs_v, tag);
	CMP_MASKED(what, "the equaliser", equ_, EQU_SLOT, equ_ptrs_v, tag);
	CMP_MASKED(what, "the V.92 modulator", v92mod_, V92MOD_SLOT,
		   v92mod_ptrs, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the K56flex modem",
		     k56_[0], k56_[1], K56_SLOT, tag);
}

/*
 * The guard past what the caller said the array holds.  Checked against the
 * SEED and not against the other side, for the reason `compare_all`'s guard
 * gives: two identical overruns would agree with each other.
 *
 * `beyond` is where the caller's bound ends, and it is NOT always `maxCount`:
 * selectors 3 and 4 write `points[0]` regardless, which is D710, so those two
 * are allowed one point and the guard starts after it.
 */
static int
guard_touched(unsigned long beyond)
{
	unsigned long k;

	for (k = beyond; k < PTS_SLOT; k++)
		if (pts[1][k].re != seed_pts[k].re ||
		    pts[1][k].im != seed_pts[k].im ||
		    pts[0][k].re != seed_pts[k].re ||
		    pts[0][k].im != seed_pts[k].im)
			return 1;
	return 0;
}

static int
run_visual(void)
{
	int what, status, ri, li, mi, si, ph, an, se;
	int n = 0;
	int sawPoints = 0, sawNone = 0, sawDrain = 0, sawSweep = 0;
	int sawOverrun = 0, sawPartial = 0, sawK56 = 0, sawAboveTable = 0;

	diff_begin("VPcmV34GetVisualDiagnostics");

	set_level(0);

	for (what = 0; what <= 9; what++)
	for (status = 0; status <= 3; status++)
	for (ri = 0; ri < NROLE; ri++)
	for (li = 0; li < NLEN; li++)
	for (mi = 0; mi < NMAX; mi++) {
		struct vtrial t;
		unsigned long got0, got1;
		long tag;

		si = (li * NMAX + mi) % NSWEEP;
		ph = (mi & 1);
		an = ((li + mi) & 1);
		se = ((li + what) & 1);

		t.status = status;
		t.ri = ri;
		t.li = li;
		t.mi = mi;
		t.si = si;
		t.phase3 = ph;
		t.analog = an;
		t.session = se;

		tag = (long)what * 1000000 + status * 100000 + ri * 10000
		    + li * 1000 + mi * 10;

		setup_visual(n++, &t);

		got0 = VPcmV34GetVisualDiagnostics(obj_[0], what,
						   pts[0], max_v[mi]);
		got1 = ref_VPcmV34GetVisualDiagnostics(obj_[1], what,
						       pts[1], max_v[mi]);

		diff_eq_int("the count (%ld)", (long)got0, (long)got1, tag);
		compare_visual("VPcmV34GetVisualDiagnostics", tag);

		/*
		 * The bound the caller stated, plus D710's one point for the
		 * two selectors that do not read it.
		 */
		{
			unsigned long beyond = max_v[mi];

			if (what == 3 || what == 4)
				if (beyond < 1ul)
					beyond = 1ul;
			diff_eq_int("nothing past the caller's bound (%ld)",
				    guard_touched(beyond), 0, tag);

			if ((what == 3 || what == 4) && max_v[mi] == 0ul &&
			    (pts[1][0].re != seed_pts[0].re ||
			     pts[1][0].im != seed_pts[0].im))
				sawOverrun = 1;
		}

		if (got1 > 0ul)
			sawPoints = 1;
		else
			sawNone = 1;

		/* The ring drain, which happens even when nothing comes out. */
		if (what == 0 && O(1)->hist1_idx == 0 &&
		    len_v[li] != 0u && (short)len_v[li] != 0)
			sawDrain = 1;

		if (XF(1)->sweepCounter != sweep_v[si])
			sawSweep = 1;

		/* A partial fill: the real half left as the caller had it. */
		if ((what == 3 || what == 4) &&
		    (status == 1 || status == 2) &&
		    pts[1][0].re == seed_pts[0].re && pts[1][0].im == 0 &&
		    seed_pts[0].im != 0)
			sawPartial = 1;

		if (status == 3 && role_v[ri] == 0x65 && got1 == 0ul &&
		    (what == 0 || what == 1 || what == 2 || what == 8))
			sawK56 = 1;

		if (what == 9 && got1 == 0ul)
			sawAboveTable = 1;
	}

	diff_eq_int("some selector returned points", sawPoints, 1, 0);
	diff_eq_int("some selector returned none", sawNone, 1, 0);
	diff_eq_int("reading the residual ring emptied it", sawDrain, 1, 0);
	diff_eq_int("the sweep counter advanced", sawSweep, 1, 0);
	diff_eq_int("D710: selectors 3 and 4 wrote past a zero bound",
		    sawOverrun, 1, 0);
	diff_eq_int("the PCM arms of 3 and 4 left the real half alone",
		    sawPartial, 1, 0);
	diff_eq_int("the K56flex stubs reported nothing", sawK56, 1, 0);
	diff_eq_int("a selector past the table returned nothing",
		    sawAboveTable, 1, 0);

	return diff_end();
}

/*
 * One axis moves, the blob runs twice, and the two point arrays must differ.
 * Same argument as `run_separation` above: an observable difference in the
 * object under test, which no path counter stands in for.
 */
static int
saw_points_differ(const struct vtrial *a, const struct vtrial *b, int what,
		  unsigned long maxCount, int n)
{
	static struct int_complex first[PTS_SLOT];

	setup_visual(n, a);
	(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], what, pts[1], maxCount);
	memcpy(first, pts[1], sizeof first);

	setup_visual(n, b);
	(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], what, pts[1], maxCount);

	return memcmp(first, pts[1], sizeof first) != 0;
}

static int
run_visual_separation(void)
{
	struct vtrial base, other;
	int n = 70000;

	diff_begin("VPcmV34GetVisualDiagnostics: the axes separate");

	set_level(0);

	base.status = 0;
	base.ri = 0;
	base.li = 4;		/* 200 -- longer than any maxCount below */
	base.mi = 3;		/* 80                                    */
	base.si = 1;
	base.phase3 = 0;
	base.analog = 1;
	base.session = 0;

	/* The V.34 residual ring against the PCM constellation. */
	other = base;
	other.status = 2;
	diff_eq_int("selector 0: status separates V.34 from the PCM modem",
		    saw_points_differ(&base, &other, 0, 80ul, n++), 1, 0);

	/* The role test on the V.90 arm. */
	other = base;
	other.status = 1;
	base.status = 1;
	base.ri = 1;		/* answer -> the PCM modem                */
	other.ri = 0;		/* call   -> V.34's own ring             */
	diff_eq_int("selector 0: the role separates on a V.90 session",
		    saw_points_differ(&base, &other, 0, 80ul, n++), 1, 0);

	base.status = 0;
	base.ri = 0;

	/* The equaliser's two arms. */
	other = base;
	other.status = 2;
	diff_eq_int("selector 1: status separates the two equalisers",
		    saw_points_differ(&base, &other, 1, 80ul, n++), 1, 0);

	/* The sweep counter drives the horizontal axis. */
	base.status = 2;
	base.session = 0;
	other = base;
	other.si = 8;
	diff_eq_int("selector 0: the sweep counter separates",
		    saw_points_differ(&base, &other, 0, 80ul, n++), 1, 0);

	/* Phase 3 and the data phase are two different geometries. */
	other = base;
	other.phase3 = 1;
	diff_eq_int("selector 0: inPhase3 separates the two geometries",
		    saw_points_differ(&base, &other, 0, 80ul, n++), 1, 0);

	/* The analog gate inside VPcmFloModem. */
	base.session = 1;
	base.analog = 1;
	other = base;
	other.analog = 0;
	diff_eq_int("selector 0: info0Layout separates",
		    saw_points_differ(&base, &other, 0, 80ul, n++), 1, 0);

	/* The two echo cancellers are two different sources. */
	base.status = 0;
	base.session = 0;
	base.analog = 1;

	{
		static struct int_complex five[PTS_SLOT];

		setup_visual(n, &base);
		(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], 5, pts[1],
						      80ul);
		memcpy(five, pts[1], sizeof five);

		setup_visual(n, &base);
		(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], 6, pts[1],
						      80ul);

		diff_eq_int("selectors 5 and 6 answer from different arrays",
			    memcmp(five, pts[1], sizeof five) != 0, 1, 0);
		n++;
	}

	/* The two resampler fields. */
	{
		static struct int_complex three[PTS_SLOT];

		setup_visual(n, &base);
		(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], 3, pts[1],
						      80ul);
		memcpy(three, pts[1], sizeof three);

		setup_visual(n, &base);
		(void)ref_VPcmV34GetVisualDiagnostics(obj_[1], 4, pts[1],
						      80ul);

		diff_eq_int("selectors 3 and 4 answer from different fields",
			    memcmp(three, pts[1], sizeof three) != 0, 1, 0);
		n++;
	}

	return diff_end();
}

/*
 * ===========================================================================
 * THE ACCESSORS BY NAME
 * ===========================================================================
 *
 * WHAT THIS ADDS OVER `run_visual`, WHICH ALREADY DRIVES THE SAME CODE.
 *
 * `VPcmV34GetVisualDiagnostics` dispatches to these eleven members, and its
 * blob-side alias calls the blob's own renamed copies internally, so every one
 * of them is ALREADY compared against the object by the section above --
 * transitively. `coverage.py` cannot see that: it counts a symbol tested when
 * some test object references its `ref_` alias BY NAME, and these were not
 * referenced by name anywhere. They read as "translated, alias exists, and NOT
 * tested" while being differentially exercised on every one of `run_visual`'s
 * trials. Finding F8326.
 *
 * So this section closes a LABELLING gap and not a coverage one, and it is
 * worth having for a second reason that is real: it pins the EXPORTED SYMBOL.
 * `run_visual` would still pass if a member stopped being called and the
 * dispatcher grew an equivalent body inline; this would not. The blob exports
 * eleven entry points and a caller outside this library may use any of them.
 *
 * The fixture is `setup_visual`'s, unchanged -- the point is the call, not a
 * new input space -- and the whole point array plus every object is compared,
 * so a member writing somewhere it should not is a failure here as well.
 */
static int
run_members(void)
{
	int li, mi, ph, ri, n = 100000;
	int sawPoints = 0, sawNone = 0, sawClamp = 0;

	diff_begin("the accessors, called by name");

	set_level(0);

	for (li = 0; li < NLEN; li++)
	for (mi = 0; mi < NMAX; mi++)
	for (ph = 0; ph < 2; ph++)
	for (ri = 0; ri < NROLE; ri++) {
		struct vtrial t;
		unsigned long g0, g1, mx = max_v[mi];
		int k0, k1;
		short jb[2][8];
		long tag;

		t.status = 0;
		t.ri = ri;
		t.li = li;
		t.mi = mi;
		t.si = (li * NMAX + mi) % NSWEEP;
		t.phase3 = ph;
		t.analog = ((li + mi) & 1);
		t.session = ((li + ph) & 1);

		tag = (long)li * 100000 + mi * 10000 + ph * 1000 + ri * 100;

		/* --- VPcmFloModem::getConstellation ---------------------- */
		setup_visual(n++, &t);
		g0 = XF(0)->getConstellation(pts[0], mx);
		g1 = ref_VPcmFloModem_getConstellation(XF(1), pts[1], mx);
		diff_eq_int("getConstellation count (%ld)", (long)g0, (long)g1,
			    tag);
		compare_visual("getConstellation", tag);
		diff_eq_int("getConstellation stayed inside the bound (%ld)",
			    guard_touched(mx), 0, tag);
		if (g1 > 0ul)
			sawPoints = 1;
		else
			sawNone = 1;
		if (g1 == mx && len_v[li] > (unsigned int)mx)
			sawClamp = 1;

		/* --- VPcmFloModem::getLinearEqualizer -------------------- */
		setup_visual(n++, &t);
		g0 = XF(0)->getLinearEqualizer(pts[0], mx);
		g1 = ref_VPcmFloModem_getLinearEqualizer(XF(1), pts[1], mx);
		diff_eq_int("getLinearEqualizer count (%ld)", (long)g0,
			    (long)g1, tag);
		compare_visual("getLinearEqualizer", tag);
		diff_eq_int("getLinearEqualizer stayed inside the bound (%ld)",
			    guard_touched(mx), 0, tag);

		/* --- VPcmFloModem::getDFE -------------------------------- */
		setup_visual(n++, &t);
		g0 = XF(0)->getDFE(pts[0], mx);
		g1 = ref_VPcmFloModem_getDFE(XF(1), pts[1], mx);
		diff_eq_int("getDFE count (%ld)", (long)g0, (long)g1, tag);
		compare_visual("getDFE", tag);
		diff_eq_int("getDFE stayed inside the bound (%ld)",
			    guard_touched(mx), 0, tag);

		/*
		 * --- the eight K56flex stubs -------------------------------
		 *
		 * Every one is `xor %eax,%eax; ret` or a bare `ret`, so the
		 * claim under test is what they do NOT do: no point is
		 * written, no short is written, and the object they are
		 * called on is untouched.  `guard_touched(0)` is the whole
		 * array, and it is checked against the SEED rather than
		 * against the other side, so two identical overruns would
		 * still fail.
		 */
		setup_visual(n++, &t);
		memset(jb, 0x5a, sizeof jb);

		k0 = ((K56FlexFloModem *)k56_[0])->getConstellation(pts[0], mx);
		k1 = ref_K56Flex_getConstellation(k56_[1], pts[1], mx);
		diff_eq_int("K56 getConstellation (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getLinearEqualizer(pts[0],
								      mx);
		k1 = ref_K56Flex_getLinearEqualizer(k56_[1], pts[1], mx);
		diff_eq_int("K56 getLinearEqualizer (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getDFE(pts[0], mx);
		k1 = ref_K56Flex_getDFE(k56_[1], pts[1], mx);
		diff_eq_int("K56 getDFE (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getDecisionErrors(pts[0],
								     mx);
		k1 = ref_K56Flex_getDecisionErrors(k56_[1], pts[1], mx);
		diff_eq_int("K56 getDecisionErrors (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getResamplerPhase(pts[0],
								     mx);
		k1 = ref_K56Flex_getResamplerPhase(k56_[1], pts[1], mx);
		diff_eq_int("K56 getResamplerPhase (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getResamplerOffset(pts[0],
								      mx);
		k1 = ref_K56Flex_getResamplerOffset(k56_[1], pts[1], mx);
		diff_eq_int("K56 getResamplerOffset (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getK56FlexMpBits(jb[0]);
		k1 = ref_K56Flex_getK56FlexMpBits(k56_[1], jb[1]);
		diff_eq_int("K56 getK56FlexMpBits (%ld)", k0, k1, tag);

		k0 = ((K56FlexFloModem *)k56_[0])->getK56FlexJaBits(jb[0]);
		k1 = ref_K56Flex_getK56FlexJaBits(k56_[1], jb[1]);
		diff_eq_int("K56 getK56FlexJaBits (%ld)", k0, k1, tag);

		((K56FlexFloModem *)k56_[0])->setMinMaxRates(li, mi);
		ref_K56Flex_setMinMaxRates(k56_[1], li, mi);

		((K56FlexFloModem *)k56_[0])->enterPhase3FullDuplex();
		ref_K56Flex_enterPhase3FullDuplex(k56_[1]);

		diff_eq_int("the K56flex stubs wrote no point (%ld)",
			    guard_touched(0ul), 0, tag);
		diff_eq_int("the K56flex stubs wrote no short (%ld)",
			    memcmp(jb[0], jb[1], sizeof jb[0]) != 0 ||
			    jb[0][0] != (short)0x5a5a, 0, tag);
		compare_visual("the K56flex stubs", tag);
	}

	/*
	 * NON-VACUITY, and it is about what the trials OBSERVED rather than
	 * about a branch being entered (finding F8163): the constellation
	 * getter must have returned points on some trial and none on another,
	 * and `maxCount` must have clamped a longer field on at least one.
	 */
	diff_eq_int("getConstellation returned points somewhere", sawPoints,
		    1, 0);
	diff_eq_int("getConstellation returned none somewhere", sawNone, 1, 0);
	diff_eq_int("maxCount clamped a longer field somewhere", sawClamp,
		    1, 0);

	return diff_end();
}

/*
 * Native-constructed owners, then an explicit DSP-component boundary.
 *
 * This is separate from setup_visual's synthetic grid. Both registered create
 * operations really construct their own graphs (9600 Hz, 48-sample fragment,
 * V.34 id, caller). The host supplies a zero dsp_info, codec 4, rates 300..56000
 * and zero I/O delay through the existing parameter apparatus. No callback or
 * live object is fabricated/copied. vpcm_create's root+0x2c and Create's status
 * 0 / role 0x65 stores are independent native anchors, not inferred from the
 * diagnostic's output. Pointers are translated only in comparison snapshots.
 *
 * The narrower boundary is intentional: the caller supplies short echo-history
 * samples, lag 0 and a short adaptation error through the exported echo APIs;
 * likewise complex short samples/errors through the equalizer APIs. This proves
 * component input histories, NOT that a modem handshake generated these errors.
 * All samples lie within the native transmit caller's half-short range. One
 * complete delay-line fill makes lag 0's read initialized and in bounds. Reset
 * clears the histories/taps before use; no count, pointer or coefficient store
 * is planted. Zero/one are output capacities, never forged filter lengths.
 *
 * Native anchors: init 0x71d70 sets taps 144/dlen 1656 and the five pointers;
 * EchoFilter 0x71ec0 shifts history and appends cursor+lag+taps-1; EchoAdapt
 * 0x71f50 adds hist*err to split 32-bit taps. Thus after one Filter/Adapt the
 * last near/far taps are +4/-8 and all earlier taps remain zero. Equalizer
 * cleanup 0x727e0 clears 0x3cc bytes and writes unity 4096 at +0x190. Adapt
 * 0x729a0 on 80 copies of (1024,-1024), error (256,128), subtracts 2 from
 * every real tap and 6 from every imaginary tap. These integer known answers
 * are independent of the visual implementation. Visual 0x7364 reads signed
 * shorts at owner+0x770/+0x810 and doubles them; echo arms use signed shorts
 * from the separately installed banks. Separate output storage cannot alias.
 *
 * PCM status 2 and selector 8's nontrivial status/role paths are NOT covered.
 */
static void
history_owners(struct v34_object *a, struct v34_object *b, long tag)
{
	int bank, side;
	struct v34_object *o[2] = { a, b };
	static unsigned char s[2][0x1080];
	static const unsigned int off[2][5] = {
		{ 0x81f8, 0x81f8, 0x9018, 0x80d8, 0x8ee8 },
		{ 0x9278, 0x9278, 0xa098, 0x9158, 0x9f68 }
	};

	for (bank = 0; bank < 2; bank++) {
		for (side = 0; side < 2; side++) {
			struct v34_echo *e = bank ? &o[side]->echo1 :
						    &o[side]->echo0;
			unsigned int k;

			memcpy(s[side], e, sizeof s[side]);
			for (k = 0; k < 5; k++) {
				unsigned int p;

				memcpy(&p, s[side] + 4 * k, 4);
				p -= (unsigned int)(size_t)o[side];
				diff_eq_int("history: echo pointer owner (%ld)",
					    p, off[bank][k],
					    tag * 100 + side * 10 + bank);
				memcpy(s[side] + 4 * k, &p, 4);
			}
			diff_eq_int("history: native tap count (%ld)",
				    e->taps, 144, tag);
			diff_eq_int("history: native delay length (%ld)",
				    e->dlen, 1656, tag);
		}
		diff_eq_obj_(__FILE__, __LINE__, "history echo owner",
			     "0x1080 bytes", s[0], s[1], sizeof s[0],
			     tag * 10 + bank);
	}
	diff_eq_obj("history equalizer owner", struct v34_equalizer,
		    &a->receiver.equalizer, &b->receiver.equalizer, tag);
}

static int
run_visual_history(void)
{
	struct dp_operations *ops[2] = { 0, 0 };
	struct dp *root[2] = { 0, 0 };
	void *runtime[2] = { 0, 0 };
	struct v34_object *o[2];
	struct dsp_info info[2];
	static unsigned char before[2][0xd258];
	static const int selector[] = { 1, 5, 6 };
	static const unsigned long capacity[] = { 0, 1, 7, 192 };
	static struct int_complex expected[PTS_SLOT];
	int side, i, stage, sel, cap, calls = 0;
	int producer_calls = 0;

	diff_begin("visual lifecycle: V.34 constructed owner / DSP component inputs");
	set_level(0);
	harness_reg_reset();
	dp_vpcm_init();
	ref_dp_vpcm_init();
	for (i = 0; i < harness_reg_ours.count; i++)
		if (harness_reg_ours.id[i] == 34)
			ops[0] = (struct dp_operations *)harness_reg_ours.ops[i];
	for (i = 0; i < harness_reg_ref.count; i++)
		if (harness_reg_ref.id[i] == 34)
			ops[1] = (struct dp_operations *)harness_reg_ref.ops[i];
	diff_eq_int("history: both native operation tables registered",
		    ops[0] != 0 && ops[1] != 0, 1, 0);
	if (!ops[0] || !ops[1])
		return diff_end();

	for (side = 0; side < 2; side++) {
		harness_param_reset();
		memset(&info[side], 0, sizeof info[side]);
		harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&info[side]);
		harness_param_set(MDMPRM_CODECTYPE, 4);
		runtime[side] = side ? ref_dp_runtime_create(&info[side]) :
				      dp_runtime_create(&info[side]);
		diff_eq_int("history: runtime constructed (%ld)",
			    runtime[side] != 0, 1, side);
		if (!runtime[side])
			break;
		harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)runtime[side]);
		harness_param_set(MDMPRM_MIN_RATE, 300);
		harness_param_set(MDMPRM_MAX_RATE, 56000);
		harness_param_set(MDMPRM_IODELAY, 0);
		root[side] = ops[side]->create(&info[side], 34, 1, 9600, 48,
					     ops[side]);
		diff_eq_int("history: graph constructed (%ld)",
			    root[side] != 0, 1, side);
		if (!root[side])
			break;
		diff_eq_int("history: native root extent (%ld)",
			    harness_alloc_reqsize(root[side]), 0xd258, side);
		o[side] = (struct v34_object *)
			  ((unsigned char *)root[side] + 0x2c);
		diff_eq_int("history: Create established V.34 status (%ld)",
			    o[side]->status, 0, side);
		diff_eq_int("history: Create established caller role (%ld)",
			    o[side]->role, 0x65, side);
	}
	if (root[0] && root[1]) {
		history_owners(o[0], o[1], -1);
		for (stage = 0; stage < 2; stage++) {
			for (side = 0; side < 2; side++) {
				struct v34_equalizer *q = &o[side]->receiver.equalizer;

				if (stage == 0) {
					if (side) {
						ref_V34EchoCleanUp(&o[side]->echo0);
						ref_V34EchoCleanUp(&o[side]->echo1);
						ref_V34EqualizerCleanUp(q);
					} else {
						V34EchoCleanUp(&o[side]->echo0);
						V34EchoCleanUp(&o[side]->echo1);
						V34EqualizerCleanUp(q);
					}
					producer_calls += 3;
				} else {
					/* Real producers; no internal field writes. */
					for (i = 0; i < 1656; i++) {
						if (side) {
							ref_V34EchoUpdateDelayLine(&o[side]->echo0, 1024);
							ref_V34EchoUpdateDelayLine(&o[side]->echo1, -2048);
						} else {
							V34EchoUpdateDelayLine(&o[side]->echo0, 1024);
							V34EchoUpdateDelayLine(&o[side]->echo1, -2048);
						}
					}
					if (side) {
						diff_eq_int("history: first near filter is zero",
							    ref_V34EchoFilter(&o[side]->echo0, 0), 0, side);
						diff_eq_int("history: first far filter is zero",
							    ref_V34EchoFilter(&o[side]->echo1, 0), 0, side);
						ref_V34EchoAdapt(&o[side]->echo0, 256);
						ref_V34EchoAdapt(&o[side]->echo1, 256);
					} else {
						diff_eq_int("history: first near filter is zero",
							    V34EchoFilter(&o[side]->echo0, 0), 0, side);
						diff_eq_int("history: first far filter is zero",
							    V34EchoFilter(&o[side]->echo1, 0), 0, side);
						V34EchoAdapt(&o[side]->echo0, 256);
						V34EchoAdapt(&o[side]->echo1, 256);
					}
					for (i = 0; i < 80; i++) {
						if (side)
							ref_V34EqualizerUpdateDelayLine(q, 1024, -1024);
						else
							V34EqualizerUpdateDelayLine(q, 1024, -1024);
					}
					if (side)
						ref_V34EqualizerAdapt(q, 256, 128);
					else
						V34EqualizerAdapt(q, 256, 128);
					producer_calls += 3312 + 4 + 80 + 1;
				}
			}
			history_owners(o[0], o[1], stage);
			for (sel = 0; sel < 3; sel++)
			for (cap = 0; cap < 4; cap++) {
				unsigned long got[2], k, n = capacity[cap];
				unsigned long bound = sel == 0 ? 80 : 144;
				long tag = stage * 100 + selector[sel] * 10 + cap;

				if (n > bound)
					n = bound;
				memset(expected, 0x5a, sizeof expected);
				memcpy(pts[0], expected, sizeof expected);
				memcpy(pts[1], expected, sizeof expected);
				for (k = 0; k < n; k++) {
					if (sel == 0) {
						expected[k].re = (k == 40 ? 8192 : 0) - stage * 4;
						expected[k].im = -stage * 12;
					} else {
						expected[k].re = 0;
						expected[k].im = stage && k == 143 ?
								 (sel == 1 ? 4 : -8) : 0;
					}
				}
				for (side = 0; side < 2; side++)
					memcpy(before[side], root[side], sizeof before[side]);
				got[0] = VPcmV34GetVisualDiagnostics(o[0], selector[sel],
							   pts[0], capacity[cap]);
				got[1] = ref_VPcmV34GetVisualDiagnostics(o[1], selector[sel],
							       pts[1], capacity[cap]);
				calls += 2;
				for (side = 0; side < 2; side++) {
					diff_eq_int("history: anchored count (%ld)",
						    got[side], n, tag * 2 + side);
					diff_eq_obj_(__FILE__, __LINE__, "history output + guard",
						     "native integer known answer", pts[side],
						     expected, sizeof expected, tag * 2 + side);
					diff_eq_obj_(__FILE__, __LINE__, "history: root unchanged",
						     "0xd258 bytes, pointers intact", root[side],
						     before[side], sizeof before[side], tag * 2 + side);
				}
				diff_eq_obj_(__FILE__, __LINE__, "history differential output",
					     "points + guard", pts[0], pts[1], sizeof pts[0], tag);
			}
		}
	}
	for (side = 0; side < 2; side++) {
		if (root[side])
			ops[side]->destroy(root[side]);
		if (runtime[side]) {
			if (side)
				ref_dp_runtime_delete(runtime[side]);
			else
				dp_runtime_delete(runtime[side]);
		}
	}
	harness_param_reset();
	diff_eq_int("history: executed diagnostic calls (both implementations)",
		    calls, 48, 0);
	diff_eq_int("history: explicit component init/input calls (both implementations)",
		    producer_calls, 6800, 0);
	printf("visual lifecycle denominator: 2 constructed graphs, 3 selectors, "
	       "2 stages, 4 capacities; %d diagnostic calls, %d component calls\n",
	       calls, producer_calls);
	return diff_end();
}

/*
 * INFO0 at the registered sample-buffer boundary (wave43, contract 6451157725).
 *
 * Each endpoint has its own real runtime and id-34 constructed graph. Caller
 * inputs are the peer's actual short outputs through a 288-sample delay, with
 * codec 4, rates 2400..33600, I/O delay 216, 9600 Hz and max fragment 48. Four
 * samples per process call are a legal subdivision of that fragment. No state,
 * callback, message, receiver count or history is planted or copied into an
 * owner. Existing visual fixtures above retain their separate input domains.
 *
 * Native TX54 counts to ten (0x6410b..0x6411e), selects TX24 and calls
 * V34SetINFO0aBits on O+0xa94c (0x685b8..0x685d1). The record has words ff,84,0,
 * length17, CRC ffff and lead-in ff72. Native FSK supplies the recovered bits;
 * 0x66944..0x66950 requires length+16 before the halfword CRC comparison at
 * 0x6bda4. The polynomial is 1021, MSB first (0x66907/0x66922). Acceptance at
 * step445 moves the caller microstate44->58, with both transmitters already
 * 24->60. The received payload is ff,84,0 (17 bits), CRC 8fd9. Received word2
 * also contains CRC bits, so it must not be treated as a third payload byte.
 *
 * The second case erases only external caller input at steps390..410, after
 * payload completion at382. It preserves the payload and reaches count33, but
 * native CRC rejection at442 resets the register before observation. The last
 * invalid-frame bit is therefore UNKNOWN; either completion of its observed
 * fifteen-bit CRC prefix must disagree. Rejection/restart is a passing case.
 *
 * This proves one INFO0 component recipe and one channel-error case. It does
 * not establish INFO1, progress1/2, negotiated PCM/K56 status2/3 or telephone
 * interoperability. The positive history continues through the first saved
 * L1 block returned after its native output FIFO; CRC rejection is unchanged.
 */
enum { INFO0_STEPS = 446, INFO0_DELAY = 288, INFO0_PAYLOAD = 17,
	INFO0_BITS = 33 };

/* Pointer-free observations. Pointers are normalised only in these copies. */
struct info0_projection {
	int rc, status, progress, role, micro, rx, tx;
	int count, pending, sr, rx_record, rx_length, rx_crc;
	int tx_record, tx_length, tx_pos, tx_crc, tx_avail, tx_repeats;
	int tx_word[3], rx_word[3];
	unsigned tx_acc;
};

struct info0_events {
	int sync_step, tx_start_step[2], tx_done_step[2];
	int terminal_step, accepts, rejects, consumed, observed;
	int bit[INFO0_BITS], bit_step[INFO0_BITS];
	unsigned payload, computed_crc, received_crc, crc_prefix;
	int payload_step, payload_crc, unexpected_returns, nonzero_status_progress;
};

static void
info0_project(struct info0_projection *p, struct v34_object *o, int rc)
{
	const struct v34_bitsource *r =
		(const struct v34_bitsource *)o->paa70;
	const struct v34_bitsource *t =
		(const struct v34_bitsource *)o->paa6c;
	int i;

	memset(p, 0, sizeof *p);
	p->rc = rc;
	p->status = o->status;                 /* O+0, not E+0 */
	p->progress = o->progress;             /* E=O+4 */
	p->role = o->role;
	p->micro = o->microstate;
	p->rx = o->rxstate;
	p->tx = o->txstate;
	p->count = o->short_aa78;
	p->pending = o->fsk.nbits;
	p->sr = o->fsk.sr;
	p->rx_record = r ? (int)((size_t)r - (size_t)o) : -1;
	p->tx_record = t ? (int)((size_t)t - (size_t)o) : -1;
	/* Read only the native-owned records, even on a failing pointer check. */
	if (r == &o->msgrec[1]) {
		p->rx_length = r->nbits;
		p->rx_crc = (unsigned short)r->crc;
		for (i = 0; i < 3; i++)
			p->rx_word[i] = (unsigned short)r->word[i];
	}
	if (t == &o->msgrec[0]) {
		p->tx_length = t->nbits;
		p->tx_pos = t->pos;
		p->tx_crc = (unsigned short)t->crc;
		p->tx_avail = t->avail;
		p->tx_repeats = t->repeats;
		p->tx_acc = (unsigned)t->acc;
		for (i = 0; i < 3; i++)
			p->tx_word[i] = (unsigned short)t->word[i];
	}
}

static unsigned
info0_crc_bit(unsigned crc, unsigned bit)
{
	unsigned feedback = (crc >> 15) ^ bit;

	crc = (crc << 1) & 0xffff;
	return feedback ? crc ^ 0x1021 : crc;
}

static void
info0_observe(struct info0_events *e, const struct info0_projection *p,
	      const struct info0_projection *prev, const char *text,
	      int step, int ep, int side)
{
	long tag = side * 10000 + ep * 1000 + step;
	int n = 0, bit = p->sr & 1;

	e->unexpected_returns += p->rc != DPSTAT_OK;
	e->nonzero_status_progress += p->status != 0 || p->progress != 0;
	if (prev->tx == 54 && p->tx == 24 && e->tx_start_step[ep] < 0)
		e->tx_start_step[ep] = step;
	if (prev->tx == 24 && p->tx == 60 && e->tx_done_step[ep] < 0)
		e->tx_done_step[ep] = step;
	if (step == 145) {
		static const int words[3] = { 0xff, 0x84, 0 };

		diff_eq_int("INFO0: generated record owner (%ld)", p->tx_record,
			    0xa94c, tag);
		diff_eq_obj("INFO0 generated words", int[3], p->tx_word, words, tag);
		diff_eq_int("INFO0: generated length (%ld)", p->tx_length, 17, tag);
		diff_eq_int("INFO0: initial producer CRC (%ld)", p->tx_crc, 0xffff, tag);
		diff_eq_int("INFO0: lead-in accumulator (%ld)", p->tx_acc, 0xff72, tag);
		diff_eq_int("INFO0: lead-in bit count (%ld)", p->tx_avail, 16, tag);
		diff_eq_int("INFO0: initial producer position (%ld)", p->tx_pos, 0, tag);
	}
	if (ep != 0)
		return;                         /* primary receive role: caller */
	if (prev->micro == 41 && p->micro == 44 && e->sync_step < 0) {
		e->sync_step = step;
		diff_eq_int("INFO0: receive record owner (%ld)", p->rx_record,
			    0xa97c, tag);
		diff_eq_int("INFO0: receive length (%ld)", p->rx_length, 17, tag);
		diff_eq_int("INFO0: receive CRC starts at ffff (%ld)",
			    p->rx_crc, 0xffff, tag);
		diff_eq_int("INFO0: receive count starts at zero (%ld)", p->count, 0, tag);
	}
	if (prev->micro == 44 && p->micro != 44) {
		/* Acceptance clears the counter; rejection clears CRC and sr. */
		n = prev->count + 1;
		e->terminal_step = step;
		e->accepts += strstr(text, "info0 received in DET_INFO") != 0;
		e->rejects += strstr(text, "info0 CRC not received properly in DET_INFO") != 0;
		if (p->micro == 41)
			bit = -1;
		else
			e->received_crc = p->sr; /* read-only caller acceptance observer */
	} else if (prev->micro == 44 && p->count != prev->count) {
		diff_eq_int("INFO0: one recovered bit per consumption (%ld)",
			    p->count, prev->count + 1, tag);
		n = p->count;
	}
	if (n == 0)
		return;
	diff_eq_int("INFO0: contiguous consumed count (%ld)", n, e->consumed + 1, tag);
	diff_eq_int("INFO0: consumed count in message bound (%ld)",
		    n >= 1 && n <= INFO0_BITS, 1, tag);
	if (n < 1 || n > INFO0_BITS)
		return;
	e->consumed = n;
	e->bit[n - 1] = bit;
	e->bit_step[n - 1] = step;
	e->observed += bit >= 0;
	if (n <= INFO0_PAYLOAD && bit >= 0) {
		e->payload = (e->payload << 1) | (unsigned)bit;
		e->computed_crc = info0_crc_bit(e->computed_crc, (unsigned)bit);
		diff_eq_int("INFO0: native payload CRC clock (%ld)", p->rx_crc,
			    e->computed_crc, tag);
		if (n == INFO0_PAYLOAD) {
			e->payload_step = step;
			e->payload_crc = p->rx_crc;
		}
	} else if (n < INFO0_BITS && bit >= 0) {
		/* Fifteen CRC bits remain observable even on native rejection. */
		e->crc_prefix = (e->crc_prefix << 1) | (unsigned)bit;
	}
}

static int
info0_tx_ring_index(const struct v34_object *o, const void *p)
{
	long offset = (long)((size_t)p - (size_t)o) - 0x2228;
	return offset >= 0 && offset < V34_TXQ_RING * 4 && (offset & 3) == 0 ?
		(int)(offset / 4) : -1;
}

static int
info0_rx_ring_index(const struct v34_object *o, const void *p)
{
	long offset = (long)((size_t)p - (size_t)&o->receiver) - 0x0c;
	return offset >= 0 && offset < V34_RXQ_RING * 4 && (offset & 3) == 0 ?
		(int)(offset / 4) : -1;
}

static int
info0_l1_arrival(struct dp_operations **ops, struct dp *root[2][2],
                 struct v34_object *o[2][2], struct info0_projection prev[2][2],
                 short wire[2][2][INFO0_DELAY], int wp, short last_out[2][2][4])
{
	/* Continue the same channel after return1127. Its 288 samples / four
	 * samples per call delay the saved peer frame by 72 calls to1199.
	 * Bound80 further calls, stopping at arrival or either microstate edge.
	 * The saved complex pairs enter RX72 demodulation one call after arrival.
	 * This does not establish later probe/negotiation completion. */
	static const short first_block[4] = { 9212, 9261, 4968, -5955 };
	struct arrival_projection {
		struct info0_projection state;
		short input[4];
	} p[2][2];
	short in[2][2][4], out[2][2][4], emitted[2][4];
	short saved_pairs[2][4][2];
	int saved_origin[2] = { -1, -1 };
	struct pipeline_projection {
		int rd_before, rd_after, wr_before, wr_after, count_before, count_after;
		int rms_before, rms_after;
		short echo_before, echo_after, pairs[4][2], rms[4], hilbert[5];
	} pipe[2];
	int arrival[2] = { -1, -1 };
	int step, side, ep, i, calls = 0;

	for (side = 0; side < 2; side++) {
		memcpy(emitted[side], last_out[side][1], sizeof emitted[side]);
		diff_eq_obj("L1 arrival saved emitted peer frame", short[4], last_out[side][1], first_block, side);
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = last_out[side][1 - ep][i];
	}
	wp = (wp + 4) % INFO0_DELAY;
	for (step = 1128; step <= 1207; step++) {
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct arrival_projection *q = &p[side][ep];
				long tag = step * 4 + ep * 2 + side;
				int rc;
				memset(q, 0, sizeof *q);
				if (ep == 0 && step >= 1199) {
					struct v34_receiver *rx = &o[side][ep]->receiver;
					struct pipeline_projection *z = &pipe[side];
					memset(z, 0, sizeof *z);
					z->rd_before = info0_rx_ring_index(o[side][ep], rx->rxq.rd);
					z->wr_before = info0_rx_ring_index(o[side][ep], rx->rxq.wr);
					z->count_before = rx->rxq.count;
					z->rms_before = rx->rms_idx;
					z->echo_before = o[side][ep]->echo_correction;
				}
				memcpy(q->input, in[side][ep], sizeof q->input);
				if (ep == 0 && arrival[side] < 0 &&
				    memcmp(q->input, emitted[side], sizeof q->input) == 0) {
					arrival[side] = step;
					diff_eq_obj("L1 arrival saved peer block at caller process input", short[4], q->input, first_block, tag);
				}
				if (ep == 0 && step == 1199)
					diff_eq_obj("L1 arrival channel-delay provenance", short[4], q->input, emitted[side], tag);
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				if (ep == 0 && step == 1199) {
					/* Native400c FILD signed input;400f FSTP root+ac78.
					 * 4031/4048 pass that float stage to VPcmV34Progress.
					 * Observe the real wrapper stage after the same call;
					 * this does not establish downstream queue/detector use. */
					float staged[4], expected[4];
					int remainder;
					for (i = 0; i < 4; i++) expected[i] = (float)emitted[side][i];
					memcpy(staged, (unsigned char *)root[side][ep] + 0xac78, sizeof staged);
					memcpy(&remainder, (unsigned char *)root[side][ep] + 0xd1e4, sizeof remainder);
					diff_eq_int("L1 arrival wrapper private owner (%ld)", root[side][ep]->dp_data == root[side][ep], 1, tag);
					diff_eq_int("L1 arrival no retained input remainder (%ld)", remainder, 0, tag);
					diff_eq_obj("L1 arrival signed input reaches real float stage", float[4], staged, expected, tag);
				}
				calls++;
				info0_project(&q->state, o[side][ep], rc);
				if (ep == 0 && step >= 1199) {
					/* Native bd43/bd50 passes each signed input to modem_serrint;
					 * 5d3d4 Hilbert output becomes a complex queue pair.
					 * RX72 calls rxtiming (65524); its private EAX-argument
					 * V34demodulate reads OLD rd (5af19/5af26), recording
					 * the ungained real half at rms_idx (5af3b onward).
					 * Native instruction probes witness the four pairs written
					 * at 1199 being read at 1200, after four older pairs. */
					static const short known_pairs[4][2] = {
						{ 14, 45 }, { -32, 94 }, { -50, 63 }, { -71, 81 }
					};
					static const short known_hilbert[5] = { 0, -7693, 3482, 8381, 8982 };
					struct v34_receiver *rx = &o[side][ep]->receiver;
					struct pipeline_projection *z = &pipe[side];
					int origin = step == 1199 ? z->wr_before : saved_origin[side];
					z->rd_after = info0_rx_ring_index(o[side][ep], rx->rxq.rd);
					z->wr_after = info0_rx_ring_index(o[side][ep], rx->rxq.wr);
					z->count_after = rx->rxq.count;
					z->rms_after = rx->rms_idx;
					z->echo_after = o[side][ep]->echo_correction;
					memcpy(z->hilbert, o[side][ep]->hilbert, sizeof z->hilbert);
					diff_eq_int("L1 pipeline owned queue origin (%ld)", origin >= 0, 1, tag);
					if (origin >= 0)
					for (i = 0; i < 4; i++)
						memcpy(z->pairs[i], (unsigned char *)rx + 0x0c +
						       ((origin + i) % V34_RXQ_RING) * 4, 4);
					for (i = 0; i < 4; i++) z->rms[i] = rx->rms_buf[(z->rms_before + i) % 36];
					diff_eq_int("L1 pipeline stays RX72 (%ld)", q->state.rx, 72, tag);
					diff_eq_int("L1 pipeline four writes (%ld)", (z->wr_after-z->wr_before+V34_RXQ_RING)%V34_RXQ_RING, 4, tag);
					diff_eq_int("L1 pipeline four reads (%ld)", (z->rd_after-z->rd_before+V34_RXQ_RING)%V34_RXQ_RING, 4, tag);
					diff_eq_int("L1 pipeline four RMS records (%ld)", (z->rms_after-z->rms_before+36)%36, 4, tag);
					diff_eq_int("L1 pipeline retained queue depth (%ld)", z->count_after, 4, tag);
					if (step == 1199) {
						short corrected[4], expected_head[5] = { 0, 0, 0, 0, 0 };
						short correction = z->echo_before;
						for (i = 0; i < 4; i++) {
							corrected[i] = (short)((unsigned short)emitted[side][i] + (unsigned short)correction);
							correction = (short)(((int)corrected[i] * 0xed8 - (int)emitted[side][i] * 4096) >> 12);
							expected_head[4-i] = corrected[i];
						}
						diff_eq_int("L1 pipeline echo feed disabled (%ld)", o[side][ep]->tx.tx_flags & V34_EC_FEED, 0, tag);
						diff_eq_int("L1 pipeline entrance filter disabled (%ld)", ((VPcmFloModem *)o[side][ep]->p3548)->entranceFilterApplied, 0, tag);
						diff_eq_int("L1 pipeline residual recurrence (%ld)", z->echo_after, correction, tag);
						diff_eq_obj("L1 pipeline actual arrived samples enter Hilbert history", short[5], z->hilbert, expected_head, tag);
						diff_eq_obj("L1 pipeline native residual known answer", short[5], z->hilbert, known_hilbert, tag);
						diff_eq_obj("L1 pipeline native complex output known answer", short[4][2], z->pairs, known_pairs, tag);
						saved_origin[side] = origin;
						memcpy(saved_pairs[side], z->pairs, sizeof z->pairs);
					} else {
						short expected_rms[4];
						for (i = 0; i < 4; i++) expected_rms[i] = saved_pairs[side][i][0];
						diff_eq_int("L1 pipeline saved write origin becomes read origin (%ld)", z->rd_before, saved_origin[side], tag);
						diff_eq_obj("L1 pipeline saved pairs remain at consumed origin", short[4][2], z->pairs, saved_pairs[side], tag);
						diff_eq_obj("L1 pipeline consumed real halves reach ungained RMS", short[4], z->rms, expected_rms, tag);
					}
				}
				diff_eq_int("L1 arrival live status zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("L1 arrival engine progress zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("L1 arrival process return OK (%ld)", rc, DPSTAT_OK, tag);
				prev[side][ep] = q->state;
			}
			diff_eq_obj("L1 arrival selected state and input", struct arrival_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("L1 arrival four-sample process output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (step >= 1199)
			diff_eq_obj("L1 pipeline pointer-free queue provenance", struct pipeline_projection, &pipe[0], &pipe[1], step);
		if (((arrival[0] >= 0 || arrival[1] >= 0) && step >= 1200) ||
		    p[0][0].state.micro != 59 || p[1][0].state.micro != 59 ||
		    p[0][1].state.micro != 51 || p[1][1].state.micro != 51) break;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		wp = (wp + 4) % INFO0_DELAY;
	}
	for (side = 0; side < 2; side++)
		diff_eq_int("L1 arrival first saved peer frame input step (%ld)", arrival[side], 1199, side);
	diff_eq_int("L1 arrival process-call denominator", calls, 292, 0);
	printf("L1 arrival denominator: 1 unchanged channel recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls; saved returned peer frame reaches caller input; "
	       "saved derived pairs consumed by RX72 demodulation one call later; no later probe/negotiation claim\n", calls / 4, calls);
	return calls;
}

static int
info0_l1_output(struct dp_operations **ops, struct dp *root[2][2],
		struct v34_object *o[2][2], struct info0_projection prev[2][2],
		short wire[2][2][INFO0_DELAY], int wp, short last_out[2][2][4],
		short sent[2][INFO0_DELAY / 4][4], int origin[INFO0_DELAY / 4],
		const int saved_index[2], const unsigned saved[2][4], const int produced_step[2])
{
	/* Native5cfd2 reads OLD rd low16,5cfda stores tx_sample;bd67 FILDs that
	 * signed short and bd74 stores float before refill. The wrapper appends at
	 * FIFO+4+2*old_count (3eca), returns oldest samples (3f77), then shifts the
	 * retained tail (3f98). In this recipe four FIFO samples survive each call.
	 * Thus saved block pops1126 but returns1127. The initial empty-FIFO timing
	 * hypothesis was falsified in scratch; this bound includes the observed FIFO.
	 */
	static const short first_block[4] = { 9212, 9261, 4968, -5955 };
	struct output_projection {
		struct info0_projection state;
		int private_self, rd_before, rd, wr, queue_count, cursor, tx_sample;
		int fifo_before, fifo_after, remainder_before, remainder_after, input_origin;
		unsigned head[4], saved[4];
		float floating[4];
		short popped[4], fifo_head_before[4], fifo_head_after[4], returned[4], input[4];
	} p[2][2];
	struct output_events {
		int producer_step, saved_index, pop_step, float_step, fifo_step, return_step;
		int pop_rd_before, pop_rd_after, return_rd_before, return_rd_after;
		int fifo_count, fifo_delay, return_tx_sample;
		short popped[4], returned[4];
	} event[2];
	short in[2][2][4], out[2][2][4], previous_fifo[2][2][4];
	int step, side, ep, i, slot = wp / 4, calls = 0;

	memset(event, 0, sizeof event);
	memset(previous_fifo, 0, sizeof previous_fifo);
	for (side = 0; side < 2; side++) {
		event[side].producer_step = produced_step[side];
		event[side].saved_index = saved_index[side];
		event[side].pop_step = event[side].float_step = event[side].fifo_step = -1;
		event[side].return_step = -1;
		diff_eq_int("L1 output: saved producer epoch (%ld)", produced_step[side], 1122, side);
		diff_eq_int("L1 output: saved ring origin (%ld)", saved_index[side], 66, side);
		memcpy(sent[side][slot], last_out[side][0], sizeof sent[side][slot]);
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = last_out[side][1 - ep][i];
	}
	origin[slot] = 1122; wp = (wp + 4) % INFO0_DELAY;
	dsplib_debug_capture_on = 1; set_level(2);
	for (step = 1123; step <= 1127; step++) {
		slot = wp / 4;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct output_projection *q = &p[side][ep];
				struct output_events *e = &event[side];
				float expected_float[4];
				long tag = step * 4 + ep * 2 + side;
				int rc, returned_saved = 1;

				memset(q, 0, sizeof *q);
				q->private_self = root[side][ep]->dp_data == root[side][ep];
				q->rd_before = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.rd);
				q->input_origin = origin[slot];
				memcpy(q->input, in[side][ep], sizeof q->input);
				if (q->private_self) {
					memcpy(&q->fifo_before, (unsigned char *)root[side][ep] + 0xd178, 4);
					memcpy(&q->remainder_before, (unsigned char *)root[side][ep] + 0xd1e4, 4);
					memcpy(q->fifo_head_before, (unsigned char *)root[side][ep] + 0xd17c, sizeof q->fifo_head_before);
				}
				if (q->rd_before >= 0)
				for (i = 0; i < 4; i++) {
					memcpy(&q->head[i], (unsigned char *)o[side][ep] + 0x2228 +
					       ((q->rd_before + i) % V34_TXQ_RING) * 4, 4);
					q->popped[i] = (short)q->head[i];
				}
				for (i = 0; i < 4; i++) expected_float[i] = (float)q->popped[i];
				if (ep == 1) {
					diff_eq_int("L1 output: actual source input frame (%ld)", q->input_origin, step - 72, tag);
					diff_eq_obj("L1 output actual emitted caller frame", short[4], q->input, sent[side][slot], tag);
					if (saved_index[side] >= 0 && saved_index[side] < V34_TXQ_RING)
					for (i = 0; i < 4; i++)
						memcpy(&q->saved[i], (unsigned char *)o[side][ep] + 0x2228 +
						       ((saved_index[side] + i) % V34_TXQ_RING) * 4, 4);
					diff_eq_obj("L1 output saved raw block remains intact", unsigned[4], q->saved, saved[side], tag);
				}
				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				info0_project(&q->state, o[side][ep], rc);
				q->rd = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.rd);
				q->wr = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.wr);
				q->queue_count = o[side][ep]->tx.txq.count;
				q->cursor = o[side][ep]->vect_idx;
				q->tx_sample = o[side][ep]->tx_sample;
				memcpy(q->returned, out[side][ep], sizeof q->returned); /* read-only returned-sample mapping observer */
				if (q->private_self) {
					memcpy(&q->fifo_after, (unsigned char *)root[side][ep] + 0xd178, 4);
					memcpy(&q->remainder_after, (unsigned char *)root[side][ep] + 0xd1e4, 4);
					memcpy(q->fifo_head_after, (unsigned char *)root[side][ep] + 0xd17c, sizeof q->fifo_head_after);
					memcpy(q->floating, (unsigned char *)root[side][ep] + 0xaef8, sizeof q->floating);
				}
				diff_eq_int("L1 output: capture complete (%ld)", dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("L1 output: process return OK (%ld)", rc, DPSTAT_OK, tag);
				diff_eq_int("L1 output: status zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("L1 output: progress zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("L1 output: wrapper private owner is root (%ld)", q->private_self, 1, tag);
				diff_eq_int("L1 output: queue pointers owned (%ld)", q->rd_before >= 0 && q->rd >= 0 && q->wr >= 0, 1, tag);
				diff_eq_int("L1 output: four queue entries dequeued (%ld)", (q->rd - q->rd_before + V34_TXQ_RING) % V34_TXQ_RING, 4, tag);
				diff_eq_int("L1 output: input remainder before (%ld)", q->remainder_before, 0, tag);
				diff_eq_int("L1 output: input remainder after (%ld)", q->remainder_after, 0, tag);
				diff_eq_int("L1 output: retained FIFO before (%ld)", q->fifo_before, 4, tag);
				diff_eq_int("L1 output: retained FIFO after (%ld)", q->fifo_after, 4, tag);
				diff_eq_obj("L1 output signed queue pops to exact float stage", float[4], q->floating, expected_float, tag);
				diff_eq_obj("L1 output current pops become next FIFO head", short[4], q->fifo_head_after, q->popped, tag);
				diff_eq_obj("L1 output returned samples map previous FIFO head", short[4], q->returned, q->fifo_head_before, tag);
				diff_eq_int("L1 output: tx_sample retains last signed queue pop (%ld)", q->tx_sample, q->popped[3], tag);
				if (step > 1123)
					diff_eq_obj("L1 output retained FIFO continuity", short[4], q->fifo_head_before, previous_fifo[side][ep], tag);
				memcpy(previous_fifo[side][ep], q->fifo_head_after, sizeof q->fifo_head_after);
				if (ep == 0) {
					diff_eq_int("L1 output: caller stays micro59 (%ld)", q->state.micro, 59, tag);
					diff_eq_int("L1 output: caller stays TX silence (%ld)", q->state.tx, 5, tag);
					diff_eq_int("L1 output: caller counter (%ld)", q->state.count, step - 961, tag);
				} else {
					diff_eq_int("L1 output: peer stays micro51 (%ld)", q->state.micro, 51, tag);
					diff_eq_int("L1 output: peer remains TX51 (%ld)", q->state.tx, 51, tag);
					diff_eq_int("L1 output: peer remains RX53 (%ld)", q->state.rx, 53, tag);
					diff_eq_int("L1 output: RX53 bypass holds count42 (%ld)", q->state.count, 42, tag);
					diff_eq_int("L1 output: raw producer cursor (%ld)", q->cursor, (step - 1121) * 4, tag);
					diff_eq_int("L1 output: read trajectory (%ld)", q->rd_before, (saved_index[side] - 12 + (step - 1123) * 4 + V34_TXQ_RING) % V34_TXQ_RING, tag);
					if (q->rd_before == saved_index[side]) {
						e->pop_step = e->float_step = e->fifo_step = step;
						e->pop_rd_before = q->rd_before; e->pop_rd_after = q->rd;
						memcpy(e->popped, q->popped, sizeof e->popped);
						diff_eq_obj("L1 output saved producer block is actually dequeued", unsigned[4], q->head, saved[side], tag);
						diff_eq_obj("L1 output original signed saved block dequeued", short[4], q->popped, first_block, tag);
					}
					for (i = 0; i < 4; i++) returned_saved &= out[side][ep][i] == first_block[i];
					if (returned_saved && e->return_step < 0) {
						e->return_step = step; e->return_rd_before = q->rd_before; e->return_rd_after = q->rd;
						e->fifo_count = q->fifo_before; e->fifo_delay = step - e->pop_step;
						e->return_tx_sample = q->tx_sample;
						memcpy(e->returned, q->returned, sizeof e->returned);
						diff_eq_obj("L1 output actual returned original-probe signed block", short[4], out[side][ep], first_block, tag);
						diff_eq_obj("L1 output copied returned signed known answer", short[4], q->returned, first_block, tag);
						diff_eq_int("L1 output: return belongs to prior queue-pop frame (%ld)", step - e->pop_step, 1, tag);
					}
				}
				prev[side][ep] = q->state;
			}
			diff_eq_obj("L1 output selected pointer-free projection", struct output_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("L1 output actual four-sample return", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (event[0].return_step >= 0 || event[1].return_step >= 0 ||
		    p[0][1].state.micro != 51 || p[1][1].state.micro != 51 ||
		    p[0][0].state.micro != 59 || p[1][0].state.micro != 59) break;
		for (side = 0; side < 2; side++) {
			memcpy(sent[side][slot], out[side][0], sizeof sent[side][slot]);
			for (ep = 0; ep < 2; ep++)
			for (i = 0; i < 4; i++)
				wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		}
		origin[slot] = step; wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0); dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct output_events *e = &event[side];
		diff_eq_int("L1 output: first saved dequeue step (%ld)", e->pop_step, 1126, side);
		diff_eq_int("L1 output: float staging step (%ld)", e->float_step, 1126, side);
		diff_eq_int("L1 output: saved block at FIFO head after (%ld)", e->fifo_step, 1126, side);
		diff_eq_int("L1 output: first complete returned block (%ld)", e->return_step, 1127, side);
		diff_eq_int("L1 output: queue-pop read origin (%ld)", e->pop_rd_before, 66, side);
		diff_eq_int("L1 output: queue-pop read result (%ld)", e->pop_rd_after, 70, side);
		diff_eq_int("L1 output: return-frame read origin (%ld)", e->return_rd_before, 70, side);
		diff_eq_int("L1 output: return-frame read result (%ld)", e->return_rd_after, 74, side);
		diff_eq_int("L1 output: observed FIFO delay (%ld)", e->fifo_delay, 1, side);
		diff_eq_int("L1 output: observed retained FIFO samples (%ld)", e->fifo_count, 4, side);
		printf("L1 output side=%s produced=%d ring=%d pop=%d float=%d fifo_head_after=%d "
		       "returned=%d pop_rd=%d->%d return_rd=%d->%d fifo=%d delay=%d "
		       "samples=%d,%d,%d,%d return_frame_tx_sample=%d\n", side ? "blob" : "ours",
		       e->producer_step, e->saved_index, e->pop_step, e->float_step, e->fifo_step,
		       e->return_step, e->pop_rd_before, e->pop_rd_after, e->return_rd_before,
		       e->return_rd_after, e->fifo_count, e->fifo_delay,
		       e->returned[0], e->returned[1], e->returned[2], e->returned[3], e->return_tx_sample);
	}
	diff_eq_obj("L1 output producer pop FIFO and returned epochs", struct output_events, &event[0], &event[1], 0);
	diff_eq_int("L1 output: process-call denominator", calls, 20, 0);
	printf("L1 output denominator: 1 unchanged peer recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls, %d paired projections and "
	       "%d paired output buffers; first saved block returned only, no peer-arrival claim\n",
	       calls / 4, calls, calls / 2, calls / 2);
	if (event[0].return_step == 1127 && event[1].return_step == 1127)
		calls += info0_l1_arrival(ops, root, o, prev, wire, wp, out);
	return calls;
}

static int
info0_l1_producer(struct dp_operations **ops, struct dp *root[2][2],
		  struct v34_object *o[2][2], struct info0_projection prev[2][2],
		  short wire[2][2][INFO0_DELAY], int wp, short last_out[2][2][4],
		  short sent[2][INFO0_DELAY / 4][4], int origin[INFO0_DELAY / 4])
{
	/* Native65c56 selects setup at count42;6b5e7 is_short==0 selects TX51,
	 * RX53 and c1200_ detectorinit. Post-microstate TX51 dispatch exits at
	 * 644c9: setup is not production. Initial TX51 dispatch62c69/65290
	 * doubles signed probe samples, scales/SAR14, then txwritequeue stores
	 * four low halfwords with zero upper halves. Original ELF known answers
	 * below are probe[0..3], c1200_, and the resulting words at scale5793.
	 * Bound42 setup calls plus4 refill calls; stop at the first raw producer.
	 */
	static const short probe_prefix[4] = { 13027, 13097, 7026, -8421 };
	static const short coeff[8] = { 0, 0, 0, 0, -22892, 15993, -22892, 15993 };
	/* Detector halfwords after its pointer: polarity,armed,count,limit,state,
	 * hi,lo,level,x[4],y[4]. The pointer is excluded from the projection. */
	static const short detector_init[16] = { 0, 0, -50, 100, 1, 0, 2048, 0,
		0, 0, 0, 0, 0, 0, 0, 0 };
	static const unsigned raw_words[4] = { 9212, 9261, 4968, 59581 };
	struct producer_projection {
		struct info0_projection state;
		int point, polarity, cursor, cursor_before, scale, target, queue_before, queue_after;
		int rd_before, wr_before, rd, wr, is_short, gain, rxflags, coeff_present, input_origin;
		short detector[16], coeff[8], input[4];
		unsigned produced[4];
	} p[2][2];
	struct producer_events {
		int setup_step, setup_count, setup_tx, setup_rx, setup_cursor;
		int producer_step, producers, cursor, wr_advance, rd_advance, old_ahead;
		int queue_before, queue_after, detector_limit, detector_lo, setup_diagnostic;
		unsigned produced[4];
	} event[2];
	short in[2][2][4], out[2][2][4];
	int step, side, ep, i, slot = wp / 4, calls = 0;

	memset(event, 0, sizeof event);
	for (side = 0; side < 2; side++) {
		event[side].setup_step = event[side].producer_step = -1;
		diff_eq_int("L1 producer: initial peer micro51 (%ld)", prev[side][1].micro, 51, side);
		diff_eq_int("L1 producer: initial peer count0 (%ld)", prev[side][1].count, 0, side);
		memcpy(sent[side][slot], last_out[side][0], sizeof sent[side][slot]);
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = last_out[side][1 - ep][i];
	}
	origin[slot] = 1077; wp = (wp + 4) % INFO0_DELAY;
	dsplib_debug_capture_on = 1; set_level(2);
	for (step = 1078; step <= 1123; step++) {
		slot = wp / 4;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct producer_projection *q = &p[side][ep];
				struct producer_events *e = &event[side];
				const struct info0_projection *before = &prev[side][ep];
				struct v34_detector *d = &o[side][ep]->detector;
				const char *text;
				long tag = step * 4 + ep * 2 + side;
				int rc;

				memset(q, 0, sizeof *q);
				q->cursor_before = o[side][ep]->vect_idx;
				q->queue_before = o[side][ep]->tx.txq.count;
				q->rd_before = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.rd);
				q->wr_before = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.wr);
				q->input_origin = origin[slot];
				memcpy(q->input, in[side][ep], sizeof q->input);
				if (ep == 1) {
					diff_eq_int("L1 producer: actual source frame index (%ld)", q->input_origin, step - 72, tag);
					diff_eq_obj("L1 producer actual emitted caller frame", short[4], q->input, sent[side][slot], tag);
				}
				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				info0_project(&q->state, o[side][ep], rc);
				q->point = o[side][ep]->tx.txpoint.word;
				q->polarity = o[side][ep]->short_358c;
				q->cursor = o[side][ep]->vect_idx;
				q->scale = o[side][ep]->tx.tx_scale;
				q->target = o[side][ep]->tx_fill_target;
				q->queue_after = o[side][ep]->tx.txq.count;
				q->rd = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.rd);
				q->wr = info0_tx_ring_index(o[side][ep], o[side][ep]->tx.txq.wr);
				q->is_short = o[side][ep]->is_short;
				q->gain = o[side][ep]->receiver.agc_gain;
				q->rxflags = (unsigned short)o[side][ep]->receiver.flags;
				q->coeff_present = d->coeff != 0;
				if (d->coeff) memcpy(q->coeff, d->coeff, sizeof q->coeff);
				memcpy(q->detector, &d->polarity, sizeof q->detector);
				text = dsplib_debug_capture_text(side);
				diff_eq_int("L1 producer: capture complete (%ld)", dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("L1 producer: process return OK (%ld)", rc, DPSTAT_OK, tag);
				diff_eq_int("L1 producer: live status zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("L1 producer: progress zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("L1 producer: queue pointers belong to owner (%ld)",
					    q->rd_before >= 0 && q->wr_before >= 0 && q->rd >= 0 && q->wr >= 0, 1, tag);
				diff_eq_int("L1 producer: four actual queue samples consumed (%ld)",
					    (q->rd - q->rd_before + V34_TXQ_RING) % V34_TXQ_RING, 4, tag);
				if (ep == 0) {
					diff_eq_int("L1 producer: caller stays micro59 (%ld)", q->state.micro, 59, tag);
					diff_eq_int("L1 producer: caller stays TX silence (%ld)", q->state.tx, 5, tag);
					diff_eq_int("L1 producer: caller counter (%ld)", q->state.count, step - 961, tag);
				} else {
					diff_eq_int("L1 producer: peer stays micro51 (%ld)", q->state.micro, 51, tag);
					diff_eq_int("L1 producer: full-session native branch (%ld)", q->is_short, 0, tag);
					diff_eq_int("L1 producer: setup timing TX (%ld)", q->state.tx, step < 1119 ? 60 : 51, tag);
					diff_eq_int("L1 producer: setup timing RX (%ld)", q->state.rx, step < 1119 ? 43 : 53, tag);
					diff_eq_int("L1 producer: native counter then RX53 bypass (%ld)", q->state.count, step < 1119 ? step - 1077 : 42, tag);
					diff_eq_int("L1 producer: cursor only advances at producer (%ld)", q->cursor, step < 1122 ? 0 : 4, tag);
					diff_eq_int("L1 producer: original recipe scale (%ld)", q->scale, 5793, tag);
					diff_eq_int("L1 producer: native queue refill target (%ld)", q->target, 16, tag);
					diff_eq_int("L1 producer: old complex point is still TONE_AB (%ld)", q->point, 300093923, tag);
					diff_eq_int("L1 producer: native live polarity retained (%ld)", q->polarity, 0, tag);
					if (step >= 1119) {
						diff_eq_int("L1 producer: detector coefficient pointer present (%ld)", q->coeff_present, 1, tag);
						diff_eq_obj("L1 producer original c1200 coefficients", short[8], q->coeff, coeff, tag);
						diff_eq_obj("L1 producer native detector setup", short[16], q->detector, detector_init, tag);
						diff_eq_int("L1 producer: DET_PENDING flag set (%ld)", (q->rxflags & 0x200) != 0, 1, tag);
						diff_eq_int("L1 producer: native low-gain arm retains gain (%ld)", q->gain, 1079, tag);
					}
					if (before->tx == 60 && q->state.tx == 51) {
						e->setup_step = step; e->setup_count = q->state.count;
						e->setup_tx = q->state.tx; e->setup_rx = q->state.rx; e->setup_cursor = q->cursor;
						e->detector_limit = q->detector[3]; e->detector_lo = q->detector[6];
						e->setup_diagnostic = strstr(text, "TONE_AB=>TX_L1") != 0 &&
							strstr(text, "RX_DPSK=>DET_AB") != 0 && strstr(text, "starting DET_AB") != 0;
						diff_eq_int("L1 producer: setup selected at increment42 (%ld)", before->count + 1, 42, tag);
					}
					if (q->state.tx == 51 && q->cursor != q->cursor_before) {
						unsigned expected[4];
						int ahead = (q->wr_before - q->rd + V34_TXQ_RING) % V34_TXQ_RING;
						if (q->wr_before >= 0)
						for (i = 0; i < 4; i++)
							memcpy(&q->produced[i], (const unsigned char *)o[side][ep] + 0x2228 +
							       ((q->wr_before + i) % V34_TXQ_RING) * 4, 4);
						for (i = 0; i < 4; i++)
							expected[i] = (unsigned short)(((short)(2 * probe_prefix[i]) * q->scale) >> 14);
						diff_eq_obj("L1 producer independent formula known answer", unsigned[4], expected, raw_words, tag);
						diff_eq_obj("L1 producer original probe scaled queue words", unsigned[4], q->produced, expected, tag);
						diff_eq_int("L1 producer: cursor before first raw block (%ld)", q->cursor_before, 0, tag);
						diff_eq_int("L1 producer: four raw samples generated (%ld)", q->cursor - q->cursor_before, 4, tag);
						diff_eq_int("L1 producer: write pointer advances four entries (%ld)",
							    (q->wr - q->wr_before + V34_TXQ_RING) % V34_TXQ_RING, 4, tag);
						diff_eq_int("L1 producer: older queue samples still ahead (%ld)", ahead, 12, tag);
						diff_eq_int("L1 producer: all new samples remain queued (%ld)", q->queue_after, ahead + 4, tag);
						e->producer_step = step; e->producers++; e->cursor = q->cursor;
						e->wr_advance = (q->wr - q->wr_before + V34_TXQ_RING) % V34_TXQ_RING;
						e->rd_advance = (q->rd - q->rd_before + V34_TXQ_RING) % V34_TXQ_RING;
						e->old_ahead = ahead; e->queue_before = q->queue_before; e->queue_after = q->queue_after;
						memcpy(e->produced, q->produced, sizeof e->produced);
					}
				}
				prev[side][ep] = q->state;
			}
			diff_eq_obj("L1 producer selected pointer-free projection", struct producer_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("L1 producer actual four-sample output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (event[0].producer_step >= 0 || event[1].producer_step >= 0 ||
		    p[0][1].state.micro != 51 || p[1][1].state.micro != 51 ||
		    p[0][0].state.micro != 59 || p[1][0].state.micro != 59) break;
		for (side = 0; side < 2; side++) {
			memcpy(sent[side][slot], out[side][0], sizeof sent[side][slot]);
			for (ep = 0; ep < 2; ep++)
			for (i = 0; i < 4; i++)
				wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		}
		origin[slot] = step; wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0); dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct producer_events *e = &event[side];
		diff_eq_int("L1 producer: native setup step (%ld)", e->setup_step, 1119, side);
		diff_eq_int("L1 producer: native setup count (%ld)", e->setup_count, 42, side);
		diff_eq_int("L1 producer: native setup TX (%ld)", e->setup_tx, 51, side);
		diff_eq_int("L1 producer: native setup RX (%ld)", e->setup_rx, 53, side);
		diff_eq_int("L1 producer: native setup cursor (%ld)", e->setup_cursor, 0, side);
		diff_eq_int("L1 producer: detector limit (%ld)", e->detector_limit, 100, side);
		diff_eq_int("L1 producer: detector lower threshold (%ld)", e->detector_lo, 2048, side);
		diff_eq_int("L1 producer: native setup diagnostics (%ld)", e->setup_diagnostic, 1, side);
		diff_eq_int("L1 producer: first actual raw producer step (%ld)", e->producer_step, 1122, side);
		diff_eq_int("L1 producer: actual raw producer blocks (%ld)", e->producers, 1, side);
		diff_eq_int("L1 producer: first block cursor (%ld)", e->cursor, 4, side);
		diff_eq_int("L1 producer: producer/output separation (%ld)", e->old_ahead, 12, side);
		printf("L1 producer side=%s setup=%d count=%d TX/RX=%d/%d cursor=%d detector=%d/%d "
		       "producer=%d blocks=%d new_cursor=%d wr/rd_advance=%d/%d old_ahead=%d "
		       "queue=%d->%d words=%u,%u,%u,%u\n", side ? "blob" : "ours", e->setup_step,
		       e->setup_count, e->setup_tx, e->setup_rx, e->setup_cursor, e->detector_limit,
		       e->detector_lo, e->producer_step, e->producers, e->cursor, e->wr_advance,
		       e->rd_advance, e->old_ahead, e->queue_before, e->queue_after,
		       e->produced[0], e->produced[1], e->produced[2], e->produced[3]);
	}
	diff_eq_obj("L1 producer setup and raw-production events", struct producer_events, &event[0], &event[1], 0);
	diff_eq_int("L1 producer: process-call denominator", calls, 180, 0);
	printf("L1 producer denominator: 1 unchanged peer recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls, %d paired projections and "
	       "%d paired output buffers; first raw L1 queue block only, external L1 not claimed\n",
	       calls / 4, calls, calls / 2, calls / 2);
	if (event[0].producer_step == 1122 && event[1].producer_step == 1122) {
		int index[2], produced_step[2];
		unsigned saved[2][4];
		for (side = 0; side < 2; side++) {
			index[side] = p[side][1].wr_before;
			produced_step[side] = event[side].producer_step;
			memcpy(saved[side], event[side].produced, sizeof saved[side]);
		}
		calls += info0_l1_output(ops, root, o, prev, wire, wp, out, sent, origin, index, saved, produced_step);
	}
	return calls;
}

static int
info0_peer_l1_entry(struct dp_operations **ops, struct dp *root[2][2],
		    struct v34_object *o[2][2], struct info0_projection prev[2][2],
		    short wire[2][2][INFO0_DELAY], int wp, short last_out[2][2][4],
		    short sent[2][INFO0_DELAY / 4][4], int origin[INFO0_DELAY / 4])
{
	/* Native peer47: (sr&15)==15 at66845, then flags==0 at6cc1b, gates
	 * recovery before increment. Otherwise6685d tests incremented count>95;
	 * 66870 clears it,6687c toggles polarity,668ad selects micro51. TX remains
	 * 60, whose post-microstate dispatch644c9 does not generate another point.
	 * Bound seven calls from the naturally reloaded89; do not process micro51.
	 */
	static const int phase_after[7] = { 0, 3, 6, 9, 12, 15, 18 };
	static const int next_after[7] = { 6, 6, 6, 18, 18, 18, 18 };
	struct l1_projection {
		struct info0_projection state;
		int polarity, producer_polarity, point, reset_flags, substate, rtd, is_short;
		int phase, next, sign, bit_len, bit_lo, bit_hi, txq, input_origin;
		short input[4];
	} p[2][2];
	struct l1_events {
		int resync_step, bit_step, bit_updates, bit_phase, bit_next, bit_sign;
		int edge_step, edge_increment, edge_counter, edge_polarity, producer_polarity;
		int point, edge_sr, edge_bits, diagnostic;
	} event[2];
	short in[2][2][4], out[2][2][4];
	int step, side, ep, i, slot = wp / 4, calls = 0;

	memset(event, 0, sizeof event);
	for (side = 0; side < 2; side++) {
		event[side].resync_step = event[side].bit_step = event[side].edge_step = -1;
		diff_eq_int("peer L1 entry: initial native microstate (%ld)", prev[side][1].micro, 47, side);
		diff_eq_int("peer L1 entry: initial reloaded counter (%ld)", prev[side][1].count, 89, side);
		diff_eq_int("peer L1 entry: initial complete register (%ld)", prev[side][1].sr, 1, side);
		diff_eq_int("peer L1 entry: initial received count (%ld)", prev[side][1].pending, 156, side);
		/* Publish the previous terminal frame1070, preserving actual lineage. */
		memcpy(sent[side][slot], last_out[side][0], sizeof sent[side][slot]);
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = last_out[side][1 - ep][i];
	}
	origin[slot] = 1070;
	wp = (wp + 4) % INFO0_DELAY;
	dsplib_debug_capture_on = 1; set_level(2);
	for (step = 1071; step <= 1077; step++) {
		slot = wp / 4;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct l1_projection *q = &p[side][ep];
				struct l1_events *e = &event[side];
				const struct info0_projection *before = &prev[side][ep];
				int phase = o[side][ep]->fsk.phase, next = o[side][ep]->fsk.next;
				int sign = o[side][ep]->fsk.prev, rc;
				const char *text;
				long tag = step * 4 + ep * 2 + side;

				memset(q, 0, sizeof *q);
				q->producer_polarity = o[side][ep]->short_358c;
				q->input_origin = origin[slot];
				memcpy(q->input, in[side][ep], sizeof q->input);
				if (ep == 1) {
					diff_eq_int("peer L1 entry: actual source frame index (%ld)", q->input_origin, step - 72, tag);
					diff_eq_obj("peer L1 entry actual emitted caller frame", short[4], q->input, sent[side][slot], tag);
				}
				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				info0_project(&q->state, o[side][ep], rc);
				q->polarity = o[side][ep]->short_358c; /* read-only live-polarity epoch observer */
				q->point = o[side][ep]->tx.txpoint.word;
				q->reset_flags = o[side][ep]->short_3588;
				q->substate = o[side][ep]->short_358a;
				q->rtd = o[side][ep]->rtd;
				q->is_short = o[side][ep]->is_short;
				q->phase = o[side][ep]->fsk.phase;
				q->next = o[side][ep]->fsk.next;
				q->sign = o[side][ep]->fsk.prev;
				q->bit_len = o[side][ep]->fsk.bit_len;
				q->bit_lo = o[side][ep]->fsk.bit_lo;
				q->bit_hi = o[side][ep]->fsk.bit_hi;
				q->txq = o[side][ep]->tx.txq.count;
				text = dsplib_debug_capture_text(side);
				diff_eq_int("peer L1 entry: capture complete (%ld)", dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("peer L1 entry: process return OK (%ld)", rc, DPSTAT_OK, tag);
				diff_eq_int("peer L1 entry: live status zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("peer L1 entry: engine progress zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("peer L1 entry: native RX stays DPSK (%ld)", q->state.rx, 43, tag);
				diff_eq_int("peer L1 entry: reset flags stay clear (%ld)", q->reset_flags, 0, tag);
				diff_eq_int("peer L1 entry: substate stays two (%ld)", q->substate, 2, tag);
				diff_eq_int("peer L1 entry: entry-polarity producer epoch (%ld)", q->producer_polarity, 1, tag);
				diff_eq_int("peer L1 entry: last native point remains vect4[2] (%ld)", q->point, -300028387, tag);
				if (ep == 0) {
					diff_eq_int("peer L1 entry: caller remains micro59 (%ld)", q->state.micro, 59, tag);
					diff_eq_int("peer L1 entry: caller counter (%ld)", q->state.count, step - 961, tag);
					diff_eq_int("peer L1 entry: caller remains TX silence (%ld)", q->state.tx, 5, tag);
				} else {
					int n = before->count + 1;
					diff_eq_int("peer L1 entry: entered from micro47 (%ld)", before->micro, 47, tag);
					diff_eq_int("peer L1 entry: repeated INFO0 conjunction false (%ld)", (q->state.sr & 15) == 15 && q->reset_flags == 0, 0, tag);
					diff_eq_int("peer L1 entry: native register known answer (%ld)", q->state.sr, step < 1074 ? 1 : 2, tag);
					diff_eq_int("peer L1 entry: native bit count (%ld)", q->state.pending, step < 1074 ? 156 : 157, tag);
					diff_eq_int("peer L1 entry: discriminator samples per bit (%ld)", q->bit_len, 12, tag);
					diff_eq_int("peer L1 entry: phase after native resync (%ld)", q->phase, phase_after[step - 1071], tag);
					diff_eq_int("peer L1 entry: next native sampling instant (%ld)", q->next, next_after[step - 1071], tag);
					diff_eq_int("peer L1 entry: bulk delay retained (%ld)", q->rtd, 148, tag);
					diff_eq_int("peer L1 entry: TX remains TONE_AB, not L1 (%ld)", q->state.tx, 60, tag);
					if (phase > q->phase) {
						e->resync_step = step;
						diff_eq_int("peer L1 entry: actual sign crossing resynchronises (%ld)", sign < 0 && q->sign > 0, 1, tag);
					}
					if (q->state.pending != before->pending) {
						e->bit_step = step; e->bit_updates++;
						e->bit_phase = phase; e->bit_next = next; e->bit_sign = sign;
						diff_eq_int("peer L1 entry: native sample is eligible (%ld)", phase >= next, 1, tag);
						diff_eq_int("peer L1 entry: actual sampled sign is positive (%ld)", sign > 0, 1, tag);
						diff_eq_int("peer L1 entry: positive discriminator maps zero (%ld)", q->bit_hi, 0, tag);
						diff_eq_int("peer L1 entry: actual zero-bit register shift (%ld)", q->state.sr, (before->sr * 2 + q->bit_hi) & 0xffff, tag);
					}
					if (q->state.micro == 47) {
						diff_eq_int("peer L1 entry: native low counter arm (%ld)", q->state.count, n, tag);
						diff_eq_int("peer L1 entry: producer/live epoch before edge (%ld)", q->polarity, q->producer_polarity, tag);
					} else {
						e->edge_step = step; e->edge_increment = n; e->edge_counter = q->state.count;
						e->edge_polarity = q->polarity; e->producer_polarity = q->producer_polarity;
						e->point = q->point; e->edge_sr = q->state.sr; e->edge_bits = q->state.pending;
						e->diagnostic = strstr(text, "TX_PHASE2_ANS=>TX_L1") != 0;
						diff_eq_int("peer L1 entry: incremented counter exceeds95 (%ld)", n > 95, 1, tag);
						diff_eq_int("peer L1 entry: native microstate becomes51 (%ld)", q->state.micro, 51, tag);
						diff_eq_int("peer L1 entry: native counter resets (%ld)", q->state.count, 0, tag);
						diff_eq_int("peer L1 entry: live epoch toggles after producer (%ld)", q->polarity, q->producer_polarity ^ 1, tag);
					}
				}
				prev[side][ep] = q->state;
			}
			diff_eq_obj("peer L1 entry selected pointer-free projection", struct l1_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("peer L1 entry four-sample output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (p[0][1].state.micro != 47 || p[1][1].state.micro != 47 ||
		    p[0][0].state.micro != 59 || p[1][0].state.micro != 59) break;
		for (side = 0; side < 2; side++) {
			memcpy(sent[side][slot], out[side][0], sizeof sent[side][slot]);
			for (ep = 0; ep < 2; ep++)
			for (i = 0; i < 4; i++)
				wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		}
		origin[slot] = step; wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0); dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct l1_events *e = &event[side];
		diff_eq_int("peer L1 entry: native sign resync step (%ld)", e->resync_step, 1071, side);
		diff_eq_int("peer L1 entry: native new-bit step (%ld)", e->bit_step, 1074, side);
		diff_eq_int("peer L1 entry: native new-bit updates (%ld)", e->bit_updates, 1, side);
		diff_eq_int("peer L1 entry: actual sampling phase (%ld)", e->bit_phase, 6, side);
		diff_eq_int("peer L1 entry: actual sampling instant (%ld)", e->bit_next, 6, side);
		diff_eq_int("peer L1 entry: actual sampled prior discriminator (%ld)", e->bit_sign, 3005, side);
		diff_eq_int("peer L1 entry: first peer47 departure (%ld)", e->edge_step, 1077, side);
		diff_eq_int("peer L1 entry: threshold known answer (%ld)", e->edge_increment, 96, side);
		diff_eq_int("peer L1 entry: reset known answer (%ld)", e->edge_counter, 0, side);
		diff_eq_int("peer L1 entry: live polarity after toggle (%ld)", e->edge_polarity, 0, side);
		diff_eq_int("peer L1 entry: last producer used entry polarity (%ld)", e->producer_polarity, 1, side);
		diff_eq_int("peer L1 entry: last point is still TONE_AB (%ld)", e->point, -300028387, side);
		diff_eq_int("peer L1 entry: full terminal register (%ld)", e->edge_sr, 2, side);
		diff_eq_int("peer L1 entry: terminal recovered count (%ld)", e->edge_bits, 157, side);
		diff_eq_int("peer L1 entry: native departure diagnostic (%ld)", e->diagnostic, 1, side);
		printf("peer L1 entry side=%s resync=%d bit_step=%d bit_updates=%d sample=%d/%d sign=%d "
		       "edge=%d increment=%d count=%d live_polarity=%d producer_polarity=%d "
		       "point=%d sr=%04x bits=%d\n", side ? "blob" : "ours", e->resync_step, e->bit_step,
		       e->bit_updates, e->bit_phase, e->bit_next, e->bit_sign, e->edge_step, e->edge_increment,
		       e->edge_counter, e->edge_polarity, e->producer_polarity, e->point, e->edge_sr, e->edge_bits);
	}
	diff_eq_obj("peer L1 entry native events and producer/live epochs", struct l1_events, &event[0], &event[1], 0);
	diff_eq_int("peer L1 entry: process-call denominator", calls, 28, 0);
	printf("peer L1 entry denominator: 1 unchanged peer recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls, %d paired projections and "
	       "%d paired output buffers; micro51 entry only, no generated L1 claim\n", calls / 4, calls, calls / 2, calls / 2);
	if (event[0].edge_step == 1077 && event[1].edge_step == 1077)
		calls += info0_l1_producer(ops, root, o, prev, wire, wp, out, sent, origin);
	return calls;
}

static int
info0_wire_receipt(struct dp_operations **ops, struct dp *root[2][2],
		   struct v34_object *o[2][2], struct info0_projection prev[2][2],
		   short wire[2][2][INFO0_DELAY], int wp, short last_out[2][2][4])
{
	/* Native TX60 selects vect4[2] at62d48 and txmit appends modulated samples
	 * at5d810..5d853. Point generation, external samples and demodulation are
	 * separate clocks. Preserve the four measured pre-reversal sample phases.
	 * The fixed channel adds288 samples=72 calls; shadow copies retain actual
	 * emitted frames and their origins, never writing into a live DSP owner.
	 * Native peer49 needs count>filtdelay+80 and sr&1 (66a6c/66a8c), then
	 * derives bulk delay=(count-filtdelay)*4-396 and reloads filtdelay (66b13).
	 * Bound172 calls: peer guard opening54 + native caller tone dwell42 +
	 * channel72 + one FSK bit4. Stop at the first peer departure, not the bound.
	 */
	static const short steady[4][4] = {
		{ 0, 2987, 4224, 2987 }, { 0, -2988, -4226, -2989 },
		{ 0, 2988, 4227, 2989 }, { 0, -2989, -4227, -2988 }
	};
	struct wire_projection {
		struct info0_projection state;
		int point, polarity, txq, filtdelay, rtd, is_short, reset_flags;
		int bit_lo, bit_hi, phase, next, sign, input_origin;
		short input[4];
	} p[2][2];
	struct wire_events {
		int point_step, changed_output, antiphase_output, changed_input, antiphase_input;
		int eligible_step, waiting, edge_step, edge_increment, edge_sr, edge_bits;
		int edge_bulk, edge_reload, edge_phase, edge_next, edge_sign, diagnostic;
	} event[2];
	short in[2][2][4], out[2][2][4], sent[2][INFO0_DELAY / 4][4];
	int origin[INFO0_DELAY / 4], step, side, ep, i, j, slot, calls = 0;

	memset(event, 0, sizeof event);
	for (side = 0; side < 2; side++) {
		event[side].point_step = event[side].changed_output = event[side].antiphase_output = -1;
		event[side].changed_input = event[side].antiphase_input = -1;
		event[side].eligible_step = event[side].edge_step = -1;
		for (j = 0; j < 4; j++) {
			short frame[4];
			for (i = 0; i < 4; i++)
				frame[i] = j == 3 ? last_out[side][0][i] :
					wire[side][1][(wp + INFO0_DELAY - 12 + j * 4 + i) % INFO0_DELAY];
			diff_eq_obj("wire receipt pre-reversal sample phase", short[4], frame, steady[(958 + j) % 4], side * 4 + j);
		}
	}
	/* The preceding loop stopped before publishing frame961. */
	for (side = 0; side < 2; side++)
	for (ep = 0; ep < 2; ep++)
	for (i = 0; i < 4; i++)
		wire[side][ep][(wp + i) % INFO0_DELAY] = last_out[side][1 - ep][i];
	wp = (wp + 4) % INFO0_DELAY;
	for (j = 0; j < INFO0_DELAY / 4; j++) {
		slot = (wp / 4 + j) % (INFO0_DELAY / 4);
		origin[slot] = 962 - INFO0_DELAY / 4 + j;
		for (side = 0; side < 2; side++)
		for (i = 0; i < 4; i++)
			sent[side][slot][i] = wire[side][1][slot * 4 + i];
	}
	dsplib_debug_capture_on = 1;
	set_level(2);
	for (step = 962; step <= 1133; step++) {
		slot = wp / 4;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct wire_projection *q = &p[side][ep];
				struct wire_events *e = &event[side];
				const struct info0_projection *before = &prev[side][ep];
				int phase = o[side][ep]->fsk.phase, next = o[side][ep]->fsk.next;
				int sign = o[side][ep]->fsk.prev, rc, changed = 0, antiphase = 1;
				const char *text;
				long tag = step * 4 + ep * 2 + side;

				memset(q, 0, sizeof *q);
				memcpy(q->input, in[side][ep], sizeof q->input);
				q->input_origin = origin[slot];
				if (ep == 1) {
					diff_eq_int("wire receipt: exact source frame index (%ld)", origin[slot], step - 72, tag);
					diff_eq_obj("wire receipt actual emitted caller frame", short[4], q->input, sent[side][slot], tag);
					for (i = 0; i < 4; i++) {
						changed |= q->input[i] != steady[(step - 72) % 4][i];
						antiphase &= q->input[i] == -steady[(step - 72) % 4][i];
					}
					if (changed && e->changed_input < 0)
						e->changed_input = step; /* read-only delayed-sample timing observer */
					if (antiphase && e->antiphase_input < 0)
						e->antiphase_input = step;
				}
				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				info0_project(&q->state, o[side][ep], rc);
				q->point = o[side][ep]->tx.txpoint.word;
				q->polarity = o[side][ep]->short_358c;
				q->txq = o[side][ep]->tx.txq.count;
				q->filtdelay = o[side][ep]->filtdelay;
				q->rtd = o[side][ep]->rtd;
				q->is_short = o[side][ep]->is_short;
				q->reset_flags = o[side][ep]->short_3588;
				q->bit_lo = o[side][ep]->fsk.bit_lo;
				q->bit_hi = o[side][ep]->fsk.bit_hi;
				q->phase = o[side][ep]->fsk.phase;
				q->next = o[side][ep]->fsk.next;
				q->sign = o[side][ep]->fsk.prev;
				text = dsplib_debug_capture_text(side);
				diff_eq_int("wire receipt: capture complete (%ld)", dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("wire receipt: process return OK (%ld)", rc, DPSTAT_OK, tag);
				diff_eq_int("wire receipt: live status zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("wire receipt: progress zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("wire receipt: filter delay unchanged (%ld)", q->filtdelay, 89, tag);
				diff_eq_int("wire receipt: polarity retained (%ld)", q->polarity, 1, tag);
				if (ep == 0) {
					changed = 0; antiphase = 1;
					for (i = 0; i < 4; i++) {
						changed |= out[side][ep][i] != steady[step % 4][i];
						antiphase &= out[side][ep][i] == -steady[step % 4][i];
					}
					if (changed && e->changed_output < 0) e->changed_output = step;
					if (antiphase && e->antiphase_output < 0) e->antiphase_output = step;
					if (q->point == -300028387 && e->point_step < 0) e->point_step = step;
					diff_eq_int("wire receipt: reversed original vect4 point (%ld)", q->point, -300028387, tag);
					diff_eq_int("wire receipt: caller remains micro59 (%ld)", q->state.micro, 59, tag);
					diff_eq_int("wire receipt: caller native counter (%ld)", q->state.count, step - 961, tag);
					diff_eq_int("wire receipt: caller tone then native silence (%ld)", q->state.tx, step < 1003 ? 60 : 5, tag);
					diff_eq_int("wire receipt: caller repeated-INFO0 guard false (%ld)", (q->state.sr & 7) == 7, 0, tag);
				} else {
					int n = before->count + 1;
					diff_eq_int("wire receipt: peer starts in micro49 (%ld)", before->micro, 49, tag);
					diff_eq_int("wire receipt: peer bit mapping low (%ld)", q->bit_lo, 1, tag);
					diff_eq_int("wire receipt: peer bit mapping high (%ld)", q->bit_hi, 0, tag);
					diff_eq_int("wire receipt: peer repeated INFO0 pattern absent (%ld)", (q->state.sr & 0x3ff) == 0x372, 0, tag);
					if (n > q->filtdelay + 80 && e->eligible_step < 0) e->eligible_step = step;
					if (q->state.micro == 49) {
						diff_eq_int("wire receipt: peer counter increment (%ld)", q->state.count, n, tag);
						diff_eq_int("wire receipt: peer full register still zero (%ld)", q->state.sr, 0, tag);
						if (n > q->filtdelay + 80) e->waiting++;
					} else {
						int bulk = (n - q->filtdelay) * 4 - 396;
						e->edge_step = step; e->edge_increment = n;
						e->edge_sr = q->state.sr; e->edge_bits = q->state.pending;
						e->edge_bulk = q->rtd; e->edge_reload = q->state.count;
						e->edge_phase = phase; e->edge_next = next; e->edge_sign = sign;
						e->diagnostic = strstr(text, "RX_PHASE1_ANS=>TX_PHASE2_ANS") != 0 &&
							strstr(text, "bulkDelay=148, filtDelay=89") != 0;
						diff_eq_int("wire receipt: peer counter guard opens (%ld)", n > q->filtdelay + 80, 1, tag);
						diff_eq_int("wire receipt: native low-bit predicate (%ld)", q->state.sr & 1, 1, tag);
						diff_eq_int("wire receipt: native first-phase sample eligible (%ld)", phase >= next, 1, tag);
						diff_eq_int("wire receipt: native sampled prior sign (%ld)", sign <= 0, 1, tag);
						diff_eq_int("wire receipt: actual register shift at edge (%ld)", q->state.sr, before->sr * 2 + q->bit_lo, tag);
						diff_eq_int("wire receipt: actual recovered bit count (%ld)", q->state.pending, before->pending + 1, tag);
						diff_eq_int("wire receipt: native peer transition (%ld)", q->state.micro, 47, tag);
						diff_eq_int("wire receipt: full-session delay arm (%ld)", q->is_short, 0, tag);
						diff_eq_int("wire receipt: native bulk formula (%ld)", q->rtd, bulk > 0 ? bulk : 1, tag);
						diff_eq_int("wire receipt: native counter reload (%ld)", q->state.count, q->filtdelay, tag);
					}
				}
				prev[side][ep] = q->state;
			}
			diff_eq_obj("wire receipt selected pointer-free projection", struct wire_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("wire receipt four-sample output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (p[0][1].state.micro != 49 || p[1][1].state.micro != 49 ||
		    p[0][0].state.micro != 59 || p[1][0].state.micro != 59) break;
		for (side = 0; side < 2; side++) {
			memcpy(sent[side][slot], out[side][0], sizeof sent[side][slot]);
			for (ep = 0; ep < 2; ep++)
			for (i = 0; i < 4; i++)
				wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		}
		origin[slot] = step;
		wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0); dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct wire_events *e = &event[side];
		diff_eq_int("wire receipt: first generated reversed point (%ld)", e->point_step, 962, side);
		diff_eq_int("wire receipt: first changed output frame (%ld)", e->changed_output, 968, side);
		diff_eq_int("wire receipt: first exact antiphase output (%ld)", e->antiphase_output, 997, side);
		diff_eq_int("wire receipt: first changed peer input (%ld)", e->changed_input, 1040, side);
		diff_eq_int("wire receipt: changed-frame transport latency (%ld)", e->changed_input - e->changed_output, 72, side);
		diff_eq_int("wire receipt: first exact antiphase peer input (%ld)", e->antiphase_input, 1069, side);
		diff_eq_int("wire receipt: antiphase-frame transport latency (%ld)", e->antiphase_input - e->antiphase_output, 72, side);
		diff_eq_int("wire receipt: native peer eligibility step (%ld)", e->eligible_step, 1015, side);
		diff_eq_int("wire receipt: eligible waiting calls (%ld)", e->waiting, 55, side);
		diff_eq_int("wire receipt: first peer departure (%ld)", e->edge_step, 1070, side);
		diff_eq_int("wire receipt: pre-reload native count (%ld)", e->edge_increment, 225, side);
		diff_eq_int("wire receipt: complete received register (%ld)", e->edge_sr, 1, side);
		diff_eq_int("wire receipt: recovered bit count (%ld)", e->edge_bits, 156, side);
		diff_eq_int("wire receipt: bulk delay known answer (%ld)", e->edge_bulk, 148, side);
		diff_eq_int("wire receipt: reloaded counter (%ld)", e->edge_reload, 89, side);
		diff_eq_int("wire receipt: observed sampling phase (%ld)", e->edge_phase, 6, side);
		diff_eq_int("wire receipt: observed sampling instant (%ld)", e->edge_next, 6, side);
		diff_eq_int("wire receipt: observed prior discriminator (%ld)", e->edge_sign, -2094, side);
		diff_eq_int("wire receipt: native transition and delay diagnostic (%ld)", e->diagnostic, 1, side);
		printf("wire receipt side=%s point=%d changed_out=%d antiphase_out=%d changed_in=%d "
		       "antiphase_in=%d eligible=%d waiting=%d edge=%d count=%d sr=%04x bits=%d "
		       "bulk=%d reload=%d phase/next=%d/%d prior_sign=%d\n", side ? "blob" : "ours",
		       e->point_step, e->changed_output, e->antiphase_output, e->changed_input,
		       e->antiphase_input, e->eligible_step, e->waiting, e->edge_step, e->edge_increment,
		       e->edge_sr, e->edge_bits, e->edge_bulk, e->edge_reload, e->edge_phase, e->edge_next, e->edge_sign);
	}
	diff_eq_obj("wire receipt generation/transport/native events", struct wire_events, &event[0], &event[1], 0);
	diff_eq_int("wire receipt: process-call denominator", calls, 436, 0);
	printf("wire receipt denominator: 1 unchanged peer recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls, %d paired projections and "
	       "%d paired output buffers; first peer49 departure only\n", calls / 4, calls, calls / 2, calls / 2);
	if (event[0].edge_step == 1070 && event[1].edge_step == 1070)
		calls += info0_peer_l1_entry(ops, root, o, prev, wire, wp, out, sent, origin);
	return calls;
}

static int
info0_tone_reversal(struct dp_operations **ops, struct dp *root[2][2],
		    struct v34_object *o[2][2], struct info0_projection prev[2][2],
		    short wire[2][2][INFO0_DELAY], int wp)
{
	/* Native caller55 increments O+aa78 and tests signed >95 at65ba4.
	 * Starting at89, seven further calls reach96:65bbe clears the counter,
	 * 65bcc toggles O+358c,65bf3 selects59. Stop on that first departure.
	 * Native TX60 loads vect4[2*(polarity&1)] at62d48..62d62; original
	 * .rodata+5370/+5378 give the two signed word known answers below.
	 * TX generation precedes receive/microstate processing (62933..62a0c).
	 * The post-microstate TX60 dispatch goes to644c9, not the point producer,
	 * so the last generated point still uses the ENTRY polarity at this edge.
	 * Peer49 needs count>filtdelay+80 at66a6c, impossible within this bound
	 * from109 with filtdelay89. No later receive edge is claimed.
	 */
	struct tone_projection {
		struct info0_projection state;
		int polarity, reset_flags, substate, filtdelay, point, producer_polarity;
	} p[2][2];
	struct tone_events {
		int bit_step, bit_updates, edge_step, edge_increment, edge_counter;
		int edge_sr, edge_bits, edge_polarity, edge_point, diagnostic, peer_counter;
	} event[2];
	short in[2][2][4], out[2][2][4];
	int step, side, ep, i, calls = 0;

	memset(event, 0, sizeof event);
	for (side = 0; side < 2; side++) {
		event[side].bit_step = event[side].edge_step = -1;
		diff_eq_int("tone edge: initial caller microstate (%ld)", prev[side][0].micro, 55, side);
		diff_eq_int("tone edge: initial caller counter (%ld)", prev[side][0].count, 89, side);
		diff_eq_int("tone edge: initial peer counter (%ld)", prev[side][1].count, 109, side);
	}
	dsplib_debug_capture_on = 1;
	set_level(2);
	for (step = 955; step <= 961; step++) {
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct tone_projection *q = &p[side][ep];
				struct tone_events *e = &event[side];
				const struct info0_projection *before = &prev[side][ep];
				const char *text;
				long tag = step * 4 + ep * 2 + side;
				int rc, new_bit, producer_polarity = o[side][ep]->short_358c;

				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				memset(q, 0, sizeof *q);
				info0_project(&q->state, o[side][ep], rc);
				q->polarity = o[side][ep]->short_358c;
				q->reset_flags = o[side][ep]->short_3588;
				q->substate = o[side][ep]->short_358a;
				q->filtdelay = o[side][ep]->filtdelay;
				q->producer_polarity = producer_polarity;
				q->point = o[side][ep]->tx.txpoint.word; /* read-only native tone producer */
				text = dsplib_debug_capture_text(side);
				new_bit = q->state.pending != before->pending;
				diff_eq_int("tone edge: capture complete (%ld)", dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("tone edge: process return OK (%ld)", rc, DPSTAT_OK, tag);
				diff_eq_int("tone edge: live status stays zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("tone edge: engine progress stays zero (%ld)", q->state.progress, 0, tag);
				diff_eq_int("tone edge: TX remains TONE_AB (%ld)", q->state.tx, 60, tag);
				diff_eq_int("tone edge: RX remains RX_DPSK (%ld)", q->state.rx, 43, tag);
				diff_eq_int("tone edge: reset flags stay clear (%ld)", q->reset_flags, 0, tag);
				diff_eq_int("tone edge: substate remains two (%ld)", q->substate, 2, tag);
				diff_eq_int("tone edge: native filter delay (%ld)", q->filtdelay, 89, tag);
				diff_eq_int("tone edge: native vect4 producer known answer (%ld)", q->point,
					    q->producer_polarity ? -300028387 : 300093923, tag);
				diff_eq_int("tone edge: received bit count (%ld)", q->state.pending, step < 958 ? 127 : 128, tag);
				diff_eq_int("tone edge: complete received register (%ld)", q->state.sr, ep ? 0 : (step < 958 ? 1 : 2), tag);
				if (new_bit) {
					diff_eq_int("tone edge: one native received bit (%ld)", q->state.pending, before->pending + 1, tag);
					diff_eq_int("tone edge: native received zero shifts register (%ld)", q->state.sr, (before->sr * 2) & 0xffff, tag);
				}
				if (ep == 0) {
					diff_eq_int("tone edge: repeated INFO0 guard remains false (%ld)", new_bit && (q->state.sr & 7) == 7, 0, tag);
					diff_eq_int("tone edge: packed late-reset guard remains false (%ld)",
						    q->reset_flags == 2 && q->substate == 2 && (q->state.sr & 0x3ff) == 0x372, 0, tag);
					diff_eq_int("tone edge: caller counter before reset (%ld)", before->count, step - 866, tag);
					diff_eq_int("tone edge: caller bounded microstate (%ld)", q->state.micro, step == 961 ? 59 : 55, tag);
					diff_eq_int("tone edge: caller counter/reset (%ld)", q->state.count, step == 961 ? 0 : before->count + 1, tag);
					diff_eq_int("tone edge: caller polarity (%ld)", q->polarity, step == 961, tag);
					if (new_bit) {
						e->bit_step = step;
						e->bit_updates++;
					}
					if (q->state.micro != 55) {
						e->edge_step = step;
						e->edge_increment = before->count + 1;
						e->edge_counter = q->state.count;
						e->edge_sr = q->state.sr;
						e->edge_bits = q->state.pending;
						e->edge_polarity = q->polarity;
						e->edge_point = q->point;
						e->diagnostic = strstr(text, "TX_PHASE1_CALL=>RX_PHASE2_CALL") != 0;
					}
				} else {
					diff_eq_int("tone edge: peer repeated INFO0 pattern absent (%ld)", q->state.sr & 0x3ff, 0, tag);
					diff_eq_int("tone edge: peer receive counter guard blocked (%ld)", q->state.count > q->filtdelay + 80, 0, tag);
					diff_eq_int("tone edge: peer remains RX_PHASE1_ANS (%ld)", q->state.micro, 49, tag);
					diff_eq_int("tone edge: peer counter increment (%ld)", q->state.count, before->count + 1, tag);
					diff_eq_int("tone edge: peer polarity retained (%ld)", q->polarity, 1, tag);
					e->peer_counter = q->state.count;
				}
				prev[side][ep] = q->state;
			}
			diff_eq_obj("tone edge pointer-free projection", struct tone_projection, &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("tone edge four-sample output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (p[0][0].state.micro != 55 || p[1][0].state.micro != 55 ||
		    p[0][1].state.micro != 49 || p[1][1].state.micro != 49)
			break;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0);
	dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct tone_events *e = &event[side];

		diff_eq_int("tone edge: native receive-bit step (%ld)", e->bit_step, 958, side);
		diff_eq_int("tone edge: native receive-bit updates (%ld)", e->bit_updates, 1, side);
		diff_eq_int("tone edge: first caller departure (%ld)", e->edge_step, 961, side);
		diff_eq_int("tone edge: incremented native threshold (%ld)", e->edge_increment, 96, side);
		diff_eq_int("tone edge: native counter reset (%ld)", e->edge_counter, 0, side);
		diff_eq_int("tone edge: native departure diagnostic (%ld)", e->diagnostic, 1, side);
		diff_eq_int("tone edge: native polarity reversal (%ld)", e->edge_polarity, 1, side);
		diff_eq_int("tone edge: peer terminal counter (%ld)", e->peer_counter, 116, side);
		printf("tone edge side=%s bit_step=%d bit_updates=%d edge=%d increment=%d "
		       "counter=%d sr=%04x bits=%d polarity=%d point=%d peer_counter=%d\n",
		       side ? "blob" : "ours", e->bit_step, e->bit_updates, e->edge_step,
		       e->edge_increment, e->edge_counter, e->edge_sr, e->edge_bits,
		       e->edge_polarity, e->edge_point, e->peer_counter);
	}
	diff_eq_obj("tone edge native event transcript", struct tone_events, &event[0], &event[1], 0);
	diff_eq_int("tone edge: process-call denominator", calls, 28, 0);
	printf("tone edge denominator: 1 unchanged peer recipe x 2 implementations x 2 endpoints; "
	       "%d further four-sample steps, %d process calls, %d paired projections and "
	       "%d paired output buffers; caller55->59 only, peer49 receive guard blocked\n",
	       calls / 4, calls, calls / 2, calls / 2);
	if (event[0].edge_step == 961 && event[1].edge_step == 961)
		calls += info0_wire_receipt(ops, root, o, prev, wire, wp, out);
	return calls;
}

static int
info0_phase1_history(struct dp_operations **ops, struct dp *root[2][2],
		     struct v34_object *o[2][2], struct info0_projection prev[2][2],
		     short wire[2][2][INFO0_DELAY], int wp)
{
	/* Native 0x66027 gates on incremented signed counter >95; 0x6604c
	 * requires the COMPLETE received register ==1, not a masked suffix.
	 * Bound at 1201 further calls (native >1200 timeout, 0x6608b), stopping
	 * at the first caller departure. The fixed peer recipe witnesses step954.
	 * Native peer 0x6b4c9/0x6b515 toggles O+358c after counter399; TX60
	 * uses its low bit to select vect4 at 0x62d48..0x62d62.
	 */
	struct phase1_projection {
		struct info0_projection state;
		int polarity, substate, filtdelay;
	} p[2][2];
	struct phase1_events {
		int threshold_step, waiting, edge_step, edge_count, edge_sr;
		int edge_diagnostic, reload, substate, peer_step, peer_polarity;
	} event[2];
	short in[2][2][4], out[2][2][4];
	int step, side, ep, i, calls = 0;
	int previous_polarity[2] = { 0, 0 };

	memset(event, 0, sizeof event);
	for (side = 0; side < 2; side++) {
		event[side].threshold_step = event[side].edge_step = -1;
		event[side].peer_step = -1;
		diff_eq_int("phase1: initial peer tone polarity (%ld)",
			    o[side][1]->short_358c, 0, side);
	}
	dsplib_debug_capture_on = 1;
	set_level(2);
	for (step = INFO0_STEPS; step < INFO0_STEPS + 1201; step++) {
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
		for (ep = 0; ep < 2; ep++) {
			for (side = 0; side < 2; side++) {
				struct phase1_projection *q = &p[side][ep];
				struct phase1_events *e = &event[side];
				const struct info0_projection *before = &prev[side][ep];
				const char *text;
				long tag = step * 4 + ep * 2 + side;
				int rc;

				dsplib_debug_capture_reset();
				rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
				calls++;
				memset(q, 0, sizeof *q);
				info0_project(&q->state, o[side][ep], rc);
				q->polarity = o[side][ep]->short_358c;
				q->substate = o[side][ep]->short_358a;
				q->filtdelay = o[side][ep]->filtdelay;
				text = dsplib_debug_capture_text(side);
				diff_eq_int("phase1: debug capture complete (%ld)",
					    dsplib_debug_capture_complete(side), 1, tag);
				diff_eq_int("phase1: process return OK (%ld)", q->state.rc, DPSTAT_OK, tag);
				diff_eq_int("phase1: live status stays zero (%ld)", q->state.status, 0, tag);
				diff_eq_int("phase1: engine progress stays zero (%ld)", q->state.progress, 0, tag);
				if (ep == 0) {
					int n = before->count + 1;

					diff_eq_int("phase1: caller entered from RX_PHASE1_CALL (%ld)", before->micro, 58, tag);
					if (q->state.micro == 58) {
						diff_eq_int("phase1: native counter increment (%ld)", q->state.count, n, tag);
						if (n > 95) {
							if (e->threshold_step < 0)
								e->threshold_step = step;
							e->waiting++;
							diff_eq_int("phase1: complete register blocks eligible caller (%ld)", q->state.sr != 1, 1, tag);
						}
					} else {
						e->edge_step = step;
						e->edge_count = n;
						e->edge_sr = q->state.sr; /* read-only complete-register observer */
						e->reload = q->state.count;
						e->substate = q->substate;
						e->edge_diagnostic = strstr(text,
							"RX_PHASE1_CALL received, count2=509,") != 0;
						diff_eq_int("phase1: first departure selects TX_PHASE1_CALL (%ld)", q->state.micro, 55, tag);
						diff_eq_int("phase1: native edge counter predicate (%ld)", n > 95, 1, tag);
						diff_eq_int("phase1: counter reload is native filtdelay (%ld)", q->state.count, q->filtdelay, tag);
					}
				} else {
					if (before->micro == 46 && q->state.micro == 49) {
						e->peer_step = step;
						e->peer_polarity = q->polarity;
						diff_eq_int("phase1: peer tone count before reversal (%ld)", before->count, 399, tag);
						diff_eq_int("phase1: peer complete received register zero (%ld)", q->state.sr, 0, tag);
						diff_eq_int("phase1: peer tone counter resets (%ld)", q->state.count, 0, tag);
						diff_eq_int("phase1: peer tone polarity toggles (%ld)", q->polarity, previous_polarity[side] ^ 1, tag);
						diff_eq_int("phase1: peer reversal diagnostic (%ld)", strstr(text,
							"TX_PHASE1_ANS=>RX_PHASE1_ANS") != 0, 1, tag);
					}
					previous_polarity[side] = q->polarity;
				}
				diff_eq_int("phase1: native transmitter remains TONE_AB (%ld)", q->state.tx, 60, tag);
				diff_eq_int("phase1: native receiver remains RX_DPSK (%ld)", q->state.rx, 43, tag);
				prev[side][ep] = q->state;
			}
			diff_eq_obj("phase1 selected pointer-free projection", struct phase1_projection,
				    &p[0][ep], &p[1][ep], step * 2 + ep);
			diff_eq_obj("phase1 four-sample output", short[4], out[0][ep], out[1][ep], step * 2 + ep);
		}
		if (p[0][0].state.micro != 58 || p[1][0].state.micro != 58)
			break;
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		wp = (wp + 4) % INFO0_DELAY;
	}
	set_level(0);
	dsplib_debug_capture_on = 0;
	for (side = 0; side < 2; side++) {
		const struct phase1_events *e = &event[side];

		diff_eq_int("phase1: first counter eligibility step (%ld)", e->threshold_step, 541, side);
		diff_eq_int("phase1: eligible calls blocked by register (%ld)", e->waiting, 413, side);
		diff_eq_int("phase1: first caller edge step (%ld)", e->edge_step, 954, side);
		diff_eq_int("phase1: observed pre-reload counter (%ld)", e->edge_count, 509, side);
		diff_eq_int("phase1: COMPLETE edge register equals one (%ld)", e->edge_sr, 1, side);
		diff_eq_int("phase1: witnessed native edge diagnostic (%ld)", e->edge_diagnostic, 1, side);
		diff_eq_int("phase1: witnessed counter reload (%ld)", e->reload, 89, side);
		diff_eq_int("phase1: witnessed edge substate (%ld)", e->substate, 2, side);
		diff_eq_int("phase1: peer tone reversal step (%ld)", e->peer_step, 845, side);
		diff_eq_int("phase1: peer tone reversal polarity (%ld)", e->peer_polarity, 1, side);
		printf("phase1 side=%s threshold=%d blocked=%d peer_reversal=%d polarity=%d "
		       "edge=%d counter=%d complete_sr=%04x reload=%d substate=%d\n",
		       side ? "blob" : "ours", e->threshold_step, e->waiting, e->peer_step,
		       e->peer_polarity, e->edge_step, e->edge_count, e->edge_sr, e->reload, e->substate);
	}
	diff_eq_obj("phase1 caller/peer event transcript", struct phase1_events, &event[0], &event[1], 0);
	diff_eq_int("phase1: continuation process-call denominator", calls, 2036, 0);
	printf("phase1 continuation denominator: 1 unchanged peer recipe x 2 implementations x "
	       "2 endpoints; 509 four-sample steps after INFO0, %d process calls, "
	       "1018 paired projections and 1018 paired output buffers; first caller edge only\n", calls);
	if (event[0].edge_step == 954 && event[1].edge_step == 954) {
		/* Publish the last frame before continuing the SAME channel history.
		 * The phase1 loop stops before its ordinary channel-buffer update.
		 */
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++)
		for (i = 0; i < 4; i++)
			wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
		wp = (wp + 4) % INFO0_DELAY;
		calls += info0_tone_reversal(ops, root, o, prev, wire, wp);
	}
	return calls;
}

static int
run_info0_history(void)
{
	struct dp_operations *ops[2] = { 0, 0 };
	int erased_case, side, ep, i, step;
	int calls = 0, graphs = 0, runtimes = 0, destroys = 0, deletes = 0;

	diff_begin("INFO0 lifecycle: generated frame / external CRC-window erasure");
	set_level(0);
	harness_reg_reset();
	dp_vpcm_init();
	ref_dp_vpcm_init();
	for (i = 0; i < harness_reg_ours.count; i++)
		if (harness_reg_ours.id[i] == 34)
			ops[0] = (struct dp_operations *)harness_reg_ours.ops[i];
	for (i = 0; i < harness_reg_ref.count; i++)
		if (harness_reg_ref.id[i] == 34)
			ops[1] = (struct dp_operations *)harness_reg_ref.ops[i];
	diff_eq_int("INFO0: both registered tables", ops[0] != 0 && ops[1] != 0, 1, 0);
	if (!ops[0] || !ops[1])
		return diff_end();

	for (erased_case = 0; erased_case < 2; erased_case++) {
		struct dsp_info info[2][2];
		struct dp *root[2][2] = { { 0, 0 }, { 0, 0 } };
		void *runtime[2][2] = { { 0, 0 }, { 0, 0 } };
		struct v34_object *o[2][2];
		struct info0_projection p[2][2], prev[2][2];
		struct info0_events event[2];
		short wire[2][2][INFO0_DELAY], in[2][2][4], out[2][2][4];
		int built = 0, wp = 0, erased = 0;
		int steps = erased_case ? 443 : INFO0_STEPS;

		memset(info, 0, sizeof info);
		memset(wire, 0, sizeof wire);
		memset(event, 0, sizeof event);
		for (side = 0; side < 2; side++) {
			event[side].sync_step = event[side].terminal_step = -1;
			event[side].computed_crc = 0xffff;
			for (i = 0; i < INFO0_BITS; i++)
				event[side].bit[i] = event[side].bit_step[i] = -1;
			for (ep = 0; ep < 2; ep++) {
				event[side].tx_start_step[ep] = event[side].tx_done_step[ep] = -1;
				harness_param_reset();
				harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&info[side][ep]);
				harness_param_set(MDMPRM_CODECTYPE, 4);
				runtime[side][ep] = side ? ref_dp_runtime_create(&info[side][ep]) :
							 dp_runtime_create(&info[side][ep]);
				diff_eq_int("INFO0: real runtime (%ld)", runtime[side][ep] != 0,
					    1, side * 2 + ep);
				if (!runtime[side][ep])
					continue;
				runtimes++;
				harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)runtime[side][ep]);
				harness_param_set(MDMPRM_MIN_RATE, 2400);
				harness_param_set(MDMPRM_MAX_RATE, 33600);
				harness_param_set(MDMPRM_IODELAY, 216);
				root[side][ep] = ops[side]->create(&info[side][ep], 34, ep == 0,
								9600, 48, ops[side]);
				diff_eq_int("INFO0: real graph (%ld)", root[side][ep] != 0,
					    1, side * 2 + ep);
				if (!root[side][ep])
					continue;
				graphs++;
				built++;
				o[side][ep] = (struct v34_object *)((unsigned char *)root[side][ep] + 0x2c);
				diff_eq_int("INFO0: root extent (%ld)", harness_alloc_reqsize(root[side][ep]),
					    0xd258, side * 2 + ep);
				diff_eq_int("INFO0: constructed role (%ld)", o[side][ep]->role,
					    ep ? 0x66 : 0x65, side * 2 + ep);
				info0_project(&prev[side][ep], o[side][ep], 0);
				diff_eq_int("INFO0: constructed microstate (%ld)", prev[side][ep].micro, 41, ep);
				diff_eq_int("INFO0: constructed RX (%ld)", prev[side][ep].rx, 43, ep);
				diff_eq_int("INFO0: constructed TX (%ld)", prev[side][ep].tx, 54, ep);
			}
		}
		if (built == 4) {
			dsplib_debug_capture_on = 1;
			set_level(2);
			for (step = 0; step < steps; step++) {
				for (side = 0; side < 2; side++)
				for (ep = 0; ep < 2; ep++)
				for (i = 0; i < 4; i++)
					in[side][ep][i] = wire[side][ep][(wp + i) % INFO0_DELAY];
				if (erased_case && step >= 390 && step <= 410) {
					for (side = 0; side < 2; side++)
						memset(in[side][0], 0, sizeof in[side][0]);
					erased += 4; /* per caller, not summed across implementations */
				}
				for (ep = 0; ep < 2; ep++) {
					for (side = 0; side < 2; side++) {
						int rc;
						long tag = erased_case * 10000 + ep * 1000 + step;

						dsplib_debug_capture_reset();
						rc = ops[side]->process(root[side][ep], in[side][ep], out[side][ep], 4);
						calls++;
						info0_project(&p[side][ep], o[side][ep], rc);
						diff_eq_int("INFO0: debug capture complete (%ld)",
							    dsplib_debug_capture_complete(side), 1, tag * 2 + side);
						info0_observe(&event[side], &p[side][ep], &prev[side][ep],
							      dsplib_debug_capture_text(side), step, ep, side);
						prev[side][ep] = p[side][ep];
					}
					diff_eq_obj("INFO0 process return/state projection", struct info0_projection,
						    &p[0][ep], &p[1][ep], erased_case * 10000 + ep * 1000 + step);
					diff_eq_obj("INFO0 process output", short[4], out[0][ep], out[1][ep],
						    erased_case * 10000 + ep * 1000 + step);
				}
				for (side = 0; side < 2; side++)
				for (ep = 0; ep < 2; ep++)
				for (i = 0; i < 4; i++)
					wire[side][ep][(wp + i) % INFO0_DELAY] = out[side][1 - ep][i];
				wp = (wp + 4) % INFO0_DELAY;
			}
			set_level(0);
			dsplib_debug_capture_on = 0;
			for (side = 0; side < 2; side++) {
				const struct info0_events *e = &event[side];
				const struct info0_projection *last = &p[side][0];
				long tag = erased_case * 2 + side;

				diff_eq_int("INFO0: sync acquired (%ld)", e->sync_step, 314, tag);
				for (ep = 0; ep < 2; ep++) {
					diff_eq_int("INFO0: matched TX record production (%ld)", e->tx_start_step[ep], 145, tag * 2 + ep);
					diff_eq_int("INFO0: matched TX completion (%ld)", e->tx_done_step[ep], 358, tag * 2 + ep);
				}
				diff_eq_int("INFO0: terminal process step (%ld)", e->terminal_step, steps - 1, tag);
				diff_eq_int("INFO0: all 17+16 bits consumed (%ld)", e->consumed, INFO0_BITS, tag);
				diff_eq_int("INFO0: observed bit values (%ld)", e->observed, erased_case ? 32 : 33, tag);
				diff_eq_int("INFO0: generated payload known answer (%ld)", e->payload, 0x1ff08, tag);
				diff_eq_int("INFO0: independently calculated CRC (%ld)", e->computed_crc, 0x8fd9, tag);
				diff_eq_int("INFO0: payload completed before erasure (%ld)", e->payload_step, 382, tag);
				diff_eq_int("INFO0: native payload CRC known answer (%ld)", e->payload_crc, 0x8fd9, tag);
				diff_eq_int("INFO0: expected acceptance count (%ld)", e->accepts, !erased_case, tag);
				diff_eq_int("INFO0: expected rejection count (%ld)", e->rejects, erased_case, tag);
				diff_eq_int("INFO0: final caller microstate (%ld)", last->micro, erased_case ? 41 : 58, tag);
				diff_eq_int("INFO0: final caller RX (%ld)", last->rx, 43, tag);
				diff_eq_int("INFO0: final caller TX (%ld)", last->tx, erased_case ? 24 : 60, tag);
				diff_eq_int("INFO0: final caller counter (%ld)", last->count, erased_case ? 33 : 0, tag);
				diff_eq_int("INFO0: all process returns OK (%ld)", e->unexpected_returns, 0, tag);
				diff_eq_int("INFO0: live status and engine progress stay zero (%ld)", e->nonzero_status_progress, 0, tag);
				if (erased_case) {
					diff_eq_int("INFO0: invalid last bit remains unknown (%ld)", e->bit[32], -1, tag);
					diff_eq_int("INFO0: either CRC completion is rejected (%ld)",
						    e->crc_prefix * 2 != e->computed_crc &&
						    e->crc_prefix * 2 + 1 != e->computed_crc, 1, tag);
					diff_eq_int("INFO0: CRC failure reset register (%ld)", last->sr, 0xffff, tag);
					diff_eq_int("INFO0: CRC failure reset accumulator (%ld)", last->rx_crc, 0xffff, tag);
				} else {
					diff_eq_int("INFO0: received CRC matches native known answer (%ld)", e->received_crc, 0x8fd9, tag);
					diff_eq_int("INFO0: observed CRC equals independent payload CRC (%ld)", e->received_crc, e->computed_crc, tag);
					diff_eq_int("INFO0: all sixteen CRC bits match received register (%ld)",
						    e->crc_prefix * 2 + (unsigned)e->bit[32], e->received_crc, tag);
					diff_eq_int("INFO0: native accepted CRC is retained (%ld)", last->rx_crc, 0x8fd9, tag);
					diff_eq_int("INFO0: matching peer microstate (%ld)", p[side][1].micro, 46, tag);
				}
				printf("INFO0 case=%s side=%s calls=%d payload_bits=%d CRC=%04x "
				       "observed_bits=%d terminal=%d accepts=%d rejects=%d erased=%d\n",
				       erased_case ? "CRC-window erasure" : "generated",
				       side ? "blob" : "ours", steps * 2, INFO0_PAYLOAD,
				       e->computed_crc, e->observed, e->terminal_step,
				       e->accepts, e->rejects, erased);
			}
			diff_eq_obj("INFO0 caller event transcript", struct info0_events,
				    &event[0], &event[1], erased_case);
			diff_eq_int("INFO0: external erased sample count", erased, erased_case ? 84 : 0, erased_case);
			if (!erased_case)
				calls += info0_phase1_history(ops, root, o, prev, wire, wp);
		}
		for (side = 0; side < 2; side++)
		for (ep = 0; ep < 2; ep++) {
			if (root[side][ep]) {
				ops[side]->destroy(root[side][ep]);
				destroys++;
			}
			if (runtime[side][ep]) {
				if (side)
					ref_dp_runtime_delete(runtime[side][ep]);
				else
					dp_runtime_delete(runtime[side][ep]);
				deletes++;
			}
		}
	}
	harness_param_reset();
	diff_eq_int("INFO0: process-call denominator", calls, 6576, 0);
	diff_eq_int("INFO0: constructed graph denominator", graphs, 8, 0);
	diff_eq_int("INFO0: constructed runtime denominator", runtimes, 8, 0);
	diff_eq_int("INFO0: graph destruction denominator", destroys, 8, 0);
	diff_eq_int("INFO0: runtime deletion denominator", deletes, 8, 0);
	printf("INFO0 lifecycle denominator: 1 id34 configuration/channel recipe, "
	       "2 external input cases x 2 implementations x 2 endpoints; "
	       "%d process calls, %d real graphs, %d real runtimes; "
	       "positive through saved peer L1 derived queue-pair consumption by RX72, CRC rejection prefix; "
	       "no INFO1/progress1/2/status2/3 claim\n",
	       calls, graphs, runtimes);
	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_sweep();
	bad |= run_separation();
	bad |= run_transcript();
	bad |= run_visual();
	bad |= run_visual_separation();
	bad |= run_members();
	bad |= run_visual_history();
	bad |= run_info0_history();

	return bad;
}
