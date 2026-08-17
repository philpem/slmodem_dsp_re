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
 *   word_2c         3 / not 3          whether the V.92 upstream rate is
 *                                      computed or reported as zero.
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
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {

void ref_VPcmV34GetDiagnostics(void *obj, void *results)
    asm("ref_VPcmV34GetDiagnostics");

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
	int	upstream;	/* V92Modulator::word_2c == 3           */
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

		((V92Modulator *)v92mod_[side])->word_2c =
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

int
main(void)
{
	int bad = 0;

	bad |= run_sweep();
	bad |= run_separation();
	bad |= run_transcript();

	return bad;
}
