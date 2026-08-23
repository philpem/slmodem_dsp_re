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

/* ------------------------------------- V90Mapper::reset, ::resetNoSpectral */

/*
 * THE `PcmType` ARGUMENT IS DECLARED `int` HERE ON PURPOSE.  The object tests
 * it for nonzero (`test %ebx,%ebx ; jne`) rather than comparing it against a
 * value, so the interesting inputs are the ones OUTSIDE the enumeration --
 * 2, 0xff, 0x100 -- and naming them as `PcmType` in the caller would be a
 * value no enumerator has.  The asm() label is exact either way, and an enum
 * and an `int` occupy the same 4-byte cdecl stack slot.
 */
extern "C" {
void our_map_reset(void *, V90MappingParams *, int)
	asm("_ZN9V90Mapper5resetEP16V90MappingParams7PcmType");
void ref_map_reset(void *, V90MappingParams *, int)
	asm("ref__ZN9V90Mapper5resetEP16V90MappingParams7PcmType");
void our_map_resetns(void *, V90MappingParams *, int)
	asm("_ZN9V90Mapper15resetNoSpectralEP16V90MappingParams7PcmType");
void ref_map_resetns(void *, V90MappingParams *, int)
	asm("ref__ZN9V90Mapper15resetNoSpectralEP16V90MappingParams7PcmType");
}

typedef void (*map_reset)(void *, V90MappingParams *, int);

/* The mapping block the reset cases are driven from; see build_mp(). */
static unsigned char rst_mp[sizeof(V90MappingParams)]
	__attribute__((aligned(8)));
#define RST_MP	((V90MappingParams *)(void *)rst_mp)

/*
 * THE SENTINELS, AND WHY A SEEDED FIXTURE IS NOT ENOUGH ON ITS OWN HERE.
 *
 * The object these two functions run over has to be CONSTRUCTED -- it owns
 * three allocations and a destructor that dereferences them -- and the
 * constructor zeroes nine of the words the resets write.  So over a
 * constructed object a reset that fails to store zero into `cleared_01c`,
 * `uint_6f8`, `signBitGroupSize` or the sign encoder is INVISIBLE: the field
 * already holds the value the store would have left.  Five mutations proved
 * exactly that and are the reason this table exists; the seed cannot reach
 * these fields because the constructor runs after it.
 *
 * Each entry is therefore a value the function in question could not have
 * computed, written over the constructor's zero on BOTH sides before the call.
 * A field the reset writes comes back zero and the mutation that drops the
 * store fails; a field it leaves alone comes back holding the sentinel, which
 * is a stronger claim than the two sides agreeing on it.
 *
 * `+0x00c` is the one that was always needed rather than one of the five:
 * `resetNoSpectral` READS `signBitsPerFrame` and never writes it, which is its
 * whole difference from `reset` at the top of the function.  0x1234 is
 * unmistakable in `word_08` because `6 - shaperSR` is at most 6.
 */
static const struct {
	unsigned int off;
	unsigned int val;
	unsigned int width;
} pokes[] = {
	{ 0x00cu, 0x1234u, 4u },	/* signBitsPerFrame  */
	{ 0x010u, 0x5151u, 4u },	/* signBitGroups     */
	{ 0x014u, 0x6262u, 4u },	/* signBitGroupSize  */
	{ 0x01cu, 0x7373u, 4u },	/* cleared_01c       */
	{ 0x6f8u, 0x8484u, 4u },	/* uint_6f8          */
	{ 0x6fcu, 0x00a5u, 1u }		/* signEncoder.prev_ */
};
#define NPOKE	((int)(sizeof(pokes) / sizeof(pokes[0])))
#define POKE_00C	0x1234u

static void
poke_fields(unsigned char *o)
{
	int i;

	for (i = 0; i < NPOKE; i++)
		memcpy(o + pokes[i].off, &pokes[i].val, pokes[i].width);
}

static unsigned int
peek(const unsigned char *o, unsigned int off, unsigned int width)
{
	unsigned int v = 0;

	memcpy(&v, o + off, width);
	return v;
}

struct reset_case {
	unsigned int size[V90MAPPER_CONSTELLATIONS];
	int	     sr;		/* mp->shaperSR                     */
	unsigned int id;		/* mp->shaperId                     */
	unsigned int word0;		/* mp->word_0, the frame's bits     */
	int	     pcm;
};

/*
 * The cases, and what each is for.
 *
 *   0 and 128 entries are both present, because the tail-fill runs to index
 *   127 whatever the count is: zero means the whole row is zeroed over the
 *   seed, and 128 means the fill does not run at all and every entry is a
 *   converted code.
 *
 *   `shaperSR` covers 0, which is the arm where `reset` stores zero into
 *   `signBitGroupSize` and skips the shaper entirely, and the divisors 1, 2,
 *   3 and 6, which are the block lengths 6, 3, 2 and 1.
 *
 *   `shaperId` DIFFERS FROM `shaperSR` in five of the eight, because
 *   `V90SpectralShaper::reset` takes them adjacent and in that order and a
 *   swap is silent whenever they are equal.  Case 7 has them equal on purpose
 *   as the control.
 *
 *   `pcm` covers both arms and three values outside the enumeration.
 */
static const struct reset_case reset_cases[] = {
	{ {   0,   0,   0,   0,   0,   0 }, 1, 0, 28u, 0 },
	{ { 128, 128, 128, 128, 128, 128 }, 1, 3, 40u, 0 },
	{ {   1,   2,   3,   4,   5,   6 }, 2, 3, 32u, 1 },
	{ { 127,   1,  64,   0, 128,   7 }, 3, 1, 35u, 1 },
	{ {  12,   0, 128,   5,   0,  64 }, 6, 2, 30u, 2 },
	{ {  64,  64,  64,  64,  64,  64 }, 0, 3, 24u, 0xff },
	{ {   8,  16,  32,  48,  96, 128 }, 1, 1, 48u, 0x100 },
	{ {   3,   3,   3,   3,   3,   3 }, 2, 2, 20u, 1 }
};
#define NRESET	((int)(sizeof(reset_cases) / sizeof(reset_cases[0])))

/*
 * Build the mapping block for one case.  Everything not named by the case is
 * varied LFSR bytes, so the four shaper floats and the tables this class does
 * not read are different on every trial; `sweep` replaces the code table with
 * a walk that covers all 256 byte values three times over, which is what makes
 * both companding conversions exercised across their whole domain rather than
 * over whatever the LFSR happened to emit.
 */
static void
build_mp(const struct reset_case *c, int sweep)
{
	unsigned int i, j;

	fill(rst_mp, rst_mp, (unsigned char *)0, sizeof(rst_mp), 0);

	if (sweep)
		for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++)
			for (j = 0; j < V90MAPPER_LEVELS; j++)
				RST_MP->constellation[i][j] =
				    (unsigned char)(i * V90MAPPER_LEVELS + j);

	for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++)
		RST_MP->constellationSize[i] = c->size[i];
	RST_MP->word_0 = c->word0;
	RST_MP->shaperSR = c->sr;
	RST_MP->shaperId = c->id;
}

/*
 * ONE SIDE'S OWN CANONICAL FORM.  `canon_mapper` makes the two sides
 * comparable; this makes one side comparable against ITSELF from an earlier
 * run, which is what the `pcm` check needs -- the 0x50 buffer and the shaper's
 * two come back at different addresses across a free/allocate cycle, and
 * nothing about which arm the mapping took is in those three words.
 */
static void
canon_one(unsigned char *o)
{
	live_refresh();
	mask_word(o, o, 0x018);
	mask_live(o, o, SHAPER_LO, SHAPER_HI);
}

/*
 * WHAT THE TAIL-FILL DETECTOR COUNTS, and it reports its denominator for
 * finding 134's reason.  A row shorter than 128 must come back ZEROED to index
 * 127 and not left holding the seed, which is only observable where the seed
 * put a nonzero byte there in the first place.  `saw` counts the bytes for
 * which that was true and which did come back zero; `had` counts the ones the
 * seed made observable at all.  If `had` is zero the check proved nothing and
 * the aggregate assertion below says so.
 */
static void
count_tail(const unsigned char *after, const unsigned char *seed,
	   const struct reset_case *c, long *had, long *saw)
{
	unsigned int i, j;

	for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++)
		for (j = c->size[i]; j < V90MAPPER_LEVELS; j++) {
			unsigned off = 0x056u
			    + 2u * (i * V90MAPPER_LEVELS + j);
			int k;

			for (k = 0; k < 2; k++) {
				if (seed[off + k] == 0)
					continue;
				(*had)++;
				if (after[off + k] == 0)
					(*saw)++;
			}
		}
}

static int
run_mapper_reset(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
		 dtor our_d, dtor ref_d, map_reset our_r, map_reset ref_r,
		 int spectral)
{
	long tail_had = 0, tail_saw = 0;
	int trial, ci, moved = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++)
		for (ci = 0; ci < NRESET; ci++) {
		const struct reset_case *c = &reset_cases[ci];
		unsigned char post_ctor[MAPPER_SLOT];
		int a_allocs, a_frees, b_allocs, b_frees;
		long tag = (long)(ci * 100 + trial);

		seed_trial(trial);
		build_mp(c, trial & 1);
		harness_alloc_reset();

		our_c(map_a, PARAMS);
		ref_c(map_b, PARAMS);

		/* See the `pokes` table: the constructor's zeroes hide five
		 * of the stores these two functions make. */
		poke_fields(map_a);
		poke_fields(map_b);

		memcpy(post_ctor, map_a, MAPPER_SLOT);

		a_allocs = harness_alloc.allocs;
		a_frees = harness_alloc.frees;
		our_r(map_a, RST_MP, c->pcm);
		a_allocs = harness_alloc.allocs - a_allocs;
		a_frees = harness_alloc.frees - a_frees;

		b_allocs = harness_alloc.allocs;
		b_frees = harness_alloc.frees;
		ref_r(map_b, RST_MP, c->pcm);
		b_allocs = harness_alloc.allocs - b_allocs;
		b_frees = harness_alloc.frees - b_frees;

		live_refresh();

		diff_eq_int("reset allocated nothing (case %ld)", a_allocs, 0,
			    tag);
		diff_eq_int("ref reset allocated nothing (case %ld)", b_allocs,
			    0, tag);
		diff_eq_int("reset freed nothing (case %ld)", a_frees, 0, tag);
		diff_eq_int("ref reset freed nothing (case %ld)", b_frees, 0,
			    tag);
		diff_eq_int("no bad free (case %ld)", harness_alloc.bad_free, 0,
			    tag);

		compare_mapper("after reset", map_a, map_b, tag);
		diff_eq_int("nothing stored past the object (case %ld)",
			    guard_intact(map_a, map_b, map_seed, MAPPER_SIZE,
					 MAPPER_SLOT), 1, tag);

		/*
		 * `+0x0c` and the four fields beside it belong to `reset`
		 * alone.  Asserting on OUR side that `resetNoSpectral` left
		 * the poked value there is a stronger claim than the two sides
		 * agreeing: it says the read is a read.
		 */
		if (!spectral) {
			diff_eq_int("resetNoSpectral did not write +0x0c "
				    "(case %ld)",
				    (int)peek(map_a, 0x00c, 4),
				    (int)POKE_00C, tag);
			diff_eq_int("word_08 is bitsPerFrame minus what it "
				    "found at +0x0c (case %ld)",
				    (int)peek(map_a, 0x008, 4),
				    (int)(c->word0 - POKE_00C), tag);
			diff_eq_int("resetNoSpectral did not write +0x10 "
				    "(case %ld)",
				    (int)peek(map_a, 0x010, 4), 0x5151, tag);
			diff_eq_int("resetNoSpectral did not write +0x14 "
				    "(case %ld)",
				    (int)peek(map_a, 0x014, 4), 0x6262, tag);
			diff_eq_int("resetNoSpectral did not write +0x1c "
				    "(case %ld)",
				    (int)peek(map_a, 0x01c, 4), 0x7373, tag);
			diff_eq_int("resetNoSpectral did not write +0x6f8 "
				    "(case %ld)",
				    (int)peek(map_a, 0x6f8, 4), 0x8484, tag);
		} else {
			diff_eq_int("reset zeroed +0x1c (case %ld)",
				    (int)peek(map_a, 0x01c, 4), 0, tag);
			diff_eq_int("reset set +0x10 to shaperSR (case %ld)",
				    (int)peek(map_a, 0x010, 4), c->sr, tag);
			diff_eq_int("reset set +0x14 to the block length "
				    "(case %ld)", (int)peek(map_a, 0x014, 4),
				    c->sr ? 6 / c->sr : 0, tag);
			diff_eq_int("reset set +0x6f8 (case %ld)",
				    (int)peek(map_a, 0x6f8, 4),
				    c->sr ? (int)c->id : 0, tag);
		}

		/* Both of them clear these two, whatever was there. */
		diff_eq_int("the sign encoder was cleared (case %ld)",
			    (int)peek(map_a, 0x6fc, 1), 0, tag);
		diff_eq_int("+0x700 was cleared (case %ld)",
			    (int)peek(map_a, 0x700, 4), 0, tag);

		count_tail(map_a, map_seed, c, &tail_had, &tail_saw);

		if (memcmp(post_ctor, map_a, MAPPER_SLOT) != 0)
			moved = 1;

		our_d(map_a);
		ref_d(map_b);
		diff_eq_int("nothing left allocated (case %ld)",
			    harness_alloc.live, 0, tag);
		}

	diff_eq_int("the reset changed the object", moved, 1, 0);

	/*
	 * The tail-fill detector's denominator, printed whether it fires or
	 * not: a run in which the seed never put a nonzero byte past a short
	 * row proved nothing about the fill, and would otherwise be
	 * indistinguishable from one in which it did.
	 */
	diff_eq_int("the seed made the tail-fill observable (%ld bytes)",
		    tail_had > 0, 1, tail_had);
	diff_eq_int("every observable tail byte came back zeroed (%ld seen)",
		    (int)(tail_had - tail_saw), 0, tail_saw);

	return diff_end();
}

/*
 * `pcm` IS TESTED FOR NONZERO AND NOT COMPARED, and this is the check that
 * says so rather than assuming it.  The object's arm selector is
 * `test %ebx,%ebx ; jne`, so 2, 0xff and 0x100 must all produce exactly what 1
 * produces -- and 0x100 is the sharpest of the three, because a byte-wide test
 * would read it as zero and take the mu-law arm.
 *
 * The claim is made on BOTH sides separately, against that side's own pcm=1
 * result, because it is a claim about one function's branch and not about the
 * two agreeing.
 */
static const int pcm_nonzero[] = { 1, 2, 0xff, 0x100, 0x7fffffff };
#define NPCM	((int)(sizeof(pcm_nonzero) / sizeof(pcm_nonzero[0])))

static int
run_mapper_pcm(const char *name, mapper_ctor our_c, mapper_ctor ref_c,
	       dtor our_d, dtor ref_d, map_reset our_r, map_reset ref_r)
{
	static unsigned char base_a[MAPPER_SIZE], base_b[MAPPER_SIZE];
	static unsigned char mu_a[MAPPER_SIZE], mu_b[MAPPER_SIZE];
	int ci, k, differ = 0;

	diff_begin(name);

	for (ci = 0; ci < NRESET; ci++) {
		/*
		 * The mu-law arm over the same block, taken first and in a
		 * construction of its own so the allocator's state is the same
		 * at the start of every run below.  `seed_trial` and `build_mp`
		 * are deterministic in `ci`, so all NPCM + 1 runs of this case
		 * see byte-for-byte the same mapper seed and the same mapping
		 * block.
		 */
		seed_trial(ci);
		build_mp(&reset_cases[ci], 1);
		harness_alloc_reset();
		our_c(map_a, PARAMS);
		ref_c(map_b, PARAMS);
		our_r(map_a, RST_MP, 0);
		ref_r(map_b, RST_MP, 0);
		memcpy(mu_a, map_a, MAPPER_SIZE);
		memcpy(mu_b, map_b, MAPPER_SIZE);
		canon_one(mu_a);
		canon_one(mu_b);
		our_d(map_a);
		ref_d(map_b);

		for (k = 0; k < NPCM; k++) {
			long tag = (long)pcm_nonzero[k];
			unsigned char now_a[MAPPER_SIZE], now_b[MAPPER_SIZE];

			seed_trial(ci);
			build_mp(&reset_cases[ci], 1);
			harness_alloc_reset();
			our_c(map_a, PARAMS);
			ref_c(map_b, PARAMS);
			our_r(map_a, RST_MP, pcm_nonzero[k]);
			ref_r(map_b, RST_MP, pcm_nonzero[k]);

			memcpy(now_a, map_a, MAPPER_SIZE);
			memcpy(now_b, map_b, MAPPER_SIZE);
			canon_one(now_a);
			canon_one(now_b);

			if (k == 0) {
				memcpy(base_a, now_a, MAPPER_SIZE);
				memcpy(base_b, now_b, MAPPER_SIZE);
			} else {
				diff_eq_int("pcm %ld maps as pcm 1 does",
					    memcmp(base_a, now_a,
						   MAPPER_SIZE) == 0, 1, tag);
				diff_eq_int("ref: pcm %ld maps as pcm 1 does",
					    memcmp(base_b, now_b,
						   MAPPER_SIZE) == 0, 1, tag);
			}

			/*
			 * And the two arms are not one arm: over a block with
			 * entries in it, mu-law must differ from A-law, or the
			 * check above would pass on a function that ignored
			 * `pcm` altogether.  Aggregated, because the case with
			 * six empty constellations legitimately agrees.
			 */
			if (memcmp(mu_a, now_a, MAPPER_SIZE) != 0
			    && memcmp(mu_b, now_b, MAPPER_SIZE) != 0)
				differ++;

			our_d(map_a);
			ref_d(map_b);
			diff_eq_int("nothing left allocated (pcm %ld)",
				    harness_alloc.live, 0, tag);
		}
	}

	diff_eq_int("mu-law and A-law differ, on both sides (%ld runs)",
		    differ > 0, 1, differ);

	return diff_end();
}

/* ------------------------------------------------------ V90Mapper::process */

extern "C" {
/*
 * The reference parameter is spelled `unsigned int *` for the same reason
 * `PcmType` is spelled `int` above: it is one cdecl stack slot either way, the
 * asm() label is exact, and the caller wants to read the answer back.
 */
void our_map_process(void *, unsigned char *, unsigned int, short *,
		     unsigned int *) asm("_ZN9V90Mapper7processEPhjPsRj");
void ref_map_process(void *, unsigned char *, unsigned int, short *,
		     unsigned int *) asm("ref__ZN9V90Mapper7processEPhjPsRj");
}

/*
 * WHAT BOUNDS A `process` CASE, and why it needs its own table rather than
 * `reset_cases`.  Three of those eight would fault rather than fail:
 *
 *   - `ModulusEncoder::progress` DIVIDES by `constellationSize[0..4]`, so a
 *     zero-sized constellation is a division by zero and not a test.  Cases 0,
 *     3 and 4 all have one.
 *   - The digit that comes back is used as `constellation[k][codes[k]]` with
 *     no bound of any kind -- the object forms `(k << 7) + codes[k]` and loads
 *     -- and the SIXTH digit is the accumulator's whole remaining quotient.
 *     So a bit string that does not fit in `128 * size[0] * .. * size[4]`
 *     reads outside the object.  `proc_case_safe` is that arithmetic and it is
 *     asserted per case rather than assumed.
 *   - `bitsPerFrame` must be at most 0x50, the size of `buf`, and at least
 *     `signBitsPerFrame`; the table gives the MODULUS bit count and the frame
 *     size is derived, so neither can drift.
 *
 * `shaperId` is capped at 3 by `V90SpectralShaper` and not by this class:
 * `actionLookupTable` has eight rows indexed `state + 2 * shaperId`, and
 * `delayLine` holds 24 entries against a `(shaperId + 1) * blockLength`
 * window.
 */
struct proc_case {
	unsigned int size[V90MAPPER_CONSTELLATIONS];
	int	     sr;		/* mp->shaperSR                     */
	unsigned int id;		/* mp->shaperId                     */
	unsigned int payload;		/* word_08, the modulus bit count   */
	int	     pcm;
};

static const struct proc_case proc_cases[] = {
	/* No shaper at all: signBitGroups is zero and the six sign bits go
	 * through the serial differential encoder instead. */
	{ { 128, 128, 128, 128, 128, 128 }, 0, 0, 24u, 0 },
	{ {  17,   9,  33,   5, 128,  64 }, 0, 2, 12u, 1 },

	/* One group of six: the widest shaper frame there is. */
	{ { 128, 128, 128, 128, 128, 128 }, 1, 0, 30u, 0 },
	{ { 128, 128, 128, 128, 128, 128 }, 1, 1, 30u, 1 },
	{ {  64,  32,  16,   8,   4, 128 }, 1, 3, 16u, 0 },

	/* Two groups of three, three of two, six of one. */
	{ { 128, 128, 128, 128, 128, 128 }, 2, 1, 28u, 0 },
	{ { 100,  60,  20,  10,   5,  99 }, 2, 3, 20u, 1 },
	{ { 128, 128, 128, 128, 128, 128 }, 3, 2, 26u, 0 },
	{ {   2,   3,   4,   5,   6,   7 }, 3, 1, 10u, 1 },
	{ { 128, 128, 128, 128, 128, 128 }, 6, 3, 22u, 0 },
	{ {  40,  40,  40,  40,  40,  40 }, 6, 0,  8u, 0xff }
};
#define NPROC	((int)(sizeof(proc_cases) / sizeof(proc_cases[0])))

#define PROC_BITS	1024u
#define PROC_SYMS	128u
#define PROC_SYMBYTES	(2u * PROC_SYMS)
#define PROC_SLOT	(PROC_SYMBYTES + GUARD)

static unsigned char proc_bits[PROC_BITS];
static unsigned char sym_a[PROC_SLOT] __attribute__((aligned(8)));
static unsigned char sym_b[PROC_SLOT] __attribute__((aligned(8)));
static unsigned char sym_seed[PROC_SLOT];

static unsigned int
proc_frame_bits(const struct proc_case *c)
{
	return c->payload + (V90MAPPER_FRAME - (unsigned int)c->sr);
}

/*
 * Can the sixth modulus digit reach outside its row?  `progress` reads
 * `payload` bits as one integer, divides it by the first five constellation
 * sizes and hands the rest over as `codes[5]`, which is then used as an index
 * into a 128-entry row.  So the case is safe exactly when
 * `2^payload <= 128 * size[0] * .. * size[4]`.
 */
static int
proc_case_safe(const struct proc_case *c)
{
	unsigned long long room = (unsigned long long)V90MAPPER_LEVELS;
	unsigned int bits = proc_frame_bits(c);
	int i;

	if (c->payload >= 63u || bits > 0x50u)
		return 0;
	for (i = 0; i < 5; i++) {
		if (c->size[i] == 0u || c->size[i] > V90MAPPER_LEVELS)
			return 0;
		room *= (unsigned long long)c->size[i];
	}
	return (1ULL << c->payload) <= room;
}

/*
 * The mapping block a `process` case is driven from.  It is `build_mp`'s work
 * plus one thing that block cannot be left random for: THE FOUR SHAPER
 * COEFFICIENTS.  `V90SpectralShaper::reset` hands them to the shaping filter
 * and `advanceTrellis` runs that filter over the delay line, so LFSR bytes
 * reinterpreted as `float` are a signalling NaN or a 10^38 as often as not --
 * and NaN semantics are exactly where the modern compiler is allowed to differ
 * from the object (CLAUDE.md's declared divergences).  Exact binary fractions
 * keep the arithmetic away from that without making it trivial.
 */
static void
build_mp_proc(const struct proc_case *c, int sweep)
{
	unsigned int i;

	fill(rst_mp, rst_mp, (unsigned char *)0, sizeof(rst_mp), 0);

	for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++) {
		unsigned int j;

		for (j = 0; j < V90MAPPER_LEVELS; j++)
			RST_MP->constellation[i][j] = sweep
			    ? (unsigned char)(i * V90MAPPER_LEVELS + j)
			    : next_byte();
		RST_MP->constellationSize[i] = c->size[i];
	}

	RST_MP->word_0 = proc_frame_bits(c);
	RST_MP->shaperSR = c->sr;
	RST_MP->shaperId = c->id;
	RST_MP->shaperA1 = 0.5f;
	RST_MP->shaperA2 = -0.25f;
	RST_MP->shaperB1 = 0.125f;
	RST_MP->shaperB2 = -0.0625f;
}

/* One bit per byte, and every fourth trial hands over whole bytes instead. */
static void
fill_bits(unsigned int n, int wide)
{
	unsigned int i;

	for (i = 0; i < PROC_BITS; i++)
		proc_bits[i] = wide ? next_byte()
				    : (unsigned char)(next_byte() & 1u);
	(void)n;
}

/*
 * How many symbols the whole priming countdown swallows, stated INDEPENDENTLY
 * of `V90Mapper::process`: it is `V90BitsToSymbol::reset`'s `extraSymbols`,
 * `6 * shaperId / shaperSR`, computed by a different function in a different
 * class out of the same two mapping parameters.  Asserting the mapper's three
 * countdown arms against it is what makes the decode of that tail a
 * measurement rather than a reading -- finding 7422.
 */
static unsigned int
proc_expected_suppressed(const struct proc_case *c)
{
	if (c->sr == 0)
		return 0u;
	return V90MAPPER_FRAME * c->id / (unsigned int)c->sr;
}

static int
run_mapper_process(void)
{
	int trial, ci;
	long saw_full = 0, saw_partial = 0, saw_subtract = 0, saw_carry = 0;
	long saw_overfull = 0;

	diff_begin("V90Mapper::process");

	for (trial = 0; trial < NTRIAL; trial++)
		for (ci = 0; ci < NPROC; ci++) {
		const struct proc_case *c = &proc_cases[ci];
		unsigned int frame = proc_frame_bits(c);
		long tag = (long)(ci * 100 + trial);
		int call;

		diff_eq_int("case %ld keeps the modulus digits inside the row",
			    proc_case_safe(c), 1, (long)ci);
		if (!proc_case_safe(c))
			continue;

		seed_trial(trial);
		build_mp_proc(c, trial & 1);
		harness_alloc_reset();

		our_mapper_c1(map_a, PARAMS);
		ref_mapper_c1(map_b, PARAMS);
		our_map_reset(map_a, RST_MP, c->pcm);
		ref_map_reset(map_b, RST_MP, c->pcm);

		/*
		 * FIVE CALLS, AND THE LENGTHS ARE NOT MULTIPLES OF THE FRAME.
		 * `bitsBuffered` is reduced by `bitsPerFrame` rather than
		 * cleared, so a call that ends part-way through a frame must
		 * leave those bits in `buf` for the next one; a `process` that
		 * cleared the count instead would agree with the blob on every
		 * call whose length divides.  The zero-length call is the
		 * object's `cmp %eax,%ebp ; jae` exit at 0x30439.
		 */
		for (call = 0; call < 5; call++) {
			static const unsigned int mul[5] = { 0u, 1u, 3u, 2u,
							     4u };
			static const unsigned int add[5] = { 0u, 0u, 1u, 3u,
							     0u };
			unsigned int nb = mul[call] * frame + add[call];
			unsigned int na = 0xa5a5a5a5u, nbo = 0x5a5a5a5au;
			unsigned int before, after;
			int allocs, frees;

			if (nb > PROC_BITS)
				nb = PROC_BITS;

			fill_bits(nb, (trial & 3) == 3);
			fill(sym_a, sym_b, sym_seed, PROC_SLOT,
			     (trial + call) & 3);

			/*
			 * THE LAST CALL STARTS WITH THE BUFFER OVER-FULL, and
			 * that is the only way to tell `bitsBuffered -=
			 * bitsPerFrame` from `bitsBuffered = 0`.  Everywhere
			 * else the two agree exactly: the count is raised one
			 * bit at a time and the frame is run the moment it
			 * REACHES `bitsPerFrame`, so the subtraction always
			 * has an identical operand pair and always yields
			 * zero.  It differs only when the count arrives at
			 * `process` already at or above a frame, which no
			 * reachable sequence of `reset` and `process` produces
			 * -- so it is poked, on both sides, exactly as finding
			 * 7423's copy arm is.  `frame + 4` stays inside the
			 * 0x50-byte buffer for every case in the table.
			 */
			if (call == 4) {
				unsigned int over = frame + 3u;

				memcpy(map_a + 0x01c, &over, sizeof(over));
				memcpy(map_b + 0x01c, &over, sizeof(over));
				saw_overfull++;
			}

			before = peek(map_a, 0x6f8, 4);
			allocs = harness_alloc.allocs;
			frees = harness_alloc.frees;

			our_map_process(map_a, proc_bits, nb,
					(short *)(void *)sym_a, &na);
			ref_map_process(map_b, proc_bits, nb,
					(short *)(void *)sym_b, &nbo);

			allocs = harness_alloc.allocs - allocs;
			frees = harness_alloc.frees - frees;
			after = peek(map_a, 0x6f8, 4);

			live_refresh();

			diff_eq_int("nofSymbols matches the blob (case %ld)",
				    (int)na, (int)nbo, tag);
			diff_eq_obj_(__FILE__, __LINE__,
				     "the symbols and the guard past them",
				     "short[]", sym_a, sym_b,
				     (size_t)PROC_SLOT, tag);
			compare_mapper("after process", map_a, map_b, tag);
			diff_eq_int("nothing stored past the mapper "
				    "(case %ld)",
				    guard_intact(map_a, map_b, map_seed,
						 MAPPER_SIZE, MAPPER_SLOT), 1,
				    tag);
			diff_eq_int("nothing stored past the symbols "
				    "(case %ld)",
				    guard_intact(sym_a, sym_b, sym_seed,
						 PROC_SYMBYTES, PROC_SLOT), 1,
				    tag);
			diff_eq_int("process allocated nothing (case %ld)",
				    allocs, 0, tag);
			diff_eq_int("process freed nothing (case %ld)", frees,
				    0, tag);

			/*
			 * Which arm of the countdown ran, so the aggregate
			 * below can say all three were driven rather than
			 * leaving that to the case table's good intentions.
			 */
			if (nb >= frame) {
				if (before == 0u)
					saw_full++;
				else if (after == 0u)
					saw_partial++;
				else
					saw_subtract++;
			}
			if (nb % frame != 0u)
				saw_carry++;
		}

		/*
		 * THE COUNTDOWN'S TOTAL, AGAINST `V90BitsToSymbol::reset`.
		 * Eight frames in one call is more than any countdown here
		 * needs -- it ends after at most `shaperId` subtractions and
		 * one partial frame -- so the symbols missing from `8 * 6` are
		 * exactly the ones the priming swallowed.  Driven over a
		 * freshly reset object so the countdown starts at `shaperId`.
		 */
		{
			unsigned int na = 0u, nbo = 1u;

			our_map_reset(map_a, RST_MP, c->pcm);
			ref_map_reset(map_b, RST_MP, c->pcm);
			fill_bits(8u * frame, 0);
			fill(sym_a, sym_b, sym_seed, PROC_SLOT, 0);

			our_map_process(map_a, proc_bits, 8u * frame,
					(short *)(void *)sym_a, &na);
			ref_map_process(map_b, proc_bits, 8u * frame,
					(short *)(void *)sym_b, &nbo);
			live_refresh();

			diff_eq_int("eight frames match the blob (case %ld)",
				    (int)na, (int)nbo, tag);
			diff_eq_obj_(__FILE__, __LINE__, "eight frames out",
				     "short[]", sym_a, sym_b,
				     (size_t)PROC_SLOT, tag);
			diff_eq_int("the countdown swallowed extraSymbols "
				    "(case %ld)",
				    (int)(8u * V90MAPPER_FRAME - na),
				    (int)proc_expected_suppressed(c), tag);
			diff_eq_int("the countdown finished (case %ld)",
				    (int)peek(map_a, 0x6f8, 4), 0, tag);
			diff_eq_int("nothing stored past the mapper over "
				    "eight frames (case %ld)",
				    guard_intact(map_a, map_b, map_seed,
						 MAPPER_SIZE, MAPPER_SLOT), 1,
				    tag);
			diff_eq_int("nothing stored past the symbols over "
				    "eight frames (case %ld)",
				    guard_intact(sym_a, sym_b, sym_seed,
						 PROC_SYMBYTES, PROC_SLOT), 1,
				    tag);
		}

		our_mapper_d1(map_a);
		ref_mapper_d1(map_b);
		diff_eq_int("nothing left allocated (case %ld)",
			    harness_alloc.live, 0, tag);
		}

	diff_eq_int("the full-frame arm was driven (%ld calls)", saw_full > 0,
		    1, saw_full);
	diff_eq_int("the partial arm was driven (%ld calls)", saw_partial > 0,
		    1, saw_partial);
	diff_eq_int("the subtract arm was driven (%ld calls)",
		    saw_subtract > 0, 1, saw_subtract);
	diff_eq_int("a part-filled frame was carried over (%ld calls)",
		    saw_carry > 0, 1, saw_carry);
	diff_eq_int("the buffer was driven over-full (%ld calls)",
		    saw_overfull > 0, 1, saw_overfull);

	return diff_end();
}

/*
 * THE ARM NO `reset` CAN REACH, and finding 7423 is why it is driven by hand.
 *
 * The object's partial-copy arm computes `start = uint_6f8 * signBitGroupSize`
 * and guards the copy with `cmp $0x5,%edx ; ja` -- but the block it jumps to
 * updates `nofOut` by `6 - start` ANYWAY.  Written with the count advanced
 * inside the loop instead, a skipped loop would leave `nofOut` alone, and the
 * two spellings agree over every state a `reset` can produce: that arm needs
 * `0 < uint_6f8 < signBitGroups`, which bounds `start` at `6 - signBitGroupSize`
 * and so at 5.
 *
 * So the three fields are poked directly, on BOTH sides, after a reset that
 * leaves the spectral shaper properly set up.  `signBitGroupSize` is left as
 * the reset computed it (6, from `shaperSR` of 1) and only the group COUNT and
 * the countdown are moved, which keeps every pointer the shaper loop forms
 * inside the object: with three groups of six the second and third frames land
 * on `signs` and on `constellation`'s first rows rather than outside the
 * allocation.  Both sides do exactly the same thing to exactly the same
 * bytes, and the answer is asserted absolutely as well as differentially --
 * `6 - 12` as an unsigned int is 0xfffffffa, and nothing else is.
 */
static int
run_mapper_process_arm(void)
{
	static const struct proc_case arm_case =
	    { { 128, 128, 128, 128, 128, 128 }, 1, 0, 20u, 0 };
	unsigned int frame = proc_frame_bits(&arm_case);
	int trial;

	diff_begin("V90Mapper::process, the unreachable copy arm");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int na = 0u, nbo = 1u;
		unsigned int poke_groups = 3u, poke_count = 2u;

		seed_trial(trial);
		build_mp_proc(&arm_case, trial & 1);
		harness_alloc_reset();

		our_mapper_c1(map_a, PARAMS);
		ref_mapper_c1(map_b, PARAMS);
		our_map_reset(map_a, RST_MP, arm_case.pcm);
		ref_map_reset(map_b, RST_MP, arm_case.pcm);

		diff_eq_int("the reset left signBitGroupSize at six (%ld)",
			    (int)peek(map_a, 0x014, 4), 6, (long)trial);

		memcpy(map_a + 0x010, &poke_groups, sizeof(poke_groups));
		memcpy(map_b + 0x010, &poke_groups, sizeof(poke_groups));
		memcpy(map_a + 0x6f8, &poke_count, sizeof(poke_count));
		memcpy(map_b + 0x6f8, &poke_count, sizeof(poke_count));

		fill_bits(frame, 0);
		fill(sym_a, sym_b, sym_seed, PROC_SLOT, trial & 3);

		our_map_process(map_a, proc_bits, frame,
				(short *)(void *)sym_a, &na);
		ref_map_process(map_b, proc_bits, frame,
				(short *)(void *)sym_b, &nbo);
		live_refresh();

		diff_eq_int("nofSymbols matches the blob (trial %ld)", (int)na,
			    (int)nbo, (long)trial);
		diff_eq_int("nofSymbols is 6 - 12 as an unsigned int "
			    "(trial %ld)", (int)na, (int)(6u - 12u),
			    (long)trial);
		diff_eq_obj_(__FILE__, __LINE__, "nothing was copied out",
			     "short[]", sym_a, sym_b, (size_t)PROC_SLOT,
			     (long)trial);
		compare_mapper("after the skipped copy", map_a, map_b,
			       (long)trial);
		diff_eq_int("the poked shaper loop stayed inside the object "
			    "(trial %ld)",
			    guard_intact(map_a, map_b, map_seed, MAPPER_SIZE,
					 MAPPER_SLOT), 1, (long)trial);
		diff_eq_int("nothing stored past the symbols (trial %ld)",
			    guard_intact(sym_a, sym_b, sym_seed, PROC_SYMBYTES,
					 PROC_SLOT), 1, (long)trial);
		diff_eq_int("the countdown still ended (trial %ld)",
			    (int)peek(map_a, 0x6f8, 4), 0, (long)trial);

		our_mapper_d1(map_a);
		ref_mapper_d1(map_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, (long)trial);
	}

	return diff_end();
}


/*
 * THE BOUNDARY BETWEEN THE SECOND AND THIRD ARMS, which is `jb` at 0x3052e and
 * therefore STRICT.  Written `<=` instead, the two arms agree for every
 * `shaperSR` that divides six: with `+0x6f8` equal to `signBitGroups` the
 * partial arm's `start` is `signBitGroups * signBitGroupSize`, which is exactly
 * 6, so it copies nothing, adds nothing to `nofOut` and clears the countdown --
 * letter for letter what the subtract arm does when the two are equal.
 *
 * `shaperSR` OF 4 IS WHAT SEPARATES THEM.  `signBitGroupSize` is `6 / 4` = 1,
 * so four groups cover four of the six samples and `start` comes out at 4
 * rather than 6: the inclusive spelling would copy `samples[4]` and
 * `samples[5]` and answer 2 where the object answers 0.  `reset` cannot put
 * `+0x6f8` equal to `signBitGroups` on its own -- it puts `shaperId` there and
 * the two parameters are independent -- so the countdown is poked to 4 after a
 * reset that built the shaper for `shaperId` of 1.  One frame is driven, which
 * is fewer than `primeFrames`, and the answer is asserted absolutely: the
 * object emits NOTHING on this arm.
 */
static int
run_mapper_process_boundary(void)
{
	static const struct proc_case bound_case =
	    { { 128, 128, 128, 128, 128, 128 }, 4, 1, 20u, 0 };
	unsigned int frame = proc_frame_bits(&bound_case);
	int trial;

	diff_begin("V90Mapper::process, the countdown boundary is strict");

	diff_eq_int("the boundary case keeps the digits inside the row",
		    proc_case_safe(&bound_case), 1, 0);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int na = 0u, nbo = 1u;
		unsigned int equal = 4u;

		seed_trial(trial);
		build_mp_proc(&bound_case, trial & 1);
		harness_alloc_reset();

		our_mapper_c1(map_a, PARAMS);
		ref_mapper_c1(map_b, PARAMS);
		our_map_reset(map_a, RST_MP, bound_case.pcm);
		ref_map_reset(map_b, RST_MP, bound_case.pcm);

		diff_eq_int("the reset left signBitGroups at four (%ld)",
			    (int)peek(map_a, 0x010, 4), 4, (long)trial);
		diff_eq_int("the reset left signBitGroupSize at one (%ld)",
			    (int)peek(map_a, 0x014, 4), 1, (long)trial);

		memcpy(map_a + 0x6f8, &equal, sizeof(equal));
		memcpy(map_b + 0x6f8, &equal, sizeof(equal));

		fill_bits(frame, 0);
		fill(sym_a, sym_b, sym_seed, PROC_SLOT, trial & 3);

		our_map_process(map_a, proc_bits, frame,
				(short *)(void *)sym_a, &na);
		ref_map_process(map_b, proc_bits, frame,
				(short *)(void *)sym_b, &nbo);
		live_refresh();

		diff_eq_int("nofSymbols matches the blob (trial %ld)", (int)na,
			    (int)nbo, (long)trial);
		diff_eq_int("the equal case emits nothing (trial %ld)",
			    (int)na, 0, (long)trial);
		diff_eq_int("the countdown lost exactly signBitGroups "
			    "(trial %ld)", (int)peek(map_a, 0x6f8, 4), 0,
			    (long)trial);
		diff_eq_obj_(__FILE__, __LINE__, "nothing was copied out",
			     "short[]", sym_a, sym_b, (size_t)PROC_SLOT,
			     (long)trial);
		compare_mapper("after the subtract arm", map_a, map_b,
			       (long)trial);
		diff_eq_int("nothing stored past the mapper (trial %ld)",
			    guard_intact(map_a, map_b, map_seed, MAPPER_SIZE,
					 MAPPER_SLOT), 1, (long)trial);
		diff_eq_int("nothing stored past the symbols (trial %ld)",
			    guard_intact(sym_a, sym_b, sym_seed, PROC_SYMBYTES,
					 PROC_SLOT), 1, (long)trial);

		our_mapper_d1(map_a);
		ref_mapper_d1(map_b);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, (long)trial);
	}

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


/* --------------------------- V90BitsToSymbol::reset, ::resetNoSpectral */

extern "C" {
void our_bts_reset(void *, V90MappingParams *, int)
	asm("_ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType");
void ref_bts_reset(void *, V90MappingParams *, int)
	asm("ref__ZN15V90BitsToSymbol5resetEP16V90MappingParams7PcmType");
void our_bts_resetns(void *, V90MappingParams *, int)
	asm("_ZN15V90BitsToSymbol15resetNoSpectralEP16V90MappingParams7PcmType");
void ref_bts_resetns(void *, V90MappingParams *, int)
	asm("ref__ZN15V90BitsToSymbol15resetNoSpectralEP16V90MappingParams7PcmType");
}

typedef void (*bts_reset)(void *, V90MappingParams *, int);

/*
 * THE SENTINELS THIS CLASS NEEDS ARE A DIFFERENT SET FROM THE MAPPER'S, and
 * which fields are in it is the whole content of the header's remark that
 * `bitsPerFrame` and `extraSymbols` are the two the CONSTRUCTOR leaves alone.
 *
 * `symbolsDone`, `symbolsBlockSize` and `extraSymbolsPending` are written by
 * the constructor with exactly the values `reset` writes -- 0, 0 and 1 -- so
 * finding 7105's masking applies to all three and each needs a value the
 * function could not have produced.  0x01 is what seed mode 1 puts everywhere,
 * so the flag byte's sentinel has to be neither 0 nor 1.
 *
 * `bitsPerFrame` and `extraSymbols` need NOTHING: the constructor does not
 * touch either, so the seed reaches them and a dropped store leaves varied
 * pseudorandom bytes.  That is also what lets `resetNoSpectral` be pinned as
 * writing the first and not the second -- the assertion below is against the
 * SEED, on our side, and no store can pass it by accident.
 */
static const struct {
	unsigned int off;
	unsigned int val;
	unsigned int width;
} bts_pokes[] = {
	{ 0x10u, 0x31313131u, 4u },	/* symbolsDone         */
	{ 0x1cu, 0x32323232u, 4u },	/* symbolsBlockSize    */
	{ 0x20u, 0x00a5u,     1u }	/* extraSymbolsPending */
};
#define NBTSPOKE ((int)(sizeof(bts_pokes) / sizeof(bts_pokes[0])))

static void
poke_bts(unsigned char *o)
{
	int i;

	for (i = 0; i < NBTSPOKE; i++)
		memcpy(o + bts_pokes[i].off, &bts_pokes[i].val,
		       bts_pokes[i].width);
}

static unsigned int
bts_expected_extra(const struct reset_case *c)
{
	if (c->sr == 0)
		return 0u;
	return V90MAPPER_FRAME * c->id / (unsigned int)c->sr;
}

/*
 * `spectral` selects which of the two is under test.  Both hand their two
 * arguments straight to the same-named member of the mapper they own, so the
 * mapper is compared as well as the 0x24 -- an argument dropped or swapped on
 * the way through shows up there and nowhere else.
 */
static int
run_bts_reset(const char *name, bts_reset our_r, bts_reset ref_r, int spectral)
{
	int trial, ci, moved = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++)
		for (ci = 0; ci < NRESET; ci++) {
		const struct reset_case *c = &reset_cases[ci];
		unsigned int n = bts_n[(trial + ci) % (int)(sizeof(bts_n)
							   / sizeof(bts_n[0]))];
		unsigned char post_ctor[BTS_SLOT];
		unsigned char *mapper_a, *mapper_b;
		long tag = (long)(ci * 100 + trial);
		int a_allocs, a_frees;

		seed_trial(trial);
		build_mp(c, trial & 1);
		harness_alloc_reset();

		our_bts_c1(bts_a, n, PARAMS);
		ref_bts_c1(bts_b, n, PARAMS);

		mapper_a = (unsigned char *)slot_ptr(bts_a, 0x00);
		mapper_b = (unsigned char *)slot_ptr(bts_b, 0x00);

		poke_bts(bts_a);
		poke_bts(bts_b);
		poke_fields(mapper_a);
		poke_fields(mapper_b);

		memcpy(post_ctor, bts_a, BTS_SLOT);

		a_allocs = harness_alloc.allocs;
		a_frees = harness_alloc.frees;
		our_r(bts_a, RST_MP, c->pcm);
		ref_r(bts_b, RST_MP, c->pcm);
		a_allocs = harness_alloc.allocs - a_allocs;
		a_frees = harness_alloc.frees - a_frees;

		live_refresh();

		diff_eq_int("the resets allocated nothing (case %ld)", a_allocs,
			    0, tag);
		diff_eq_int("the resets freed nothing (case %ld)", a_frees, 0,
			    tag);
		diff_eq_int("no bad free (case %ld)", harness_alloc.bad_free, 0,
			    tag);

		compare_bts("after reset", bts_a, bts_b, tag);
		compare_mapper("the mapper it owns", mapper_a, mapper_b, tag);
		diff_eq_int("nothing stored past the object (case %ld)",
			    guard_intact(bts_a, bts_b, bts_seed, BTS_SIZE,
					 BTS_SLOT), 1, tag);

		/* Both of them, and it is the ONE store `resetNoSpectral`
		 * makes of its own. */
		diff_eq_int("bitsPerFrame is the mapping block's first word "
			    "(case %ld)", (int)peek(bts_a, 0x14, 4),
			    (int)c->word0, tag);

		if (spectral) {
			diff_eq_int("extraSymbols is 6 * shaperId / shaperSR "
				    "(case %ld)", (int)peek(bts_a, 0x18, 4),
				    (int)bts_expected_extra(c), tag);
			diff_eq_int("symbolsDone was cleared (case %ld)",
				    (int)peek(bts_a, 0x10, 4), 0, tag);
			diff_eq_int("symbolsBlockSize was cleared (case %ld)",
				    (int)peek(bts_a, 0x1c, 4), 0, tag);
			diff_eq_int("extraSymbolsPending was set (case %ld)",
				    (int)peek(bts_a, 0x20, 1), 1, tag);
		} else {
			/*
			 * FOUR FIELDS `resetNoSpectral` MUST NOT TOUCH, and
			 * the claim is made on our side against what was there
			 * rather than against the blob: two sides agreeing on
			 * a field neither wrote is not evidence that neither
			 * wrote it.  `extraSymbols` is checked against the
			 * SEED because nothing has written it since.
			 */
			diff_eq_int("resetNoSpectral did not write "
				    "extraSymbols (case %ld)",
				    (int)peek(bts_a, 0x18, 4),
				    (int)peek(bts_seed, 0x18, 4), tag);
			diff_eq_int("resetNoSpectral did not write "
				    "symbolsDone (case %ld)",
				    (int)peek(bts_a, 0x10, 4), 0x31313131, tag);
			diff_eq_int("resetNoSpectral did not write "
				    "symbolsBlockSize (case %ld)",
				    (int)peek(bts_a, 0x1c, 4), 0x32323232, tag);
			diff_eq_int("resetNoSpectral did not write "
				    "extraSymbolsPending (case %ld)",
				    (int)peek(bts_a, 0x20, 1), 0xa5, tag);
		}

		if (memcmp(post_ctor, bts_a, BTS_SLOT) != 0)
			moved = 1;

		our_bts_d1(bts_a);
		ref_bts_d1(bts_b);
		diff_eq_int("nothing left allocated (case %ld)",
			    harness_alloc.live, 0, tag);
		}

	diff_eq_int("the reset changed the object", moved, 1, 0);

	return diff_end();
}

/*
 * THE DIVIDE IS `div` AND NOT `idiv`, and no case above can tell.  The object
 * builds `6 * shaperId` and divides by `shaperSR` with `f7 f3` at 0x2f919,
 * although `V90MappingParams::shaperSR` is declared `int` -- so a NEGATIVE
 * `shaperSR` is the discriminator: unsigned it is an enormous divisor and the
 * quotient is zero, signed it is `-6 * shaperId`.  The wide `shaperId` cases
 * are the other half of the same question, where `6 * shaperId` wraps 32 bits
 * before the divide rather than after it.
 *
 * NONE OF THESE OBJECTS IS EVER HANDED TO `process`.  A negative `shaperSR`
 * gives the mapper a `signBitGroupSize` of zero and the shaper a `blockLength`
 * of zero, which both resets survive because neither indexes anything by them;
 * `process` would.
 */
static const struct {
	int	     sr;
	unsigned int id;
	unsigned int word0;
} bts_div_cases[] = {
	{	  -1,	       3u, 40u },
	{	  -1,	       0u, 30u },
	{	  -6,	       1u, 26u },
	{ (int)0x80000000u,    2u, 24u },
	{	   4,	       2u, 24u },
	{	   5,	       1u, 26u },
	{	   7,	       3u, 28u },
	{     0x10000,	       2u, 32u },
	{	   6,	       7u, 20u },
	{	   1,  0x40000000u, 18u },
	{	   3,  0xffffffffu, 22u }
};
#define NBTSDIV	((int)(sizeof(bts_div_cases) / sizeof(bts_div_cases[0])))

static int
run_bts_reset_div(void)
{
	int trial, ci, saw_wrap = 0, saw_zero = 0;

	diff_begin("V90BitsToSymbol::reset, the divide is unsigned");

	for (trial = 0; trial < 4; trial++)
		for (ci = 0; ci < NBTSDIV; ci++) {
		struct reset_case rc;
		unsigned int expect;
		long tag = (long)(ci * 100 + trial);
		int i;

		for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++)
			rc.size[i] = (unsigned int)(1 + i);
		rc.sr = bts_div_cases[ci].sr;
		rc.id = bts_div_cases[ci].id;
		rc.word0 = bts_div_cases[ci].word0;
		rc.pcm = trial & 1;

		seed_trial(trial);
		build_mp(&rc, trial & 1);
		harness_alloc_reset();

		our_bts_c1(bts_a, 0x40u, PARAMS);
		ref_bts_c1(bts_b, 0x40u, PARAMS);
		poke_bts(bts_a);
		poke_bts(bts_b);
		poke_fields((unsigned char *)slot_ptr(bts_a, 0x00));
		poke_fields((unsigned char *)slot_ptr(bts_b, 0x00));

		our_bts_reset(bts_a, RST_MP, rc.pcm);
		ref_bts_reset(bts_b, RST_MP, rc.pcm);
		live_refresh();

		compare_bts("after reset", bts_a, bts_b, tag);
		compare_mapper("the mapper it owns",
			       (unsigned char *)slot_ptr(bts_a, 0x00),
			       (unsigned char *)slot_ptr(bts_b, 0x00), tag);
		diff_eq_int("nothing stored past the object (case %ld)",
			    guard_intact(bts_a, bts_b, bts_seed, BTS_SIZE,
					 BTS_SLOT), 1, tag);

		/*
		 * `bts_expected_extra` guards the zero and this must too: a
		 * later case with `sr` of 0 would divide by it here, in the
		 * FIXTURE, and a crash is not a verdict.
		 */
		diff_eq_int("the case has a divisor (case %ld)", rc.sr != 0, 1,
			    tag);
		expect = bts_expected_extra(&rc);
		diff_eq_int("extraSymbols is the UNSIGNED quotient (case %ld)",
			    (int)peek(bts_a, 0x18, 4), (int)expect, tag);

		if (rc.sr < 0) {
			diff_eq_int("a negative shaperSR gives zero, not a "
				    "negative (case %ld)",
				    (int)peek(bts_a, 0x18, 4), 0, tag);
			saw_zero++;
		}
		if (V90MAPPER_FRAME * rc.id < rc.id)
			saw_wrap++;

		our_bts_d1(bts_a);
		ref_bts_d1(bts_b);
		diff_eq_int("nothing left allocated (case %ld)",
			    harness_alloc.live, 0, tag);
		}

	diff_eq_int("a negative shaperSR was tried (%ld cases)", saw_zero > 0,
		    1, (long)saw_zero);
	diff_eq_int("6 * shaperId wrapped 32 bits (%ld cases)", saw_wrap > 0, 1,
		    (long)saw_wrap);

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

	rc |= run_mapper_reset("V90Mapper::reset", our_mapper_c1, ref_mapper_c1,
			       our_mapper_d1, ref_mapper_d1, our_map_reset,
			       ref_map_reset, 1);
	rc |= run_mapper_reset("V90Mapper::resetNoSpectral", our_mapper_c1,
			       ref_mapper_c1, our_mapper_d1, ref_mapper_d1,
			       our_map_resetns, ref_map_resetns, 0);
	rc |= run_mapper_pcm("V90Mapper::reset, pcm is a nonzero test",
			     our_mapper_c1, ref_mapper_c1, our_mapper_d1,
			     ref_mapper_d1, our_map_reset, ref_map_reset);
	rc |= run_mapper_pcm("V90Mapper::resetNoSpectral, pcm is a nonzero "
			     "test", our_mapper_c1, ref_mapper_c1,
			     our_mapper_d1, ref_mapper_d1, our_map_resetns,
			     ref_map_resetns);

	rc |= run_mapper_process();
	rc |= run_mapper_process_arm();
	rc |= run_mapper_process_boundary();

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

	rc |= run_bts_reset("V90BitsToSymbol::reset", our_bts_reset,
			    ref_bts_reset, 1);
	rc |= run_bts_reset("V90BitsToSymbol::resetNoSpectral",
			    our_bts_resetns, ref_bts_resetns, 0);
	rc |= run_bts_reset_div();

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
