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
 * function land in no object any comparison can reach; finding 3403's batch
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
 * Finding 4756 is the standing warning: a suite green over 100,000 checks per
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
 *   f21a, f248      a table            the two dB loops.  `f21a <= 0` skips
 *                                      both; the pairs below make the -6 dB
 *                                      loop run 0, 1 and many times and the
 *                                      -1 dB loop likewise, which is the only
 *                                      way the `+= 6` and the `+= 1` are
 *                                      separately observable.
 *   fac0c           0 / +ve / -ve      +0x228's zero test and its sign
 *                                      extension.
 *   f25dc           a table            the transmit level's subtrahend, both
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
 * finding 3509 rules worthless.  `run_separation` holds every axis fixed but
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

extern "C" {

void ref_VPcmV34GetDiagnostics(void *obj, void *results)
    asm("ref_VPcmV34GetDiagnostics");

unsigned long ref_VPcmV34GetVisualDiagnostics(void *obj, int what,
					      void *points,
					      unsigned long maxCount)
    asm("ref_VPcmV34GetVisualDiagnostics");

extern unsigned int ref_dsplibs_debug_level;

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
 * THE dB LADDER.  `f248 / f21a` is the ratio and the two loops step it down
 * by 0.2511597 (a -6 dB step, `+= 6`) and then by 0.79418945 (a -1 dB step,
 * `+= 1`), both as `(x * k) >> 14`, until it reaches zero.
 *
 * The pairs below are chosen so that BOTH loop trip counts take the values 0,
 * 1 and many across the table, and so that `f21a <= 0` skips the pair
 * entirely.  Without the zero-trip rows a mutation that deletes either `+=`
 * survives on every row where the other loop dominates the answer.
 */
static const struct {
	int	num;	/* v34_receiver::f248 */
	short	den;	/* v34_receiver::f21a */
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
	{  1000000,	  0 },	/* f21a == 0: the outer test fails       */
	{  1000000,	 -3 }	/* f21a < 0                              */
};
#define NSNR		((int)(sizeof snr_v / sizeof snr_v[0]))

/* +0x228's zero test, and the sign extension on the value that is not zero. */
static const short fac_v[] = { 0, 1, -1, 0x7fff, (short)0x8000, 0x1234 };
#define NFAC		((int)(sizeof fac_v / sizeof fac_v[0]))

/* `-12.0f - f25dc`: both signs, because the instruction cannot tell them. */
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
		o->f25dc = pr_v[t->pi];
		o->fac0c = fac_v[t->fi];

		RX(side)->f248 = snr_v[t->si].num;
		RX(side)->f21a = snr_v[t->si].den;

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
		d->byte_280 = 1;
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
			 * them is the "a branch ran" claim finding 3509 rules
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
	diff_eq_int("fac0c separates", saw_records_differ(&base, &other, n++),
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
 * level 2 and differs at level 1 (debug.h's note, finding 150).
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
 * `status` 0..3 and `f359c` over both roles and a third value that is
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
 * and `::dfeLength`, `V90Demodulator::word_258`,
 * `V92EchoCanceller::filterLength` and both `v34_echo::taps`.
 *
 * THE THREE MUTATING ARMS ARE WHY THE OBJECTS ARE COMPARED AND NOT JUST THE
 * POINTS.  `VPcmFloModem::sweepCounter` moves once per point in both
 * constellation arms and not at all when none come out, and selector 0's V.34
 * arm clears `v34_object::f2aa4` unconditionally -- reading the residual ring
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

/* `v34_object::f359c`: both roles the object tests for, and a third value. */
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
		o->f359c = role_v[t->ri];

		/* Selector 0's V.34 arm: the ring's write cursor. */
		o->f2aa4 = (short)len_v[t->li];

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
		d->word_258 = len_v[t->li];
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
		if (what == 0 && O(1)->f2aa4 == 0 &&
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

int
main(void)
{
	int bad = 0;

	bad |= run_sweep();
	bad |= run_separation();
	bad |= run_transcript();
	bad |= run_visual();
	bad |= run_visual_separation();

	return bad;
}
