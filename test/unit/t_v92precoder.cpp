/*
 * t_v92precoder.cpp -- differential test of V92Precoder's and V92PreFilter's
 * constructors and destructors against the blob.
 *
 * THE OBJECTS CANNOT LIVE IN A UNION and cannot be assigned into.  Both
 * classes declare a constructor and a destructor -- which are the four
 * symbols under test -- so both are non-trivial and a union holding one has
 * its default members deleted (finding 232).  The slots are plain aligned
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
 * checked against the seed the trial actually used (findings 223, 224).
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
 *     own answer, BY ORDINAL rather than by address (finding 1353) -- a
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
		 * reasons that had nothing to do with either.  Finding 1353.
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

int
main(void)
{
	int bad = 0;

	bad |= run_precoder_ctor();
	bad |= run_precoder_dtor();
	bad |= run_prefilter_ctor();
	bad |= run_prefilter_dtor();

	return bad;
}
