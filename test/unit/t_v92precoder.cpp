/*
 * t_v92precoder.cpp -- differential test of V92Precoder and V92PreFilter
 * against the blob: both constructors and destructors, and all six of the
 * members that do the work.
 *
 * THE OBJECTS CANNOT LIVE IN A UNION and cannot be assigned into.  Both
 * classes declare a constructor and a destructor -- which are the four
 * symbols under test -- so both are non-trivial and a union holding one has
 * its default members deleted (finding F232).  The slots are plain aligned
 * byte arrays reached through a cast, and both sides' constructors are called
 * through asm() labels, because C++ has no syntax for running a constructor
 * over storage that already exists and this build has no <new> to spell
 * placement form with.  Writing `*O() = V92Precoder(n)` instead would
 * construct a temporary over uninitialised stack and copy the whole object,
 * which is exactly the "seeded, never zeroed" property this file depends on.
 *
 * THE OBJECTS ARE NEVER ZEROED.  Both sides get the SAME varied pseudorandom
 * bytes before every trial and the fill is reseeded each time.  Neither
 * constructor writes more than eight bytes of its object, so a zero fill
 * would make "the other 120 bytes are untouched" true by accident; here it is
 * checked against the seed the trial actually used (findings F223, F224).
 *
 * TWO WORDS OF EACH OBJECT CAN NEVER COMPARE EQUAL.  The filters are separate
 * sysdep_malloc returns on the two sides.  They are not skipped: the snapshot
 * replaces each pointer with whether THAT side's is null, which is the only
 * property of a heap address the two runs can share, and the objects behind
 * them are then compared in full -- five fields each, plus every float of
 * both history buffers.
 *
 * WHAT NO BYTE COMPARISON HERE CAN SEE, and what is done about it:
 *
 *   - V92Precoder's two filters are built with identical arguments, so
 *     swapping the two stores produces a byte-identical object.  The only
 *     observable is which allocation each pointer holds, so the relation
 *     which allocation each field holds is compared against the REFERENCE's
 *     own answer, BY ORDINAL rather than by address (finding F1353) -- a
 *     differential comparison, not an assumption about the allocator.
 *   - V92PreFilter's second sub-object is a FloatIIR, and FloatIIR and
 *     FloatFIR have the same layout and the same constructor behaviour.  A
 *     reconstruction that built two FIRs would pass every check in this file.
 *     What settles it is the relocation in the blob and the codegen tier;
 *     include/dsplib/V92PreFilter.h says so, and the mutation set records the
 *     swap as unobservable here rather than pretending otherwise.
 *
 * AND COMPARING TWO DESTROYED OBJECTS COMPARES THE ALLOCATORS.  Neither
 * destructor nulls what it frees, so afterwards both objects hold dangling
 * addresses that the snapshot flattens to "non-null" on both sides -- the
 * vacuous comparison this project has been caught by three times.  The
 * destructors are therefore checked through `harness_alloc`: how many frees,
 * no bad frees, no null frees, nothing left live -- and with each pointer
 * nulled by hand in turn, so a destructor that frees the wrong member shows
 * up as a bad free rather than as the same total.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Precoder.h"
#include "dsplib/V92PreFilter.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

void our_pre_ctor(void *self, unsigned int n) asm("_ZN11V92PrecoderC1Ej");
void our_pre_ctor2(void *self, unsigned int n) asm("_ZN11V92PrecoderC2Ej");
void our_pre_dtor(void *self) asm("_ZN11V92PrecoderD1Ev");
void our_pre_dtor2(void *self) asm("_ZN11V92PrecoderD2Ev");
void ref_pre_ctor(void *self, unsigned int n) asm("ref__ZN11V92PrecoderC1Ej");
void ref_pre_ctor2(void *self, unsigned int n) asm("ref__ZN11V92PrecoderC2Ej");
void ref_pre_dtor(void *self) asm("ref__ZN11V92PrecoderD1Ev");
void ref_pre_dtor2(void *self) asm("ref__ZN11V92PrecoderD2Ev");

void our_pf_ctor(void *self, unsigned int n) asm("_ZN12V92PreFilterC1Ej");
void our_pf_ctor2(void *self, unsigned int n) asm("_ZN12V92PreFilterC2Ej");
void our_pf_dtor(void *self) asm("_ZN12V92PreFilterD1Ev");
void our_pf_dtor2(void *self) asm("_ZN12V92PreFilterD2Ev");
void ref_pf_ctor(void *self, unsigned int n) asm("ref__ZN12V92PreFilterC1Ej");
void ref_pf_ctor2(void *self, unsigned int n) asm("ref__ZN12V92PreFilterC2Ej");
void ref_pf_dtor(void *self) asm("ref__ZN12V92PreFilterD1Ev");
void ref_pf_dtor2(void *self) asm("ref__ZN12V92PreFilterD2Ev");

/* The blob's own filter destructors, for the trials that free a sub-object by
 * hand: the reference side's filters were built by the reference side's
 * constructors and are torn down by the matching ones. */
void ref_fir_dtor(void *self) asm("ref__ZN8FloatFIRD1Ev");
void ref_iir_dtor(void *self) asm("ref__ZN8FloatIIRD1Ev");

/*
 * The three V92PreFilter members that do the work.  `this` is the first
 * STACK argument -- plain cdecl, finding F215 -- so a free function of the
 * right shape reaches them, and none of the three returns anything the
 * object leaves in %eax.
 */
void our_pf_reset(void *self) asm("_ZN12V92PreFilter5resetEv");
void ref_pf_reset(void *self) asm("ref__ZN12V92PreFilter5resetEv");
void our_pf_setcoef(void *self, float *cf, float *ci, unsigned int tf,
		    unsigned int ti)
	asm("_ZN12V92PreFilter15setCoefficientsEPfS0_jj");
void ref_pf_setcoef(void *self, float *cf, float *ci, unsigned int tf,
		    unsigned int ti)
	asm("ref__ZN12V92PreFilter15setCoefficientsEPfS0_jj");
void our_pf_process(void *self, float *in, float *out)
	asm("_ZN12V92PreFilter7processEPfS0_");
void ref_pf_process(void *self, float *in, float *out)
	asm("ref__ZN12V92PreFilter7processEPfS0_");

/* And V92Precoder's three.  `reset()` -- the one with no argument -- is not
 * written in this tree and is not driven here. */
void our_pre_reset(void *self, void *params)
	asm("_ZN11V92Precoder5resetEP16V92MappingParams");
void ref_pre_reset(void *self, void *params)
	asm("ref__ZN11V92Precoder5resetEP16V92MappingParams");
void our_pre_setcoef(void *self, float *c1, float *c2, unsigned int t1,
		     unsigned int t2)
	asm("_ZN11V92Precoder15setCoefficientsEPfS0_jj");
void ref_pre_setcoef(void *self, float *c1, float *c2, unsigned int t1,
		     unsigned int t2)
	asm("ref__ZN11V92Precoder15setCoefficientsEPfS0_jj");
void our_pre_process(void *self, unsigned int *in, int a, int b, int *out,
		     float *outf) asm("_ZN11V92Precoder7processEPjiiPiPf");
void ref_pre_process(void *self, unsigned int *in, int a, int b, int *out,
		     float *outf) asm("ref__ZN11V92Precoder7processEPjiiPiPf");

extern unsigned int ref_dsplibs_debug_level;
}

/*
 * The two filter classes have the same five fields at the same offsets, so
 * one set of accessors serves both and V92PreFilter's IIR -- whose members
 * are private -- can be read at all.  The offsets are FloatFIR.h's and
 * FloatIIR.h's; the assertions below are what keeps that true.
 */
#define FILT_SIZE	0x14
#define FILT_HIST	0x04	/* float *, sysdep_malloc'd */
#define FILT_TAPS	0x08
#define FILT_LEN	0x0c	/* entries in the history   */
#define FILT_POS	0x10

typedef char filt_fir_size[(sizeof(FloatFIR) == FILT_SIZE) ? 1 : -1];
typedef char filt_iir_size[(sizeof(FloatIIR) == FILT_SIZE) ? 1 : -1];

/* The objects, plus room past the end to catch a store that overruns. */
#define PRE_SIZE	0x80
#define PRE_SLOT	0x90
#define PF_SIZE		0x14
#define PF_SLOT		0x24

/* (0x140 & ~3) + 99, the largest history the sweep below asks for. */
#define MAXHIST		512

static unsigned char pre_ours[PRE_SLOT] __attribute__((aligned(8)));
static unsigned char pre_theirs[PRE_SLOT] __attribute__((aligned(8)));
static unsigned char pf_ours[PF_SLOT] __attribute__((aligned(8)));
static unsigned char pf_theirs[PF_SLOT] __attribute__((aligned(8)));
static unsigned char seed_copy[PRE_SLOT];

struct hist_buf {
	unsigned int w[MAXHIST];
};

static V92Precoder *
PO(void)
{
	return (V92Precoder *)pre_ours;
}

static V92Precoder *
PT(void)
{
	return (V92Precoder *)pre_theirs;
}

static V92PreFilter *
FO(void)
{
	return (V92PreFilter *)pf_ours;
}

static V92PreFilter *
FT(void)
{
	return (V92PreFilter *)pf_theirs;
}

/*
 * Seeds.  Never zero, and different on every trial, so "the constructor left
 * this alone" is a statement about the bytes the trial actually used.
 */
static void
seed(unsigned char *a, unsigned char *b, int slot, int trial)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	int i;

	for (i = 0; i < slot; i++) {
		unsigned char v;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		v = (unsigned char)((lfsr >> 3) | 1u);
		a[i] = v;
		b[i] = v;
		seed_copy[i] = v;
	}
}

/* Read one word of a filter without naming a member, so FloatIIR's private
 * section is not in the way. */
static unsigned int
filt_word(const void *p, int off)
{
	unsigned int v;

	memcpy(&v, (const unsigned char *)p + off, sizeof(v));
	return v;
}

static void *
filt_hist(const void *p)
{
	void *v;

	memcpy(&v, (const unsigned char *)p + FILT_HIST, sizeof(v));
	return v;
}

/*
 * A comparable copy of a filter: everything as it stands, except that the
 * history pointer becomes that side's own answer to "is it null".
 */
static void
filt_snapshot(void *dst, const void *src)
{
	unsigned int flag = (filt_hist(src) != 0) ? 1u : 0u;

	memcpy(dst, src, FILT_SIZE);
	memcpy((unsigned char *)dst + FILT_HIST, &flag, sizeof(flag));
}

static void
cmp_filter(const char *what, const char *type, const void *a, const void *b,
	   long input)
{
	unsigned char sa[FILT_SIZE], sb[FILT_SIZE];
	struct hist_buf ha, hb;
	unsigned int n;

	filt_snapshot(sa, a);
	filt_snapshot(sb, b);
	diff_eq_obj_(__FILE__, __LINE__, what, type, sa, sb, FILT_SIZE, input);

	n = filt_word(a, FILT_LEN);
	if (n > MAXHIST)
		n = MAXHIST;
	memset(&ha, 0, sizeof(ha));
	memset(&hb, 0, sizeof(hb));
	if (filt_hist(a) != 0)
		memcpy(&ha, filt_hist(a), n * sizeof(float));
	if (filt_hist(b) != 0)
		memcpy(&hb, filt_hist(b), n * sizeof(float));
	diff_eq_obj_(__FILE__, __LINE__, what, "struct hist_buf", &ha, &hb,
		     sizeof(ha), input);
}

/*
 * A comparable copy of a precoder: the two owned pointers flattened to their
 * nullness, everything else as it stands.
 */
static void
pre_snapshot(void *dst, const V92Precoder *src)
{
	V92Precoder *d = (V92Precoder *)dst;

	memcpy(dst, src, PRE_SIZE);
	d->fir1 = (FloatFIR *)(src->fir1 != 0 ? 1 : 0);
	d->fir2 = (FloatFIR *)(src->fir2 != 0 ? 1 : 0);
}

static void
pf_snapshot(void *dst, const V92PreFilter *src)
{
	V92PreFilter *d = (V92PreFilter *)dst;

	memcpy(dst, src, PF_SIZE);
	d->fir = (FloatFIR *)(src->fir != 0 ? 1 : 0);
	d->iir = (FloatIIR *)(src->iir != 0 ? 1 : 0);
}

/*
 * The tap counts.  0 makes both filters zero-tap with a 99-entry history, 1
 * through 3 round DOWN to zero and are the only rows where the rounding is
 * visible at all, 4 and 8 are already multiples, and 0x140 is the count the
 * object itself uses -- `V92Transmitter::V92Transmitter` passes $0x140 to
 * both constructors and nothing else ever calls them.
 */
static const unsigned int taps[] = {
	0, 1, 2, 3, 4, 5, 7, 8, 12, 16, 63, 64, 100, 0x140
};
#define NTAPS ((int)(sizeof(taps) / sizeof(taps[0])))

static int
run_precoder_ctor(void)
{
	int i;
	unsigned int seen_len = 0;
	int distinct = 0;

	diff_begin("V92Precoder::V92Precoder");

	for (i = 0; i < NTAPS; i++) {
		unsigned char sa[PRE_SIZE], sb[PRE_SIZE];
		unsigned int n = taps[i];
		unsigned int rounded = n & ~3u;
		unsigned int len = rounded + V92PRECODER_BLOCK;

		seed(pre_ours, pre_theirs, PRE_SLOT, i);
		harness_alloc_reset();

		/* C1 on half the rows and C2 on the other half: GCC emits both
		 * from one definition and this test fails to link if it does
		 * not. */
		if (i & 1) {
			our_pre_ctor2(pre_ours, n);
			ref_pre_ctor2(pre_theirs, n);
		} else {
			our_pre_ctor(pre_ours, n);
			ref_pre_ctor(pre_theirs, n);
		}

		pre_snapshot(sa, PO());
		pre_snapshot(sb, PT());
		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V92Precoder", sa, sb, PRE_SIZE, (long)n);
		diff_eq_int("no store past the object (taps %ld)",
			    memcmp(pre_ours + PRE_SIZE, pre_theirs + PRE_SIZE,
				   PRE_SLOT - PRE_SIZE) == 0, 1, n);

		/*
		 * The constructor writes the two pointers and NOTHING else:
		 * every other byte still holds this trial's seed.
		 */
		diff_eq_int("the bytes below +0x68 keep their seed"
			    " (taps %ld)",
			    memcmp(pre_ours, seed_copy, 0x68) == 0, 1, n);
		diff_eq_int("the bytes from +0x70 up keep their seed"
			    " (taps %ld)",
			    memcmp(pre_ours + 0x70, seed_copy + 0x70,
				   PRE_SLOT - 0x70) == 0, 1, n);
		diff_eq_int("and it did write the two it owns (taps %ld)",
			    memcmp(pre_ours + 0x68, seed_copy + 0x68, 8) != 0,
			    1, n);

		diff_eq_int("both filters exist (taps %ld)",
			    PO()->fir1 != 0 && PO()->fir2 != 0, 1, n);
		diff_eq_int("and they are two objects, not one (taps %ld)",
			    PO()->fir1 != PO()->fir2, 1, n);
		/*
		 * The only thing that separates the two identical filters is
		 * which allocation each holds, and the reference's own answer
		 * is what it is compared against.
		 *
		 * BY ORDINAL, NOT BY ADDRESS.  This asked `fir1 < fir2`, and
		 * an address comparison does not answer it: both allocators
		 * recycle LIFO, so the destructor's free(fir1); free(fir2)
		 * makes the NEXT construction hand fir2's chunk out first and
		 * the comparison inverts on alternate trials.  It agreed with
		 * the reference under one libc and not under another, for
		 * reasons that had nothing to do with either.  Finding F1353.
		 */
		diff_eq_int("stored in the reference's allocation order"
			    " (taps %ld)",
			    harness_alloc_ordinal(PO()->fir1) <
			    harness_alloc_ordinal(PO()->fir2),
			    harness_alloc_ordinal(PT()->fir1) <
			    harness_alloc_ordinal(PT()->fir2), n);

		cmp_filter("fir1 after construction", "FloatFIR", PO()->fir1,
			   PT()->fir1, (long)n);
		cmp_filter("fir2 after construction", "FloatFIR", PO()->fir2,
			   PT()->fir2, (long)n);

		/* Both filters got the constructor's one tap count and 99
		 * samples of slack, and FloatFIR rounded the count down. */
		diff_eq_int("fir1 taps are the argument rounded down"
			    " (taps %ld)", filt_word(PO()->fir1, FILT_TAPS),
			    rounded, n);
		diff_eq_int("fir2 taps are the argument rounded down"
			    " (taps %ld)", filt_word(PO()->fir2, FILT_TAPS),
			    rounded, n);
		diff_eq_int("fir1 history is taps + 99 (taps %ld)",
			    filt_word(PO()->fir1, FILT_LEN), len, n);
		diff_eq_int("fir2 history is taps + 99 (taps %ld)",
			    filt_word(PO()->fir2, FILT_LEN), len, n);
		diff_eq_int("fir1 starts at the block's end (taps %ld)",
			    filt_word(PO()->fir1, FILT_POS), len - rounded, n);

		/* Four allocations a side: two filters and their histories. */
		diff_eq_int("four allocations a side (taps %ld)",
			    harness_alloc.allocs, 8, n);
		diff_eq_int("and they are the sizes the map says (taps %ld)",
			    harness_alloc.bytes,
			    2 * (2 * FILT_SIZE + 2 * len * sizeof(float)), n);

		if (i == 0)
			seen_len = len;
		else if (len != seen_len)
			distinct = 1;

		if (i & 1) {
			our_pre_dtor2(pre_ours);
			ref_pre_dtor2(pre_theirs);
		} else {
			our_pre_dtor(pre_ours);
			ref_pre_dtor(pre_theirs);
		}
		diff_eq_int("nothing left live (taps %ld)",
			    harness_alloc.live, 0, n);
	}

	/* The sweep is not producing one answer over and over. */
	diff_eq_int("the sweep reached more than one shape (%ld)", distinct,
		    1, 0);

	return diff_end();
}

static int
run_precoder_dtor(void)
{
	int which;

	diff_begin("V92Precoder::~V92Precoder");

	/*
	 * Neither null, each null in turn, both null.  A pointer is nulled by
	 * tearing its filter down by hand first, so the heap stays balanced
	 * and a destructor that frees the wrong member reads as a bad free
	 * rather than as the same total.
	 */
	for (which = 0; which < 4; which++) {
		FloatFIR *was1, *was2;

		seed(pre_ours, pre_theirs, PRE_SLOT, 100 + which);
		harness_alloc_reset();
		our_pre_ctor(pre_ours, 32);
		ref_pre_ctor(pre_theirs, 32);

		if (which & 1) {
			PO()->fir1->~FloatFIR();
			sysdep_free(PO()->fir1);
			PO()->fir1 = 0;
			ref_fir_dtor(PT()->fir1);
			sysdep_free(PT()->fir1);
			PT()->fir1 = 0;
		}
		if (which & 2) {
			PO()->fir2->~FloatFIR();
			sysdep_free(PO()->fir2);
			PO()->fir2 = 0;
			ref_fir_dtor(PT()->fir2);
			sysdep_free(PT()->fir2);
			PT()->fir2 = 0;
		}

		was1 = PO()->fir1;
		was2 = PO()->fir2;

		our_pre_dtor(pre_ours);
		ref_pre_dtor(pre_theirs);

		/* Four blocks a side either way -- by hand or by the
		 * destructor. */
		diff_eq_int("eight frees, however they were split (case %ld)",
			    harness_alloc.frees, 8, which);
		diff_eq_int("no bad free (case %ld)", harness_alloc.bad_free,
			    0, which);
		/*
		 * The null tests are what this counter sees.  sysdep_free
		 * tolerates NULL, so a destructor that dropped its `if`s would
		 * change no byte of either object and only show up here.
		 */
		diff_eq_int("no null free (case %ld)", harness_alloc.free_null,
			    0, which);
		diff_eq_int("nothing left live (case %ld)",
			    harness_alloc.live, 0, which);
		/*
		 * D181: the destructor does not null what it frees.  Both
		 * words hold exactly what they held going in -- a freed
		 * address where the filter was, or the zero this fixture put
		 * there -- which is checkable, and is what a destructor that
		 * tidied up after itself would break.
		 */
		diff_eq_int("fir1 still holds what it held (case %ld)",
			    PO()->fir1 == was1, 1, which);
		diff_eq_int("fir2 still holds what it held (case %ld)",
			    PO()->fir2 == was2, 1, which);

		/* And the destructor scribbled on nothing: the two objects
		 * still agree everywhere but the dangling addresses. */
		{
			unsigned char sa[PRE_SIZE], sb[PRE_SIZE];

			pre_snapshot(sa, PO());
			pre_snapshot(sb, PT());
			diff_eq_obj_(__FILE__, __LINE__, "after destruction",
				     "V92Precoder", sa, sb, PRE_SIZE,
				     (long)which);
			diff_eq_int("no store past the object (case %ld)",
				    memcmp(pre_ours + PRE_SIZE,
					   pre_theirs + PRE_SIZE,
					   PRE_SLOT - PRE_SIZE) == 0, 1,
				    which);
		}
	}

	return diff_end();
}

static int
run_prefilter_ctor(void)
{
	int i;

	diff_begin("V92PreFilter::V92PreFilter");

	for (i = 0; i < NTAPS; i++) {
		unsigned char sa[PF_SIZE], sb[PF_SIZE];
		unsigned int n = taps[i];
		unsigned int rounded = n & ~3u;
		unsigned int len = rounded + V92PREFILTER_BLOCK;

		seed(pf_ours, pf_theirs, PF_SLOT, 200 + i);
		harness_alloc_reset();

		if (i & 1) {
			our_pf_ctor2(pf_ours, n);
			ref_pf_ctor2(pf_theirs, n);
		} else {
			our_pf_ctor(pf_ours, n);
			ref_pf_ctor(pf_theirs, n);
		}

		pf_snapshot(sa, FO());
		pf_snapshot(sb, FT());
		diff_eq_obj_(__FILE__, __LINE__, "after construction",
			     "V92PreFilter", sa, sb, PF_SIZE, (long)n);
		diff_eq_int("no store past the object (taps %ld)",
			    memcmp(pf_ours + PF_SIZE, pf_theirs + PF_SIZE,
				   PF_SLOT - PF_SIZE) == 0, 1, n);

		/* +0x00 and the two tap counts at +0x0c, +0x10 keep the
		 * trial's seed: the constructor writes +0x04 and +0x08 only. */
		diff_eq_int("+0x00 keeps its seed (taps %ld)",
			    memcmp(pf_ours, seed_copy, 4) == 0, 1, n);
		diff_eq_int("the two counts keep their seed (taps %ld)",
			    memcmp(pf_ours + 0x0c, seed_copy + 0x0c,
				   PF_SLOT - 0x0c) == 0, 1, n);
		diff_eq_int("and it did write the two it owns (taps %ld)",
			    memcmp(pf_ours + 4, seed_copy + 4, 8) != 0, 1, n);

		diff_eq_int("both filters exist (taps %ld)",
			    FO()->fir != 0 && FO()->iir != 0, 1, n);
		/* By ordinal, not by address -- see the V92Precoder case. */
		diff_eq_int("stored in the reference's allocation order"
			    " (taps %ld)",
			    harness_alloc_ordinal(FO()->fir) <
			    harness_alloc_ordinal(FO()->iir),
			    harness_alloc_ordinal(FT()->fir) <
			    harness_alloc_ordinal(FT()->iir), n);

		cmp_filter("fir after construction", "FloatFIR", FO()->fir,
			   FT()->fir, (long)n);
		cmp_filter("iir after construction", "FloatIIR", FO()->iir,
			   FT()->iir, (long)n);

		diff_eq_int("fir taps are the argument rounded down"
			    " (taps %ld)", filt_word(FO()->fir, FILT_TAPS),
			    rounded, n);
		diff_eq_int("iir taps are the argument rounded down"
			    " (taps %ld)", filt_word(FO()->iir, FILT_TAPS),
			    rounded, n);
		diff_eq_int("fir history is taps + 99 (taps %ld)",
			    filt_word(FO()->fir, FILT_LEN), len, n);
		diff_eq_int("iir history is taps + 99 (taps %ld)",
			    filt_word(FO()->iir, FILT_LEN), len, n);

		diff_eq_int("four allocations a side (taps %ld)",
			    harness_alloc.allocs, 8, n);
		diff_eq_int("and they are the sizes the map says (taps %ld)",
			    harness_alloc.bytes,
			    2 * (2 * FILT_SIZE + 2 * len * sizeof(float)), n);

		if (i & 1) {
			our_pf_dtor2(pf_ours);
			ref_pf_dtor2(pf_theirs);
		} else {
			our_pf_dtor(pf_ours);
			ref_pf_dtor(pf_theirs);
		}
		diff_eq_int("nothing left live (taps %ld)",
			    harness_alloc.live, 0, n);
	}

	return diff_end();
}

static int
run_prefilter_dtor(void)
{
	int which;

	diff_begin("V92PreFilter::~V92PreFilter");

	for (which = 0; which < 4; which++) {
		seed(pf_ours, pf_theirs, PF_SLOT, 300 + which);
		harness_alloc_reset();
		our_pf_ctor(pf_ours, 32);
		ref_pf_ctor(pf_theirs, 32);

		if (which & 1) {
			FO()->fir->~FloatFIR();
			sysdep_free(FO()->fir);
			FO()->fir = 0;
			ref_fir_dtor(FT()->fir);
			sysdep_free(FT()->fir);
			FT()->fir = 0;
		}
		if (which & 2) {
			FO()->iir->~FloatIIR();
			sysdep_free(FO()->iir);
			FO()->iir = 0;
			ref_iir_dtor(FT()->iir);
			sysdep_free(FT()->iir);
			FT()->iir = 0;
		}

		our_pf_dtor(pf_ours);
		ref_pf_dtor(pf_theirs);

		diff_eq_int("eight frees, however they were split (case %ld)",
			    harness_alloc.frees, 8, which);
		diff_eq_int("no bad free (case %ld)", harness_alloc.bad_free,
			    0, which);
		diff_eq_int("no null free (case %ld)", harness_alloc.free_null,
			    0, which);
		diff_eq_int("nothing left live (case %ld)",
			    harness_alloc.live, 0, which);

		{
			unsigned char sa[PF_SIZE], sb[PF_SIZE];

			pf_snapshot(sa, FO());
			pf_snapshot(sb, FT());
			diff_eq_obj_(__FILE__, __LINE__, "after destruction",
				     "V92PreFilter", sa, sb, PF_SIZE,
				     (long)which);
		}
	}

	return diff_end();
}

/*
 * V92PreFilter::reset, ::setCoefficients and ::process.
 *
 * WHAT MAKES THIS MORE THAN A FORWARDING CHECK.  All three members are thin
 * over FloatFIR and FloatIIR, and both filters carry state, so the only way
 * to see an argument passed to the wrong filter -- or a block routed the
 * wrong way -- is to give the two filters DIFFERENT coefficients and then run
 * enough blocks for the histories to fill and wrap.  A single call would
 * compare equal for either routing whenever the histories are still zero.
 *
 * The output buffer is filled with junk before every call, both sides alike,
 * so a `process` that wrote nothing would be caught rather than agreeing.
 * The guard past the end catches the twelve becoming thirteen.
 */
#define PF_COEF		20
#define PF_GUARD	4
#define PF_BLOCKS	40

/*
 * ONE COEFFICIENT ARRAY PER FILTER, SHARED BY THE TWO SIDES.  A filter stores
 * the pointer it was handed at +0x00 and does not own it, so giving each side
 * its own copy would put two different addresses in a field this file
 * compares -- a difference the fixture created.  Both sides therefore point
 * at the same read-only arrays, and that nothing wrote through them is
 * checked against a snapshot instead.
 */
static float pf_cf[PF_COEF], pf_ci[PF_COEF];
static float pf_cf_copy[PF_COEF], pf_ci_copy[PF_COEF];
static float pf_in_ours[V92PREFILTER_SAMPLES];
static float pf_in_theirs[V92PREFILTER_SAMPLES];
static float pf_out_ours[V92PREFILTER_SAMPLES + PF_GUARD];
static float pf_out_theirs[V92PREFILTER_SAMPLES + PF_GUARD];

/* A deterministic spread of magnitudes and both signs; nothing denormal and
 * nothing that overflows a float when convolved with twenty taps. */
static float
pf_next(unsigned int *state)
{
	unsigned int v = *state;

	v = v * 1103515245u + 12345u;
	*state = v;
	return (float)((int)((v >> 9) & 0xffffu) - 32768) / 512.0f;
}

static void
pf_fill_pair(float *a, float *b, int n, unsigned int seedval)
{
	unsigned int st = seedval;
	int i;

	for (i = 0; i < n; i++) {
		a[i] = pf_next(&st);
		b[i] = a[i];
	}
}

struct pf_case {
	unsigned int tapsFir;
	unsigned int tapsIir;
};

/* One block of output, named so diff_eq_obj can say which sample differed. */
struct pf_out {
	float s[V92PREFILTER_SAMPLES];
};

/*
 * The four arms of `process`, and counts that are not multiples of four so
 * the filters' rounding is in play.
 *
 * NO COUNT IS 1, 2 OR 3, AND THAT IS A DEVIATION AND NOT AN OVERSIGHT.  Such
 * a count leaves +0x0c or +0x10 non-zero and the filter behind it with no
 * taps at all, and `process` gates on the count it was given rather than on
 * the filter's -- so it calls a filter that then walks its history backwards
 * without a bound.  The blob's own loop is the same one
 * (`FloatFIR::process`, 0x46c20: `dec %edx; jne` entered with the tap count
 * already decremented from zero), so this is the object's behaviour and not
 * ours; docs/deviations.md D260.
 *
 * NO COUNT EXCEEDS PF_COEF either.  A filter given more taps than the
 * coefficient array holds reads past it, and that would be a difference this
 * file created rather than one the reconstruction has.
 */
static const struct pf_case pf_cases[] = {
	{  0,  0 },
	{  4,  0 },
	{  0,  4 },
	{  4,  4 },
	{  8, 16 },
	{ 16,  8 },
	{ 20, 20 },
	{  5,  7 },
	{  7,  5 },
	{ 12,  4 },
	{ 20,  0 },
	{  0, 20 }
};

static int
run_prefilter_methods(void)
{
	int ci;
	int ncase = (int)(sizeof(pf_cases) / sizeof(pf_cases[0]));
	int moved = 0, differed = 0;

	diff_begin("V92PreFilter::reset / setCoefficients / process");

	for (ci = 0; ci < ncase; ci++) {
		unsigned int tf = pf_cases[ci].tapsFir;
		unsigned int ti = pf_cases[ci].tapsIir;
		unsigned char sa[PF_SIZE], sb[PF_SIZE];
		int blk;

		seed(pf_ours, pf_theirs, PF_SLOT, 400 + ci);
		harness_alloc_reset();
		our_pf_ctor(pf_ours, 0x140);
		ref_pf_ctor(pf_theirs, 0x140);

		/* Two different coefficient sets, so a call that gave the IIR
		 * the FIR's array produces different numbers. */
		pf_fill_pair(pf_cf, pf_cf_copy, PF_COEF, 0x51a3u + ci);
		pf_fill_pair(pf_ci, pf_ci_copy, PF_COEF, 0x9e37u + ci);

		our_pf_setcoef(pf_ours, pf_cf, pf_ci, tf, ti);
		ref_pf_setcoef(pf_theirs, pf_cf, pf_ci, tf, ti);

		pf_snapshot(sa, FO());
		pf_snapshot(sb, FT());
		diff_eq_obj_(__FILE__, __LINE__, "after setCoefficients",
			     "V92PreFilter", sa, sb, PF_SIZE, (long)ci);
		diff_eq_int("setCoefficients stored the caller's fir count"
			    " (case %ld)", FO()->tapsFir, tf, ci);
		diff_eq_int("setCoefficients stored the caller's iir count"
			    " (case %ld)", FO()->tapsIir, ti, ci);
		diff_eq_int("no store past the object (case %ld)",
			    memcmp(pf_ours + PF_SIZE, pf_theirs + PF_SIZE,
				   PF_SLOT - PF_SIZE) == 0, 1, ci);
		diff_eq_int("neither coefficient array was written (case %ld)",
			    memcmp(pf_cf, pf_cf_copy, sizeof(pf_cf)) == 0
			    && memcmp(pf_ci, pf_ci_copy, sizeof(pf_ci)) == 0, 1,
			    ci);
		cmp_filter("fir after setCoefficients", "FloatFIR", FO()->fir,
			   FT()->fir, (long)ci);
		cmp_filter("iir after setCoefficients", "FloatIIR", FO()->iir,
			   FT()->iir, (long)ci);

		/*
		 * Forty blocks of twelve: the histories are 419 entries, so
		 * this fills them and wraps, which is where a filter driven
		 * with the wrong input diverges from one driven with the
		 * right one.
		 */
		for (blk = 0; blk < PF_BLOCKS; blk++) {
			unsigned int st = 0x2f81u + 977u * (unsigned)blk
					  + 13u * (unsigned)ci;
			int i;

			for (i = 0; i < V92PREFILTER_SAMPLES; i++) {
				float v;

				switch (blk & 3) {
				case 0:
					v = pf_next(&st);
					break;
				case 1:	/* an impulse, then silence */
					v = (i == 0) ? 64.0f : 0.0f;
					break;
				case 2:	/* alternating full scale */
					v = (i & 1) ? -32.0f : 32.0f;
					break;
				default:	/* a step */
					v = (i < 6) ? 0.0f : 16.0f;
					break;
				}
				pf_in_ours[i] = v;
				pf_in_theirs[i] = v;
			}

			/* Junk in the output, both sides alike, so writing
			 * nothing is not the same as writing zeroes. */
			pf_fill_pair(pf_out_ours, pf_out_theirs,
				     V92PREFILTER_SAMPLES + PF_GUARD,
				     0x7c11u + 31u * (unsigned)blk);

			our_pf_process(pf_ours, pf_in_ours, pf_out_ours);
			ref_pf_process(pf_theirs, pf_in_theirs, pf_out_theirs);

			diff_eq_obj_(__FILE__, __LINE__, "process output",
				     "struct pf_out", pf_out_ours,
				     pf_out_theirs, sizeof(struct pf_out),
				     (long)(ci * 100 + blk));
			diff_eq_int("process wrote no thirteenth sample"
				    " (case %ld)",
				    memcmp(pf_out_ours + V92PREFILTER_SAMPLES,
					   pf_out_theirs + V92PREFILTER_SAMPLES,
					   PF_GUARD * sizeof(float)) == 0, 1,
				    ci * 100 + blk);
			diff_eq_int("process did not write its input"
				    " (case %ld)",
				    memcmp(pf_in_ours, pf_in_theirs,
					   sizeof(pf_in_ours)) == 0, 1,
				    ci * 100 + blk);

			pf_snapshot(sa, FO());
			pf_snapshot(sb, FT());
			diff_eq_obj_(__FILE__, __LINE__, "after process",
				     "V92PreFilter", sa, sb, PF_SIZE,
				     (long)(ci * 100 + blk));
			cmp_filter("fir after process", "FloatFIR", FO()->fir,
				   FT()->fir, (long)(ci * 100 + blk));
			cmp_filter("iir after process", "FloatIIR", FO()->iir,
				   FT()->iir, (long)(ci * 100 + blk));

			/* Two samples of one block differing says the buffer
			 * holds a signal and not the fill. */
			if (pf_out_ours[0] != pf_out_ours[1])
				differed = 1;
		}

		/*
		 * `reset` reaches both filters, and after forty blocks both
		 * histories are full of numbers, so the zeroing is visible.
		 */
		memcpy(sa, pf_ours, PF_SIZE);
		our_pf_reset(pf_ours);
		ref_pf_reset(pf_theirs);

		diff_eq_int("reset left the object itself alone (case %ld)",
			    memcmp(pf_ours, sa, PF_SIZE) == 0, 1, ci);
		pf_snapshot(sa, FO());
		pf_snapshot(sb, FT());
		diff_eq_obj_(__FILE__, __LINE__, "after reset", "V92PreFilter",
			     sa, sb, PF_SIZE, (long)ci);
		cmp_filter("fir after reset", "FloatFIR", FO()->fir, FT()->fir,
			   (long)ci);
		cmp_filter("iir after reset", "FloatIIR", FO()->iir, FT()->iir,
			   (long)ci);

		{
			unsigned int i, n = filt_word(FO()->fir, FILT_LEN);
			const float *h = (const float *)filt_hist(FO()->fir);
			int nonzero = 0;

			for (i = 0; i < n; i++)
				if (h[i] != 0.0f)
					nonzero = 1;
			diff_eq_int("reset zeroed the fir history (case %ld)",
				    nonzero, 0, ci);
			if (filt_word(FO()->fir, FILT_TAPS) != 0)
				moved = 1;
		}

		our_pf_dtor(pf_ours);
		ref_pf_dtor(pf_theirs);
		diff_eq_int("nothing left live (case %ld)", harness_alloc.live,
			    0, ci);
	}

	/* The sweep really did run filters with taps, and really did produce
	 * varying output -- findings F223 and F224. */
	diff_eq_int("some case ran a filter with taps (%ld)", moved, 1, 0);
	diff_eq_int("process produced varying samples (%ld)", differed, 1, 0);

	return diff_end();
}

/*
 * V92Precoder::reset(V92MappingParams *), ::setCoefficients and ::process.
 *
 * THE PARAMETER BLOCK IS A `struct V92ParamsInfo`, which is finding F1321's
 * identification and not this file's guess -- one 180-byte block, the sixth
 * argument of V92Modulator's constructor by the mangling and the argument of
 * the four C functions that fill it.  `reset` copies twenty-five of its words
 * and takes a pointer to its last twenty-four bytes; the six pointers it
 * copies out of +0x84 are the constellations, and `process` dereferences the
 * one it is told to.
 *
 * ONE BLOCK AND ONE SET OF CONSTELLATIONS, SHARED BY BOTH SIDES.  Everything
 * `reset` copies is an input, and a pointer copied out of two different
 * blocks would differ in the object for a reason this file created.  That
 * nothing wrote through them is checked against a snapshot instead.
 *
 * WHAT THE SWEEP HAS TO REACH, beyond agreeing:
 *
 *   - the UNSIGNED conversion of a constellation entry.  A signed reading
 *     differs only for entries above 2^31, and such an entry can only ever
 *     win the search if the carried state is about -2^31 -- so one case pokes
 *     `state0` there and fills a constellation with values just above the
 *     boundary.  Without it the two readings choose the same point every
 *     time and the `fildll` is untested.
 *   - the fourth symbol's parity, which is why `b` is swept and why `out` is
 *     seeded rather than zeroed: the parity reads out[0..2].
 *   - the EMPTY interval, which is D261.  A negative modulus produces one,
 *     and it is driven on the second symbol and never the first, because on
 *     the first the two locals the object reads back are two different pieces
 *     of uninitialised stack and comparing them would be comparing the
 *     fixtures.
 */
#define PRE_CONST	6
#define PRE_POINTS	1024
#define PRE_SYMBOLS	4
#define PRE_IN		12

static unsigned int pre_const[PRE_CONST][PRE_POINTS];
static unsigned int pre_const_huge[PRE_POINTS];
static unsigned int pre_const_falling[PRE_POINTS];
static float pre_cf[PF_COEF], pre_ci[PF_COEF];
static unsigned char pre_params[sizeof(struct V92ParamsInfo)]
	__attribute__((aligned(8)));
static unsigned char pre_params_copy[sizeof(struct V92ParamsInfo)];

/* Which set of constellations a block points at. */
enum {
	PRE_KIND_PLAIN = 0,	/* six unrelated small ones               */
	PRE_KIND_HUGE,		/* one whose points are just above 2^31   */
	PRE_KIND_FALLING	/* one whose points fall as the index rises */
};

struct pre_spec {
	int kind;
	int selectors[6];	/* +0x9c, what `process` indexes mod six  */
	int moduli[6];		/* +0x6c, tableB                          */
	int steps[12];		/* +0x1c, tableA -- never zero, it divides */
};

/*
 * Six constellations of small points, and a seventh of points just above 2^31
 * for the unsigned reading.
 *
 * THE POINTS ARE SMALL ON PURPOSE.  The search keeps whichever candidate
 * minimises the square of `state0 + point + state1` and starts from 1e12, so
 * a constellation big enough to push that square past 1e12 makes EVERY
 * candidate lose -- and then the two locals the object reads back afterwards
 * are uninitialised, which is D261 and not a comparison between two
 * reconstructions.  The precoder's own filters are given coefficients small
 * enough that the two carried samples contract rather than grow, for the same
 * reason: this sweep is meant to exercise the search, and D261 is driven
 * deliberately, once, where the values are still comparable.
 */
static void
build_constellations(void)
{
	unsigned int lfsr = 0x5eedu;
	int c, t;

	for (c = 0; c < PRE_CONST; c++) {
		for (t = 0; t < PRE_POINTS; t++) {
			lfsr = lfsr * 1103515245u + 12345u;
			pre_const[c][t] = ((lfsr >> 11) % 1000u) + 1u;
		}
	}
	for (t = 0; t < PRE_POINTS; t++)
		pre_const_huge[t] = 0x80000000u
				    + (unsigned int)((t * 7) % 1000);

	/*
	 * A CONSTELLATION WHOSE POINTS FALL AS THE INDEX RISES, which is what
	 * makes the two ENDS of the search interval observable. With unrelated
	 * points the winner is somewhere in the middle and an interval one
	 * candidate too long usually chooses the same one anyway: two claims
	 * about the bounds -- the `(m - 1) / 2` and the parity in the fourth
	 * symbol's numerator -- survived their mutations until this existed.
	 * Here the outermost candidate has the smallest magnitude and
	 * therefore wins, so an interval off by one at either end picks a
	 * different point.
	 */
	for (t = 0; t < PRE_POINTS; t++)
		pre_const_falling[t] = (unsigned int)(4096 - 4 * t);
}

static void
build_params(const struct pre_spec *spec, int trial)
{
	struct V92ParamsInfo *p = (struct V92ParamsInfo *)(void *)pre_params;
	unsigned int lfsr = 0x3bd1u + 0x77u * (unsigned int)trial;
	int *w;
	int i;

	/* Seeded, never zeroed: the words `reset` does not copy must be able
	 * to show up if it copies them by mistake. */
	for (i = 0; i < (int)sizeof(pre_params); i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		pre_params[i] = (unsigned char)((lfsr >> 3) | 1u);
	}

	/*
	 * These three runs were `pad_00 + 7`, `pad_6c` and `pad_9c` until the
	 * unpacker named them.  The offsets they land on have not moved --
	 * `w` is kept so the arithmetic below is unchanged -- but the names
	 * now say which quantity each one is.
	 */
	w = (int *)(void *)p->m;
	for (i = 0; i < 12; i++)
		w[i] = spec->steps[i];		/* +0x1c .. +0x48 */
	w = (int *)(void *)p->LC;
	for (i = 0; i < 6; i++)
		w[i] = spec->moduli[i];		/* +0x6c .. +0x80 */
	w = (int *)(void *)p->indexConstel;
	for (i = 0; i < 6; i++)
		w[i] = spec->selectors[i];	/* +0x9c .. +0xb0 */
	for (i = 0; i < PRE_CONST; i++) {
		switch (spec->kind) {
		case PRE_KIND_HUGE:
			p->constellations[i] =
			    (int *)(void *)&pre_const_huge[0];
			break;
		case PRE_KIND_FALLING:
			p->constellations[i] =
			    (int *)(void *)&pre_const_falling[0];
			break;
		default:
			p->constellations[i] =
			    (int *)(void *)&pre_const[i][0];
			break;
		}
	}

	memcpy(pre_params_copy, pre_params, sizeof(pre_params));
}

/* The blocks the sweep uses.  Every step is non-zero because `process`
 * divides by it, and every modulus but the negative ones keeps the chosen
 * index inside PRE_POINTS. */
static const struct pre_spec pre_specs[] = {
	{ PRE_KIND_PLAIN,
	  { 0, 1, 2, 3, 4, 5 },
	  { 64, 128, 96, 256, 512, 32 },
	  { 1, 2, 3, 4, 5, 6, 7, 8, 2, 4, 8, 16 } },
	{ PRE_KIND_PLAIN,
	  { 5, 4, 3, 2, 1, 0 },
	  { 32, 512, 256, 96, 128, 64 },
	  { 8, 4, 2, 1, 16, 8, 4, 2, 3, 5, 7, 9 } },
	{ PRE_KIND_PLAIN,
	  { 2, 2, 2, 2, 2, 2 },
	  { 16, 16, 300, 16, 16, 16 },
	  { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 } },
	/*
	 * D261: symbol 0 has a modulus and symbol 1 has a NEGATIVE one, which
	 * makes its interval empty -- `lo` comes out above `hi` and the loop
	 * never runs.  Symbol 0 having run first is what makes the two locals
	 * the object then reads back comparable: they hold symbol 0's answer
	 * on both sides rather than two different stacks.
	 */
	{ PRE_KIND_PLAIN,
	  { 0, 1, 2, 3, 4, 5 },
	  { 64, -8, 96, 256, 512, 32 },
	  { 1, 2, 3, 4, 5, 6, 7, 8, 2, 4, 8, 16 } },
	/* Every symbol on the constellation above 2^31. */
	{ PRE_KIND_HUGE,
	  { 0, 1, 2, 3, 4, 5 },
	  { 64, 64, 64, 64, 64, 64 },
	  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
	/*
	 * Eight candidates on a falling constellation, so the outermost one
	 * wins and both ends of the interval decide the answer.
	 */
	{ PRE_KIND_FALLING,
	  { 0, 1, 2, 3, 4, 5 },
	  { 4, 4, 4, 4, 4, 4 },
	  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
	{ PRE_KIND_FALLING,
	  { 0, 1, 2, 3, 4, 5 },
	  { 6, 5, 4, 7, 6, 5 },
	  { 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 } }
};
#define PRE_SPEC_EMPTY	3	/* the one whose symbol 1 finds nothing */
#define PRE_SPEC_HUGE	4	/* the one above 2^31                   */
#define NPRESPEC ((int)(sizeof(pre_specs) / sizeof(pre_specs[0])))

static int
run_precoder_reset(void)
{
	int trial;
	int varied = 0;
	unsigned int lvl;

	diff_begin("V92Precoder::reset(V92MappingParams *)");

	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < NPRESPEC * 2; trial++) {
		const struct pre_spec *spec = &pre_specs[trial % NPRESPEC];
		unsigned char sa[PRE_SIZE], sb[PRE_SIZE];
		int i;

		seed(pre_ours, pre_theirs, PRE_SLOT, 500 + trial);
		harness_alloc_reset();
		our_pre_ctor(pre_ours, 0x140);
		ref_pre_ctor(pre_theirs, 0x140);
		build_params(spec, trial);

		/* Sweep the level so a site at the wrong threshold shows up
		 * (the argument in dsplib/debug.h). */
		lvl = (unsigned int)(trial % 4);
		dsplibs_debug_level = lvl;
		ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_reset();

		our_pre_reset(pre_ours, pre_params);
		ref_pre_reset(pre_theirs, pre_params);

		diff_eq_int("the same diagnostics (level %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), lvl);
		diff_eq_int("and the same text (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, lvl);
		diff_eq_int("one line above level 1, none below (level %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    lvl > 1 ? 1 : 0, lvl);

		pre_snapshot(sa, PO());
		pre_snapshot(sb, PT());
		diff_eq_obj_(__FILE__, __LINE__, "after reset", "V92Precoder",
			     sa, sb, PRE_SIZE, (long)trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(pre_ours + PRE_SIZE, pre_theirs + PRE_SIZE,
				   PRE_SLOT - PRE_SIZE) == 0, 1, trial);
		diff_eq_int("the parameter block was not written (trial %ld)",
			    memcmp(pre_params, pre_params_copy,
				   sizeof(pre_params)) == 0, 1, trial);

		/* The map, field by field, against the block itself. */
		diff_eq_int("paramsAt9c points at the block's +0x9c"
			    " (trial %ld)",
			    (int)((unsigned char *)PO()->paramsAt9c
				  - pre_params), 0x9c, trial);
		for (i = 0; i < 6; i++) {
			const int *six = (const int *)(void *)
			    (pre_params + 0x6c);

			const struct V92ParamsInfo *p =
			    (const struct V92ParamsInfo *)(void *)pre_params;

			diff_eq_int("head[%ld] is the block's constellation",
				    (int)((void *)PO()->head[i]
					  == p->constellations[i]), 1, i);
			diff_eq_int("tableB[%ld] came from +0x6c",
				    PO()->tableB[i], six[i], i);
		}
		for (i = 0; i < 12; i++) {
			const int *w = (const int *)(void *)
			    (pre_params + 0x1c);

			diff_eq_int("tableA[%ld] came from +0x1c",
				    PO()->tableA[i], w[i], i);
		}
		diff_eq_int("state0 was zeroed (trial %ld)",
			    PO()->state0 == 0.0f, 1, trial);
		diff_eq_int("state1 was zeroed (trial %ld)",
			    PO()->state1 == 0.0f, 1, trial);

		/* +0x00, the two filters and the two tap counts are NOT
		 * reset's: the first and the last two still hold the seed. */
		diff_eq_int("+0x00 keeps its seed (trial %ld)",
			    memcmp(pre_ours, seed_copy, 4) == 0, 1, trial);
		diff_eq_int("the two tap counts keep theirs (trial %ld)",
			    memcmp(pre_ours + 0x78, seed_copy + 0x78,
				   PRE_SLOT - 0x78) == 0, 1, trial);
		diff_eq_int("both filters survived (trial %ld)",
			    PO()->fir1 != 0 && PO()->fir2 != 0, 1, trial);
		cmp_filter("fir1 across reset", "FloatFIR", PO()->fir1,
			   PT()->fir1, (long)trial);
		cmp_filter("fir2 across reset", "FloatFIR", PO()->fir2,
			   PT()->fir2, (long)trial);

		if (trial > 0 && PO()->tableA[0] != pre_specs[0].steps[0])
			varied = 1;

		our_pre_dtor(pre_ours);
		ref_pre_dtor(pre_theirs);
		diff_eq_int("nothing left live (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	diff_eq_int("the sweep used more than one block (%ld)", varied, 1, 0);

	return diff_end();
}

static int
run_precoder_setcoef(void)
{
	static const unsigned int t1[] = { 0, 4, 8, 20, 4, 16 };
	static const unsigned int t2[] = { 0, 0, 16, 4, 20, 8 };
	int trial;
	unsigned int lvl;

	diff_begin("V92Precoder::setCoefficients");

	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < (int)(sizeof(t1) / sizeof(t1[0])); trial++) {
		unsigned char sa[PRE_SIZE], sb[PRE_SIZE];

		seed(pre_ours, pre_theirs, PRE_SLOT, 600 + trial);
		harness_alloc_reset();
		our_pre_ctor(pre_ours, 0x140);
		ref_pre_ctor(pre_theirs, 0x140);

		pf_fill_pair(pf_cf, pf_cf_copy, PF_COEF, 0x11a3u + trial);
		pf_fill_pair(pf_ci, pf_ci_copy, PF_COEF, 0x8e37u + trial);

		lvl = (unsigned int)(trial % 4);
		dsplibs_debug_level = lvl;
		ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_reset();

		our_pre_setcoef(pre_ours, pf_cf, pf_ci, t1[trial], t2[trial]);
		ref_pre_setcoef(pre_theirs, pf_cf, pf_ci, t1[trial],
				t2[trial]);

		diff_eq_int("the same diagnostics (level %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), lvl);
		diff_eq_int("and the same text (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, lvl);
		diff_eq_int("one line above level 1, none below (level %ld)",
			    (long)dsplib_debug_capture_lines(0),
			    lvl > 1 ? 1 : 0, lvl);

		pre_snapshot(sa, PO());
		pre_snapshot(sb, PT());
		diff_eq_obj_(__FILE__, __LINE__, "after setCoefficients",
			     "V92Precoder", sa, sb, PRE_SIZE, (long)trial);
		diff_eq_int("taps1 is the caller's (trial %ld)", PO()->taps1,
			    t1[trial], trial);
		diff_eq_int("taps2 is the caller's (trial %ld)", PO()->taps2,
			    t2[trial], trial);
		diff_eq_int("neither coefficient array was written (trial %ld)",
			    memcmp(pf_cf, pf_cf_copy, sizeof(pf_cf)) == 0
			    && memcmp(pf_ci, pf_ci_copy, sizeof(pf_ci)) == 0, 1,
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(pre_ours + PRE_SIZE, pre_theirs + PRE_SIZE,
				   PRE_SLOT - PRE_SIZE) == 0, 1, trial);
		cmp_filter("fir1 after setCoefficients", "FloatFIR",
			   PO()->fir1, PT()->fir1, (long)trial);
		cmp_filter("fir2 after setCoefficients", "FloatFIR",
			   PO()->fir2, PT()->fir2, (long)trial);

		our_pre_dtor(pre_ours);
		ref_pre_dtor(pre_theirs);
		diff_eq_int("nothing left live (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	return diff_end();
}

/* One call's two outputs, named so diff_eq_obj can say which symbol moved. */
struct pre_out {
	int idx[PRE_SYMBOLS];
};

struct pre_outf {
	float sum[PRE_SYMBOLS];
};

#define PRE_CALLS	24

static int
run_precoder_process(void)
{
	int spec_i;
	int wrote = 0, varied = 0, empty_seen = 0;

	diff_begin("V92Precoder::process");

	for (spec_i = 0; spec_i < NPRESPEC * 2; spec_i++) {
		int which = spec_i % NPRESPEC;
		const struct pre_spec *spec = &pre_specs[which];
		int huge = which == PRE_SPEC_HUGE;
		unsigned int t1 = huge ? 0u : 8u;
		unsigned int t2 = huge ? 0u : 4u;
		unsigned char sa[PRE_SIZE], sb[PRE_SIZE];
		int call;
		int prev = 0;
		int i;

		seed(pre_ours, pre_theirs, PRE_SLOT, 700 + spec_i);
		harness_alloc_reset();
		our_pre_ctor(pre_ours, 0x140);
		ref_pre_ctor(pre_theirs, 0x140);
		build_params(spec, spec_i);
		our_pre_reset(pre_ours, pre_params);
		ref_pre_reset(pre_theirs, pre_params);

		/*
		 * Coefficients an order of magnitude smaller than the
		 * pre-filter's: both filters are inside the loop that feeds
		 * them their own output's neighbourhood, and a gain above one
		 * walks the carried state out of the search's range within a
		 * dozen calls.
		 */
		for (i = 0; i < PF_COEF; i++) {
			unsigned int st = 0x21a3u + 37u * (unsigned int)i
					  + 5u * (unsigned int)spec_i;

			pre_cf[i] = pf_next(&st) / 1024.0f;
			pre_ci[i] = pf_next(&st) / 1024.0f;
		}
		our_pre_setcoef(pre_ours, pre_cf, pre_ci, t1, t2);
		ref_pre_setcoef(pre_theirs, pre_cf, pre_ci, t1, t2);

		/*
		 * The constellation above 2^31 is only reachable with the
		 * carried state down there too; with the filters switched off
		 * it stays put across all four symbols.
		 */
		if (huge) {
			PO()->state0 = -2147483648.0f;
			PT()->state0 = -2147483648.0f;
		}

		for (call = 0; call < PRE_CALLS; call++) {
			unsigned int in_ours[PRE_IN], in_theirs[PRE_IN];
			int out_seed[PRE_SYMBOLS];
			int out_ours[PRE_SYMBOLS], out_theirs[PRE_SYMBOLS];
			float outf_ours[PRE_SYMBOLS], outf_theirs[PRE_SYMBOLS];
			unsigned int st = 0x4d21u + 131u * (unsigned int)call
					  + 7u * (unsigned int)spec_i;
			int a = call % 3;
			int b = (call >> 1) & 1;
			int i;

			for (i = 0; i < PRE_IN; i++) {
				int v;

				switch (call & 3) {
				case 0:
					v = 0;
					break;
				case 1:
					v = (i & 1) ? -7 : 7;
					break;
				case 2:
					v = 1000 + i;
					break;
				default:
					st = st * 1103515245u + 12345u;
					v = (int)((st >> 13) % 4096u) - 2048;
					break;
				}
				in_ours[i] = (unsigned int)v;
				in_theirs[i] = (unsigned int)v;
			}

			/*
			 * Seeded outputs, with a sentinel no index the search
			 * can produce ever collides with: the parity reads
			 * out[0..2] before anything writes them, an unwritten
			 * symbol has to keep what was there, and "was this
			 * symbol written" has to be answerable.  The low bit
			 * varies so the parity does too.
			 */
			for (i = 0; i < PRE_SYMBOLS; i++) {
				out_seed[i] = 0x40000000 + i * 7 + call;
				out_ours[i] = out_seed[i];
				out_theirs[i] = out_seed[i];
				st = st * 1103515245u + 12345u;
				outf_ours[i] = (float)((int)(st >> 18) - 4096);
				outf_theirs[i] = outf_ours[i];
			}

			our_pre_process(pre_ours, in_ours, a, b, out_ours,
					outf_ours);
			ref_pre_process(pre_theirs, in_theirs, a, b,
					out_theirs, outf_theirs);

			/*
			 * EVERY CALL MUST HAVE FOUND SOMETHING FOR SYMBOL 0.
			 * Without this the sweep can drift into the state
			 * where nothing is ever accepted -- every square above
			 * the initial 1e12 -- and then it compares two
			 * uninitialised locals and reports whatever the two
			 * stacks happened to hold.  It did, before this line
			 * existed.
			 */
			diff_eq_int("symbol 0 chose a point (call %ld)",
				    out_ours[0] != out_seed[0], 1,
				    spec_i * 100 + call);
			/*
			 * The empty interval, and only where it is one: the
			 * negative modulus is `selectors[1]`, which symbol 1
			 * uses when `a` is zero and does not otherwise.
			 */
			if (which == PRE_SPEC_EMPTY && a == 0) {
				diff_eq_int("and symbol 1 found nothing to"
					    " choose (call %ld)",
					    out_ours[1] == out_seed[1], 1,
					    spec_i * 100 + call);
				empty_seen = 1;
			}

			diff_eq_obj_(__FILE__, __LINE__, "process indices",
				     "struct pre_out", out_ours, out_theirs,
				     sizeof(struct pre_out),
				     (long)(spec_i * 100 + call));
			diff_eq_obj_(__FILE__, __LINE__, "process sums",
				     "struct pre_outf", outf_ours, outf_theirs,
				     sizeof(struct pre_outf),
				     (long)(spec_i * 100 + call));
			diff_eq_int("process did not write its input"
				    " (call %ld)",
				    memcmp(in_ours, in_theirs, sizeof(in_ours))
				    == 0, 1, spec_i * 100 + call);
			diff_eq_int("nor the parameter block (call %ld)",
				    memcmp(pre_params, pre_params_copy,
					   sizeof(pre_params)) == 0, 1,
				    spec_i * 100 + call);

			pre_snapshot(sa, PO());
			pre_snapshot(sb, PT());
			diff_eq_obj_(__FILE__, __LINE__, "after process",
				     "V92Precoder", sa, sb, PRE_SIZE,
				     (long)(spec_i * 100 + call));
			diff_eq_int("no store past the object (call %ld)",
				    memcmp(pre_ours + PRE_SIZE,
					   pre_theirs + PRE_SIZE,
					   PRE_SLOT - PRE_SIZE) == 0, 1,
				    spec_i * 100 + call);
			cmp_filter("fir1 after process", "FloatFIR", PO()->fir1,
				   PT()->fir1, (long)(spec_i * 100 + call));
			cmp_filter("fir2 after process", "FloatFIR", PO()->fir2,
				   PT()->fir2, (long)(spec_i * 100 + call));

			if (out_ours[0] != out_theirs[0])
				break;	/* already reported */
			if (call > 0 && out_ours[0] != prev)
				varied = 1;
			prev = out_ours[0];
			if (outf_ours[0] != 0.0f)
				wrote = 1;
		}

		our_pre_dtor(pre_ours);
		ref_pre_dtor(pre_theirs);
		diff_eq_int("nothing left live (spec %ld)", harness_alloc.live,
			    0, spec_i);
	}

	/* The search really ran, and did not choose the same point every
	 * time -- findings F223 and F224. */
	diff_eq_int("process produced sums (%ld)", wrote, 1, 0);
	diff_eq_int("and not one fixed index (%ld)", varied, 1, 0);
	/* And D261 was actually driven, rather than merely provided for. */
	diff_eq_int("an empty interval was reached (%ld)", empty_seen, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	build_constellations();

	bad |= run_precoder_ctor();
	bad |= run_precoder_dtor();
	bad |= run_prefilter_ctor();
	bad |= run_prefilter_dtor();
	bad |= run_prefilter_methods();
	bad |= run_precoder_reset();
	bad |= run_precoder_setcoef();
	bad |= run_precoder_process();

	return bad;
}
