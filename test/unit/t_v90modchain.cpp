/*
 * t_v90modchain.cpp -- differential test of the V.90 modulator construction
 * chain: V90Mapper, V90BitsToSymbol, V90Phase4Modulator and V90Modulator,
 * all four symbol variants (C1, C2, D1, D2) of each.
 *
 * C++ HAS NO SYNTAX FOR RUNNING A CONSTRUCTOR OVER STORAGE THAT ALREADY
 * EXISTS, so both sides are called by symbol through asm() labels over raw
 * static blocks, exactly as t_v90cp.cpp does.  `OBJ = V90Mapper(args)` would
 * build a temporary over uninitialised stack and copy the whole object, which
 * destroys the one property this fixture depends on.
 *
 * THE SEED IS THE TEST.  Both sides are filled with the SAME varied
 * pseudorandom bytes before every trial and are NEVER zeroed: a zero-filled
 * object would let a store that never happened pass, because the field was
 * already zero, and would make "did anything happen" unanswerable (findings
 * 223 and 224).  Every block carries 64 bytes of guard past the object, and
 * both sides' guards are compared against the seed as well as against each
 * other.
 *
 * THE HEAP POINTERS CANNOT AGREE AND ARE NOT ASKED TO.  `sysdep_malloc` in
 * the harness is plain `malloc`, so our side's allocations and the blob's are
 * at different addresses for ever.  Two mechanisms deal with that:
 *
 *   - A field KNOWN to hold an allocation is replaced by a constant in a copy
 *     of each side, and then asserted separately for being non-null and for
 *     being distinct from the class's other allocations.
 *   - A field inside a FOREIGN subobject -- the V90SpectralShaper embedded in
 *     V90Mapper, whose internals belong to t_v90spectral -- is found by
 *     value: every aligned word that holds a pointer the harness allocator
 *     handed out AND still owns, on BOTH sides, is replaced.  That needs no
 *     hand-maintained offset list and cannot go stale when that class grows a
 *     buffer.  A word that is not a pointer but coincides with one would have
 *     to coincide on both sides at the same offset to be masked.
 *
 * Everything else -- every borrowed pointer, every count, every flag, and
 * every byte the constructor did not touch -- is compared whole.
 *
 * THE ARGUMENTS ARE SHARED AND DISTINGUISHABLE.  Both sides are handed the
 * SAME instance of each pointed-to object, so a borrowed pointer compares by
 * value and an argument landing at the wrong offset fails instead of
 * agreeing.
 *
 * NO WILD POINTERS.  t_v90cp can hand a destructor six seeded garbage
 * pointers because it only frees them; these destructors DEREFERENCE what
 * they free -- `mapper->~V90Mapper()` before `sysdep_free(mapper)` -- so a
 * wild value is a segfault and not a test.  The null guards are driven
 * instead over a fully constructed object with one pointer at a time released
 * by hand and nulled, which exercises both arms of every guard, keeps the
 * live count returning to zero, and reads the difference off the harness
 * allocator's counters.
 *
 * WHY THE POST-DESTRUCTOR COMPARISON IS PER-SIDE.  After the call the
 * allocations are gone, so the mask that made the two sides comparable can no
 * longer be computed -- the pointers are no longer live.  The claim is made
 * the other way instead, and it is the stronger one: EACH side is compared
 * against ITSELF before the call, which says the destructor stored nothing at
 * all, where the two sides being equal would not.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/sysdep.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Mapper.h"
#include "dsplib/V90Modulator.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90Phase4Modulator.h"
#include "dsplib/V92Jd.h"

extern "C" {
void our_mapper_c1(void *, V90Parameters *)
	asm("_ZN9V90MapperC1EP13V90Parameters");
void our_mapper_c2(void *, V90Parameters *)
	asm("_ZN9V90MapperC2EP13V90Parameters");
void our_mapper_d1(void *) asm("_ZN9V90MapperD1Ev");
void our_mapper_d2(void *) asm("_ZN9V90MapperD2Ev");
void ref_mapper_c1(void *, V90Parameters *)
	asm("ref__ZN9V90MapperC1EP13V90Parameters");
void ref_mapper_c2(void *, V90Parameters *)
	asm("ref__ZN9V90MapperC2EP13V90Parameters");
void ref_mapper_d1(void *) asm("ref__ZN9V90MapperD1Ev");
void ref_mapper_d2(void *) asm("ref__ZN9V90MapperD2Ev");

void our_bts_c1(void *, unsigned int, V90Parameters *)
	asm("_ZN15V90BitsToSymbolC1EjP13V90Parameters");
void our_bts_c2(void *, unsigned int, V90Parameters *)
	asm("_ZN15V90BitsToSymbolC2EjP13V90Parameters");
void our_bts_d1(void *) asm("_ZN15V90BitsToSymbolD1Ev");
void our_bts_d2(void *) asm("_ZN15V90BitsToSymbolD2Ev");
void ref_bts_c1(void *, unsigned int, V90Parameters *)
	asm("ref__ZN15V90BitsToSymbolC1EjP13V90Parameters");
void ref_bts_c2(void *, unsigned int, V90Parameters *)
	asm("ref__ZN15V90BitsToSymbolC2EjP13V90Parameters");
void ref_bts_d1(void *) asm("ref__ZN15V90BitsToSymbolD1Ev");
void ref_bts_d2(void *) asm("ref__ZN15V90BitsToSymbolD2Ev");

#define P4M_MANGLE(v) \
	"_ZN18V90Phase4ModulatorC" #v "EP13V90ParametersjP15V90BitsToSymbol" \
	"P5V90MPP16V90MappingParamsS7_P5V90CPj"
#define MOD_MANGLE(v) \
	"_ZN12V90ModulatorC" #v "EjP13V90Phase2InfoP5V90JdP5V92Jd" \
	"P19tagV90DILdescriptorP16V90MappingParamsS9_P22tagV90AdditionalCPinfo" \
	"P5V90CPP5V90MPP13V90Parametersj"

void our_p4m_c1(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm(P4M_MANGLE(1));
void our_p4m_c2(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm(P4M_MANGLE(2));
void our_p4m_d1(void *) asm("_ZN18V90Phase4ModulatorD1Ev");
void our_p4m_d2(void *) asm("_ZN18V90Phase4ModulatorD2Ev");
void ref_p4m_c1(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm("ref_" P4M_MANGLE(1));
void ref_p4m_c2(void *, V90Parameters *, unsigned int, V90BitsToSymbol *,
		V90MP *, V90MappingParams *, V90MappingParams *, V90CP *,
		unsigned int) asm("ref_" P4M_MANGLE(2));
void ref_p4m_d1(void *) asm("ref__ZN18V90Phase4ModulatorD1Ev");
void ref_p4m_d2(void *) asm("ref__ZN18V90Phase4ModulatorD2Ev");

void our_mod_c1(void *, unsigned int, V90Phase2Info *, V90Jd *, V92Jd *,
		tagV90DILdescriptor *, V90MappingParams *, V90MappingParams *,
		tagV90AdditionalCPinfo *, V90CP *, V90MP *, V90Parameters *,
		unsigned int) asm(MOD_MANGLE(1));
void our_mod_c2(void *, unsigned int, V90Phase2Info *, V90Jd *, V92Jd *,
		tagV90DILdescriptor *, V90MappingParams *, V90MappingParams *,
		tagV90AdditionalCPinfo *, V90CP *, V90MP *, V90Parameters *,
		unsigned int) asm(MOD_MANGLE(2));
void our_mod_d1(void *) asm("_ZN12V90ModulatorD1Ev");
void our_mod_d2(void *) asm("_ZN12V90ModulatorD2Ev");
void ref_mod_c1(void *, unsigned int, V90Phase2Info *, V90Jd *, V92Jd *,
		tagV90DILdescriptor *, V90MappingParams *, V90MappingParams *,
		tagV90AdditionalCPinfo *, V90CP *, V90MP *, V90Parameters *,
		unsigned int) asm("ref_" MOD_MANGLE(1));
void ref_mod_c2(void *, unsigned int, V90Phase2Info *, V90Jd *, V92Jd *,
		tagV90DILdescriptor *, V90MappingParams *, V90MappingParams *,
		tagV90AdditionalCPinfo *, V90CP *, V90MP *, V90Parameters *,
		unsigned int) asm("ref_" MOD_MANGLE(2));
void ref_mod_d1(void *) asm("ref__ZN12V90ModulatorD1Ev");
void ref_mod_d2(void *) asm("ref__ZN12V90ModulatorD2Ev");

/* The phase 3 modulator is released by hand in the null-guard arms. */
void our_p3m_d1(void *) asm("_ZN18V90Phase3ModulatorD1Ev");
void ref_p3m_d1(void *) asm("ref__ZN18V90Phase3ModulatorD1Ev");
}

typedef void (*mapper_ctor)(void *, V90Parameters *);
typedef void (*bts_ctor)(void *, unsigned int, V90Parameters *);
typedef void (*p4m_ctor)(void *, V90Parameters *, unsigned int,
			 V90BitsToSymbol *, V90MP *, V90MappingParams *,
			 V90MappingParams *, V90CP *, unsigned int);
typedef void (*mod_ctor)(void *, unsigned int, V90Phase2Info *, V90Jd *,
			 V92Jd *, tagV90DILdescriptor *, V90MappingParams *,
			 V90MappingParams *, tagV90AdditionalCPinfo *,
			 V90CP *, V90MP *, V90Parameters *, unsigned int);
typedef void (*dtor)(void *);

#define GUARD		64
#define MAPPER_SIZE	0x704u
#define BTS_SIZE	0x24u
#define P4M_SIZE	0x2facu
#define MOD_SIZE	0x70u
#define P3M_SIZE	0x398u
#define MAPPER_SLOT	(MAPPER_SIZE + GUARD)
#define BTS_SLOT	(BTS_SIZE + GUARD)
#define P4M_SLOT	(P4M_SIZE + GUARD)
#define MOD_SLOT	(MOD_SIZE + GUARD)
#define NTRIAL		16

/* The embedded Scramblers: V90Phase4Modulator's and V90Modulator's. */
#define P4M_SCRAMBLER	0x0058u
#define MOD_SCRAMBLER	0x0044u
#define P3M_SCRAMBLER	0x0020u

/* The V90SpectralShaper embedded in V90Mapper: +0x68c, 0x6c bytes. */
#define SHAPER_LO	0x68cu
#define SHAPER_HI	0x6f8u

static unsigned char map_a[MAPPER_SLOT] __attribute__((aligned(8)));
static unsigned char map_b[MAPPER_SLOT] __attribute__((aligned(8)));
static unsigned char map_seed[MAPPER_SLOT];
static unsigned char bts_a[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_b[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_seed[BTS_SLOT];

/* Scratch for the canonicalised copies, big enough for the largest block. */
static unsigned char cmp_a[MAPPER_SLOT];
static unsigned char cmp_b[MAPPER_SLOT];

static unsigned char p4m_a[P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m_b[P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m_seed[P4M_SLOT];
static unsigned char mod_a[MOD_SLOT] __attribute__((aligned(8)));
static unsigned char mod_b[MOD_SLOT] __attribute__((aligned(8)));
static unsigned char mod_seed[MOD_SLOT];

/* Scratch, big enough for the largest block compared. */
static unsigned char big_a[P4M_SLOT];
static unsigned char big_b[P4M_SLOT];

/*
 * ONE SHARED INSTANCE OF EACH ARGUMENT, SEEDED PER TRIAL AND NEVER ZEROED.
 * Both sides are handed the same address, so a borrowed pointer compares by
 * value and a swap of two same-typed arguments -- which V90Modulator really
 * does perform on its two V90MappingParams -- fails instead of agreeing.
 * Nothing dereferences any of them, so the storage need only be distinct,
 * distinguishable and correctly sized.
 */
static unsigned char par_store[sizeof(V90Parameters)]
	__attribute__((aligned(8)));
static unsigned char mp_store[sizeof(V90MP)] __attribute__((aligned(8)));
static unsigned char cp_store[sizeof(V90CP)] __attribute__((aligned(8)));
static unsigned char mpsa_store[sizeof(V90MappingParams)]
	__attribute__((aligned(8)));
static unsigned char mpsb_store[sizeof(V90MappingParams)]
	__attribute__((aligned(8)));
static unsigned char p2_store[sizeof(V90Phase2Info)]
	__attribute__((aligned(8)));
static unsigned char jd_store[sizeof(V90Jd)] __attribute__((aligned(8)));
static unsigned char v92jd_store[sizeof(V92Jd)] __attribute__((aligned(8)));
static unsigned char dil_store[sizeof(tagV90DILdescriptor)]
	__attribute__((aligned(8)));
/* `tagV90AdditionalCPinfo` is named only by the mangling; nothing declares it. */
static unsigned char acp_store[64] __attribute__((aligned(8)));

/* The externally supplied converter, built once per trial and shared. */
static unsigned char ext_bts[BTS_SIZE] __attribute__((aligned(8)));

#define PARAMS		((V90Parameters *)(void *)par_store)
#define MP		((V90MP *)(void *)mp_store)
#define CP		((V90CP *)(void *)cp_store)
#define MPS_A		((V90MappingParams *)(void *)mpsa_store)
#define MPS_B		((V90MappingParams *)(void *)mpsb_store)
#define P2INFO		((V90Phase2Info *)(void *)p2_store)
#define JD		((V90Jd *)(void *)jd_store)
#define V92JD		((V92Jd *)(void *)v92jd_store)
#define DIL		((tagV90DILdescriptor *)(void *)dil_store)
#define ACP		((tagV90AdditionalCPinfo *)(void *)acp_store)
#define EXT_BTS		((V90BitsToSymbol *)(void *)ext_bts)

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * Fill both sides and the record of what was put there.  `mode` varies how
 * the bytes are chosen, because a store of a constant is invisible against a
 * seed that already holds it: mode 1 seeds 0x01 everywhere, which is what the
 * two constructors write to their trailing flag byte, and mode 2 seeds 0xff.
 * None of the four is zero.
 */
static void
fill(unsigned char *a, unsigned char *b, unsigned char *rec, unsigned n,
     int mode)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		unsigned char v;

		switch (mode) {
		case 1:
			v = 0x01;	/* extraSymbolsPending's value */
			break;
		case 2:
			v = 0xff;
			break;
		case 3:
			v = (unsigned char)(next_byte() | 1u);
			break;
		default:
			v = next_byte();
			break;
		}
		a[i] = v;
		b[i] = v;
		if (rec)
			rec[i] = v;
	}
}

static void
seed_trial(int trial)
{
	lfsr_state = 0x4d1u + 0x9e37u * (unsigned)trial;
	fill(map_a, map_b, map_seed, MAPPER_SLOT, trial & 3);
	fill(bts_a, bts_b, bts_seed, BTS_SLOT, (trial + 1) & 3);
	fill(par_store, par_store, (unsigned char *)0, sizeof(par_store),
	     trial & 3);
	fill(p4m_a, p4m_b, p4m_seed, P4M_SLOT, (trial + 2) & 3);
	fill(mod_a, mod_b, mod_seed, MOD_SLOT, (trial + 3) & 3);
	fill(mp_store, mp_store, (unsigned char *)0, sizeof(mp_store), 0);
	fill(cp_store, cp_store, (unsigned char *)0, sizeof(cp_store), 0);
	fill(mpsa_store, mpsa_store, (unsigned char *)0, sizeof(mpsa_store), 0);
	fill(mpsb_store, mpsb_store, (unsigned char *)0, sizeof(mpsb_store), 0);
	fill(p2_store, p2_store, (unsigned char *)0, sizeof(p2_store), 0);
	fill(jd_store, jd_store, (unsigned char *)0, sizeof(jd_store), 0);
	fill(v92jd_store, v92jd_store, (unsigned char *)0, sizeof(v92jd_store),
	     0);
	fill(dil_store, dil_store, (unsigned char *)0, sizeof(dil_store), 0);
	fill(acp_store, acp_store, (unsigned char *)0, sizeof(acp_store), 0);
}

/* ------------------------------------------------ the live allocation set */

#define MAXLIVE	512
static void *live[MAXLIVE];
static int nlive;

static void
live_refresh(void)
{
	nlive = harness_alloc_live_set(live, MAXLIVE);
	if (nlive > MAXLIVE)
		nlive = MAXLIVE;
}

static int
word_is_live(const unsigned char *w)
{
	void *p;
	int i;

	memcpy(&p, w, sizeof(p));
	if (p == 0)
		return 0;
	for (i = 0; i < nlive; i++)
		if (live[i] == p)
			return 1;
	return 0;
}

static void
mask_word(unsigned char *a, unsigned char *b, unsigned off)
{
	memset(a + off, 0x5a, sizeof(void *));
	memset(b + off, 0x5a, sizeof(void *));
}

/* Every aligned word in [lo, hi) that holds a live allocation on both sides. */
static void
mask_live(unsigned char *a, unsigned char *b, unsigned lo, unsigned hi)
{
	unsigned o;

	for (o = lo; o + sizeof(void *) <= hi; o += sizeof(void *))
		if (word_is_live(a + o) && word_is_live(b + o))
			mask_word(a, b, o);
}

/* V90Mapper: the 0x50 buffer, plus whatever the spectral shaper allocated. */
static void
canon_mapper(unsigned char *a, unsigned char *b)
{
	mask_word(a, b, 0x018);
	mask_live(a, b, SHAPER_LO, SHAPER_HI);
}

/* V90BitsToSymbol: the mapper it owns and the symbol buffer. */
static void
canon_bts(unsigned char *a, unsigned char *b)
{
	mask_word(a, b, 0x00);
	mask_word(a, b, 0x08);
}

/*
 * An embedded Scrambler, WITHOUT losing what is in it.  Its seven pointers all
 * point into one allocation, so replacing the six derived ones by their
 * DISTANCE FROM `pLimit` leaves only the base address to be masked -- and a
 * tap set to the wrong distance, or a restart pointer that did not move, still
 * differs.  Masking all seven would have thrown that away.  `tailLength` at
 * +0x1c is not a pointer and is compared as it stands.
 */
static void
canon_scrambler(unsigned char *o, unsigned base)
{
	unsigned int lim, v, i;

	memcpy(&lim, o + base, sizeof(lim));
	for (i = 1; i < 7; i++) {
		memcpy(&v, o + base + 4 * i, sizeof(v));
		v -= lim;
		memcpy(o + base + 4 * i, &v, sizeof(v));
	}
	memset(o + base, 0x5a, sizeof(void *));
}

/*
 * V90Phase4Modulator.  `bitsToSymbol` is masked only when the object OWNS it:
 * when it was supplied, both sides hold the one shared address and comparing
 * it is the whole point.
 */
static void
canon_p4m(unsigned char *a, unsigned char *b, int owned)
{
	if (owned)
		mask_word(a, b, 0x0044);
	canon_scrambler(a, P4M_SCRAMBLER);
	canon_scrambler(b, P4M_SCRAMBLER);
}

/* V90Modulator: three owned objects, two buffers and the scrambler. */
static void
canon_mod(unsigned char *a, unsigned char *b)
{
	mask_word(a, b, 0x38);
	mask_word(a, b, 0x3c);
	mask_word(a, b, 0x40);
	mask_word(a, b, 0x68);
	mask_word(a, b, 0x6c);
	canon_scrambler(a, MOD_SCRAMBLER);
	canon_scrambler(b, MOD_SCRAMBLER);
}

static void *
slot_ptr(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof(p));
	return p;
}

static void
compare_mapper(const char *what, const unsigned char *a, const unsigned char *b,
	       long tag)
{
	memcpy(cmp_a, a, MAPPER_SIZE);
	memcpy(cmp_b, b, MAPPER_SIZE);
	canon_mapper(cmp_a, cmp_b);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Mapper", cmp_a, cmp_b,
		     (size_t)MAPPER_SIZE, tag);
}

static void
compare_bts(const char *what, const unsigned char *a, const unsigned char *b,
	    long tag)
{
	memcpy(cmp_a, a, BTS_SIZE);
	memcpy(cmp_b, b, BTS_SIZE);
	canon_bts(cmp_a, cmp_b);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90BitsToSymbol", cmp_a, cmp_b,
		     (size_t)BTS_SIZE, tag);
}

static void
compare_p4m(const char *what, const unsigned char *a, const unsigned char *b,
	    int owned, long tag)
{
	memcpy(big_a, a, P4M_SIZE);
	memcpy(big_b, b, P4M_SIZE);
	canon_p4m(big_a, big_b, owned);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Modulator", big_a,
		     big_b, (size_t)P4M_SIZE, tag);
}

static void
compare_mod(const char *what, const unsigned char *a, const unsigned char *b,
	    long tag)
{
	memcpy(big_a, a, MOD_SIZE);
	memcpy(big_b, b, MOD_SIZE);
	canon_mod(big_a, big_b);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Modulator", big_a, big_b,
		     (size_t)MOD_SIZE, tag);
}

/*
 * The V90Phase3Modulator V90Modulator builds.  Its map is not this test's, so
 * every word of it that holds a live allocation is found by value; what
 * remains is its scalar state, which is what says it was constructed with the
 * same two arguments on both sides.
 */
static void
compare_p3m(const char *what, const unsigned char *a, const unsigned char *b,
	    long tag)
{
	memcpy(big_a, a, P3M_SIZE);
	memcpy(big_b, b, P3M_SIZE);
	canon_scrambler(big_a, P3M_SCRAMBLER);
	canon_scrambler(big_b, P3M_SCRAMBLER);
	mask_live(big_a, big_b, 0, P3M_SIZE);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Modulator", big_a,
		     big_b, (size_t)P3M_SIZE, tag);
}

/* The guard, on BOTH sides, against what the seed left there. */
static int
guard_intact(const unsigned char *a, const unsigned char *b,
	     const unsigned char *rec, unsigned size, unsigned slot)
{
	return memcmp(a + size, rec + size, slot - size) == 0
	    && memcmp(b + size, rec + size, slot - size) == 0;
}

/* ================================================================ V90Mapper */

static int
run_mapper_ctor(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
		dtor our_d, dtor ref_d)
{
	unsigned char first[MAPPER_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;

		seed_trial(trial);
		harness_alloc_reset();

		our_c(map_a, PARAMS);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_c(map_b, PARAMS);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		live_refresh();

		/*
		 * The class allocates 0x50 for itself and hands the rest to
		 * V90SpectralShaper, so no literal count belongs here; what IS
		 * absolute is that both sides did the same thing, in the same
		 * number of pieces and to the same total.
		 */
		diff_eq_int("allocations match the blob (trial %ld)",
			    a_allocs, b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("the 0x50 buffer and the shaper's two, at least "
			    "(trial %ld)", a_allocs >= 3, 1, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		diff_eq_int("buf is not null (trial %ld)",
			    slot_ptr(map_a, 0x018) != 0, 1, trial);
		diff_eq_int("ref buf is not null (trial %ld)",
			    slot_ptr(map_b, 0x018) != 0, 1, trial);
		diff_eq_int("params is the argument (trial %ld)",
			    slot_ptr(map_a, 0x000) == (void *)PARAMS, 1, trial);
		diff_eq_int("ref params is the argument (trial %ld)",
			    slot_ptr(map_b, 0x000) == (void *)PARAMS, 1, trial);

		compare_mapper("after the constructor", map_a, map_b, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(map_a, map_b, map_seed, MAPPER_SIZE,
					 MAPPER_SLOT), 1, trial);

		if (memcmp(map_seed, map_a, MAPPER_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, map_a, MAPPER_SLOT);
		else if (memcmp(first, map_a, MAPPER_SLOT) != 0)
			distinct = 1;

		our_d(map_a);
		ref_d(map_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * The destructor over what the constructor built, with `buf` either live or
 * released by hand and nulled.  Nulling it must remove exactly one free and
 * must NOT turn into a `sysdep_free(NULL)`: counting the two apart is what
 * makes the guard testable.  The spectral shaper's own frees are identical in
 * both arms, so the DIFFERENCE between them is this class's contribution and
 * nothing else.
 */
static int
run_mapper_dtor(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
		dtor our_d, dtor ref_d)
{
	unsigned char before_a[MAPPER_SLOT], before_b[MAPPER_SLOT];
	int trial, saw_live = 0, saw_null = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int base_frees = 0, arm;

		for (arm = 0; arm < 2; arm++) {
			int a_frees, b_frees, a_null, b_null;

			seed_trial(trial);
			harness_alloc_reset();
			our_c(map_a, PARAMS);
			ref_c(map_b, PARAMS);

			if (arm == 1) {
				sysdep_free(slot_ptr(map_a, 0x018));
				sysdep_free(slot_ptr(map_b, 0x018));
				memset(map_a + 0x018, 0, sizeof(void *));
				memset(map_b + 0x018, 0, sizeof(void *));
				saw_null = 1;
			} else {
				saw_live = 1;
			}
			memcpy(before_a, map_a, MAPPER_SLOT);
			memcpy(before_b, map_b, MAPPER_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(map_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(map_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob (arm %ld)", a_frees,
				    b_frees, arm);
			diff_eq_int("sysdep_free(NULL) calls (arm %ld)", a_null,
				    0, arm);
			diff_eq_int("ref sysdep_free(NULL) calls (arm %ld)",
				    b_null, 0, arm);
			diff_eq_int("no bad free (arm %ld)",
				    harness_alloc.bad_free, 0, arm);
			diff_eq_int("nothing left allocated (arm %ld)",
				    harness_alloc.live, 0, arm);

			/*
			 * The destructor stores nothing of its own, on either
			 * side.  The shaper's span is excluded because that
			 * class's destructor is not this one's claim to make.
			 */
			diff_eq_int("stored nothing below the shaper (arm %ld)",
				    memcmp(before_a, map_a, SHAPER_LO) == 0, 1,
				    arm);
			diff_eq_int("stored nothing above the shaper (arm %ld)",
				    memcmp(before_a + SHAPER_HI,
					   map_a + SHAPER_HI,
					   MAPPER_SLOT - SHAPER_HI) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing below the shaper "
				    "(arm %ld)",
				    memcmp(before_b, map_b, SHAPER_LO) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing above the shaper "
				    "(arm %ld)",
				    memcmp(before_b + SHAPER_HI,
					   map_b + SHAPER_HI,
					   MAPPER_SLOT - SHAPER_HI) == 0, 1,
				    arm);

			if (arm == 0)
				base_frees = a_frees;
			else
				diff_eq_int("nulling buf removes exactly one "
					    "free (trial %ld)",
					    base_frees - a_frees, 1, trial);
		}
	}

	diff_eq_int("a live buffer was tried", saw_live, 1, 0);
	diff_eq_int("a null buffer was tried", saw_null, 1, 0);

	return diff_end();
}

/* ========================================================== V90BitsToSymbol */

/* Varied, and never zero: nofSymbols scales the second allocation. */
static const unsigned int bts_n[] = {
	1u, 2u, 3u, 7u, 0x10u, 0x40u, 0x140u, 0x200u
};

static int
run_bts_ctor(const char *name, bts_ctor our_c, bts_ctor ref_c, dtor our_d,
	     dtor ref_d)
{
	unsigned char first[BTS_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = bts_n[trial & 7];
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;
		void *am, *bm;

		seed_trial(trial);
		harness_alloc_reset();

		our_c(bts_a, n, PARAMS);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_c(bts_b, n, PARAMS);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		live_refresh();

		diff_eq_int("allocations match the blob (trial %ld)", a_allocs,
			    b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		am = slot_ptr(bts_a, 0x00);
		bm = slot_ptr(bts_b, 0x00);
		diff_eq_int("mapper is not null (trial %ld)", am != 0, 1,
			    trial);
		diff_eq_int("ref mapper is not null (trial %ld)", bm != 0, 1,
			    trial);
		diff_eq_int("symbols is not null (trial %ld)",
			    slot_ptr(bts_a, 0x08) != 0, 1, trial);
		diff_eq_int("symbols is its own allocation (trial %ld)",
			    slot_ptr(bts_a, 0x08) != am, 1, trial);
		diff_eq_int("params is the argument (trial %ld)",
			    slot_ptr(bts_a, 0x04) == (void *)PARAMS, 1, trial);
		diff_eq_int("ref params is the argument (trial %ld)",
			    slot_ptr(bts_b, 0x04) == (void *)PARAMS, 1, trial);

		compare_bts("after the constructor", bts_a, bts_b, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(bts_a, bts_b, bts_seed, BTS_SIZE,
					 BTS_SLOT), 1, trial);

		/*
		 * The mapper it owns, compared whole.  This is what says the
		 * mapper was built with THIS object's `params` and not with
		 * something else: the field is at a known offset inside a
		 * block neither side wrote directly.
		 */
		compare_mapper("the mapper the constructor built",
			       (const unsigned char *)am,
			       (const unsigned char *)bm, trial);
		diff_eq_int("the mapper holds the same params (trial %ld)",
			    slot_ptr((const unsigned char *)am, 0x000)
			    == (void *)PARAMS, 1, trial);

		if (memcmp(bts_seed, bts_a, BTS_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, bts_a, BTS_SLOT);
		else if (memcmp(first, bts_a, BTS_SLOT) != 0)
			distinct = 1;

		our_d(bts_a);
		ref_d(bts_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * Both guards, all four combinations.  The pointers are REAL -- the mapper is
 * dereferenced before it is freed, so a wild value is a crash and not a test
 * -- and the one nulled is released by hand first, so `live` still returns to
 * zero and a leak in the destructor is still visible.
 *
 * The expected free count is exact.  Subset 0 measures the whole chain; the
 * mapper's share of it is that total minus the one free of `symbols`, so
 * nulling the mapper must leave exactly one free and nulling `symbols` must
 * leave exactly the mapper's share.
 */
static int
run_bts_dtor(const char *name, bts_ctor our_c, bts_ctor ref_c, dtor our_d,
	     dtor ref_d, dtor our_md, dtor ref_md)
{
	unsigned char before_a[BTS_SLOT], before_b[BTS_SLOT];
	int trial, saw_null = 0, saw_live = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = bts_n[trial & 7];
		int base_frees = 0;
		unsigned subset;

		for (subset = 0; subset < 4; subset++) {
			int a_frees, b_frees, a_null, b_null, want;

			seed_trial(trial);
			harness_alloc_reset();
			our_c(bts_a, n, PARAMS);
			ref_c(bts_b, n, PARAMS);

			if (subset & 1u) {
				void *am = slot_ptr(bts_a, 0x00);
				void *bm = slot_ptr(bts_b, 0x00);

				our_md(am);
				ref_md(bm);
				sysdep_free(am);
				sysdep_free(bm);
				memset(bts_a + 0x00, 0, sizeof(void *));
				memset(bts_b + 0x00, 0, sizeof(void *));
				saw_null = 1;
			}
			if (subset & 2u) {
				sysdep_free(slot_ptr(bts_a, 0x08));
				sysdep_free(slot_ptr(bts_b, 0x08));
				memset(bts_a + 0x08, 0, sizeof(void *));
				memset(bts_b + 0x08, 0, sizeof(void *));
				saw_null = 1;
			}
			if (subset == 0)
				saw_live = 1;
			memcpy(before_a, bts_a, BTS_SLOT);
			memcpy(before_b, bts_b, BTS_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(bts_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(bts_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob, subset %ld",
				    a_frees, b_frees, (long)subset);
			diff_eq_int("sysdep_free(NULL), subset %ld", a_null, 0,
				    (long)subset);
			diff_eq_int("ref sysdep_free(NULL), subset %ld", b_null,
				    0, (long)subset);
			diff_eq_int("no bad free, subset %ld",
				    harness_alloc.bad_free, 0, (long)subset);
			diff_eq_int("nothing left allocated, subset %ld",
				    harness_alloc.live, 0, (long)subset);
			diff_eq_int("the destructor stored nothing, subset %ld",
				    memcmp(before_a, bts_a, BTS_SLOT) == 0, 1,
				    (long)subset);
			diff_eq_int("ref stored nothing, subset %ld",
				    memcmp(before_b, bts_b, BTS_SLOT) == 0, 1,
				    (long)subset);

			if (subset == 0) {
				base_frees = a_frees;
				continue;
			}
			/* base_frees - 1 is the mapper chain; 1 is `symbols`. */
			want = 0;
			if ((subset & 1u) == 0)
				want += base_frees - 1;
			if ((subset & 2u) == 0)
				want += 1;
			diff_eq_int("one free per live pointer, subset %ld",
				    a_frees, want, (long)subset);
		}
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a live pointer was tried", saw_live, 1, 0);

	return diff_end();
}


static unsigned int
slot_u32(const unsigned char *o, unsigned off)
{
	unsigned int v;

	memcpy(&v, o + off, sizeof(v));
	return v;
}

/* ====================================================== V90Phase4Modulator */

/* Varied and never zero; `flag` and `arg8` are drawn apart so they differ. */
static const unsigned int flagv[] = {
	1u, 2u, 0x80u, 0x8000u, 0xffffu, 0x7fffffffu, 0xffffffffu, 5u
};
static const unsigned int arg8v[] = {
	0xcu, 3u, 0x11u, 0x1234u, 0x40u, 0xfeu, 0x7fu, 0x100u
};

/*
 * `supplied` picks the arm: a non-null third argument is borrowed and +0x2fa4
 * becomes 1, a null one makes the constructor allocate 0x24 and build a
 * V90BitsToSymbol(0x140, params) of its own with +0x2fa4 at 0.  Both are
 * driven, because the two are different code and the flag is what the
 * destructor reads.
 */
static int
run_p4m_ctor(const char *name, p4m_ctor our_c, p4m_ctor ref_c, dtor our_d,
	     dtor ref_d, int supplied)
{
	static unsigned char first[P4M_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int flag = flagv[trial & 7];
		unsigned int arg8 = arg8v[(trial + 3) & 7];
		V90BitsToSymbol *bts = 0;
		int base_allocs, a_allocs, b_allocs;
		unsigned base_bytes, a_bytes, b_bytes;

		seed_trial(trial);
		harness_alloc_reset();

		if (supplied) {
			our_bts_c1(ext_bts, 0x40, PARAMS);
			bts = EXT_BTS;
		}
		base_allocs = harness_alloc.allocs;
		base_bytes = harness_alloc.bytes;

		our_c(p4m_a, PARAMS, flag, bts, MP, MPS_A, MPS_B, CP, arg8);
		a_allocs = harness_alloc.allocs - base_allocs;
		a_bytes = harness_alloc.bytes - base_bytes;

		ref_c(p4m_b, PARAMS, flag, bts, MP, MPS_A, MPS_B, CP, arg8);
		b_allocs = harness_alloc.allocs - base_allocs - a_allocs;
		b_bytes = harness_alloc.bytes - base_bytes - a_bytes;

		live_refresh();

		diff_eq_int("allocations match the blob (trial %ld)", a_allocs,
			    b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("the scrambler's buffer, at least (trial %ld)",
			    a_allocs >= 1, 1, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		/* Every argument, at the offset the blob puts it at. */
		diff_eq_int("sessionFlag is argument 2 (trial %ld)",
			    (long)slot_u32(p4m_a, 0x0000) == (long)flag, 1,
			    trial);
		diff_eq_int("ref sessionFlag is argument 2 (trial %ld)",
			    (long)slot_u32(p4m_b, 0x0000) == (long)flag, 1,
			    trial);
		diff_eq_int("mp is argument 4 (trial %ld)",
			    slot_ptr(p4m_a, 0x0048) == (void *)MP, 1, trial);
		diff_eq_int("mappingParams is argument 5 (trial %ld)",
			    slot_ptr(p4m_a, 0x004c) == (void *)MPS_A, 1, trial);
		diff_eq_int("mappingParams2 is argument 6 (trial %ld)",
			    slot_ptr(p4m_a, 0x0050) == (void *)MPS_B, 1, trial);
		diff_eq_int("cp is argument 7 (trial %ld)",
			    slot_ptr(p4m_a, 0x0054) == (void *)CP, 1, trial);
		diff_eq_int("ctorArg8 is argument 8 (trial %ld)",
			    (long)slot_u32(p4m_a, 0x2f98) == (long)arg8, 1,
			    trial);
		diff_eq_int("params is argument 1 (trial %ld)",
			    slot_ptr(p4m_a, 0x2fa8) == (void *)PARAMS, 1,
			    trial);
		diff_eq_int("+0x2f9c is cleared (trial %ld)",
			    (long)slot_u32(p4m_a, 0x2f9c), 0, trial);
		diff_eq_int("+0x2fa0 is cleared (trial %ld)",
			    (long)slot_u32(p4m_a, 0x2fa0), 0, trial);
		diff_eq_int("the ownership flag (trial %ld)",
			    (long)slot_u32(p4m_a, 0x2fa4), supplied ? 1 : 0,
			    trial);
		diff_eq_int("ref ownership flag (trial %ld)",
			    (long)slot_u32(p4m_b, 0x2fa4), supplied ? 1 : 0,
			    trial);

		if (supplied) {
			diff_eq_int("the supplied converter is stored "
				    "(trial %ld)",
				    slot_ptr(p4m_a, 0x0044) == (void *)bts, 1,
				    trial);
			diff_eq_int("ref stores the supplied converter "
				    "(trial %ld)",
				    slot_ptr(p4m_b, 0x0044) == (void *)bts, 1,
				    trial);
			diff_eq_int("nothing was allocated for it (trial %ld)",
				    a_allocs, b_allocs, trial);
		} else {
			const unsigned char *ao = (const unsigned char *)
			    slot_ptr(p4m_a, 0x0044);
			const unsigned char *bo = (const unsigned char *)
			    slot_ptr(p4m_b, 0x0044);

			diff_eq_int("a converter was built (trial %ld)",
				    ao != 0 && bo != 0, 1, trial);
			compare_bts("the converter the constructor built", ao,
				    bo, trial);
			compare_mapper("its mapper", (const unsigned char *)
				       slot_ptr(ao, 0x00),
				       (const unsigned char *)
				       slot_ptr(bo, 0x00), trial);
			diff_eq_int("it was built with 0x140 symbols "
				    "(trial %ld)",
				    (long)slot_u32(ao, 0x0c), 0x140, trial);
			diff_eq_int("it was built with our params (trial %ld)",
				    slot_ptr(ao, 0x04) == (void *)PARAMS, 1,
				    trial);
		}

		compare_p4m("after the constructor", p4m_a, p4m_b, !supplied,
			    trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(p4m_a, p4m_b, p4m_seed, P4M_SIZE,
					 P4M_SLOT), 1, trial);

		if (memcmp(p4m_seed, p4m_a, P4M_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, p4m_a, P4M_SLOT);
		else if (memcmp(first, p4m_a, P4M_SLOT) != 0)
			distinct = 1;

		our_d(p4m_a);
		ref_d(p4m_b);
		if (supplied)
			our_bts_d1(ext_bts);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * Four arms, which is both guards from both sides:
 *
 *   0  supplied      -- the flag is 1, so the converter must NOT be released
 *   1  owned         -- the flag is 0 and the pointer is live, so it must be
 *   2  owned, nulled -- the flag is 0 and the pointer is not
 *   3  owned, flagged -- the pointer is live and the flag says it is not ours
 *
 * Arms 0, 2 and 3 must all free exactly what the scrambler frees and no more,
 * and arm 1 must free strictly more.  Arm 0 additionally compares the shared
 * converter before and after, which is what says "not released" rather than
 * "released and the counter happened to agree".
 */
static int
run_p4m_dtor(const char *name, p4m_ctor our_c, p4m_ctor ref_c, dtor our_d,
	     dtor ref_d)
{
	static unsigned char before_a[P4M_SLOT], before_b[P4M_SLOT];
	static unsigned char ext_before[BTS_SIZE];
	int trial, arm, base_frees = 0, owned_frees = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		for (arm = 0; arm < 4; arm++) {
			unsigned int flag = flagv[trial & 7];
			unsigned int arg8 = arg8v[(trial + 3) & 7];
			V90BitsToSymbol *bts = 0;
			int a_frees, b_frees, a_null, b_null;

			seed_trial(trial);
			harness_alloc_reset();

			if (arm == 0) {
				our_bts_c1(ext_bts, 0x40, PARAMS);
				bts = EXT_BTS;
				memcpy(ext_before, ext_bts, BTS_SIZE);
			}
			our_c(p4m_a, PARAMS, flag, bts, MP, MPS_A, MPS_B, CP,
			      arg8);
			ref_c(p4m_b, PARAMS, flag, bts, MP, MPS_A, MPS_B, CP,
			      arg8);

			if (arm == 2 || arm == 3) {
				void *ao = slot_ptr(p4m_a, 0x0044);
				void *bo = slot_ptr(p4m_b, 0x0044);

				our_bts_d1(ao);
				ref_bts_d1(bo);
				sysdep_free(ao);
				sysdep_free(bo);
				if (arm == 2) {
					memset(p4m_a + 0x0044, 0,
					       sizeof(void *));
					memset(p4m_b + 0x0044, 0,
					       sizeof(void *));
				} else {
					unsigned int one = 1u;

					memcpy(p4m_a + 0x2fa4, &one,
					       sizeof(one));
					memcpy(p4m_b + 0x2fa4, &one,
					       sizeof(one));
				}
			}
			memcpy(before_a, p4m_a, P4M_SLOT);
			memcpy(before_b, p4m_b, P4M_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(p4m_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(p4m_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob, arm %ld", a_frees,
				    b_frees, arm);
			diff_eq_int("sysdep_free(NULL), arm %ld", a_null, 0,
				    arm);
			diff_eq_int("ref sysdep_free(NULL), arm %ld", b_null, 0,
				    arm);
			diff_eq_int("no bad free, arm %ld",
				    harness_alloc.bad_free, 0, arm);
			diff_eq_int("the destructor stored nothing, arm %ld",
				    memcmp(before_a, p4m_a, P4M_SLOT) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing, arm %ld",
				    memcmp(before_b, p4m_b, P4M_SLOT) == 0, 1,
				    arm);

			if (arm == 0) {
				diff_eq_int("the supplied converter is "
					    "untouched (trial %ld)",
					    memcmp(ext_before, ext_bts,
						   BTS_SIZE) == 0, 1, trial);
				our_bts_d1(ext_bts);
				base_frees = a_frees;
			} else if (arm == 1) {
				owned_frees = a_frees;
				diff_eq_int("owning it frees strictly more "
					    "(trial %ld)",
					    owned_frees > base_frees, 1, trial);
			} else {
				diff_eq_int("no converter release, arm %ld",
					    a_frees, base_frees, arm);
			}
			diff_eq_int("nothing left allocated, arm %ld",
				    harness_alloc.live, 0, arm);
		}
	}

	return diff_end();
}

/* =========================================================== V90Modulator */

static const unsigned int modn[] = {
	1u, 2u, 3u, 7u, 0x10u, 0x40u, 0x60u, 0x80u
};

static int
run_mod_ctor(const char *name, mod_ctor our_c, mod_ctor ref_c, dtor our_d,
	     dtor ref_d)
{
	static unsigned char first[MOD_SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int n = modn[trial & 7];
		unsigned int flag = flagv[trial & 7];
		const unsigned char *ap4, *bp4, *ab, *bb, *ap3, *bp3;
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;

		seed_trial(trial);
		harness_alloc_reset();

		our_c(mod_a, n, P2INFO, JD, V92JD, DIL, MPS_A, MPS_B, ACP, CP,
		      MP, PARAMS, flag);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_c(mod_b, n, P2INFO, JD, V92JD, DIL, MPS_A, MPS_B, ACP, CP,
		      MP, PARAMS, flag);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		live_refresh();

		diff_eq_int("allocations match the blob (trial %ld)", a_allocs,
			    b_allocs, trial);
		diff_eq_int("bytes allocated match the blob (trial %ld)",
			    (int)a_bytes, (int)b_bytes, trial);
		diff_eq_int("five of its own and the sub-objects' (trial %ld)",
			    a_allocs >= 5, 1, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);
		diff_eq_int("live set did not overflow (trial %ld)",
			    harness_alloc.overflow, 0, trial);

		/*
		 * All twelve arguments, at the offsets the blob puts them at.
		 * Nothing about this ordering is inferable from the signature:
		 * argument 1 lands at +0x64, argument 12 at +0x28, and the
		 * V90CP (argument 9) lands ABOVE the V90MP (argument 10).
		 */
		diff_eq_int("nofSymbols is argument 1 (trial %ld)",
			    (long)slot_u32(mod_a, 0x64) == (long)n, 1, trial);
		diff_eq_int("phase2Info is argument 2 (trial %ld)",
			    slot_ptr(mod_a, 0x00) == (void *)P2INFO, 1, trial);
		diff_eq_int("jd is argument 3 (trial %ld)",
			    slot_ptr(mod_a, 0x04) == (void *)JD, 1, trial);
		diff_eq_int("v92Jd is argument 4 (trial %ld)",
			    slot_ptr(mod_a, 0x08) == (void *)V92JD, 1, trial);
		diff_eq_int("dil is argument 5 (trial %ld)",
			    slot_ptr(mod_a, 0x0c) == (void *)DIL, 1, trial);
		diff_eq_int("mappingParams is argument 6 (trial %ld)",
			    slot_ptr(mod_a, 0x10) == (void *)MPS_A, 1, trial);
		diff_eq_int("mappingParams2 is argument 7 (trial %ld)",
			    slot_ptr(mod_a, 0x14) == (void *)MPS_B, 1, trial);
		diff_eq_int("additionalCPinfo is argument 8 (trial %ld)",
			    slot_ptr(mod_a, 0x18) == (void *)ACP, 1, trial);
		diff_eq_int("mp is argument 10 (trial %ld)",
			    slot_ptr(mod_a, 0x1c) == (void *)MP, 1, trial);
		diff_eq_int("cp is argument 9 (trial %ld)",
			    slot_ptr(mod_a, 0x20) == (void *)CP, 1, trial);
		diff_eq_int("params is argument 11 (trial %ld)",
			    slot_ptr(mod_a, 0x24) == (void *)PARAMS, 1, trial);
		diff_eq_int("sessionFlag is argument 12 (trial %ld)",
			    (long)slot_u32(mod_a, 0x28) == (long)flag, 1,
			    trial);

		diff_eq_int("symbolBuf is not null (trial %ld)",
			    slot_ptr(mod_a, 0x68) != 0, 1, trial);
		diff_eq_int("frameBuf is not null (trial %ld)",
			    slot_ptr(mod_a, 0x6c) != 0, 1, trial);
		diff_eq_int("the two buffers are two allocations (trial %ld)",
			    slot_ptr(mod_a, 0x68) != slot_ptr(mod_a, 0x6c), 1,
			    trial);

		ap3 = (const unsigned char *)slot_ptr(mod_a, 0x38);
		bp3 = (const unsigned char *)slot_ptr(mod_b, 0x38);
		ap4 = (const unsigned char *)slot_ptr(mod_a, 0x3c);
		bp4 = (const unsigned char *)slot_ptr(mod_b, 0x3c);
		ab = (const unsigned char *)slot_ptr(mod_a, 0x40);
		bb = (const unsigned char *)slot_ptr(mod_b, 0x40);
		diff_eq_int("the three sub-objects were built (trial %ld)",
			    ap3 != 0 && ap4 != 0 && ab != 0, 1, trial);

		compare_p3m("the phase 3 modulator it built", ap3, bp3, trial);
		compare_p4m("the phase 4 modulator it built", ap4, bp4, 1,
			    trial);
		compare_bts("the converter it built", ab, bb, trial);
		compare_mapper("the converter's mapper",
			       (const unsigned char *)slot_ptr(ab, 0x00),
			       (const unsigned char *)slot_ptr(bb, 0x00),
			       trial);

		diff_eq_int("the converter takes 3n + 0x1388 symbols "
			    "(trial %ld)",
			    (long)slot_u32(ab, 0x0c),
			    (long)(3u * n + 0x1388u), trial);
		diff_eq_int("the converter takes our params (trial %ld)",
			    slot_ptr(ab, 0x04) == (void *)PARAMS, 1, trial);

		/*
		 * THE TWO MAPPING-PARAMETER POINTERS CROSS.  Argument 6 was
		 * stored at +0x10 and is handed on as the phase 4 modulator's
		 * SIXTH argument, landing at its +0x50; argument 7 goes the
		 * other way.  Two distinct shared instances are the only thing
		 * that can tell.
		 */
		diff_eq_int("phase 4 got argument 7 as its fifth (trial %ld)",
			    slot_ptr(ap4, 0x004c) == (void *)MPS_B, 1, trial);
		diff_eq_int("phase 4 got argument 6 as its sixth (trial %ld)",
			    slot_ptr(ap4, 0x0050) == (void *)MPS_A, 1, trial);
		diff_eq_int("phase 4 borrows our converter (trial %ld)",
			    slot_ptr(ap4, 0x0044) == (void *)ab, 1, trial);
		diff_eq_int("ref phase 4 borrows its converter (trial %ld)",
			    slot_ptr(bp4, 0x0044) == (void *)bb, 1, trial);
		diff_eq_int("phase 4 does not own it (trial %ld)",
			    (long)slot_u32(ap4, 0x2fa4), 1, trial);
		diff_eq_int("phase 4's eighth argument is 0xc (trial %ld)",
			    (long)slot_u32(ap4, 0x2f98), 0xc, trial);
		diff_eq_int("phase 4 got our session flag (trial %ld)",
			    (long)slot_u32(ap4, 0x0000) == (long)flag, 1,
			    trial);
		diff_eq_int("phase 4 got our params (trial %ld)",
			    slot_ptr(ap4, 0x2fa8) == (void *)PARAMS, 1, trial);
		diff_eq_int("phase 4 got our mp (trial %ld)",
			    slot_ptr(ap4, 0x0048) == (void *)MP, 1, trial);
		diff_eq_int("phase 4 got our cp (trial %ld)",
			    slot_ptr(ap4, 0x0054) == (void *)CP, 1, trial);

		compare_mod("after the constructor", mod_a, mod_b, trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(mod_a, mod_b, mod_seed, MOD_SIZE,
					 MOD_SLOT), 1, trial);

		if (memcmp(mod_seed, mod_a, MOD_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, mod_a, MOD_SLOT);
		else if (memcmp(first, mod_a, MOD_SLOT) != 0)
			distinct = 1;

		our_d(mod_a);
		ref_d(mod_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

/*
 * Six arms: everything live, then each of the five guarded pointers released
 * by hand and nulled in turn.  Every arm must free the same number as the
 * blob, must never pass NULL to the allocator, and must leave nothing
 * outstanding -- and each nulled arm must free strictly fewer than the first,
 * which is what says the guard skipped something rather than the field being
 * unreachable.
 */
static const unsigned int mod_owned[5] = { 0x38u, 0x3cu, 0x40u, 0x68u, 0x6cu };

static int
run_mod_dtor(const char *name, mod_ctor our_c, mod_ctor ref_c, dtor our_d,
	     dtor ref_d)
{
	static unsigned char before_a[MOD_SLOT], before_b[MOD_SLOT];
	int trial, arm, base_frees = 0, saw_drop = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		for (arm = 0; arm < 6; arm++) {
			unsigned int n = modn[trial & 7];
			unsigned int flag = flagv[trial & 7];
			int a_frees, b_frees, a_null, b_null;

			seed_trial(trial);
			harness_alloc_reset();
			our_c(mod_a, n, P2INFO, JD, V92JD, DIL, MPS_A, MPS_B,
			      ACP, CP, MP, PARAMS, flag);
			ref_c(mod_b, n, P2INFO, JD, V92JD, DIL, MPS_A, MPS_B,
			      ACP, CP, MP, PARAMS, flag);

			if (arm > 0) {
				unsigned off = mod_owned[arm - 1];
				void *ao = slot_ptr(mod_a, off);
				void *bo = slot_ptr(mod_b, off);

				if (off == 0x38) {
					our_p3m_d1(ao);
					ref_p3m_d1(bo);
				} else if (off == 0x3c) {
					our_p4m_d1(ao);
					ref_p4m_d1(bo);
				} else if (off == 0x40) {
					our_bts_d1(ao);
					ref_bts_d1(bo);
				}
				sysdep_free(ao);
				sysdep_free(bo);
				memset(mod_a + off, 0, sizeof(void *));
				memset(mod_b + off, 0, sizeof(void *));
			}
			memcpy(before_a, mod_a, MOD_SLOT);
			memcpy(before_b, mod_b, MOD_SLOT);

			a_frees = harness_alloc.frees;
			a_null = harness_alloc.free_null;
			our_d(mod_a);
			a_frees = harness_alloc.frees - a_frees;
			a_null = harness_alloc.free_null - a_null;

			b_frees = harness_alloc.frees;
			b_null = harness_alloc.free_null;
			ref_d(mod_b);
			b_frees = harness_alloc.frees - b_frees;
			b_null = harness_alloc.free_null - b_null;

			diff_eq_int("frees match the blob, arm %ld", a_frees,
				    b_frees, arm);
			diff_eq_int("sysdep_free(NULL), arm %ld", a_null, 0,
				    arm);
			diff_eq_int("ref sysdep_free(NULL), arm %ld", b_null, 0,
				    arm);
			diff_eq_int("no bad free, arm %ld",
				    harness_alloc.bad_free, 0, arm);
			diff_eq_int("nothing left allocated, arm %ld",
				    harness_alloc.live, 0, arm);
			diff_eq_int("the destructor stored nothing, arm %ld",
				    memcmp(before_a, mod_a, MOD_SLOT) == 0, 1,
				    arm);
			diff_eq_int("ref stored nothing, arm %ld",
				    memcmp(before_b, mod_b, MOD_SLOT) == 0, 1,
				    arm);

			if (arm == 0) {
				base_frees = a_frees;
			} else {
				diff_eq_int("nulling +0x%02lx frees fewer",
					    a_frees < base_frees, 1,
					    (long)mod_owned[arm - 1]);
				saw_drop = 1;
			}
		}
	}

	diff_eq_int("every guard was driven both ways", saw_drop, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_mapper_ctor("V90Mapper::V90Mapper (C1)", our_mapper_c1,
			      ref_mapper_c1, our_mapper_d1, ref_mapper_d1);
	rc |= run_mapper_ctor("V90Mapper::V90Mapper (C2)", our_mapper_c2,
			      ref_mapper_c2, our_mapper_d2, ref_mapper_d2);
	rc |= run_mapper_dtor("V90Mapper::~V90Mapper (D1)", our_mapper_c1,
			      ref_mapper_c1, our_mapper_d1, ref_mapper_d1);
	rc |= run_mapper_dtor("V90Mapper::~V90Mapper (D2)", our_mapper_c2,
			      ref_mapper_c2, our_mapper_d2, ref_mapper_d2);

	rc |= run_bts_ctor("V90BitsToSymbol::V90BitsToSymbol (C1)",
			   our_bts_c1, ref_bts_c1, our_bts_d1, ref_bts_d1);
	rc |= run_bts_ctor("V90BitsToSymbol::V90BitsToSymbol (C2)",
			   our_bts_c2, ref_bts_c2, our_bts_d2, ref_bts_d2);
	rc |= run_bts_dtor("V90BitsToSymbol::~V90BitsToSymbol (D1)",
			   our_bts_c1, ref_bts_c1, our_bts_d1, ref_bts_d1,
			   our_mapper_d1, ref_mapper_d1);
	rc |= run_bts_dtor("V90BitsToSymbol::~V90BitsToSymbol (D2)",
			   our_bts_c2, ref_bts_c2, our_bts_d2, ref_bts_d2,
			   our_mapper_d2, ref_mapper_d2);

	rc |= run_p4m_ctor("V90Phase4Modulator (C1, converter supplied)",
			   our_p4m_c1, ref_p4m_c1, our_p4m_d1, ref_p4m_d1, 1);
	rc |= run_p4m_ctor("V90Phase4Modulator (C1, converter owned)",
			   our_p4m_c1, ref_p4m_c1, our_p4m_d1, ref_p4m_d1, 0);
	rc |= run_p4m_ctor("V90Phase4Modulator (C2, converter supplied)",
			   our_p4m_c2, ref_p4m_c2, our_p4m_d2, ref_p4m_d2, 1);
	rc |= run_p4m_ctor("V90Phase4Modulator (C2, converter owned)",
			   our_p4m_c2, ref_p4m_c2, our_p4m_d2, ref_p4m_d2, 0);
	rc |= run_p4m_dtor("V90Phase4Modulator::~V90Phase4Modulator (D1)",
			   our_p4m_c1, ref_p4m_c1, our_p4m_d1, ref_p4m_d1);
	rc |= run_p4m_dtor("V90Phase4Modulator::~V90Phase4Modulator (D2)",
			   our_p4m_c2, ref_p4m_c2, our_p4m_d2, ref_p4m_d2);

	rc |= run_mod_ctor("V90Modulator::V90Modulator (C1)", our_mod_c1,
			   ref_mod_c1, our_mod_d1, ref_mod_d1);
	rc |= run_mod_ctor("V90Modulator::V90Modulator (C2)", our_mod_c2,
			   ref_mod_c2, our_mod_d2, ref_mod_d2);
	rc |= run_mod_dtor("V90Modulator::~V90Modulator (D1)", our_mod_c1,
			   ref_mod_c1, our_mod_d1, ref_mod_d1);
	rc |= run_mod_dtor("V90Modulator::~V90Modulator (D2)", our_mod_c2,
			   ref_mod_c2, our_mod_d2, ref_mod_d2);

	return rc;
}
