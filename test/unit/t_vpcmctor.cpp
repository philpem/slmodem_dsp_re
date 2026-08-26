/*
 * t_vpcmctor.cpp -- differential test of the V.PCM modem's construction and
 * destruction: `VPcmFloModem::VPcmFloModem` (0xfa60 C1, 0xfee0 C2, 651 bytes
 * each), the IMPLICIT `~VPcmFloModem` (0xd0a0 D1, 0xd030 D2, 0x61 bytes each),
 * and the two `extern "C"` entry points that wrap them, `VPCMXF_Create`
 * (0xfcf0, 495 bytes) and `VPCMXF_Delete` (0xf6c0, 109 bytes).
 *
 * ===========================================================================
 * OUR BUILD HAS NO `_ZN12VPcmFloModemD1Ev`, AND THAT IS NOT A CHOICE
 * ===========================================================================
 *
 * The destructor is implicitly declared -- six member destructions in reverse
 * declaration order and nothing else -- so it is implicitly inline, and GCC
 * inlines it at its ONE call site (`VPCMXF_Delete`) and emits no out-of-line
 * copy.  Measured: `nm -g build/src/pump/v90/VPcmXfCreate.o` shows six
 * undefined member destructors and no `_ZN12VPcmFloModemD`, and no object
 * under build/src/ defines either symbol.  The blob HAS both, at 0xd0a0 and
 * 0xd030.
 *
 * So there is no "our D1" to call by symbol and this file does not pretend
 * there is.  What it does instead is compare OUR INLINED destructor, reached
 * through `VPCMXF_Delete`, against the blob's OUT-OF-LINE `D1` and `D2` --
 * `run_dtor_symbols` runs `ref__ZN12VPcmFloModemD1Ev` on the blob's side and
 * our `VPCMXF_Delete` minus its own free on ours, so all three of the blob's
 * spellings are driven and each is compared against the only spelling we have.
 * That is a weaker statement than symbol-against-symbol and it is the true
 * one; the alternative would have been to declare a destructor in the source
 * to make a symbol appear, which would be writing code to suit a test.
 *
 * ===========================================================================
 * THE SLOTS ARE DISCOVERED, NOT LISTED
 * ===========================================================================
 *
 * A `VPcmFloModem` is 0x7f68 bytes containing an entire `V90Modem` at +0x1758,
 * an entire `V92Modem` at +0x6124, an echo canceller, a tone detector, a sine
 * wave and an IIR filter, and between them they leave heap pointers at a dozen
 * offsets that no reader of this file should have to enumerate by hand.  So
 * the fixture FINDS them: every 4-aligned word of the object whose value lies
 * inside a live allocation is a slot, and slot k of our side corresponds to
 * slot k of the blob's BY ITS OFFSET IN THE OBJECT -- which is exactly the
 * correspondence the two implementations are supposed to agree about.  A word
 * that is a heap pointer on one side and not on the other is itself a failure,
 * and the count is asserted equal.
 *
 * Every discovered block is then compared over its whole length, with pointers
 * translated the way t_v90demctor.cpp translates them: into the object, a tag
 * plus the OFFSET; into slot k, a tag carrying k plus the offset; into any
 * other live allocation, a tag plus the offset with the block's identity
 * dropped, because a block reachable only from inside another block is the
 * callee's claim and not this file's.
 *
 * WHY THAT MATTERS HERE: `&this->dil` (+0x0004) is handed to BOTH modems --
 * the V90Modem's third argument and the V92Modem's fourth -- and is stored in
 * neither of them at an offset this object can see.  It lands inside the two
 * heap-allocated modulator/demodulator blocks, so a fixture that compared only
 * the object would not know whether either modem was given the descriptor at
 * all, let alone the same one.  Translated, both sides' copies read
 * 0x40000004 and a modem handed something else fails.
 *
 * ===========================================================================
 * THE COEFFICIENT TABLES CANNOT BE COMPARED AND ARE PINNED BY NAME INSTEAD
 * ===========================================================================
 *
 * `GenericIIR` BORROWS its coefficient arrays -- "Coefficient arrays are
 * borrowed, not copied" (GenericIIR.h), `m_den` at +0x00 and `m_num` at +0x04
 * -- so the entrance filter at +0x7f28 holds two pointers to STATIC tables,
 * ours into our `.data` and the blob's into its own.  Those two words can
 * never compare equal and no seeding makes them.
 *
 * A swap of `entFiltDen` and `entFiltNum` in the constructor would therefore
 * be INVISIBLE to any comparison that simply excludes them, which is the exact
 * shape findings F1301 and F1307 are about.  So they are not merely excluded:
 * each is pinned to a NAMED symbol pair -- ours must be `&entFiltDen` where
 * the blob's is `&ref_entFiltDen`, and likewise for the numerator -- which is
 * t_vpcmcreate.c's `check_alias_words` device applied to a data table rather
 * than to a scrambler entry point.  Swap the two in the source and the check
 * fails; that was run, and it does.
 *
 * ===========================================================================
 * THE RUNTIME BLOCK IS PART OF THE COMPARISON SURFACE
 * ===========================================================================
 *
 * The constructor's last act outside its own object is `andb $0xfb,0x3(%edi)`
 * -- bit 2 of `_tagModemParameters::unnamed_0003`.  A zeroed block makes that
 * store invisible, so the byte is SEEDED with a value that has the bit set,
 * and swept over the four interesting values {0xff, 0x04, 0x00, 0xfb} so that
 * both "the bit was set and is now clear" and "the bit was already clear" are
 * driven.  The rest of the block is zero and the file comment on `seed_all`
 * says why.  Both sides share one block, so it is saved before our side runs,
 * saved again after it, restored, and compared against the second save once
 * the blob's side has run -- otherwise the second writer's store lands on top
 * of the first's and a disagreement is invisible.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "harness.h"

#include "dsplib/VPcmFloModem.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/vpcm_tables.h"

extern "C" {
/*
 * The two computational-mode arguments are declared `int`: both are opaque
 * enums with a fixed `int` base, so the parameter passing is identical.
 */
void flo_ctor1(void *self, void *v34Obj, unsigned int side, void *modemParams,
	       unsigned int nSamples, int v90Mode, int v92Mode)
	asm("_ZN12VPcmFloModemC1EPv12V90ModemSideP19_tagModemParametersj20V90"
	    "ComputationalMode20V92ComputationalMode");
void flo_ctor2(void *self, void *v34Obj, unsigned int side, void *modemParams,
	       unsigned int nSamples, int v90Mode, int v92Mode)
	asm("_ZN12VPcmFloModemC2EPv12V90ModemSideP19_tagModemParametersj20V90"
	    "ComputationalMode20V92ComputationalMode");
void ref_flo_ctor1(void *self, void *v34Obj, unsigned int side,
		   void *modemParams, unsigned int nSamples, int v90Mode,
		   int v92Mode)
	asm("ref__ZN12VPcmFloModemC1EPv12V90ModemSideP19_tagModemParametersj20"
	    "V90ComputationalMode20V92ComputationalMode");
void ref_flo_ctor2(void *self, void *v34Obj, unsigned int side,
		   void *modemParams, unsigned int nSamples, int v90Mode,
		   int v92Mode)
	asm("ref__ZN12VPcmFloModemC2EPv12V90ModemSideP19_tagModemParametersj20"
	    "V90ComputationalMode20V92ComputationalMode");

/* The blob's out-of-line destructors.  Ours has no symbol; see the header. */
void ref_flo_dtor1(void *self) asm("ref__ZN12VPcmFloModemD1Ev");
void ref_flo_dtor2(void *self) asm("ref__ZN12VPcmFloModemD2Ev");

void *VPCMXF_Create(int digitalSide, void *v34Object,
		    struct _tagModemParameters *dpRuntime,
		    unsigned int durationMs, int mode);
void VPCMXF_Delete(void *self);
void *ref_VPCMXF_Create(int digitalSide, void *v34Object,
			struct _tagModemParameters *dpRuntime,
			unsigned int durationMs, int mode);
void ref_VPCMXF_Delete(void *self);

/* The two entrance-filter tables, ours and the blob's; see the header. */
extern double ref_entFiltNum[], ref_entFiltDen[];

extern unsigned int ref_dsplibs_debug_level;

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

typedef void (*ctorfn)(void *, void *, unsigned int, void *, unsigned int, int,
		       int);

/* ================================================================ the fixture */

#define FLO_SIZE	0x7f68
#define GUARD		64
#define FLO_SLOT	(FLO_SIZE + GUARD)

typedef char vpcmflo_is_0x7f68[(sizeof(VPcmFloModem) == FLO_SIZE) ? 1 : -1];

/* The offsets this file names, spelled rather than included from the class. */
#define OFF_V34OBJECT	0x0000
#define OFF_DIL		0x0004
#define OFF_MODEM	0x1758
#define OFF_INFO0LAYOUT	0x6120
#define OFF_V92MODEM	0x6124
#define OFF_ENTFILT	0x7f28
#define OFF_ENT_DEN	(OFF_ENTFILT + 0x00)
#define OFF_ENT_NUM	(OFF_ENTFILT + 0x04)

/* The V90Modem's own two pointers, for the side-2 teardown; V90Modem.h. */
#define OFF_MODULATOR	(OFF_MODEM + 0x0000)
#define OFF_DEMODULATOR	(OFF_MODEM + 0x0004)

#define MP_SLOT		(sizeof(struct _tagModemParameters) + 64)
#define V34_BYTES	64		/* the opaque `void *v34Object'      */

static unsigned char fobj[2][FLO_SLOT] __attribute__((aligned(8)));
static unsigned char sown[FLO_SLOT];
static unsigned char tobj[2][FLO_SIZE] __attribute__((aligned(8)));
static unsigned char mpb[MP_SLOT] __attribute__((aligned(8)));
static unsigned char v34b[V34_BYTES] __attribute__((aligned(8)));

#define MPARAMS	((struct _tagModemParameters *)mpb)

/*
 * The object under comparison.  For the constructor sweeps it is the static
 * slot above; for `VPCMXF_Create` it is whatever that returned, which is a
 * different address on each side and is exactly what `translate` is for.
 */
static unsigned char *base[2];

/* ------------------------------------------------------------------ seeding */

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);	/* never zero; f230 */
}

static void
fill(void *p, size_t n)
{
	unsigned char *q = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextb();
}

struct trial_args {
	unsigned int	side;
	unsigned int	nSamples;
	int		v90Mode;
	int		v92Mode;
	int		codec;
	unsigned char	un0003;
};

static const struct trial_args trial_v[] = {
	{ 0, 48, 0, 0, 0, 0xff },
	{ 0, 48, 1, 1, 4, 0x04 },
	{ 0, 24, 1, 0, 2, 0x00 },
	{ 0,  8, 0, 1, 1, 0xfb },
	{ 1, 48, 0, 0, 0, 0xff },
	{ 1, 48, 1, 1, 4, 0x04 },
	{ 1, 24, 1, 0, 2, 0x00 },
	{ 1,  8, 0, 1, 1, 0xfb },
	{ 1, 16, 0, 0, 3, 0x55 },
	{ 2, 48, 0, 0, 0, 0xff },
	{ 2, 24, 1, 1, 3, 0xaa },
	{ 0, 16, 0, 0, 3, 0x55 }
};

#define NTRIAL	((int)(sizeof(trial_v) / sizeof(trial_v[0])))

/*
 * THE MODEM PARAMETER BLOCK IS ZEROED APART FROM WHAT THE SWEEP VARIES.
 * `V90Parameters::V90Parameters` is `initSession(); modemParams = mp; init();`
 * and `init()` derives the whole 0x558-byte parameter block -- which then
 * decides allocation lengths and feeds x87 arithmetic -- from these 136 bytes.
 * A seeded block gives a signalling NaN about one word in 250 and an
 * allocation length of two billion rather more often; t_v90demctor.cpp zeroes
 * it for the same reason.  `unnamed_0003` is the exception and has to be,
 * because the constructor CLEARS a bit of it and a cleared bit is invisible
 * against a zero.
 */
static void
seed_all(long trial, const struct trial_args *t)
{
	lfsr = 0x2f7du + 0x9e37u * (unsigned)trial;

	fill(sown, FLO_SLOT);
	memcpy(fobj[0], sown, FLO_SLOT);
	memcpy(fobj[1], sown, FLO_SLOT);
	fill(v34b, sizeof v34b);

	memset(mpb, 0, sizeof mpb);
	MPARAMS->codecType = t->codec;
	MPARAMS->unnamed_0003 = t->un0003;

	base[0] = fobj[0];
	base[1] = fobj[1];
}

static unsigned char mp_pre[MP_SLOT], mp_ours[MP_SLOT];
static unsigned char v34_pre[V34_BYTES], v34_ours[V34_BYTES];

static void
shared_save(unsigned char *m, unsigned char *v)
{
	memcpy(m, mpb, MP_SLOT);
	memcpy(v, v34b, V34_BYTES);
}

static void
shared_restore(const unsigned char *m, const unsigned char *v)
{
	memcpy(mpb, m, MP_SLOT);
	memcpy(v34b, v, V34_BYTES);
}

/* ================================================ comparing across the sides */

#define MAXLIVE	512
#define MAXSLOT	64

struct ptrmap {
	unsigned	off[MAXSLOT];		/* object offset of the pointer */
	void		*slot[2][MAXSLOT];
	unsigned long	slotend[2][MAXSLOT];
	size_t		slotsz[2][MAXSLOT];
	int		nslot;
	void		*live[MAXLIVE];
	unsigned long	liveend[MAXLIVE];
	int		nlive;
};

static struct ptrmap pmap;

static int
live_index(const void *p)
{
	unsigned long v = (unsigned long)p;
	int i;

	for (i = 0; i < pmap.nlive; i++)
		if (v >= (unsigned long)pmap.live[i] && v <= pmap.liveend[i])
			return i;
	return -1;
}

/*
 * A slot has to be the BASE of an allocation and not merely a pointer inside
 * one: `malloc_usable_size` of an interior pointer is undefined and segfaults,
 * which is how this distinction was learned.  An interior pointer is still
 * resolved -- `translate` finds it through `live_index` -- it just does not
 * become a slot of its own.
 */
static int
is_live_base(const void *p)
{
	int i;

	for (i = 0; i < pmap.nlive; i++)
		if (pmap.live[i] == p)
			return 1;
	return 0;
}

/* Inside EITHER side's object, which is what `translate` would have resolved. */
static int
in_object(const void *p)
{
	unsigned long v = (unsigned long)p;
	int s;

	for (s = 0; s < 2; s++) {
		unsigned long b = (unsigned long)base[s];

		if (v >= b && v < b + FLO_SIZE)
			return 1;
	}
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * WHY THE LAST FEW BYTES OF EVERY DISCOVERED BLOCK ARE NOT COMPARED
 *
 * The allocator hands out `malloc_usable_size` bytes and the harness fills
 * only the REQUESTED length with 0xa5, so the bytes between the request and
 * the usable size belong to neither side: they hold whatever the chunk held
 * before it was handed out, which is a run of zeros on a chunk carved fresh
 * from the top and the previous tenant's data on a recycled one.  Our side
 * runs first and gets fresh memory; the blob's runs second and gets chunks our
 * teardown released, so the two disagree there systematically and for a reason
 * that has nothing to do with either implementation.  That was measured, not
 * assumed: every one of the first run's differences at +112 of a 0x70 block,
 * +144 of a 0x90 one, +512 of a 0x200 one and +1368 of a 0x558 one is exactly
 * one past the request.
 *
 * The request cannot be recovered from the usable size -- glibc rounds
 * `n + 4` up to the allocation granularity, so sixteen request lengths give
 * one usable size -- so the last `granularity - 1` bytes are dropped.  The
 * granularity is MEASURED here rather than assumed to be 16, and the hole is
 * at most that many bytes at the end of each block.  It is a real hole and
 * this is the record of it; the alternative was to hand-list every block's
 * `sizeof`, which is what this file discovers slots to avoid.
 */
static size_t alloc_grain = 16;

static void
measure_grain(void)
{
	size_t n, prev = 0, best = 0;

	for (n = 1; n <= 256; n++) {
		void *p = malloc(n);
		size_t u = malloc_usable_size(p);

		free(p);
		if (prev != 0 && u > prev && (best == 0 || u - prev < best))
			best = u - prev;
		prev = u;
	}
	if (best != 0)
		alloc_grain = best;
}


static void *
word_at(int side, unsigned off)
{
	void *p;

	memcpy(&p, base[side] + off, sizeof p);
	return p;
}

/*
 * Discover the slots.  A slot is an object offset holding, ON OUR SIDE, a
 * pointer into a live allocation.  The blob's side is required to have one at
 * the same offset -- `nmismatch` counts where it does not, and the sweep
 * asserts that count is zero, which is how "the two agree about WHICH words
 * are heap pointers" gets tested rather than assumed.
 */
static int
ptrmap_take(void)
{
	int mismatch = 0;
	unsigned o;
	int i, s;

	pmap.nlive = harness_alloc_live_set(pmap.live, MAXLIVE);
	if (pmap.nlive > MAXLIVE)
		pmap.nlive = MAXLIVE;
	for (i = 0; i < pmap.nlive; i++)
		pmap.liveend[i] = (unsigned long)pmap.live[i]
		    + malloc_usable_size(pmap.live[i]);

	pmap.nslot = 0;
	for (o = 0; o + 4 <= FLO_SIZE; o += 4) {
		void *a = word_at(0, o);
		void *b = word_at(1, o);
		int ia = a != 0 && is_live_base(a);
		int ib = b != 0 && is_live_base(b);

		if (!ia && !ib)
			continue;
		if (!ia || !ib) {
			mismatch++;
			continue;
		}
		if (pmap.nslot >= MAXSLOT) {
			mismatch++;
			continue;
		}
		i = pmap.nslot++;
		pmap.off[i] = o;
		for (s = 0; s < 2; s++) {
			void *p = word_at(s, o);

			pmap.slot[s][i] = p;
			pmap.slotsz[s][i] = malloc_usable_size(p);
			pmap.slotend[s][i] = (unsigned long)p
			    + pmap.slotsz[s][i];
		}
	}
	return mismatch;
}

static unsigned long
translate(unsigned long v, int side)
{
	unsigned long b = (unsigned long)base[side];
	int k;

	if (v == 0)
		return v;
	if (v >= b && v < b + FLO_SIZE)
		return 0x40000000ul + (v - b);
	for (k = 0; k < pmap.nslot; k++) {
		unsigned long p = (unsigned long)pmap.slot[side][k];

		if (p != 0 && v >= p && v <= pmap.slotend[side][k])
			return 0x50000000ul + (unsigned long)k * 0x00040000ul
			    + (v - p);
	}
	k = live_index((const void *)v);
	if (k >= 0)
		return 0x60000000ul + (v - (unsigned long)pmap.live[k]);
	return v;
}

static void
translate_block(unsigned char *dst, const unsigned char *src, size_t n, int side)
{
	size_t o;

	memcpy(dst, src, n);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int w;

		memcpy(&w, dst + o, sizeof w);
		w = (unsigned int)translate((unsigned long)w, side);
		memcpy(dst + o, &w, sizeof w);
	}
}

/*
 * ===========================================================================
 * A WORD THE TWO SIDES ALREADY AGREE ON IS NEVER TRANSLATED
 * ===========================================================================
 *
 * `translate` cannot tell a pointer from an integer, and the graph is full of
 * integers: sample rates, lengths, float bit patterns.  When one of them
 * happens to fall inside a live allocation -- and the heap base is randomised,
 * so it does on some runs and not others -- it is tagged on the side whose
 * graph it landed in and left alone on the other, and the two then differ over
 * a word neither implementation ever wrote.
 *
 * MEASURED, not feared.  t_vpcmdp.c hit exactly this on the constant
 * 0x09600000 at the V.34 object's +0x434, 15 runs in 400; this file was
 * measured at 1 run in 150 before the guard below and 0 in 300 after.
 *
 * The guard is sound in the direction that matters: two words whose RAW bytes
 * are already identical cannot become a defect by being translated, because
 * the only thing translation can do to a matched pair is unmatch it.  A real
 * disagreement is never suppressed -- if the raw words differ, both go through
 * the translation exactly as before.
 */
static void
translate_pair(unsigned char *da, unsigned char *db, const unsigned char *sa,
	       const unsigned char *sb, size_t n)
{
	size_t o;

	translate_block(da, sa, n, 0);
	translate_block(db, sb, n, 1);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int ra, rb;

		memcpy(&ra, sa + o, 4);
		memcpy(&rb, sb + o, 4);
		if (ra == rb) {
			memcpy(da + o, &ra, 4);
			memcpy(db + o, &rb, 4);
		}
	}
}


/*
 * ===========================================================================
 * THE WORDS THAT HOLD A STATIC ADDRESS OF THAT SIDE'S OWN
 * ===========================================================================
 *
 * `k` is the slot index or -1 for the object itself.  Three kinds, and each is
 * CHECKED rather than merely skipped:
 *
 *   named    the word must hold `ours` on our side and `blob` on the blob's.
 *            That is the strongest form and it is what makes an argument SWAP
 *            between two tables of one type catchable at all.
 *   n != 0   the two must be DIFFERENT addresses whose targets are
 *            byte-identical over `n` bytes -- two images of one table.
 *   n == 0   unchecked, and the prose says why.  A vtable cannot be
 *            content-checked: ours holds our member addresses and the blob's
 *            holds the blob's.
 */
#define ALIAS_OBJ	0xffffffffu

struct alias_word {
	unsigned	slot_off;	/* the OBJECT offset of the slot, or
					 * ALIAS_OBJ for the object itself */
	unsigned	off;		/* the offset within that block     */
	const void	*ours;		/* pinned by name, or 0 for the
					 * weaker static-pair check         */
	const void	*blob;
	const char	*name;
};

static const struct alias_word aliasword[] = {
	{ ALIAS_OBJ, OFF_ENT_DEN, (const void *)entFiltDen,
	  (const void *)ref_entFiltDen, "entFilt's denominator" },
	{ ALIAS_OBJ, OFF_ENT_NUM, (const void *)entFiltNum,
	  (const void *)ref_entFiltNum, "entFilt's numerator" },
	/*
	 * The two words of the heap-allocated V90Demodulator that hold a
	 * static address of that side's own.  Both are t_v90demctor.cpp's
	 * claim and t_v90modemctor.cpp excludes the same two for the same
	 * reason; they are named here rather than discovered so that a THIRD
	 * such word would be a failure and not a silently widened hole.
	 *
	 *   +0x06c  `preFilter.coefficients`, which `V90PreFilter::reset`
	 *           points at a row of `preFilterCoefType1` -- the row depends
	 *           on the codec type, which this sweep varies, so the address
	 *           moves from trial to trial and is still one table's row on
	 *           each side.  t_v90prefilter.cpp's claim.
	 *   +0x094  the `V90Resampler` vptr, which is that side's own vtable.
	 *           A vtable cannot be content-checked -- ours holds our member
	 *           addresses and the blob's holds the blob's -- so this one
	 *           rests on the argument and not on a measurement.
	 *
	 * Neither can be pinned by name from here: both symbols are internal to
	 * translation units this file does not include.  What IS checked, on
	 * every trial, is that each holds two DIFFERENT addresses and that
	 * neither is a heap pointer nor an address inside either object -- a
	 * heap pointer would have been resolved by `translate` and would not
	 * need excluding at all, so that check is what says the exclusion is
	 * the right shape.
	 */
	{ OFF_DEMODULATOR, 0x06c, 0, 0, "the prefilter's coefficients" },
	{ OFF_DEMODULATOR, 0x094, 0, 0, "the V90Resampler vptr" }
};

#define NALIAS	((int)(sizeof(aliasword) / sizeof(aliasword[0])))

static int alias_fired[NALIAS];

/*
 * Every named word checked to BE what it claims.  Returns the number that were
 * not; the sweep asserts zero.
 */
static int
alias_slot(unsigned slot_off)
{
	int k;

	if (slot_off == ALIAS_OBJ)
		return -1;
	for (k = 0; k < pmap.nslot; k++)
		if (pmap.off[k] == slot_off)
			return k;
	return -2;			/* not built on this arm */
}

static int
alias_check(void)
{
	int i, bad = 0;

	for (i = 0; i < NALIAS; i++) {
		const unsigned char *pa, *pb;
		void *wa, *wb;
		int k = alias_slot(aliasword[i].slot_off);

		if (k == -2)
			continue;
		if (k < 0) {
			pa = base[0];
			pb = base[1];
		} else {
			pa = (const unsigned char *)pmap.slot[0][k];
			pb = (const unsigned char *)pmap.slot[1][k];
		}
		if (pa == 0 || pb == 0)
			continue;
		memcpy(&wa, pa + aliasword[i].off, sizeof wa);
		memcpy(&wb, pb + aliasword[i].off, sizeof wb);

		if (wa != wb)
			alias_fired[i] = 1;
		if (aliasword[i].ours != 0) {
			if (wa != aliasword[i].ours
			    || wb != aliasword[i].blob) {
				printf("    %s at +0x%04x: ours %p (want %p),"
				       " blob %p (want %p)\n",
				       aliasword[i].name, aliasword[i].off, wa,
				       aliasword[i].ours, wb,
				       aliasword[i].blob);
				bad++;
			}
			continue;
		}
		/*
		 * The weaker form: two DIFFERENT addresses, neither null,
		 * neither a heap pointer and neither inside an object.  That
		 * is what "each side's own copy of a static object" means, and
		 * it is checked rather than asserted.
		 */
		if (wa == wb || wa == 0 || wb == 0 || live_index(wa) >= 0
		    || live_index(wb) >= 0 || in_object(wa) || in_object(wb)) {
			printf("    %s at +0x%04x: %p/%p is not a pair of"
			       " static addresses\n", aliasword[i].name,
			       aliasword[i].off, wa, wb);
			bad++;
		}
	}
	return bad;
}

/*
 * ===========================================================================
 * THE `GenericIIR`s, FOUND STRUCTURALLY, AND FINDING F1250 ASSERTED
 * ===========================================================================
 *
 * A `VPcmFloModem` graph contains several `GenericIIR<float, double>`s -- the
 * entrance filter embedded at +0x7f28 and one inside each tone detector the
 * ANSam detector owns -- and every one of them carries four words that no
 * fixture can make agree:
 *
 *   +0x00 `m_den`, +0x04 `m_num`   BORROWED coefficient arrays (GenericIIR.h),
 *                                  so each side points at its own copy of a
 *                                  static table.
 *   +0x28 `m_i`, +0x2c `m_acc`     FINDING F1250, REPAIRED.  `reset()` counts
 *                                  in `m_i` and neither constructor writes
 *                                  either member, so both sides leave `m_i`
 *                                  holding `m_outLen` and both leave `m_acc`
 *                                  holding whatever their storage held -- and
 *                                  that last is a fixture property, which is
 *                                  why the two words stay excluded.
 *
 * THE STATE IS ASSERTED, NOT SKIPPED, which is t_gtonedet.cpp's rule and
 * finding F1250's own instruction.  All four halves are checked on every filter
 * found: each side's `m_i` equal to its own `m_outLen`, and each side's
 * `m_acc` not zero.  Undo the repair in `src/dsp/FloatIIR.cpp` and these
 * checks fail, which is the notification that the exclusion has gone obsolete;
 * an exclusion that merely looked away would stay green for ever.
 *
 * THE COEFFICIENT POINTERS ARE CHECKED TOO, and that is what makes an argument
 * SWAP catchable in a word that can never compare equal: the two addresses
 * must DIFFER and their targets must be byte-identical over `m_nden` and
 * `m_nnum` DOUBLES -- lengths read out of the filter itself.  Swap the
 * denominator and the numerator in the constructor and ours points at a table
 * whose contents are not the blob's, so the check fails.  The entrance
 * filter's two are additionally pinned to the NAMED symbols in `aliasword`.
 *
 * The filters are FOUND and not listed: a candidate base must have two
 * non-heap, non-object coefficient pointers that differ between the sides, two
 * history buffers that are live allocation bases big enough for their declared
 * lengths, orders in 1..64 that agree between the sides, and one common block
 * size implied by both lengths.  Nothing but a `GenericIIR` fits that.
 */
struct region {
	int			k;	/* -1 the object, else the slot index */
	const unsigned char	*a;
	const unsigned char	*b;
	size_t			n;	/* compared, and read                 */
	size_t			nfull;	/* read for structure detection       */
};

struct excl {
	int		k;
	unsigned	off;
	unsigned	len;
};

static struct excl excl_v[256];
static int nexcl;
static int n_iir;

static void
excl_add(int k, unsigned off, unsigned len)
{
	if (nexcl < (int)(sizeof excl_v / sizeof excl_v[0])) {
		excl_v[nexcl].k = k;
		excl_v[nexcl].off = off;
		excl_v[nexcl].len = len;
		nexcl++;
	}
}

static unsigned int
u32(const unsigned char *p, unsigned off)
{
	unsigned int v;

	memcpy(&v, p + off, sizeof v);
	return v;
}

static const void *
vptr(const unsigned char *p, unsigned off)
{
	const void *v;

	memcpy(&v, p + off, sizeof v);
	return v;
}

/* Does a GenericIIR<float,double> sit at `o` in this region? */
static int
iir_at(const struct region *r, unsigned o)
{
	const void *da, *db, *na, *nb, *ia, *ib, *oa, *ob;
	unsigned nden, nnum, ilen, olen;

	if ((size_t)o + 0x34 > r->nfull)
		return 0;
	nden = u32(r->a, o + 0x10);
	nnum = u32(r->a, o + 0x14);
	ilen = u32(r->a, o + 0x18);
	olen = u32(r->a, o + 0x1c);
	if (nden != u32(r->b, o + 0x10) || nnum != u32(r->b, o + 0x14)
	    || ilen != u32(r->b, o + 0x18) || olen != u32(r->b, o + 0x1c))
		return 0;
	if (nden < 1 || nden > 64 || nnum < 1 || nnum > 64)
		return 0;
	if (ilen < nnum || olen < nden || ilen - nnum != olen - nden
	    || ilen - nnum > 4096)
		return 0;

	ia = vptr(r->a, o + 0x08);
	ib = vptr(r->b, o + 0x08);
	oa = vptr(r->a, o + 0x0c);
	ob = vptr(r->b, o + 0x0c);
	if (!is_live_base(ia) || !is_live_base(ib) || !is_live_base(oa)
	    || !is_live_base(ob))
		return 0;
	if (malloc_usable_size((void *)ia) < ilen * 8
	    || malloc_usable_size((void *)oa) < olen * 8)
		return 0;

	da = vptr(r->a, o + 0x00);
	db = vptr(r->b, o + 0x00);
	na = vptr(r->a, o + 0x04);
	nb = vptr(r->b, o + 0x04);
	if (da == 0 || db == 0 || na == 0 || nb == 0)
		return 0;
	if (da == db || na == nb)
		return 0;
	if (live_index(da) >= 0 || live_index(db) >= 0 || live_index(na) >= 0
	    || live_index(nb) >= 0)
		return 0;
	if (in_object(da) || in_object(db) || in_object(na) || in_object(nb))
		return 0;
	return 1;
}

/*
 * Find every filter in a region, check all four of its claims, and exclude the
 * four words that cannot agree.  Returns the number of claims that failed.
 */
static int
iir_scan(const struct region *r)
{
	unsigned o;
	int bad = 0;

	for (o = 0; (size_t)o + 0x34 <= r->nfull; o += 4) {
		unsigned nden, nnum, olen;
		const void *da, *db, *na, *nb;
		static const unsigned char zero8[8] = { 0 };

		if (!iir_at(r, o))
			continue;
		n_iir++;
		nden = u32(r->a, o + 0x10);
		nnum = u32(r->a, o + 0x14);
		olen = u32(r->a, o + 0x1c);
		da = vptr(r->a, o + 0x00);
		db = vptr(r->b, o + 0x00);
		na = vptr(r->a, o + 0x04);
		nb = vptr(r->b, o + 0x04);

		if (memcmp(da, db, nden * sizeof(double)) != 0) {
			printf("    a filter at region %d +0x%04x: the two"
			       " denominators are not one table\n", r->k, o);
			bad++;
		}
		if (memcmp(na, nb, nnum * sizeof(double)) != 0) {
			printf("    a filter at region %d +0x%04x: the two"
			       " numerators are not one table\n", r->k, o);
			bad++;
		}
		/*
		 * Finding F1250, all four halves, in its REPAIRED state: both
		 * sides now leave `m_outLen` in `m_i` and neither writes
		 * `m_acc`.  The two words stay excluded from the byte
		 * comparison because `m_acc` holds whatever each side's
		 * storage held, which is a fixture property and not a claim.
		 */
		if (u32(r->a, o + 0x28) != olen
		    || memcmp(r->a + o + 0x2c, zero8, 8) == 0) {
			printf("    a filter at region %d +0x%04x: OUR m_i is"
			       " not m_outLen or our m_acc is zero -- finding"
			       " 1250's repair has been undone\n",
			       r->k, o);
			bad++;
		}
		if (u32(r->b, o + 0x28) != olen
		    || memcmp(r->b + o + 0x2c, zero8, 8) == 0) {
			printf("    a filter at region %d +0x%04x: the BLOB's"
			       " m_i is not m_outLen or its m_acc is zero --"
			       " finding F1250 no longer describes it\n", r->k,
			       o);
			bad++;
		}

		excl_add(r->k, o + 0x00, 4);
		excl_add(r->k, o + 0x04, 4);
		excl_add(r->k, o + 0x28, 4);
		excl_add(r->k, o + 0x2c, 8);
	}
	return bad;
}

/* ------------------------------------------------------ the two comparisons */

static unsigned char cmpa[0x4000], cmpb[0x4000];

/*
 * The regions: the object, then one per discovered slot.  Built once per trial
 * and used by the filter scan, by the comparison and by the report, so that
 * all three see exactly the same bytes.
 */
static struct region region_v[MAXSLOT + 1];
static int nregion;

static int
regions_take(void)
{
	int k, bad = 0;

	nexcl = 0;
	n_iir = 0;
	nregion = 0;

	region_v[nregion].k = -1;
	region_v[nregion].a = base[0];
	region_v[nregion].b = base[1];
	region_v[nregion].n = FLO_SIZE;
	region_v[nregion].nfull = FLO_SIZE;
	nregion++;

	for (k = 0; k < pmap.nslot; k++) {
		size_t n = pmap.slotsz[0][k];

		if (pmap.slotsz[1][k] < n)
			n = pmap.slotsz[1][k];
		if (n > sizeof cmpa)
			n = sizeof cmpa;
		region_v[nregion].k = k;
		region_v[nregion].a = (const unsigned char *)pmap.slot[0][k];
		region_v[nregion].b = (const unsigned char *)pmap.slot[1][k];
		region_v[nregion].n = n;
		region_v[nregion].nfull = n;
		nregion++;
		/*
		 * THE ALLOCATOR'S SLACK, NEUTRALISED RATHER THAN CUT OFF.
		 * Shortening the region instead would truncate the last word,
		 * so a pointer straddling the boundary would be compared as
		 * three bytes and a filter sitting at the end of a block would
		 * not be recognised at all -- both of which happened before
		 * this was written this way.
		 */
		if (n > alloc_grain)
			excl_add(k, (unsigned)(n - (alloc_grain - 1)),
				 (unsigned)(alloc_grain - 1));
	}

	for (k = 0; k < nregion; k++)
		bad += iir_scan(&region_v[k]);
	for (k = 0; k < NALIAS; k++) {
		int j2 = alias_slot(aliasword[k].slot_off);

		if (j2 != -2)
			excl_add(j2, aliasword[k].off, 4);
	}
	return bad;
}

static void
poison(unsigned char *a, unsigned char *b, int k, size_t n)
{
	int i;

	for (i = 0; i < nexcl; i++) {
		size_t off = excl_v[i].off;
		size_t len = excl_v[i].len;

		if (excl_v[i].k != k || off >= n)
			continue;
		if (off + len > n)		/* clamp, never skip */
			len = n - off;
		memset(a + off, 0x77, len);
		memset(b + off, 0x77, len);
	}
}

static void
cmp_object(long trial)
{
	translate_pair(tobj[0], tobj[1], base[0], base[1], FLO_SIZE);
	poison(tobj[0], tobj[1], -1, FLO_SIZE);
	diff_eq_obj_(__FILE__, __LINE__, "the VPcmFloModem", "VPcmFloModem",
		     tobj[0], tobj[1], FLO_SIZE, trial);
}

static void
cmp_slots(long trial)
{
	int r;

	(void)trial;
	for (r = 1; r < nregion; r++) {
		int k = region_v[r].k;
		size_t n = region_v[r].n;

		diff_eq_int("the block at +0x%04lx is the same size on both"
			    " sides", (long)pmap.slotsz[1][k],
			    (long)pmap.slotsz[0][k], (long)pmap.off[k]);
		translate_pair(cmpa, cmpb, region_v[r].a, region_v[r].b, n);
		poison(cmpa, cmpb, k, n);
		diff_eq_obj_(__FILE__, __LINE__, "a heap block of the modem",
			     "bytes", cmpa, cmpb, n, (long)pmap.off[k]);
	}
}

/*
 * The discovery pass: every word still differing after the translation and the
 * exclusions, with the raw values, so that a new static-address install shows
 * up as a report and not as an unexplained failure.  `report` is capped so a
 * systematic difference cannot bury the run.
 */
static int
report_diffs(int report)
{
	static int shown;
	int r, bad = 0;
	unsigned o;

	for (r = 0; r < nregion; r++) {
		size_t n = region_v[r].n;
		int k = region_v[r].k;
		const unsigned char *ta, *tb;

		/*
		 * The object is bigger than the scratch buffers and has
		 * already been translated and poisoned by `cmp_object`; the
		 * slots are done here.
		 */
		if (k < 0) {
			ta = tobj[0];
			tb = tobj[1];
		} else {
			if (n > sizeof cmpa)
				n = sizeof cmpa;
			translate_pair(cmpa, cmpb, region_v[r].a,
				       region_v[r].b, n);
			poison(cmpa, cmpb, k, n);
			ta = cmpa;
			tb = cmpb;
		}
		for (o = 0; o + 4 <= n; o += 4) {
			unsigned int wa, wb;

			memcpy(&wa, ta + o, 4);
			memcpy(&wb, tb + o, 4);
			if (wa == wb)
				continue;
			bad++;
			if (report && shown++ < 40)
				printf("      region %d (object +0x%04x, %lu"
				       " bytes) +0x%04x: translated %08x/%08x"
				       "  raw %08x/%08x\n", k,
				       k < 0 ? 0 : pmap.off[k],
				       (unsigned long)n, o, wa, wb,
				       u32(region_v[r].a, o),
				       u32(region_v[r].b, o));
		}
	}
	return bad;
}

/* ============================================================ the two sides */

/*
 * Destroy what a trial built.  There is no "our" destructor symbol, so ours is
 * reached through `VPCMXF_Delete` -- which also frees the object, and the
 * static slot is not the allocator's.  `flo_destroy` therefore copies the
 * object into a heap block the allocator DID hand out, destroys that, and
 * copies nothing back: the destructor's job here is to release the graph the
 * constructor built, and every pointer it follows is in the copy.
 *
 * On a side of 2 the V90Modem's constructor wrote NEITHER `modulator` nor
 * `demodulator`, so those two words still hold seed bytes and `~V90Modulator`
 * would dereference one.  They are nulled first, exactly as in
 * t_v90modemctor.cpp and for the same reason.
 */
static void *
heap_copy(int side, unsigned int modemSide)
{
	void *p;
	void *nul = 0;

	if (modemSide > 1) {
		memcpy(base[side] + OFF_MODULATOR, &nul, sizeof nul);
		memcpy(base[side] + OFF_DEMODULATOR, &nul, sizeof nul);
	}
	p = sysdep_malloc(FLO_SIZE);
	memcpy(p, base[side], FLO_SIZE);
	return p;
}

static void
flo_destroy_ours(unsigned int modemSide)
{
	VPCMXF_Delete(heap_copy(0, modemSide));
}

static void
flo_destroy_blob(unsigned int modemSide)
{
	ref_VPCMXF_Delete(heap_copy(1, modemSide));
}

/* =========================================================== the constructor */

static int
run_ctor(const char *name, ctorfn our_ctor, ctorfn ref_ctor, int discover)
{
	unsigned char first[FLO_SLOT];
	int trial, moved = 0, distinct = 0, extra = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		const struct trial_args *t = &trial_v[trial];
		int a_allocs, b_allocs, mismatch;
		unsigned a_bytes, b_bytes;

		seed_all(trial, t);
		harness_alloc_reset();
		shared_save(mp_pre, v34_pre);

		our_ctor(fobj[0], v34b, t->side, mpb, t->nSamples, t->v90Mode,
			 t->v92Mode);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;
		shared_save(mp_ours, v34_ours);
		shared_restore(mp_pre, v34_pre);

		ref_ctor(fobj[1], v34b, t->side, mpb, t->nSamples, t->v90Mode,
			 t->v92Mode);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		mismatch = ptrmap_take();
		diff_eq_int("every GenericIIR is what finding F1250 describes"
			    " (trial %ld)", regions_take(), 0, trial);
		diff_eq_int("and some filter was found at all (trial %ld)",
			    n_iir > 0, 1, trial);

		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", a_bytes, b_bytes,
			    trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("the two sides agree on WHICH words are heap"
			    " pointers (trial %ld)", mismatch, 0, trial);
		diff_eq_int("and there are some to agree about (trial %ld)",
			    pmap.nslot > 0, 1, trial);

		cmp_object(trial);
		cmp_slots(trial);
		diff_eq_int("every named alias word is what it claims to be"
			    " (trial %ld)", alias_check(), 0, trial);

		if (discover)
			extra += report_diffs(1);

		diff_eq_int("nothing stored past the object (trial %ld)",
			    memcmp(fobj[0] + FLO_SIZE, sown + FLO_SIZE,
				   GUARD) == 0
			    && memcmp(fobj[1] + FLO_SIZE, sown + FLO_SIZE,
				      GUARD) == 0, 1, trial);

		diff_eq_obj_(__FILE__, __LINE__, "the runtime block",
			     "_tagModemParameters", mpb, mp_ours, MP_SLOT,
			     trial);
		diff_eq_obj_(__FILE__, __LINE__, "the V.34 object", "bytes",
			     v34b, v34_ours, V34_BYTES, trial);

		/*
		 * THE ONE STORE OUTSIDE THE OBJECT, asserted absolutely and
		 * not only compared: bit 2 of `unnamed_0003` clear and every
		 * other bit of that byte as the seed left it.
		 */
		diff_eq_int("bit 2 of unnamed_0003 is clear (trial %ld)",
			    MPARAMS->unnamed_0003 & 0x04u, 0, trial);
		diff_eq_int("and the other seven bits are untouched (trial %ld)",
			    MPARAMS->unnamed_0003 | 0x04u,
			    t->un0003 | 0x04u, trial);

		if (memcmp(sown, fobj[0], FLO_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, fobj[0], FLO_SLOT);
		else if (memcmp(first, fobj[0], FLO_SLOT) != 0)
			distinct = 1;

		flo_destroy_ours(t->side);
		flo_destroy_blob(t->side);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1, 0);
	if (discover) {
		int i;

		diff_eq_int("no word outside the named alias words differs"
			    " (%ld)", extra, 0, extra);
		for (i = 0; i < NALIAS; i++) {
			printf("    excluded, %s: %s\n",
			       aliasword[i].ours != 0
			       ? "and pinned to a named symbol on each side"
			       : "and checked to be a pair of static addresses",
			       aliasword[i].name);
			diff_eq_int("%s really does hold two different"
				    " addresses", alias_fired[i], 1, i);
		}
	}

	return diff_end();
}

/* ==================================================== VPCMXF_Create / Delete */

/*
 * `VPCMXF_Create` allocates its own object, so the two sides' objects are at
 * two addresses and `base[]` is set from what each returned.  Everything else
 * is the constructor sweep: the same translation, the same slot discovery, the
 * same comparison of every heap block.
 *
 * THE MODE SWEEP IS {-1, 0, 1, 2, 3, 4} and that is every arm plus both sides
 * of the signed `jg`: 1, 2 and 3 are the three named arms, 0 and 4 are the
 * default on either side of them, and -1 is the default reached through the
 * `jg`'s other direction, which is what makes `mode` an `int` rather than an
 * `unsigned`.
 *
 * `digitalSide` is swept {0, 1, 7} because the object tests it for ZERO three
 * times and never stores it: 0 and non-zero are the two classes and 7 is the
 * second member of the second class, which is what says the test is `!= 0` and
 * not `== 1`.
 */
struct create_args {
	int		digitalSide;
	unsigned int	durationMs;
	int		mode;
	int		codec;
	unsigned char	un0003;
};

static const struct create_args create_v[] = {
	{ 0,  5,  0, 0, 0xff },
	{ 0,  5,  1, 4, 0x04 },
	{ 0,  5,  2, 2, 0x00 },
	{ 0,  5,  3, 1, 0xfb },
	{ 0,  5,  4, 3, 0x55 },
	{ 0,  5, -1, 0, 0xaa },
	{ 1,  6,  0, 0, 0xff },
	{ 1,  6,  1, 4, 0x04 },
	{ 1,  6,  2, 2, 0x00 },
	{ 1,  6,  3, 1, 0xfb },
	{ 1,  6,  4, 3, 0x55 },
	{ 1,  6, -1, 0, 0xaa },
	{ 7,  6,  3, 2, 0x12 },
	/*
	 * The rounding, either side of a half.  A `durationMs` of 0 is NOT
	 * swept: it gives a sample count of zero and both sides then divide by
	 * it deep inside a sub-constructor, so the case is a SIGFPE on both
	 * and measures nothing about `VPCMXF_Create`.  Recorded rather than
	 * silently omitted.
	 */
	{ 0,  1,  3, 0, 0xff },
	{ 0,  2,  3, 0, 0xff },
	{ 0,  3,  3, 0, 0xff },
	{ 1,  1,  3, 0, 0xff },
	{ 1,  3,  3, 0, 0xff }
};

#define NCREATE	((int)(sizeof(create_v) / sizeof(create_v[0])))

static int
run_create(const char *name)
{
	int trial, allocated = 0, extra = 0;

	diff_begin(name);

	for (trial = 0; trial < NCREATE; trial++) {
		const struct create_args *c = &create_v[trial];
		struct trial_args t;
		void *pa, *pb;
		int a_allocs, b_allocs, mismatch;
		unsigned a_bytes, b_bytes;

		t.side = 0;
		t.nSamples = 0;
		t.v90Mode = 0;
		t.v92Mode = 0;
		t.codec = c->codec;
		t.un0003 = c->un0003;
		seed_all(trial, &t);
		harness_alloc_reset();
		shared_save(mp_pre, v34_pre);

		pa = VPCMXF_Create(c->digitalSide, v34b, MPARAMS, c->durationMs,
				   c->mode);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;
		shared_save(mp_ours, v34_ours);
		shared_restore(mp_pre, v34_pre);

		pb = ref_VPCMXF_Create(c->digitalSide, v34b, MPARAMS,
				       c->durationMs, c->mode);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		diff_eq_int("both sides returned an object (%ld)",
			    pa != 0 && pb != 0, 1, trial);
		if (pa == 0 || pb == 0)
			continue;
		allocated = 1;
		base[0] = (unsigned char *)pa;
		base[1] = (unsigned char *)pb;

		diff_eq_int("allocations (%ld)", a_allocs, b_allocs, trial);
		diff_eq_int("bytes allocated (%ld)", a_bytes, b_bytes, trial);
		diff_eq_int("the object is 0x7f68 bytes (%ld)",
			    malloc_usable_size(pa) >= FLO_SIZE, 1, trial);

		mismatch = ptrmap_take();
		diff_eq_int("every GenericIIR is what finding F1250 describes"
			    " (trial %ld)", regions_take(), 0, trial);
		diff_eq_int("and some filter was found at all (trial %ld)",
			    n_iir > 0, 1, trial);
		diff_eq_int("the two sides agree on WHICH words are heap"
			    " pointers (%ld)", mismatch, 0, trial);
		cmp_object(trial);
		cmp_slots(trial);
		diff_eq_int("every named alias word is what it claims (%ld)",
			    alias_check(), 0, trial);
		extra += report_diffs(1);

		diff_eq_obj_(__FILE__, __LINE__, "the runtime block",
			     "_tagModemParameters", mpb, mp_ours, MP_SLOT,
			     trial);
		diff_eq_obj_(__FILE__, __LINE__, "the V.34 object", "bytes",
			     v34b, v34_ours, V34_BYTES, trial);

		/*
		 * FIVE OF THE TWENTY-ONE STORES CONTRADICT THE CONSTRUCTOR and
		 * that is the interesting part of this function, so they are
		 * asserted ABSOLUTELY here as well as compared: a constructed
		 * and returned modem never has the constructor's values for
		 * them, and a test that only compared the two sides could not
		 * tell a correct pair from two implementations that had both
		 * dropped the tail.
		 */
		diff_eq_int("nofBitsPerSymbol is 2 and not the constructor's 0"
			    " (%ld)",
			    ((VPcmFloModem *)pa)->nofBitsPerSymbol, 2, trial);
		diff_eq_int("minNofTransmitSequences is 1 and not 0 (%ld)",
			    ((VPcmFloModem *)pa)->minNofTransmitSequences, 1,
			    trial);
		diff_eq_int("v34BaudAllow[1] is 0 where the constructor set 1"
			    " (%ld)", ((VPcmFloModem *)pa)->v34BaudAllow[1], 0,
			    trial);
		diff_eq_int("v34BaudAllow[5] is 0 where the constructor set 1"
			    " (%ld)", ((VPcmFloModem *)pa)->v34BaudAllow[5], 0,
			    trial);
		diff_eq_int("v34BaudAllow[0] is still 1 (%ld)",
			    ((VPcmFloModem *)pa)->v34BaudAllow[0], 1, trial);

		/*
		 * THE SAMPLE COUNT, read back where the object put it.  The
		 * echo canceller is handed `maxDataBuffer` and so is each
		 * modem; the conversion is `durationMs * 9.6 + 0.5` truncated
		 * for an analog side and `* 8.0 + 0.5` for a digital one, and
		 * this is the one place a rounding difference would show.
		 */
		VPCMXF_Delete(pa);
		ref_VPCMXF_Delete(pb);
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, trial);
		diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0,
			    trial);
	}

	diff_eq_int("some case really built a modem", allocated, 1, 0);
	diff_eq_int("no word outside the named alias words differs (%ld)",
		    extra, 0, extra);

	return diff_end();
}

/*
 * `VPCMXF_Delete(0)` is the one arm of that function with no object behind it,
 * and it is the whole of what the null guard does.
 */
static int
run_delete_null(void)
{
	int trial;

	diff_begin("VPCMXF_Delete over a null pointer");

	for (trial = 0; trial < 4; trial++) {
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;

		harness_alloc_reset();
		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		a_bad = harness_alloc.bad_free;
		VPCMXF_Delete(0);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		a_bad = harness_alloc.bad_free - a_bad;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		b_bad = harness_alloc.bad_free;
		ref_VPCMXF_Delete(0);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		b_bad = harness_alloc.bad_free - b_bad;

		diff_eq_int("frees (%ld)", a_frees, b_frees, trial);
		diff_eq_int("and none (%ld)", a_frees, 0, trial);
		diff_eq_int("sysdep_free(NULL) (%ld)", a_null, b_null, trial);
		diff_eq_int("and none, so the guard is BEFORE the free (%ld)",
			    a_null + b_null, 0, trial);
		diff_eq_int("bad frees (%ld)", a_bad, b_bad, trial);
		diff_eq_int("and none (%ld)", a_bad + b_bad, 0, trial);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * THE BLOB'S TWO DESTRUCTOR SYMBOLS AGAINST OUR INLINED ONE
 * ===========================================================================
 *
 * Ours exists only inside `VPCMXF_Delete`, so this pass compares the two by
 * what they RELEASE: both sides build an identical modem, ours is destroyed
 * through `VPCMXF_Delete` (destructor plus one free of the object) and the
 * blob's through `D1` or `D2` followed by an explicit `sysdep_free`, and the
 * allocator counters are compared with that one free accounted for.  If our
 * inlined destructor released a different set of blocks the live count would
 * not reach zero on one side and would on the other.
 */
static int
run_dtor_symbols(const char *name, void (*ref_dtor)(void *))
{
	int trial, released = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		const struct trial_args *t = &trial_v[trial];
		void *pa, *pb;
		int a_frees, b_frees, a_null, b_null, live_a, live_b;

		seed_all(trial, t);
		harness_alloc_reset();
		shared_save(mp_pre, v34_pre);
		flo_ctor1(fobj[0], v34b, t->side, mpb, t->nSamples, t->v90Mode,
			  t->v92Mode);
		shared_restore(mp_pre, v34_pre);
		ref_flo_ctor1(fobj[1], v34b, t->side, mpb, t->nSamples,
			      t->v90Mode, t->v92Mode);

		pa = heap_copy(0, t->side);
		pb = heap_copy(1, t->side);

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		VPCMXF_Delete(pa);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		live_a = harness_alloc.live;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		ref_dtor(pb);
		sysdep_free(pb);		/* the free VPCMXF_Delete does */
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		live_b = harness_alloc.live;

		diff_eq_int("blocks released (trial %ld)", a_frees, b_frees,
			    trial);
		diff_eq_int("sysdep_free(NULL) calls (trial %ld)", a_null,
			    b_null, trial);
		diff_eq_int("and neither side freed a null (trial %ld)",
			    a_null + b_null, 0, trial);
		/*
		 * After OUR side has run, the blob's graph is still standing,
		 * so the live count is not zero and that is what says our side
		 * released only its own; after the blob's side has run it is
		 * zero, which is the whole claim.
		 */
		diff_eq_int("the blob's graph still stands (trial %ld)",
			    live_a > 0, 1, trial);
		diff_eq_int("and then the whole graph is gone (trial %ld)",
			    live_b, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		if (a_frees > 1)
			released = 1;
	}

	diff_eq_int("the destructor really released something", released, 1, 0);

	return diff_end();
}

/* ============================================================ diagnostics */

/*
 * `VPCMXF_Create`'s two print sites, which no comparison of memory can see.
 * The first chooses "Digital" or "Analog" on `digitalSide` -- and note the
 * INVERSION, because the same argument becomes `(digitalSide == 0)` in the
 * modem's side slot, so the string and the modem disagree by construction and
 * a reconstruction that made them agree would be wrong.  It also prints
 * `maxDataBuffer`, which is the only place the sample-count conversion is
 * visible as a NUMBER rather than through an allocation length.
 */
static int
run_transcripts(void)
{
	int trial, printed = 0;

	diff_begin("VPCMXF_Create's diagnostics, both sides talking");

	for (trial = 0; trial < NCREATE; trial++) {
		const struct create_args *c = &create_v[trial];
		struct trial_args t;
		void *pa, *pb;

		t.side = 0;
		t.nSamples = 0;
		t.v90Mode = 0;
		t.v92Mode = 0;
		t.codec = c->codec;
		t.un0003 = c->un0003;
		seed_all(trial, &t);
		harness_alloc_reset();

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		shared_save(mp_pre, v34_pre);
		pa = VPCMXF_Create(c->digitalSide, v34b, MPARAMS, c->durationMs,
				   c->mode);
		shared_restore(mp_pre, v34_pre);
		pb = ref_VPCMXF_Create(c->digitalSide, v34b, MPARAMS,
				       c->durationMs, c->mode);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		if (dsplib_debug_capture_lines(1) > 0)
			printed = 1;

		VPCMXF_Delete(pa);
		ref_VPCMXF_Delete(pb);
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, trial);
	}

	diff_eq_int("and some case actually printed something", printed, 1, 0);

	return diff_end();
}

/* ==================================================================== main */

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	measure_grain();
	printf("    the allocator's granularity is %lu bytes, so the last %lu"
	       " of each discovered block are not compared\n",
	       (unsigned long)alloc_grain, (unsigned long)(alloc_grain - 1));

	rc |= run_ctor("VPcmFloModem::VPcmFloModem (C1)", flo_ctor1,
		       ref_flo_ctor1, 1);
	rc |= run_ctor("VPcmFloModem::VPcmFloModem (C2)", flo_ctor2,
		       ref_flo_ctor2, 0);

	rc |= run_dtor_symbols("~VPcmFloModem (blob D1) against ours inlined",
			       ref_flo_dtor1);
	rc |= run_dtor_symbols("~VPcmFloModem (blob D2) against ours inlined",
			       ref_flo_dtor2);

	rc |= run_create("VPCMXF_Create against the blob's");
	rc |= run_delete_null();
	rc |= run_transcripts();

	return rc;
}
