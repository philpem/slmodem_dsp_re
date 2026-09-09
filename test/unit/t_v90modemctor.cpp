/*
 * t_v90modemctor.cpp -- differential test of `V90Modem`'s lifecycle pair: the
 * 597-byte constructor at 0x194e0 (C1) and 0x19740 (C2), and the 321-byte
 * destructor at 0x192b0 (D1) and 0x19160 (D2).
 *
 * All four symbols of the pair are driven, both sides through asm() labels.
 * C++ has no syntax for running a constructor over storage that already
 * exists, and `OBJ = V90Modem(...)` would build a temporary over uninitialised
 * stack and copy it in, throwing the seed away (findings F223, F224).  The blob
 * holds C1 and C2 as two copies at two addresses and our compiler emits one
 * body under both names, so calling only one leaves half the pair untested.
 *
 * ===========================================================================
 * WHY THE TWO SIDES GET SEPARATE STORAGE AND BOTH GRAPHS LIVE AT ONCE
 * ===========================================================================
 *
 * The obvious fixture runs OURS over one buffer, tears the heap down, and runs
 * the BLOB'S over the same buffer so that every heap address comes back
 * identical and nothing has to be excluded.  IT DOES NOT WORK HERE, and the
 * reason is measured rather than feared -- `run_congruence` below is the
 * measurement and it runs on every invocation of this file.
 *
 * `V90Demodulator`'s constructor allocates five bare blocks whose sizes are
 * `levels` times 4, 12, 4, 8 and 8 (t_v90demctor.cpp's slot table), so two
 * PAIRS of them are the same size, and its destructor frees them in FORWARD
 * order.  glibc's tcache is LIFO per size class, so the second construction
 * gets the two same-sized chunks back the other way round: the addresses are
 * the same SET, and their assignment to fields is permuted.  A comparison that
 * expects address equality then reports two pointer words as defects when
 * nothing is wrong.
 *
 * WHAT `run_congruence` ACTUALLY MEASURES, on the analog arm at 8 levels: the
 * five slots that arm fills all come back at the SAME address, and 9 of the 60
 * live regions do not come back at all -- they are chunks the second
 * construction reached in a different order and coalesced differently.  So the
 * naive fixture would have survived a comparison of the six pointers and
 * failed inside the demodulator's own allocations, which is the worst of the
 * three possible outcomes: it would have looked like a defect.  The
 * measurement is printed on every run rather than asserted, because what it
 * establishes is that the translation is NECESSARY and not that the allocator
 * is wrong.
 *
 * So this file uses t_v90demctor.cpp's shape instead, which is the one the
 * tree already has passing for a constructor with nested allocations: two
 * object slots seeded IDENTICALLY, both sides constructed before either is
 * destroyed, and every pointer-valued word canonicalised by `translate` before
 * the comparison -- into the object it becomes a tag plus the OFFSET, into one
 * of the six slots a tag carrying WHICH slot plus the offset, and into any
 * other live allocation a tag plus the offset with the block's identity
 * dropped.  Nothing about the fixture then depends on the allocator handing
 * back the same addresses, and `run_congruence` is kept anyway because the
 * measurement is the justification.
 *
 * ===========================================================================
 * THE SIX SLOTS, AND WHY THE COMPARISON MUST DESCEND INTO THEM
 * ===========================================================================
 *
 *   +0x0000  modulator     0x70   V90Modulator      side == 0 only
 *   +0x0004  demodulator   0x298  V90Demodulator    side == 1 only
 *   +0x0008  phase2Info    0x24   V90Phase2Info     freed with NO destructor
 *   +0x000c  jd            0x90   V90Jd
 *   +0x0010  jd92          0xdc   V92Jd
 *   +0x49b4  parameters    0x558  V90Parameters
 *
 * THE CONSTRUCTOR HANDS `&this->mappingParams` (+0x18) AND
 * `&this->mappingParamsAlt` (+0x668) TO BOTH `V90Modulator` AND
 * `V90Demodulator` AS ARGUMENTS 6 AND 7 -- two pointers of one type in
 * adjacent slots, which is exactly the shape findings F1301 and F1307 are about,
 * and NEITHER IS STORED IN THIS OBJECT.  They are stored inside the 0x70 or
 * 0x298 block the constructor just allocated, so a swap of the two is visible
 * ONLY if the comparison reads the CONTENTS of that block.  It does: every one
 * of the six slots is compared over its whole length, translated, and the two
 * addresses land at 0x40000000+0x18 and 0x40000000+0x668, which are different
 * numbers.  The same argument covers `&this->additionalCPinfo` (+0xcb8),
 * `&this->cp` (+0xdf4) and `&this->mp` (+0xcd0), which are arguments 8, 9 and
 * 10 and are likewise stored only in the callee.
 *
 * A comparison of the six POINTERS alone would prove none of that, and a
 * comparison of the V90Modem object alone would prove none of it either --
 * `nofSymbols`, `compMode` and the codec type reach no field of this class at
 * all and live only inside the modulator or the demodulator.
 *
 * WHAT IS NOT COMPARED, stated as a bound: the blocks the SIX slots' own
 * constructors allocate -- the demodulator's thirteen, the parameter block's
 * none -- have no identity that survives across two independent allocation
 * sequences, so `translate` maps a pointer into one of them to a tag plus its
 * offset and the block's contents are not walked.  Those are the callees'
 * claims and they have their own files: t_v90demctor.cpp, t_v90modchain.cpp,
 * t_v90jd.cpp, t_v90p2info.cpp and t_v90params.cpp.
 *
 * ONE ARGUMENT IS BEYOND THAT BOUND AND IS NAMED RATHER THAN LEFT IMPLICIT.
 * `compMode`, the constructor's fifth argument, is forwarded to
 * `V90Demodulator` as argument 14 and reaches nothing but the EQUALISER's
 * eleventh argument -- and the equaliser is one of the demodulator's own
 * thirteen blocks, one level below the six compared here.  It is swept {0, 1}
 * because a value that stopped being forwarded at all would show, but a
 * mutation that forwarded the WRONG value would not, and
 * test/mutations/v90modemctor.json says so instead of pretending otherwise.
 * t_v90demctor.cpp makes that claim, against the equaliser's own contents.
 *
 * ===========================================================================
 * THE THREE ARMS OF THE SWITCH, AND WHY {0, 1, 2} IS THE WHOLE SWEEP
 * ===========================================================================
 *
 * `V90ModemSide` is an opaque `enum : unsigned int` (V90Modem.h) with no
 * enumerators in the mangling, so there is no list of legal values to sweep.
 * What there is instead is the object's own branching, and it is exhaustive
 * over three classes:
 *
 *   the constructor  `switch (side)` with `case 0`, `case 1` and a default
 *                    that writes NEITHER pointer
 *   the destructor   `cmpl $0x1,0x49bc(%esi); jbe` -- UNSIGNED, so `<= 1`
 *                    against `> 1`
 *
 * 0 takes the first arm of both, 1 the second arm of both, and 2 the default
 * of the one and the `> 1` of the other.  A fourth value is another member of
 * a class already covered and adds nothing; that is the argument for stopping,
 * and it is the same one t_v92modem-shaped files make.
 *
 * `V90ComputationalMode` is FORWARDED -- the constructor passes it to
 * `V90Demodulator` as argument 14 and reads no bit of it -- so {0, 1} proves
 * the forwarding and nothing more is available to prove.
 *
 * ===========================================================================
 * THE SEED IS NEVER ZERO AND THERE IS A GUARD PAST THE END
 * ===========================================================================
 *
 * Both slots get the SAME varied pseudorandom bytes before every trial and
 * neither is ever zeroed: a zero-filled object lets a clear that missed a
 * field pass, because the field it failed to clear was already zero, and makes
 * "did anything happen" unanswerable.  Each slot is 64 bytes longer than the
 * object and that tail is compared on both sides AND against the seed, so a
 * store one byte past the end fails and so does a store 60 bytes past it.
 *
 * THE SEED IS ALSO WHAT TESTS THE THIRD ARM.  On a side of 2 the constructor
 * writes neither `modulator` nor `demodulator`, so what is at +0x00 and +0x04
 * afterwards is what the seed left there -- and because both sides were seeded
 * identically, the two agree only if BOTH left it alone.  The teardown for
 * that arm nulls the two words before calling the destructor, because the seed
 * is not a pointer and `~V90Modulator` would dereference it; that is a
 * property of the fixture and it is written here rather than left to be
 * rediscovered.
 *
 * ===========================================================================
 * THE MODEM PARAMETER BLOCK IS ZEROED AND NOT SEEDED
 * ===========================================================================
 *
 * `V90Parameters::V90Parameters` is `initSession(); modemParams = mp; init();`
 * and `init()` is `setToDefault(); loadModemParamsData();`, so the whole
 * parameter block -- which then decides the demodulator's allocation lengths
 * and feeds its x87 arithmetic -- is derived from this 136-byte block.  A
 * seeded one gives a signalling NaN about one word in 250 and an allocation
 * length of two billion rather more often than that; t_v90demctor.cpp zeroes
 * it for the same reason and this file follows.  The fields the sweep VARIES
 * are set on top of the zero and each says what it is for.
 *
 * Both sides are handed the SAME block, which is what makes the pointer at
 * `V90Parameters+0x000` compare equal, so the block is saved before our side
 * runs, saved again after it, restored, and compared against the second save
 * once the blob's side has run.  Without that a disagreement about what was
 * written into it would be overwritten by the second writer and invisible.
 */

#include <string.h>
#include <malloc.h>

#include "harness.h"

#include "dsplib/V90Modem.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"

/*
 * `run_reset` needs the three complete types V90Modem.h only forward-declares:
 * the descriptor, so that `dilb` can be `sizeof(tagV90DILdescriptor)` wide and
 * its fields asserted by name, and the two side objects, so that the arm each
 * `reset` took is a claim about a named field rather than about an offset.
 */
#include "dsplib/V90DilDescriptorSettings.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Modulator.h"

extern "C" {
/*
 * The two enum arguments are declared as their underlying types.  Both are
 * opaque enums with a fixed base (V90Modem.h), so the parameter passing is
 * identical and the driver avoids a cast at every call site.
 */
void modem_ctor1(void *self, unsigned int side, void *modemParams, void *dil,
		 unsigned int nofSymbols, int compMode, unsigned int flag)
	asm("_ZN8V90ModemC1E12V90ModemSideP19_tagModemParametersP19tagV90DIL"
	    "descriptorj20V90ComputationalModej");
void modem_ctor2(void *self, unsigned int side, void *modemParams, void *dil,
		 unsigned int nofSymbols, int compMode, unsigned int flag)
	asm("_ZN8V90ModemC2E12V90ModemSideP19_tagModemParametersP19tagV90DIL"
	    "descriptorj20V90ComputationalModej");
void ref_modem_ctor1(void *self, unsigned int side, void *modemParams,
		     void *dil, unsigned int nofSymbols, int compMode,
		     unsigned int flag)
	asm("ref__ZN8V90ModemC1E12V90ModemSideP19_tagModemParametersP19tagV90"
	    "DILdescriptorj20V90ComputationalModej");
void ref_modem_ctor2(void *self, unsigned int side, void *modemParams,
		     void *dil, unsigned int nofSymbols, int compMode,
		     unsigned int flag)
	asm("ref__ZN8V90ModemC2E12V90ModemSideP19_tagModemParametersP19tagV90"
	    "DILdescriptorj20V90ComputationalModej");

void modem_dtor1(void *self) asm("_ZN8V90ModemD1Ev");
void modem_dtor2(void *self) asm("_ZN8V90ModemD2Ev");
void ref_modem_dtor1(void *self) asm("ref__ZN8V90ModemD1Ev");
void ref_modem_dtor2(void *self) asm("ref__ZN8V90ModemD2Ev");

/* `V90Modem::reset`, .text+0x199a0; see run_reset below. */
void ref_modem_reset(void *self, unsigned int qcFlag)
	asm("ref__ZN8V90Modem5resetEj");

/*
 * The five sub-object destructors, used ONLY by the null sweep's teardown:
 * taking a slot away means destroying what is in it properly and then nulling
 * the field, because every one of these reaches further -- `~V90Demodulator`
 * frees thirteen blocks of its own.  Planting a raw block or freeing without
 * the destructor leaks or crashes, which is what t_v90rxctor.cpp learned.
 * `phase2Info` has no destructor and is not in this list: the object frees it
 * with a bare `sysdep_free`, which is the reading V90Phase2Info.h records.
 */
void sub_mod_dtor(void *self) asm("_ZN12V90ModulatorD1Ev");
void sub_dem_dtor(void *self) asm("_ZN14V90DemodulatorD1Ev");
void sub_jd_dtor(void *self) asm("_ZN5V90JdD1Ev");
void sub_jd92_dtor(void *self) asm("_ZN5V92JdD1Ev");
void sub_parm_dtor(void *self) asm("_ZN13V90ParametersD1Ev");
void ref_sub_mod_dtor(void *self) asm("ref__ZN12V90ModulatorD1Ev");
void ref_sub_dem_dtor(void *self) asm("ref__ZN14V90DemodulatorD1Ev");
void ref_sub_jd_dtor(void *self) asm("ref__ZN5V90JdD1Ev");
void ref_sub_jd92_dtor(void *self) asm("ref__ZN5V92JdD1Ev");
void ref_sub_parm_dtor(void *self) asm("ref__ZN13V90ParametersD1Ev");

extern unsigned int ref_dsplibs_debug_level;

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

typedef void (*ctorfn)(void *, unsigned int, void *, void *, unsigned int, int,
		       unsigned int);
typedef void (*dtorfn)(void *);

/* ================================================================ the fixture */

/*
 * `sizeof(V90Modem)` is 0x49c0 and the assertion is here as well as in
 * src/pump/v90/VpcmFloModem.cpp because this file's storage is a byte array:
 * a class that shrank would leave the tail of the slot inside the guard and
 * the guard check would then be testing the object.
 */
#define MODEM_SIZE	0x49c0
#define GUARD		64
#define MODEM_SLOT	(MODEM_SIZE + GUARD)

typedef char v90modem_is_0x49c0[(sizeof(V90Modem) == MODEM_SIZE) ? 1 : -1];

/*
 * The DIL descriptor.  THE CONSTRUCTOR ONLY STORES THE POINTER AND `reset`
 * WRITES THROUGH IT, which is why this is a whole descriptor plus a guard and
 * not the 96 bytes it was while only the lifecycle pair ran here.
 * `setDilDescriptor` fills `dilCode` to 144 entries, whose last byte is
 * +0x1a2, so 96 would have been a 323-byte scribble past the block -- made
 * identically by both sides, so it would have compared equal and passed.
 * The 64 bytes past the object are compared against the seed on every trial.
 */
#define DIL_GUARD	64
#define DIL_BYTES	((int)sizeof(tagV90DILdescriptor) + DIL_GUARD)

#define MP_SLOT		(sizeof(struct _tagModemParameters) + 64)

static unsigned char mobj[2][MODEM_SLOT] __attribute__((aligned(8)));
static unsigned char sown[MODEM_SLOT];		/* the seed, for the guard  */
static unsigned char mpb[MP_SLOT] __attribute__((aligned(8)));
static unsigned char dilb[DIL_BYTES] __attribute__((aligned(8)));

#define MPARAMS	((struct _tagModemParameters *)mpb)

/*
 * The six slots, in the DESTRUCTOR's order, which is also the order the
 * constructor allocates them in except that the modulator and the demodulator
 * come last there.  `size` is the constructor's own `movl $N,(%esp)`
 * immediately before the sub-constructor -- finding F1246, the allocation in
 * front of a constructor call IS the original compiler's `sizeof`.
 */
struct slot {
	unsigned	off;
	const char	*name;
	unsigned	size;
	int		bare;		/* freed without a destructor call */
};

static const struct slot slot_v[] = {
	{ 0x0000, "V90Modulator",	0x070, 0 },	/* 0x1965a */
	{ 0x0004, "V90Demodulator",	0x298, 0 },	/* 0x196a6 */
	{ 0x0008, "V90Phase2Info",	0x024, 1 },	/* 0x19578 */
	{ 0x000c, "V90Jd",		0x090, 0 },	/* 0x1959b */
	{ 0x0010, "V92Jd",		0x0dc, 0 },	/* 0x195c0 */
	{ 0x49b4, "V90Parameters",	0x558, 0 }	/* 0x19551 */
};

#define NSLOT	((int)(sizeof(slot_v) / sizeof(slot_v[0])))
#define SLOT_MOD	0
#define SLOT_DEM	1
#define MAXSLOTSZ	0x558

/* ------------------------------------------------------------------ seeding */

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	/* `| 1` so no seeded byte is ever zero; finding F230. */
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill(void *p, size_t n)
{
	unsigned char *q = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextb();
}

/*
 * One trial's inputs.  `codec` is `modemParams->codecType` at +0x54, which the
 * constructor loads and passes on as `V90Demodulator`'s ELEVENTH argument; it
 * is varied so that a reconstruction reading a neighbouring word of the block
 * fails.
 */
struct trial_args {
	unsigned int	side;
	unsigned int	nofSymbols;
	int		compMode;
	unsigned int	flag;
	int		codec;
};

static const struct trial_args trial_v[] = {
	{ 0,  8, 0, 0x00000000u, 0 },
	{ 0,  8, 1, 0x00000001u, 4 },
	{ 0, 12, 0, 0x5a5a5a5au, 2 },
	{ 0,  4, 1, 0xffffffffu, 1 },
	{ 1,  8, 0, 0x00000000u, 0 },
	{ 1,  8, 1, 0x00000001u, 4 },
	{ 1, 12, 0, 0x5a5a5a5au, 2 },
	{ 1,  4, 1, 0xffffffffu, 1 },
	{ 1, 16, 0, 0x0000000fu, 3 },
	{ 2,  8, 0, 0x00000000u, 0 },
	{ 2,  8, 1, 0x12345678u, 4 },
	{ 2, 12, 1, 0xffffffffu, 2 },
	/* Three more of the two live arms, at other seeds. */
	{ 0,  6, 1, 0x00ff00ffu, 3 },
	{ 1,  6, 0, 0x00ff00ffu, 3 },
	{ 0, 16, 0, 0x0000000fu, 0 }
};

#define NTRIAL	((int)(sizeof(trial_v) / sizeof(trial_v[0])))

static void
seed_all(long trial, const struct trial_args *t)
{
	lfsr = 0x4d1bu + 0x9e37u * (unsigned)trial;

	fill(sown, MODEM_SLOT);
	memcpy(mobj[0], sown, MODEM_SLOT);
	memcpy(mobj[1], sown, MODEM_SLOT);
	fill(dilb, sizeof dilb);

	/* See the file comment: zeroed, then the fields the sweep varies. */
	memset(mpb, 0, sizeof mpb);
	MPARAMS->codecType = t->codec;
}

/* ------------------------------------------- the shared block, round each side */

static unsigned char mp_pre[MP_SLOT], mp_ours[MP_SLOT];
static unsigned char dil_pre[DIL_BYTES], dil_ours[DIL_BYTES];

static void
shared_save(unsigned char *m, unsigned char *d)
{
	memcpy(m, mpb, MP_SLOT);
	memcpy(d, dilb, DIL_BYTES);
}

static void
shared_restore(const unsigned char *m, const unsigned char *d)
{
	memcpy(mpb, m, MP_SLOT);
	memcpy(dilb, d, DIL_BYTES);
}

/* ================================================ comparing across the sides */

#define MAXLIVE	256

struct ptrmap {
	void		*slot[2][NSLOT];
	unsigned long	slotend[2][NSLOT];
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

static void *
slot_ptr(int side, int k)
{
	void *p;

	memcpy(&p, mobj[side] + slot_v[k].off, sizeof p);
	return p;
}

static void
slot_set(int side, int k, void *p)
{
	memcpy(mobj[side] + slot_v[k].off, &p, sizeof p);
}

/* Take the live set and both sides' six slots.  Called once both sides have run. */
static void
ptrmap_take(void)
{
	int i, s, k;

	pmap.nlive = harness_alloc_live_set(pmap.live, MAXLIVE);
	if (pmap.nlive > MAXLIVE)
		pmap.nlive = MAXLIVE;
	for (i = 0; i < pmap.nlive; i++)
		pmap.liveend[i] = (unsigned long)pmap.live[i]
		    + malloc_usable_size(pmap.live[i]);

	for (s = 0; s < 2; s++)
		for (k = 0; k < NSLOT; k++) {
			void *p = slot_ptr(s, k);

			pmap.slot[s][k] = p;
			pmap.slotend[s][k] = 0;
			if (p != 0 && live_index(p) >= 0)
				pmap.slotend[s][k] = (unsigned long)p
				    + malloc_usable_size(p);
			else
				pmap.slot[s][k] = 0;
		}
}

/*
 * A pointer as both sides can agree about it.
 *
 *   - into that side's own V90Modem      ->  a tag plus the OFFSET, so an
 *                                            argument that should have been
 *                                            +0x18 and is +0x668 still fails
 *   - into the block that side put in
 *     slot k                             ->  a tag carrying k plus the offset
 *   - into any other live allocation     ->  a tag plus the offset; the two
 *                                            sides allocate separately and the
 *                                            block's identity is the callee's
 *                                            claim, not this file's
 *   - anything else                      ->  itself
 *
 * THE SLOT TAG IS WHAT MAKES A SUB-OBJECT SWAP VISIBLE.  With every heap
 * pointer mapped to one constant, handing `V90Modulator` the V.92 jitter
 * detector where the object hands it the V.90 one changes a word from one live
 * allocation to another, both of which would become the same number on both
 * sides and nothing would see it.
 */
static unsigned long
translate(unsigned long v, int side)
{
	unsigned long base = (unsigned long)mobj[side];
	int k;

	if (v == 0)
		return v;
	if (v >= base && v < base + MODEM_SIZE)
		return 0x40000000ul + (v - base);
	for (k = 0; k < NSLOT; k++) {
		unsigned long p = (unsigned long)pmap.slot[side][k];

		if (p != 0 && v >= p && v <= pmap.slotend[side][k])
			return 0x50000000ul + (unsigned long)k * 0x00100000ul
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
 * `translate` cannot tell a pointer from an integer, and these objects are
 * full of integers: lengths, sample rates, float bit patterns.  When one of
 * them happens to fall inside a live allocation -- and the heap base is
 * randomised, so it does on some runs and not others -- it is tagged on the
 * side whose graph it landed in and left alone on the other, and the two then
 * differ over a word neither implementation ever wrote.
 *
 * MEASURED IN THIS BATCH, not feared: t_vpcmdp.c hit it on the constant
 * 0x09600000 at the V.34 object's +0x434, 15 runs in 400, and t_vpcmctor.cpp
 * at 1 run in 150.  This file measured 0 in 150 without the guard and carries
 * it anyway, because "did not happen to fire in 150 runs" is not the same
 * claim as "cannot fire".
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
 * Two words inside the 0x298 demodulator block cannot be made to agree by any
 * fixture, because each side installs the address of ITS OWN copy of a static
 * object and both copies are linked into this binary at two addresses:
 *
 *   +0x06c  `preFilter.coefficients`, which `V90PreFilter::reset` points
 *           at `&preFilterCoefType1[0][0]`.  t_v90prefilter.cpp's claim.
 *   +0x094  the `V90Resampler` vptr, which is that side's own vtable.  A
 *           vtable cannot be content-checked -- ours holds our member
 *           addresses and the blob's holds the blob's -- so this one is
 *           excluded on the argument and not on a measurement, exactly as
 *           t_v90demctor.cpp:256-263 excludes it.
 *
 * Both offsets are t_v90demctor.cpp's, which owns that class; they are named
 * here rather than discovered so that a THIRD such word appearing would be a
 * failure and not a silently widened hole.  `run_alias_words` is the check
 * that each really is a pair of two DIFFERENT addresses rather than a defect
 * that happens to sit at a listed offset, and the sweep asserts that nothing
 * else in any slot disagrees.
 */
struct alias_word {
	int		k;		/* slot index                        */
	unsigned	off;		/* byte offset within the slot       */
	const char	*name;
};

static const struct alias_word aliasword[] = {
	{ SLOT_DEM, 0x06c, "preFilter.coefficients" },
	{ SLOT_DEM, 0x094, "the V90Resampler vptr" }
};

#define NALIAS	((int)(sizeof(aliasword) / sizeof(aliasword[0])))

static int
is_alias(int k, size_t off)
{
	int i;

	for (i = 0; i < NALIAS; i++)
		if (aliasword[i].k == k && (size_t)aliasword[i].off == off)
			return 1;
	return 0;
}

/*
 * AN EXCLUSION THAT NEVER FIRES IS A HOLE FOR NOTHING, so each of the two is
 * checked to be what it claims: on at least one trial the word must hold two
 * DIFFERENT addresses, and neither may be null or inside a live allocation --
 * a heap pointer would have been resolved by `translate` and would not need
 * excluding at all.  CLAUDE.md's rule that a tool must be shown to fire,
 * applied to a comparison's blind spot.
 */
static int alias_fired[NALIAS];
static int alias_wrong[NALIAS];

static void
alias_check(int trial)
{
	int i;

	(void)trial;
	for (i = 0; i < NALIAS; i++) {
		const unsigned char *pa, *pb;
		unsigned int wa, wb;
		int k = aliasword[i].k;

		pa = (const unsigned char *)pmap.slot[0][k];
		pb = (const unsigned char *)pmap.slot[1][k];
		if (pa == 0 || pb == 0)
			continue;
		memcpy(&wa, pa + aliasword[i].off, 4);
		memcpy(&wb, pb + aliasword[i].off, 4);
		if (wa != wb)
			alias_fired[i] = 1;
		if (wa == 0 || wb == 0 || live_index((const void *)
						     (unsigned long)wa) >= 0
		    || live_index((const void *)(unsigned long)wb) >= 0)
			alias_wrong[i] = 1;
	}
}

static unsigned char cmpa[MAXSLOTSZ + 64], cmpb[MAXSLOTSZ + 64];
static unsigned char tobj[2][MODEM_SIZE] __attribute__((aligned(8)));

/*
 * Compare one slot's CONTENTS, translated, with the alias words poisoned to a
 * constant in both copies rather than skipped -- poisoning keeps `diff_eq_obj`
 * reporting the first REAL difference first, which a skip that shortened the
 * buffer would not.
 */
static void
cmp_slot(int k, long trial)
{
	unsigned n = slot_v[k].size;
	void *pa = pmap.slot[0][k];
	void *pb = pmap.slot[1][k];
	int i;

	if (pa == 0 || pb == 0)
		return;

	translate_pair(cmpa, cmpb, (const unsigned char *)pa,
		       (const unsigned char *)pb, n);
	for (i = 0; i < NALIAS; i++)
		if (aliasword[i].k == k && aliasword[i].off + 4 <= n) {
			memset(cmpa + aliasword[i].off, 0x77, 4);
			memset(cmpb + aliasword[i].off, 0x77, 4);
		}
	diff_eq_obj_(__FILE__, __LINE__, slot_v[k].name, slot_v[k].name,
		     cmpa, cmpb, n, trial);
}

/*
 * Every differing word of a slot, printed.  Used by the discovery pass, which
 * is what says the exclusion list above is complete rather than convenient.
 */
static int
slot_diffs(int k, int report)
{
	unsigned n = slot_v[k].size;
	void *pa = pmap.slot[0][k];
	void *pb = pmap.slot[1][k];
	unsigned o;
	int bad = 0;

	if (pa == 0 || pb == 0)
		return 0;

	translate_pair(cmpa, cmpb, (const unsigned char *)pa,
		       (const unsigned char *)pb, n);
	for (o = 0; o + 4 <= n; o += 4) {
		unsigned int wa, wb, ra, rb;

		memcpy(&wa, cmpa + o, 4);
		memcpy(&wb, cmpb + o, 4);
		if (wa == wb || is_alias(k, o))
			continue;
		memcpy(&ra, (const unsigned char *)pa + o, 4);
		memcpy(&rb, (const unsigned char *)pb + o, 4);
		bad++;
		if (report)
			printf("      %s +0x%03x: ours %08x blob %08x"
			       " (translated %08x/%08x)\n", slot_v[k].name, o,
			       ra, rb, wa, wb);
	}
	return bad;
}

/* ============================================================ the two sides */

static void
run_side(int side, ctorfn ctor, const struct trial_args *t)
{
	ctor(mobj[side], t->side, mpb, dilb, t->nofSymbols, t->compMode,
	     t->flag);
}

/*
 * Destroy what a trial built.  For a side of 2 the constructor wrote NEITHER
 * pointer, so +0x00 and +0x04 still hold seed bytes; they are nulled first,
 * because the destructor calls `~V90Modulator` and `~V90Demodulator` THROUGH
 * them and a seeded word is not an object.  Nulling is the only teardown that
 * works and it costs nothing: the ctor comparison has already been made, and
 * the destructor's own null sweep drives both branches of both guards.
 */
static void
teardown(int side, dtorfn dtor, unsigned int modemSide)
{
	if (modemSide > 1) {
		slot_set(side, SLOT_MOD, 0);
		slot_set(side, SLOT_DEM, 0);
	}
	dtor(mobj[side]);
}

/* =========================================================== the constructor */

static int
run_ctor(const char *name, ctorfn our_ctor, ctorfn ref_ctor, dtorfn our_dtor,
	 dtorfn ref_dtor, int discover)
{
	unsigned char first[MODEM_SLOT];
	int trial, moved = 0, distinct = 0, aliasdiffs = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		const struct trial_args *t = &trial_v[trial];
		int a_allocs, b_allocs, k;
		unsigned a_bytes, b_bytes;

		seed_all(trial, t);
		harness_alloc_reset();
		shared_save(mp_pre, dil_pre);

		run_side(0, our_ctor, t);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;
		shared_save(mp_ours, dil_ours);
		shared_restore(mp_pre, dil_pre);

		run_side(1, ref_ctor, t);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		ptrmap_take();

		/*
		 * The two sides must agree on the SHAPE of what they allocated
		 * as well as on its contents: a reconstruction that allocated
		 * the right total in the wrong number of pieces would pass a
		 * content comparison of the six slots and still be a different
		 * program.
		 */
		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", a_bytes, b_bytes,
			    trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);

		/*
		 * WHICH SLOTS ARE FILLED IS THE SWITCH'S WHOLE OBSERVABLE
		 * EFFECT on this object, so it is asserted against the arm
		 * rather than merely compared between the sides: side 0 fills
		 * the modulator and nulls the demodulator, side 1 the reverse,
		 * and side 2 fills neither and leaves the seed.
		 */
		for (k = 0; k < NSLOT; k++) {
			int want = 1;
			int got;

			if (k == SLOT_MOD)
				want = t->side == 0;
			else if (k == SLOT_DEM)
				want = t->side == 1;
			if (t->side > 1 && (k == SLOT_MOD || k == SLOT_DEM)) {
				/* Neither written: the seed, not a block. */
				diff_eq_int("%s keeps the seed (side 2)",
					    memcmp(mobj[0] + slot_v[k].off,
						   sown + slot_v[k].off, 4) == 0
					    && memcmp(mobj[1] + slot_v[k].off,
						      sown + slot_v[k].off, 4)
					    == 0, 1, k);
				continue;
			}
			got = pmap.slot[0][k] != 0;
			diff_eq_int("%s is a live allocation", got, want, k);
			diff_eq_int("ref %s is a live allocation",
				    pmap.slot[1][k] != 0, want, k);
			if (want == 0)
				diff_eq_int("%s is null on both sides",
					    slot_ptr(0, k) == 0
					    && slot_ptr(1, k) == 0, 1, k);
		}

		/*
		 * The object whole, translated -- all 0x49c0 bytes of it,
		 * including the two embedded mapping-parameter blocks, the
		 * embedded V90MP and the embedded V90CP, whose own constructors
		 * run before the body and whose six buffers are live
		 * allocations that the translation resolves.
		 */
		translate_pair(tobj[0], tobj[1], mobj[0], mobj[1], MODEM_SIZE);
		diff_eq_obj_(__FILE__, __LINE__, "the V90Modem", "V90Modem",
			     tobj[0], tobj[1], MODEM_SIZE, trial);

		for (k = 0; k < NSLOT; k++)
			cmp_slot(k, trial);

		if (discover) {
			int n = 0;

			for (k = 0; k < NSLOT; k++)
				n += slot_diffs(k, trial < 2);
			aliasdiffs += n;
			alias_check(trial);
		}

		/* The guard, on both sides, against what the seed left. */
		diff_eq_int("nothing stored past the object (trial %ld)",
			    memcmp(mobj[0] + MODEM_SIZE, sown + MODEM_SIZE,
				   GUARD) == 0
			    && memcmp(mobj[1] + MODEM_SIZE, sown + MODEM_SIZE,
				      GUARD) == 0, 1, trial);

		/* The modem parameter block and the DIL block. */
		diff_eq_obj_(__FILE__, __LINE__, "the modem parameter block",
			     "_tagModemParameters", mpb, mp_ours, MP_SLOT,
			     trial);
		diff_eq_obj_(__FILE__, __LINE__, "the DIL descriptor", "bytes",
			     dilb, dil_ours, DIL_BYTES, trial);

		/* Anti-vacuity, over the whole slot rather than one field. */
		if (memcmp(sown, mobj[0], MODEM_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, mobj[0], MODEM_SLOT);
		else if (memcmp(first, mobj[0], MODEM_SLOT) != 0)
			distinct = 1;

		teardown(0, our_dtor, t->side);
		teardown(1, ref_dtor, t->side);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1, 0);
	if (discover) {
		int i;

		diff_eq_int("no word outside the two named alias words differs"
			    " (%ld)", aliasdiffs, 0, aliasdiffs);
		for (i = 0; i < NALIAS; i++) {
			printf("    excluded: %s at %s +0x%03x\n",
			       aliasword[i].name, slot_v[aliasword[i].k].name,
			       aliasword[i].off);
			diff_eq_int("%s really does hold two different"
				    " addresses", alias_fired[i], 1, i);
			diff_eq_int("%s is a static address on both sides,"
				    " not a heap one", alias_wrong[i], 0, i);
		}
	}

	return diff_end();
}

/* ============================================================ the destructor */

/*
 * The destructor over a constructed object: the six pointers are live, so this
 * is the path that really frees.  Both sides destroy their own graph and the
 * two are compared on the counters, on the object, and on the guard.
 */
static int
run_dtor_live(const char *name, ctorfn our_ctor, ctorfn ref_ctor,
	      dtorfn our_dtor, dtorfn ref_dtor)
{
	int trial, freed = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		const struct trial_args *t = &trial_v[trial];
		unsigned char before[MODEM_SLOT];
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;

		seed_all(trial, t);
		harness_alloc_reset();
		shared_save(mp_pre, dil_pre);
		run_side(0, our_ctor, t);
		shared_restore(mp_pre, dil_pre);
		run_side(1, ref_ctor, t);

		if (t->side > 1) {
			slot_set(0, SLOT_MOD, 0);
			slot_set(0, SLOT_DEM, 0);
			slot_set(1, SLOT_MOD, 0);
			slot_set(1, SLOT_DEM, 0);
		}
		memcpy(before, mobj[0], MODEM_SLOT);

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		a_bad = harness_alloc.bad_free;
		our_dtor(mobj[0]);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		a_bad = harness_alloc.bad_free - a_bad;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		b_bad = harness_alloc.bad_free;
		ref_dtor(mobj[1]);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		b_bad = harness_alloc.bad_free - b_bad;

		diff_eq_int("frees (trial %ld)", a_frees, b_frees, trial);
		diff_eq_int("sysdep_free(NULL) calls (trial %ld)", a_null,
			    b_null, trial);
		diff_eq_int("free of an unknown pointer (trial %ld)", a_bad,
			    b_bad, trial);
		diff_eq_int("and neither side freed a null (trial %ld)", a_null,
			    0, trial);
		diff_eq_int("and neither side freed a wild pointer (trial %ld)",
			    a_bad + b_bad, 0, trial);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);

		/*
		 * The destructor stores nothing -- not even a null over the
		 * pointer it just released.  Comparing OUR side against ITSELF
		 * before the call is what says so; the two sides being equal
		 * would not.
		 */
		diff_eq_int("the destructor stored nothing (trial %ld)",
			    memcmp(before, mobj[0], MODEM_SLOT) == 0, 1, trial);

		if (a_frees > 0)
			freed = 1;
	}

	diff_eq_int("the destructor released the sub-objects", freed, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * ALL 64 NULL/NON-NULL COMBINATIONS OF THE SIX GUARDED POINTERS
 * ===========================================================================
 *
 * A slot is TAKEN AWAY by destroying what is in it properly and then nulling
 * the field -- never by planting a raw block and never by a bare free.  Every
 * one of the five with a destructor reaches further than itself and
 * `~V90Demodulator` frees thirteen blocks; that is why the ladder below runs
 * the sub-destructor first, and it is what t_v90rxctor.cpp paid a crash to
 * learn.
 *
 * THE PARAMETER BLOCK IS TAKEN AWAY LAST WITHIN A SUBSET, because the
 * modulator and the demodulator hold a pointer to it and their destructors may
 * read it.  Removing it before them would hand a destructor a freed block and
 * the two sides would then be comparing two undefined behaviours.
 *
 * There is no WILD-pointer sweep here and its absence is a result rather than
 * an omission: five of the six fields are dereferenced by a destructor call
 * before they are freed, so a seeded word in one of them faults in the callee
 * on both sides and measures nothing about `~V90Modem`.  Only `phase2Info` is
 * freed with a bare `sysdep_free` and could take a wild pointer; it is swept
 * that way in `run_dtor_wild_ph2` below, where the harness's allocator counts
 * the free of an unknown pointer instead of performing it.
 */
static void
slot_kill(int side, int k, dtorfn sub[NSLOT])
{
	void *p = slot_ptr(side, k);

	if (p == 0)
		return;
	if (!slot_v[k].bare && sub[k] != 0)
		sub[k](p);
	sysdep_free(p);
	slot_set(side, k, 0);
}

static int
run_dtor_null(const char *name, ctorfn our_ctor, ctorfn ref_ctor,
	      dtorfn our_dtor, dtorfn ref_dtor, dtorfn our_sub[NSLOT],
	      dtorfn ref_sub[NSLOT])
{
	/*
	 * The trial that fills the most slots, so that every subset has
	 * something to take away: side 1 builds five of the six.
	 */
	static const struct trial_args t = { 1, 8, 0, 0x0000f00du, 3 };
	unsigned subset;
	int saw_null = 0, saw_live = 0;

	diff_begin(name);

	for (subset = 0; subset < 64u; subset++) {
		unsigned char before[MODEM_SLOT];
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad, k;

		seed_all((long)subset, &t);
		harness_alloc_reset();
		shared_save(mp_pre, dil_pre);
		run_side(0, our_ctor, &t);
		shared_restore(mp_pre, dil_pre);
		run_side(1, ref_ctor, &t);

		/* Forward, so the parameter block at index 5 goes last. */
		for (k = 0; k < NSLOT; k++)
			if ((subset >> k) & 1u) {
				slot_kill(0, k, our_sub);
				slot_kill(1, k, ref_sub);
			}
		for (k = 0; k < NSLOT; k++) {
			if (slot_ptr(0, k) == 0)
				saw_null = 1;
			else
				saw_live = 1;
		}
		memcpy(before, mobj[0], MODEM_SLOT);

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		a_bad = harness_alloc.bad_free;
		our_dtor(mobj[0]);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		a_bad = harness_alloc.bad_free - a_bad;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		b_bad = harness_alloc.bad_free;
		ref_dtor(mobj[1]);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		b_bad = harness_alloc.bad_free - b_bad;

		/*
		 * ONE FREE ATTEMPT PER SURVIVING POINTER AND NONE FOR A NULL
		 * ONE: an unguarded `sysdep_free(field)` would show up as
		 * free_null instead, which is why the harness counts the two
		 * apart.
		 */
		diff_eq_int("frees, subset 0x%02lx", a_frees, b_frees,
			    (long)subset);
		diff_eq_int("sysdep_free(NULL), subset 0x%02lx", a_null, b_null,
			    (long)subset);
		diff_eq_int("and it was never called, subset 0x%02lx",
			    a_null + b_null, 0, (long)subset);
		diff_eq_int("free of an unknown pointer, subset 0x%02lx", a_bad,
			    b_bad, (long)subset);
		diff_eq_int("nothing left allocated, subset 0x%02lx",
			    harness_alloc.live, 0, (long)subset);
		diff_eq_int("the destructor stored nothing, subset 0x%02lx",
			    memcmp(before, mobj[0], MODEM_SLOT) == 0, 1,
			    (long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%02lx",
			    memcmp(mobj[0] + MODEM_SIZE, sown + MODEM_SIZE,
				   GUARD) == 0, 1, (long)subset);
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a non-null pointer was tried", saw_live, 1, 0);

	return diff_end();
}

/*
 * `phase2Info` is the one field freed WITHOUT a destructor call -- 0x192ed
 * tests it and 0x193a0 goes straight to `sysdep_free` with no
 * `_ZN13V90Phase2InfoD` between -- so it is the one field a WILD pointer can be
 * planted in.  The harness swallows the free of a pointer it never handed out
 * and counts it, which is what makes the bare-free reading testable at all: a
 * reconstruction that called a destructor there would fault, and one that
 * guarded the free differently would show a different count.
 */
static int
run_dtor_wild_ph2(const char *name, ctorfn our_ctor, ctorfn ref_ctor,
		  dtorfn our_dtor, dtorfn ref_dtor, dtorfn our_sub[NSLOT],
		  dtorfn ref_sub[NSLOT])
{
	static const struct trial_args t = { 0, 8, 1, 0x00abcdefu, 2 };
	int trial, wild = 0;

	diff_begin(name);

	for (trial = 0; trial < 8; trial++) {
		int a_bad, b_bad, a_frees, b_frees;
		void *junk = (void *)(unsigned long)(0x00b0b000u + trial * 16u);

		seed_all((long)trial, &t);
		harness_alloc_reset();
		shared_save(mp_pre, dil_pre);
		run_side(0, our_ctor, &t);
		shared_restore(mp_pre, dil_pre);
		run_side(1, ref_ctor, &t);

		slot_kill(0, 2, our_sub);	/* release the real one first */
		slot_kill(1, 2, ref_sub);
		slot_set(0, 2, junk);
		slot_set(1, 2, junk);

		a_bad = harness_alloc.bad_free;
		a_frees = harness_alloc.frees;
		our_dtor(mobj[0]);
		a_bad = harness_alloc.bad_free - a_bad;
		a_frees = harness_alloc.frees - a_frees;

		b_bad = harness_alloc.bad_free;
		b_frees = harness_alloc.frees;
		ref_dtor(mobj[1]);
		b_bad = harness_alloc.bad_free - b_bad;
		b_frees = harness_alloc.frees - b_frees;

		diff_eq_int("the wild phase2Info was passed to free (%ld)",
			    a_bad, b_bad, trial);
		diff_eq_int("and exactly once (%ld)", a_bad, 1, trial);
		diff_eq_int("the real frees (%ld)", a_frees, b_frees, trial);
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, trial);
		if (a_bad > 0)
			wild = 1;
	}

	diff_eq_int("a wild pointer really reached sysdep_free", wild, 1, 0);

	return diff_end();
}

/* ============================================================ diagnostics */

/*
 * THE FOUR PRINT SITES, WHICH NO COMPARISON OF MEMORY CAN SEE.
 *
 * `main` runs at debug level 0 and `DSPLIB_DEBUG_ON()` is `> 1`, so every
 * `dsplibs_debug_printf` in this pair is dark in all of the sweeps above.  One
 * of the four is not merely unobserved but UNOBSERVABLE any other way: the
 * constructor's opening line chooses between the strings "Analog" and
 * "Digital" on the ARGUMENT -- 0x19514 tests `%ebx`, which is still
 * `0x54(%esp)` -- where the switch below it reloads `0x49bc(%esi)` and tests
 * the MEMBER.  The two readings agree on every value the object stores, so a
 * reconstruction that tested the member there would pass every byte comparison
 * in this file; only the transcript separates them, and only on the arm where
 * the two differ, which is why the sweep pokes `side` between the constructor
 * and the destructor.
 *
 * `printTitle` is called unconditionally and is gated inside, so its lines
 * appear here too; that is a second claim this pass carries -- V90Modem.cpp is
 * a separate translation unit and a dropped call would leave a shorter
 * transcript.
 */
static int
run_transcripts(const char *name, ctorfn our_ctor, ctorfn ref_ctor,
		dtorfn our_dtor, dtorfn ref_dtor)
{
	static const unsigned int poke_v[] = { 0u, 1u, 2u, 0xffffffffu };
	int trial, printed = 0;
	long tag = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		const struct trial_args *t = &trial_v[trial];
		unsigned pi;

		for (pi = 0; pi < sizeof poke_v / sizeof poke_v[0]; pi++) {
			seed_all(trial, t);
			harness_alloc_reset();

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			dsplibs_debug_level = 2;
			ref_dsplibs_debug_level = 2;

			shared_save(mp_pre, dil_pre);
			run_side(0, our_ctor, t);
			shared_restore(mp_pre, dil_pre);
			run_side(1, ref_ctor, t);

			/*
			 * The member overwritten between the two calls, so that
			 * the destructor's `> 1` test and the constructor's
			 * argument test are driven independently of each other.
			 */
			memcpy(mobj[0] + 0x49bc, &poke_v[pi], 4);
			memcpy(mobj[1] + 0x49bc, &poke_v[pi], 4);

			teardown(0, our_dtor, t->side > 1 ? 2u : poke_v[pi]);
			teardown(1, ref_dtor, t->side > 1 ? 2u : poke_v[pi]);

			dsplibs_debug_level = 0;
			ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;

			diff_eq_int("the transcripts agree (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			if (dsplib_debug_capture_lines(1) > 0)
				printed = 1;
			tag++;
		}
	}

	/* Two empty transcripts compare equal; this is what says they are not. */
	diff_eq_int("and some case actually printed something", printed, 1, 0);

	return diff_end();
}


/*
 * ===========================================================================
 * `V90Modem::reset` -- .text+0x199a0, 221 bytes
 * ===========================================================================
 *
 * The lifecycle pair above builds the graph this needs and nothing else in
 * the tree does, which is why `reset` is tested here rather than in a fixture
 * of its own: the analogue arm calls `setDilDescriptor` through the
 * descriptor the CONSTRUCTOR was handed and `V90Demodulator::reset` on the
 * 0x298 block the constructor allocated, so a hand-built pair of slots would
 * have to stand up both.
 *
 * ===========================================================================
 * THE ONE CORNER THAT PARTS THE TWO READINGS
 * ===========================================================================
 *
 * `PROBING_MODE` non-zero masks quick connect, and the object masks the
 * VARIABLE rather than the descriptor select alone -- `xor %esi,%esi` at
 * 0x19a6d, where `%esi` is `qcFlag`, before rejoining the common path.  So a
 * reconstruction that chose `DIL_TYPE_ADI` on the probe path but still passed
 * the CALLER's flag to `V90Demodulator::reset` agrees with the object on
 * every other corner and differs only where `PROBING_MODE` and `qcFlag` are
 * BOTH non-zero.  That corner is driven, and `sawMasked` asserts it was
 * reached; the pair of assertions on `quickConnect` beside it is what makes
 * the difference visible rather than merely present.
 *
 * `qcFlag` IS SWEPT OVER MORE THAN {0, 1}.  `V90Demodulator::reset` stores
 * its argument WHOLE into `quickConnect` and into the equaliser's, while the
 * descriptor select is a truth test -- so a value of 2 or 0xffffffff tells a
 * forwarded flag from a re-derived `dilType != DIL_TYPE_ADI`, which agree
 * over {0, 1} and part company everywhere else.
 *
 * ===========================================================================
 * THE DESCRIPTOR IS SHARED, SO IT IS SNAPSHOT-RUN-RESTORE-RUN
 * ===========================================================================
 *
 * Both sides were constructed against the SAME `dilb`, which is what makes
 * `V90Modem::dil` compare equal (finding F1105).  `setDilDescriptor` WRITES
 * through it, so without restoring the block between the two calls the second
 * writer would hide the first and a reconstruction that wrote nothing at all
 * would pass.  Finding F805's shape, and `run_ctor` above already uses it for
 * the modem parameter block.
 *
 * `dilb` HAD TO GROW.  It was 96 bytes, which was "comfortably more than the
 * 0x2c the packers reach" while the constructor only STORED the pointer;
 * `setDilDescriptor` fills `dilCode` to 144, whose last byte is +0x1a2, so 96
 * would have been a 323-byte scribble into whatever the linker parked next --
 * identically on both sides, so it would have PASSED while measuring nothing.
 * It is now `sizeof(tagV90DILdescriptor)` plus a guard, and the guard is
 * compared against the seed on every trial.
 *
 * ===========================================================================
 * THE THREE ARMS
 * ===========================================================================
 *
 *   side 0  `modulator->reset()`, and NOTHING touches the descriptor -- which
 *           is asserted against the seed rather than left to the comparison,
 *           because both sides leaving it alone compares equal too.
 *   side 1  the analogue arm above.
 *   side 2  `printTitle()` and a gated "Illegal modemSide", and the object is
 *           compared against a snapshot of ITSELF taken before the call.
 *
 * The debug sweep is {0, 1, 2} because every gate here is `cmpl $0x1` and a
 * {0, 2} sweep cannot separate `> 1` from `> 0`.
 */

#define SLOT_PARM	5
#define PARM_PROBING_MODE	0x004
#define PARM_LINE_CONNECTION	0x00c
#define PARM_PRE_FILTER_GAIN	0x04c

/*
 * Two parameters held at 0 on both sides, for t_v90rundemod.cpp's reason:
 * `PRE_FILTER_GAIN` at -1 sends `V90PreFilter::selectFilter` into
 * `autoSelection`, which reads the Phase 2 record's `L2` through a pointer a
 * freshly constructed demodulator has not filled.  Nothing `reset` decides is
 * affected.
 */
static void
reset_prepare_params(int side, int probing)
{
	unsigned char *p = (unsigned char *)slot_ptr(side, SLOT_PARM);

	if (p == 0)
		return;
	*(int *)(p + PARM_PROBING_MODE) = probing;
	*(int *)(p + PARM_LINE_CONNECTION) = 0;
	*(int *)(p + PARM_PRE_FILTER_GAIN) = 0;
}

struct reset_trial {
	unsigned int	side;
	int		probing;
	unsigned int	qcFlag;
	unsigned int	level;
};

static const struct reset_trial reset_v[] = {
	/* The analogue arm: every corner of (PROBING_MODE, qcFlag). */
	{ 1, 0, 0u, 0 }, { 1, 0, 1u, 0 }, { 1, 0, 2u, 0 },
	{ 1, 0, 0xffffffffu, 0 },
	{ 1, 1, 0u, 0 }, { 1, 1, 1u, 0 }, { 1, 1, 2u, 0 },
	{ 1, 1, 0xffffffffu, 0 },
	/*
	 * `PROBING_MODE` with a zero LOW BYTE and a zero low half.  The object
	 * tests the whole word (`cmpl $0x0`); a byte or short test would take
	 * the other arm on these two and on nothing else.
	 */
	{ 1, 0x100, 1u, 0 }, { 1, 0x10000, 1u, 0 },
	{ 1, -1, 1u, 0 },

	/* The digital arm and the illegal one. */
	{ 0, 0, 0u, 0 }, { 0, 0, 1u, 0 }, { 0, 1, 1u, 0 },
	{ 2, 0, 0u, 0 }, { 2, 0, 1u, 0 }, { 2, 1, 1u, 0 },

	/* The same corners again at each of the three debug levels. */
	{ 1, 0, 1u, 1 }, { 1, 1, 1u, 1 }, { 0, 0, 1u, 1 }, { 2, 0, 1u, 1 },
	{ 1, 0, 1u, 2 }, { 1, 1, 1u, 2 }, { 1, 0, 0u, 2 }, { 1, 1, 0u, 2 },
	{ 0, 0, 1u, 2 }, { 2, 0, 1u, 2 }, { 1, 0, 2u, 2 }
};

#define NRESET	((int)(sizeof(reset_v) / sizeof(reset_v[0])))

static int
run_reset(ctorfn our_ctor, ctorfn ref_ctor, dtorfn our_dtor, dtorfn ref_dtor)
{
	static unsigned char before[MODEM_SLOT];
	static unsigned char dil_seed[DIL_BYTES];
	int trial, k;
	int sawMasked = 0, sawUnmasked = 0;
	int sawSide[3];
	int quiet = 0, loud = 0;
	int qcLenMasked = -1, qcLenUnmasked = -1;

	diff_begin("V90Modem::reset");

	sawSide[0] = sawSide[1] = sawSide[2] = 0;
	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < NRESET; trial++) {
		const struct reset_trial *r = &reset_v[trial];
		struct trial_args t;
		unsigned int qcSeen;
		int side;
		int allocs;

		t.side = r->side;
		t.nofSymbols = 8;
		t.compMode = (int)(trial & 1);
		t.flag = 0x5a5a0000u + (unsigned)trial;
		t.codec = trial % 5;

		seed_all(1000 + trial, &t);
		harness_alloc_reset();
		shared_save(mp_pre, dil_pre);
		run_side(0, our_ctor, &t);
		shared_restore(mp_pre, dil_pre);
		run_side(1, ref_ctor, &t);

		for (side = 0; side < 2; side++)
			reset_prepare_params(side, r->probing);

		/*
		 * THE DESCRIPTOR IS RESEEDED AFTER CONSTRUCTION and never
		 * zeroed: `setDilDescriptor` writes only as far as each count
		 * says, and over a zeroed block "not written" and "written
		 * zero" are the same bytes (finding F7602).
		 */
		{
			int i;

			for (i = 0; i < DIL_BYTES; i++) {
				unsigned char v = nextb();

				dilb[i] = v != 0 ? v : (unsigned char)0xa5;
			}
			memcpy(dil_seed, dilb, DIL_BYTES);
		}

		memcpy(before, mobj[1], MODEM_SLOT);

		dsplib_debug_capture_reset();
		dsplibs_debug_level = r->level;
		ref_dsplibs_debug_level = r->level;

		allocs = harness_alloc.allocs;

		((V90Modem *)mobj[0])->reset(r->qcFlag);
		memcpy(dil_ours, dilb, DIL_BYTES);
		memcpy(dilb, dil_seed, DIL_BYTES);

		ref_modem_reset(mobj[1], r->qcFlag);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("reset allocated nothing (%ld)",
			    harness_alloc.allocs - allocs, 0, trial);

		ptrmap_take();

		/* The object whole, translated, and every one of the six slots. */
		translate_pair(tobj[0], tobj[1], mobj[0], mobj[1], MODEM_SIZE);
		diff_eq_obj_(__FILE__, __LINE__, "after V90Modem::reset",
			     "V90Modem", tobj[0], tobj[1], MODEM_SIZE, trial);
		for (k = 0; k < NSLOT; k++)
			cmp_slot(k, trial);

		diff_eq_int("nothing stored past the object (trial %ld)",
			    memcmp(mobj[0] + MODEM_SIZE, sown + MODEM_SIZE,
				   GUARD) == 0
			    && memcmp(mobj[1] + MODEM_SIZE, sown + MODEM_SIZE,
				      GUARD) == 0, 1, trial);

		/* The descriptor, ours against the blob's over the same seed. */
		diff_eq_obj_(__FILE__, __LINE__, "after V90Modem::reset",
			     "the DIL descriptor", dil_ours, dilb, DIL_BYTES,
			     trial);
		diff_eq_obj_(__FILE__, __LINE__,
			     "nothing is stored past the descriptor",
			     "the DIL guard",
			     dilb + sizeof(tagV90DILdescriptor),
			     dil_seed + sizeof(tagV90DILdescriptor),
			     DIL_BYTES - sizeof(tagV90DILdescriptor), trial);

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		diff_eq_int("the transcript line counts agree (%ld)",
			    (long)dsplib_debug_capture_lines(0),
			    (long)dsplib_debug_capture_lines(1), trial);
		if (dsplib_debug_capture_lines(1) == 0)
			quiet++;
		else
			loud++;

		sawSide[r->side < 3 ? r->side : 2] = 1;

		/*
		 * THE ARMS, ASSERTED BY VALUE ON THE BLOB'S SIDE, so that each
		 * is a claim about the object and not about the two sides
		 * agreeing.
		 */
		if (r->side == 1) {
			V90Demodulator *d =
			    (V90Demodulator *)pmap.slot[1][SLOT_DEM];
			tagV90DILdescriptor *dd =
			    (tagV90DILdescriptor *)dilb;

			qcSeen = r->probing != 0 ? 0u : r->qcFlag;

			diff_eq_int("the demodulator was reset (%ld)",
				    (long)d->inPhase3, 0, trial);
			diff_eq_int("V90Demodulator::reset got the MASKED "
				    "flag (%ld)", (long)d->quickConnect,
				    (long)qcSeen, trial);
			diff_eq_int("the descriptor's dilCount (%ld)",
				    (long)dd->dilCount, 144, trial);
			diff_eq_int("the DilType the mask chose (%ld)",
				    (long)dd->seq1Length,
				    qcSeen != 0 ? 60 : 120, trial);

			if (r->probing != 0 && r->qcFlag != 0) {
				sawMasked = 1;
				qcLenMasked = dd->seq1Length;
			} else if (r->probing == 0 && r->qcFlag != 0) {
				sawUnmasked = 1;
				qcLenUnmasked = dd->seq1Length;
			}
		} else {
			/*
			 * NEITHER OTHER ARM TOUCHES THE DESCRIPTOR, and that is
			 * asserted against the SEED: both sides leaving it
			 * alone compares equal, so the two-sided comparison
			 * above cannot say it.
			 */
			diff_eq_obj_(__FILE__, __LINE__,
				     "the descriptor is untouched off the "
				     "analogue arm", "the DIL descriptor",
				     dilb, dil_seed, DIL_BYTES, trial);
		}

		if (r->side == 0) {
			V90Modulator *m =
			    (V90Modulator *)pmap.slot[1][SLOT_MOD];

			diff_eq_int("V90Modulator::reset cleared state (%ld)",
				    (long)m->state, 0, trial);
			diff_eq_int("...symbolCount (%ld)",
				    (long)m->symbolCount, 0, trial);
			diff_eq_int("...eventCode (%ld)", (long)m->eventCode,
				    0, trial);
		}

		if (r->side > 1)
			diff_eq_int("the illegal arm stored nothing (%ld)",
				    memcmp(before, mobj[1], MODEM_SLOT) == 0,
				    1, trial);

		teardown(0, our_dtor, t.side);
		teardown(1, ref_dtor, t.side);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	dsplib_debug_capture_on = 0;

	diff_eq_int("the probe-masked corner was reached", sawMasked, 1, 0);
	diff_eq_int("and the unmasked one", sawUnmasked, 1, 0);
	/*
	 * AND THE TWO DIFFER.  Without this the mask could be a no-op and
	 * every assertion above would still hold.
	 */
	diff_eq_int("the mask changes which descriptor is installed",
		    qcLenMasked == 120 && qcLenUnmasked == 60, 1, 0);
	diff_eq_int("side 0 was driven", sawSide[0], 1, 0);
	diff_eq_int("side 1 was driven", sawSide[1], 1, 0);
	diff_eq_int("side 2 was driven", sawSide[2], 1, 0);
	diff_eq_int("both arms of the debug gate were taken",
		    quiet > 0 && loud > 0, 1, 0);

	return diff_end();
}

/* ============================================================== congruence */

/*
 * THE MEASUREMENT THAT SETTLES THE FIXTURE'S SHAPE, and it is kept because the
 * shape is unusual and the reason is not obvious from the code.
 *
 * If the allocator handed the same addresses back after a balanced teardown,
 * this file could run both sides over ONE buffer and compare byte for byte
 * with no translation at all.  It runs OUR constructor, records the six slot
 * addresses, destroys, runs OUR constructor again over the same seed, and
 * compares the two sets of addresses.  The result is REPORTED rather than
 * asserted either way: what it establishes is that the translated comparison
 * is necessary, not that the allocator is wrong.
 */
static int
run_congruence(ctorfn our_ctor, dtorfn our_dtor)
{
	static const struct trial_args t = { 1, 8, 0, 0x11223344u, 3 };
	void *first[NSLOT], *second[NSLOT];
	int k, same = 0, differ = 0;
	int n1, n2;
	void *live1[MAXLIVE], *live2[MAXLIVE];

	diff_begin("congruence: does a balanced teardown give the addresses back");

	seed_all(0, &t);
	harness_alloc_reset();
	run_side(0, our_ctor, &t);
	for (k = 0; k < NSLOT; k++)
		first[k] = slot_ptr(0, k);
	n1 = harness_alloc_live_set(live1, MAXLIVE);
	teardown(0, our_dtor, t.side);

	seed_all(0, &t);
	run_side(0, our_ctor, &t);
	for (k = 0; k < NSLOT; k++)
		second[k] = slot_ptr(0, k);
	n2 = harness_alloc_live_set(live2, MAXLIVE);
	teardown(0, our_dtor, t.side);

	for (k = 0; k < NSLOT; k++) {
		if (first[k] == 0 && second[k] == 0)
			continue;
		if (first[k] == second[k])
			same++;
		else
			differ++;
	}
	printf("    the six slots: %d came back at the same address, %d did"
	       " not; %d live regions then, %d now\n", same, differ, n1, n2);
	if (n1 == n2 && n1 <= MAXLIVE) {
		int i, moved = 0;

		/*
		 * The live SET, not the assignment.  The interesting answer is
		 * that the same chunks come back and are handed to DIFFERENT
		 * fields, which is what a LIFO free list does with two blocks
		 * of one size freed in forward order.
		 */
		for (i = 0; i < n1; i++) {
			int j, found = 0;

			for (j = 0; j < n2; j++)
				if (live1[i] == live2[j])
					found = 1;
			if (!found)
				moved++;
		}
		printf("    and %d of the %d chunks were not in the second"
		       " live set at all\n", moved, n1);
	}
	diff_eq_int("the measurement ran", n1 > 0, 1, 0);
	diff_eq_int("nothing left allocated", harness_alloc.live, 0, 0);

	return diff_end();
}

/* ==================================================================== main */

int
main(void)
{
	dtorfn our_sub[NSLOT], ref_sub[NSLOT];
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	our_sub[0] = sub_mod_dtor;
	our_sub[1] = sub_dem_dtor;
	our_sub[2] = 0;			/* freed bare */
	our_sub[3] = sub_jd_dtor;
	our_sub[4] = sub_jd92_dtor;
	our_sub[5] = sub_parm_dtor;
	ref_sub[0] = ref_sub_mod_dtor;
	ref_sub[1] = ref_sub_dem_dtor;
	ref_sub[2] = 0;
	ref_sub[3] = ref_sub_jd_dtor;
	ref_sub[4] = ref_sub_jd92_dtor;
	ref_sub[5] = ref_sub_parm_dtor;

	rc |= run_congruence(modem_ctor1, modem_dtor1);

	rc |= run_ctor("V90Modem::V90Modem (C1)", modem_ctor1, ref_modem_ctor1,
		       modem_dtor1, ref_modem_dtor1, 1);
	rc |= run_ctor("V90Modem::V90Modem (C2)", modem_ctor2, ref_modem_ctor2,
		       modem_dtor2, ref_modem_dtor2, 0);

	rc |= run_reset(modem_ctor1, ref_modem_ctor1, modem_dtor1,
			ref_modem_dtor1);

	rc |= run_dtor_live("V90Modem::~V90Modem (D1)", modem_ctor1,
			    ref_modem_ctor1, modem_dtor1, ref_modem_dtor1);
	rc |= run_dtor_live("V90Modem::~V90Modem (D2)", modem_ctor2,
			    ref_modem_ctor2, modem_dtor2, ref_modem_dtor2);

	rc |= run_dtor_null("V90Modem::~V90Modem over 64 null combinations (D1)",
			    modem_ctor1, ref_modem_ctor1, modem_dtor1,
			    ref_modem_dtor1, our_sub, ref_sub);
	rc |= run_dtor_null("V90Modem::~V90Modem over 64 null combinations (D2)",
			    modem_ctor2, ref_modem_ctor2, modem_dtor2,
			    ref_modem_dtor2, our_sub, ref_sub);

	rc |= run_dtor_wild_ph2("V90Modem::~V90Modem over a wild phase2Info",
				modem_ctor1, ref_modem_ctor1, modem_dtor1,
				ref_modem_dtor1, our_sub, ref_sub);

	rc |= run_transcripts("V90Modem's diagnostics, both sides talking",
			      modem_ctor1, ref_modem_ctor1, modem_dtor1,
			      ref_modem_dtor1);

	return rc;
}
