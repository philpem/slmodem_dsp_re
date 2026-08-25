/*
 * t_v92modem.cpp -- differential test of `V92Modem`'s constructor and
 * destructor, all four symbol variants, against the blob.
 *
 * THE CONSTRUCTOR IS 393 BYTES OF WIRING.  It allocates five objects, calls
 * two C functions on one of them and hands SEVEN arguments to the last, six
 * of which it reads back out of itself.  Every one of those seven is a
 * pointer, and an argument in the wrong position there is invisible in every
 * one of the sub-classes' own tests -- they each see a pointer of the right
 * type holding a plausible address.  So the fixture's real subject is not
 * "the two objects agree" but "each argument reached the position the
 * disassembly puts it in", and that is checked by ADDRESS, one identity at a
 * time, on both sides.
 *
 * WHAT IS COMPARED DIRECTLY.  Everything except the four owned heap pointers
 * at +0x000 (analog side only), +0x004, +0xaa0 and +0xaa4 -- which is to say
 * `dil` at +0xa9c and `modemSide` at +0xaa8, and the 2,704-byte `V92Ja` at
 * +0x00c that NOTHING WRITES.  That span is the fixture's best anti-vacuity
 * guard: both sides carry the seed there, so a reconstruction that cleared
 * any of it fails, and the seed is varied and never zero for that reason.
 *
 * WHAT STANDS IN FOR THE POISONED POINTERS.  Three things, as in t_v92mod:
 *
 *   - `harness_alloc.allocs` and `.bytes` against the reference.  Five
 *     `sysdep_malloc(sizeof(X))` immediates plus the ten the two creators
 *     make; a wrong size anywhere moves one of the two counters, and the
 *     blob is the oracle for both.
 *   - Every piece is DEREFERENCED and compared in its own right, with only
 *     its own cross-object pointers skipped.  The allocator fills fresh
 *     memory with a fixed byte, so a field a constructor left alone is the
 *     same on both sides and a field it wrote is not.
 *   - Every stored pointer is checked non-null and distinct.
 *
 * THE ARGUMENTS ARE TWO DISTINCT SHARED BLOCKS, one `_tagModemParameters`
 * and one stand-in `tagV90DILdescriptor`, pointed at by both sides -- so the
 * ADDRESS is the witness for which field each lands in.  Separately seeded
 * per-side blocks would agree whatever was read.  `paramFile` is forced NULL
 * in the modem block: the blob's `V92Parameters::init` tail-calls
 * `loadParams` when it is set and this tree does not reconstruct that
 * (finding F879), so a non-null there would compare our omission rather than
 * this constructor.
 *
 * THE THREE ARMS OF THE SWITCH ARE ALL DRIVEN, and the third one is the
 * point.  `V92ModemSide` is declared opaque with a fixed underlying type
 * precisely so that a value outside {0, 1} is well defined; the constructor
 * and the destructor each carry an arm for one.  The set swept is complete
 * because the object partitions the type into exactly three cases -- zero,
 * one, and everything else -- and the sweep drives 0, 1, 2, 3, 0x7f and
 * 0xffffffff.  0xffffffff is not decoration: the destructor's range test is
 * `cmpl $0x1,...; jbe`, so a signed reading of the enum would take -1 for
 * legal and print nothing, and the level-2 transcript is where that shows.
 *
 * THE FIFTH ARGUMENT IS SWEPT AND MUST NOT MATTER.  `V92ComputationalMode`
 * is never read by either function, so it is varied per trial rather than
 * held constant: both sides get the same value, and a reconstruction that
 * read it would move our object and not the blob's.
 *
 * ONE GUARD CANNOT BE DRIVEN AND THE FILE SAYS SO RATHER THAN PRETENDING.
 * The destructor's `if (mappingParams)` at .text+0x139da is preceded two
 * calls earlier by `V92deleteConstellations(mappingParams)`, which
 * dereferences the same pointer unguarded.  A null +0xaa0 therefore faults
 * before the guard is reached, on the blob exactly as on ours, so the
 * subset sweep is over the FOUR pointers that can be nulled and the fifth is
 * covered only by `live == 0` and a differential free count.  Finding F1322.
 *
 * THE DESTRUCTOR DOES STORE, once.  `movl $0x0,0x8(%esi)` nulls +0x008 and
 * nothing else, on the path where it was non-null.  The check below is not
 * t_v92mod's "the destructor stored nothing" -- it is that exact statement:
 * +0x008 is zero afterwards iff it was non-null before, and every other byte
 * of the 2,796-byte slot is unchanged.  With -fno-lifetime-dse in CXXFLAGS
 * that store is real in our object too (finding F1272), so a reconstruction
 * that nulled all five or none of them fails here.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/V92Modem.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"
#include "dsplib/modem_params.h"

extern "C" {
/*
 * Both sides by symbol.  C++ has no syntax for running a constructor over
 * storage that already exists, and `OBJ = V92Modem(...)` would build a
 * temporary over uninitialised stack and copy it -- destroying the
 * seeded-never-zeroed property every check above depends on.  The enums are
 * `unsigned` here because that is what the ABI passes.
 */
void our_c1(void *, unsigned, void *, unsigned, void *, unsigned)
	asm("_ZN8V92ModemC1E12V92ModemSideP19_tagModemParametersjP19tagV90DILde"
	    "scriptor20V92ComputationalMode");
void our_c2(void *, unsigned, void *, unsigned, void *, unsigned)
	asm("_ZN8V92ModemC2E12V92ModemSideP19_tagModemParametersjP19tagV90DILde"
	    "scriptor20V92ComputationalMode");
void our_d1(void *) asm("_ZN8V92ModemD1Ev");
void our_d2(void *) asm("_ZN8V92ModemD2Ev");

void ref_c1(void *, unsigned, void *, unsigned, void *, unsigned)
	asm("ref__ZN8V92ModemC1E12V92ModemSideP19_tagModemParametersjP19tagV90D"
	    "ILdescriptor20V92ComputationalMode");
void ref_c2(void *, unsigned, void *, unsigned, void *, unsigned)
	asm("ref__ZN8V92ModemC2E12V92ModemSideP19_tagModemParametersjP19tagV90D"
	    "ILdescriptor20V92ComputationalMode");
void ref_d1(void *) asm("ref__ZN8V92ModemD1Ev");
void ref_d2(void *) asm("ref__ZN8V92ModemD2Ev");

void our_reset(void *) asm("_ZN8V92Modem5resetEv");
void ref_reset(void *) asm("ref__ZN8V92Modem5resetEv");
void our_progress(void *, int *, unsigned int *, float *, unsigned int)
	asm("_ZN8V92Modem8progressEPiRjPfj");
void ref_progress(void *, int *, unsigned int *, float *, unsigned int)
	asm("ref__ZN8V92Modem8progressEPiRjPfj");

extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;
}

typedef void (*ctor_fn)(void *, unsigned, void *, unsigned, void *, unsigned);
typedef void (*dtor_fn)(void *);

#define GUARD	64u
#define OBJSZ	((unsigned)sizeof(V92Modem))
#define SLOT	(OBJSZ + GUARD)
#define MPSZ	((unsigned)sizeof(struct _tagModemParameters))
#define PARAMSZ	((unsigned)sizeof(V92Parameters))
#define P2ISZ	((unsigned)sizeof(V92Phase2Info))
#define CPSZ	((unsigned)sizeof(V92CP))
#define PISZ	((unsigned)sizeof(struct V92ParamsInfo))
#define MODSZ	((unsigned)sizeof(V92Modulator))

/* The eight the object map fixes; the fixture reads them by number so that a
 * header edit that moved one shows up here rather than silently. */
#define OFF_MODULATOR	0x000u
#define OFF_PARAMETERS	0x004u
#define OFF_PHASE2INFO	0x008u
#define OFF_JA		0x00cu
#define OFF_DIL		0xa9cu
#define OFF_MAPPARAMS	0xaa0u
#define OFF_CP		0xaa4u
#define OFF_MODEMSIDE	0xaa8u

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));
static unsigned char sown[SLOT];
static unsigned char cmp_a[SLOT];
static unsigned char cmp_b[SLOT];
static unsigned char ctor_a[SLOT];
static unsigned char ctor_b[SLOT];

/*
 * The two pointed-to arguments, one block each and shared between the sides.
 * The descriptor is a byte array rather than a `tagV90DILdescriptor`: nothing
 * in either function dereferences it -- it is stored at +0xa9c and handed on
 * to `V92Modulator`, which also only stores it -- and 0x200 bytes is more
 * than the real descriptor so a mutation that wrote through it would stay in
 * bounds rather than corrupt the fixture.
 */
static unsigned char arg_mp_raw[MPSZ] __attribute__((aligned(8)));
static unsigned char arg_dil[0x200] __attribute__((aligned(8)));

#define ARG_MP	((struct _tagModemParameters *)(void *)arg_mp_raw)

static unsigned
lfsr_step(unsigned *s)
{
	*s = (*s >> 1) ^ (-(int)(*s & 1u) & 0xb400u);
	return *s;
}

/*
 * The six sides the sweep drives.  See the file comment for why this set is
 * complete: the object tests for zero, then for one, then falls through, so
 * three classes exhaust it, and 0xffffffff is the one that tells a signed
 * reading of the enum from an unsigned one.
 */
static unsigned
trial_side(int trial)
{
	static const unsigned sides[6] = { 0u, 1u, 2u, 3u, 0x7fu, 0xffffffffu };

	return sides[(unsigned)trial % 6u];
}

static void
seed(int trial)
{
	unsigned lfsr = 0x2f1du + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		lfsr_step(&lfsr);
		switch (trial & 3) {
		case 0:
			v = (unsigned char)(lfsr >> 3);
			break;
		case 1:
			v = 0x17;
			break;
		case 2:
			v = 0xff;
			break;
		default:
			v = (unsigned char)((lfsr >> 5) | 1u);
			break;
		}
		ours[i] = v;
		theirs[i] = v;
		sown[i] = v;
	}
	for (i = 0; i < sizeof(arg_dil); i++)
		arg_dil[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);

	/*
	 * Every slot of the modem block a small positive int, varied by the
	 * trial: nothing here reads one, but a mutation that handed this
	 * block somewhere it does not belong should find sane values rather
	 * than a wild count.
	 */
	for (i = 0; i + 4 <= MPSZ; i += 4) {
		int v = (int)(4u + ((i * 7u + (unsigned)trial) & 0x3fu));

		memcpy(arg_mp_raw + i, &v, 4);
	}
	ARG_MP->paramFile = 0;
}

static void *
slot_ptr(const unsigned char *o, unsigned off)
{
	void *p;

	memcpy(&p, o + off, sizeof(p));
	return p;
}

static void
slot_set_ptr(unsigned char *o, unsigned off, void *p)
{
	memcpy(o + off, &p, sizeof(p));
}

static int
guard_intact(void)
{
	return memcmp(ours + OBJSZ, sown + OBJSZ, GUARD) == 0
	    && memcmp(theirs + OBJSZ, sown + OBJSZ, GUARD) == 0;
}

/*
 * The whole object, with the owned heap pointers poisoned.  +0x000 IS IN THE
 * SKIP SET ONLY ON THE ANALOG SIDE: on the digital side it is a compared
 * literal NULL, and on any other side it is never written at all and both
 * sides carry the seed -- which is the check that the illegal arm stores
 * nothing.
 */
static void
compare(unsigned side, const char *what, long tag)
{
	memcpy(cmp_a, ours, OBJSZ);
	memcpy(cmp_b, theirs, OBJSZ);

	memset(cmp_a + OFF_PARAMETERS, 0x77, 4);
	memset(cmp_b + OFF_PARAMETERS, 0x77, 4);
	memset(cmp_a + OFF_PHASE2INFO, 0x77, 4);
	memset(cmp_b + OFF_PHASE2INFO, 0x77, 4);
	memset(cmp_a + OFF_MAPPARAMS, 0x77, 4);
	memset(cmp_b + OFF_MAPPARAMS, 0x77, 4);
	memset(cmp_a + OFF_CP, 0x77, 4);
	memset(cmp_b + OFF_CP, 0x77, 4);
	if (side == (unsigned)V92_MODEM_SIDE_ANALOG) {
		memset(cmp_a + OFF_MODULATOR, 0x77, 4);
		memset(cmp_b + OFF_MODULATOR, 0x77, 4);
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V92Modem", cmp_a, cmp_b,
		     (size_t)OBJSZ, tag);
}

/*
 * The four pieces, each compared in its own right.  Only the pointers that
 * CANNOT agree are skipped, and each of those has an identity check instead.
 */
static void
subobject_compare(long tag)
{
	V92Modem *m = (V92Modem *)(void *)ours;
	V92Modem *r = (V92Modem *)(void *)theirs;
	unsigned char pa[P2ISZ], pb[P2ISZ];
	unsigned char qa[PISZ], qb[PISZ];
	unsigned i, j;
	unsigned owned[10];

	/*
	 * V92Parameters, 0xdc bytes.  Its ONE pointer is +0x000, and that is
	 * the shared modem block both sides were handed, so nothing is
	 * skipped -- the field that proves the argument was stored is inside
	 * the comparison rather than beside it.
	 */
	diff_eq_obj_(__FILE__, __LINE__, "the V92Parameters it built",
		     "V92Parameters", m->parameters, r->parameters,
		     (size_t)PARAMSZ, tag);
	diff_eq_int("the parameters got the modem block (tag %ld)",
		    m->parameters->modemParams == ARG_MP, 1, tag);
	diff_eq_int("and the blob's did too (tag %ld)",
		    r->parameters->modemParams == ARG_MP, 1, tag);

	/* V92Phase2Info, 0x2c bytes; +0x28 is each side's own V92Parameters. */
	memcpy(pa, m->phase2Info, sizeof(pa));
	memcpy(pb, r->phase2Info, sizeof(pb));
	memset(pa + 0x28, 0x77, 4);
	memset(pb + 0x28, 0x77, 4);
	diff_eq_obj_(__FILE__, __LINE__, "the V92Phase2Info it built",
		     "V92Phase2Info", pa, pb, (size_t)P2ISZ, tag);
	diff_eq_int("the phase 2 info got our parameters (tag %ld)",
		    m->phase2Info->params == m->parameters, 1, tag);
	diff_eq_int("and the blob's got the blob's (tag %ld)",
		    r->phase2Info->params == r->parameters, 1, tag);

	/* V92CP, 0x918 bytes and not a pointer in it. */
	diff_eq_obj_(__FILE__, __LINE__, "the V92CP it built", "V92CP",
		     m->cp, r->cp, (size_t)CPSZ, tag);

	/*
	 * The 180-byte parameter block.  Ten heap pointers -- four filter
	 * coefficient arrays and six constellations -- and everything else is
	 * the allocator's fill, identical on both sides because nothing
	 * writes it.  That the untouched bytes ARE compared is the check that
	 * neither creator strayed outside its own span.
	 */
	memcpy(qa, m->mappingParams, sizeof(qa));
	memcpy(qb, r->mappingParams, sizeof(qb));
	memset(qa + __builtin_offsetof(struct V92ParamsInfo, z1), 0x77,
	       V92_PARAMSINFO_FILTERCOEFS * sizeof(void *));
	memset(qb + __builtin_offsetof(struct V92ParamsInfo, z1), 0x77,
	       V92_PARAMSINFO_FILTERCOEFS * sizeof(void *));
	memset(qa + __builtin_offsetof(struct V92ParamsInfo, constellations),
	       0x77, V92_PARAMSINFO_CONSTELLATIONS * sizeof(void *));
	memset(qb + __builtin_offsetof(struct V92ParamsInfo, constellations),
	       0x77, V92_PARAMSINFO_CONSTELLATIONS * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, "the parameter block it filled",
		     "V92ParamsInfo", qa, qb, (size_t)PISZ, tag);

	/*
	 * The four coefficient pointers are four named fields since the
	 * unpacker was read -- z1, p1, z2, p2 -- and still one contiguous run
	 * of four, which t_v92alloc.c asserts field by field.
	 */
	for (i = 0; i < V92_PARAMSINFO_FILTERCOEFS; i++)
		owned[i] = (unsigned)__builtin_offsetof(struct V92ParamsInfo,
							z1)
			 + i * (unsigned)sizeof(void *);
	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++)
		owned[V92_PARAMSINFO_FILTERCOEFS + i] =
		    (unsigned)__builtin_offsetof(struct V92ParamsInfo,
						 constellations)
		    + i * (unsigned)sizeof(void *);
	for (i = 0; i < 10u; i++) {
		const unsigned char *b = (const unsigned char *)m->mappingParams;

		diff_eq_int("parameter-block array +0x%02lx is not null",
			    slot_ptr(b, owned[i]) != 0, 1, (long)owned[i]);
		for (j = 0; j < i; j++)
			diff_eq_int("parameter-block array +0x%02lx is its own "
				    "allocation",
				    slot_ptr(b, owned[i])
				    != slot_ptr(b, owned[j]), 1,
				    (long)owned[i]);
	}
}

/*
 * THE SEVEN ARGUMENTS OF THE `V92Modulator` CALL, which is what this whole
 * fixture exists for.  Six of the seven are pointers the constructor read
 * back out of itself and one is the count it was handed; each is checked by
 * identity, on OUR side and on the blob's, because "they agree" would be
 * satisfied by two constructors making the same mistake and "ours is right"
 * would be satisfied by a fixture the blob never ran.
 */
static void
modulator_arguments(unsigned n, long tag)
{
	V92Modem *m = (V92Modem *)(void *)ours;
	V92Modem *r = (V92Modem *)(void *)theirs;
	V92Modulator *a = m->modulator;
	V92Modulator *b = r->modulator;
	unsigned char ma[MODSZ], mb[MODSZ];

	diff_eq_int("the analog side built a modulator (tag %ld)", a != 0, 1,
		    tag);
	diff_eq_int("and so did the blob (tag %ld)", b != 0, 1, tag);
	if (a == 0 || b == 0)
		return;

	/*
	 * Argument 1, the count, seen through the only thing that carries it:
	 * `blockSize` is round(5n/6) and the exact integer form of the same
	 * rounding is floor((5n + 3) / 6).  The two disagree only where
	 * n * 5/6 lands on a half -- n congruent to 3 mod 6 -- and there the
	 * float's own rounding decides, so those are left to the comparison
	 * against the reference.
	 */
	if (n % 6u != 3u)
		diff_eq_int("the modulator got our nSamples (tag %ld)",
			    (int)a->blockSize, (int)((5u * n + 3u) / 6u), tag);
	diff_eq_int("and the blob's modulator got the same (tag %ld)",
		    (int)a->blockSize, (int)b->blockSize, tag);

	diff_eq_int("argument 2 is our V92Phase2Info (tag %ld)",
		    (void *)a->phase2Info == (void *)m->phase2Info, 1, tag);
	diff_eq_int("ref argument 2 is the blob's (tag %ld)",
		    (void *)b->phase2Info == (void *)r->phase2Info, 1, tag);

	/*
	 * ARGUMENT 3 IS THE EMBEDDED `V92Ja`, at +0x00c INSIDE the modem --
	 * the one argument that is neither a heap block nor a fixture block,
	 * and the reason the constructor's `lea 0xc(%esi)` is a claim about
	 * the object map rather than about a local.
	 */
	diff_eq_int("argument 3 is our own +0x00c (tag %ld)",
		    (void *)a->ja == (void *)(ours + OFF_JA), 1, tag);
	diff_eq_int("ref argument 3 is the blob's own +0x00c (tag %ld)",
		    (void *)b->ja == (void *)(theirs + OFF_JA), 1, tag);

	diff_eq_int("argument 4 is the descriptor we passed (tag %ld)",
		    (void *)a->dil == (void *)arg_dil, 1, tag);
	diff_eq_int("ref argument 4 is the same descriptor (tag %ld)",
		    (void *)b->dil == (void *)arg_dil, 1, tag);

	diff_eq_int("argument 5 is our V92CP (tag %ld)",
		    (void *)a->cp == (void *)m->cp, 1, tag);
	diff_eq_int("ref argument 5 is the blob's (tag %ld)",
		    (void *)b->cp == (void *)r->cp, 1, tag);

	diff_eq_int("argument 6 is our parameter block (tag %ld)",
		    (void *)a->mappingParams == (void *)m->mappingParams, 1,
		    tag);
	diff_eq_int("ref argument 6 is the blob's (tag %ld)",
		    (void *)b->mappingParams == (void *)r->mappingParams, 1,
		    tag);

	diff_eq_int("argument 7 is our V92Parameters (tag %ld)",
		    (void *)a->params == (void *)m->parameters, 1, tag);
	diff_eq_int("ref argument 7 is the blob's (tag %ld)",
		    (void *)b->params == (void *)r->parameters, 1, tag);

	/*
	 * And the modulator's own non-pointer words, which carry the four
	 * counts `reset` derives.  +0x10..+0x23 and +0x40..+0x8f are
	 * addresses on both sides and are covered by the identities above and
	 * by t_v92mod.
	 */
	memcpy(ma, a, sizeof(ma));
	memcpy(mb, b, sizeof(mb));
	memset(ma + 0x10, 0x77, 0x14);
	memset(mb + 0x10, 0x77, 0x14);
	memset(ma + 0x40, 0x77, 0x50);
	memset(mb + 0x40, 0x77, 0x50);
	diff_eq_obj_(__FILE__, __LINE__, "the modulator's own words",
		     "V92Modulator", ma, mb, (size_t)MODSZ, tag);
}

#define NTRIAL	30

/*
 * `nSamples` is kept small so that a wrong multiplier down in the modulator
 * asks for a wrong allocation rather than an impossible one.
 */
static unsigned
trial_samples(int trial)
{
	return (unsigned)(trial * 5 + 3);
}

/* Varied, and it must not matter: see the file comment. */
static unsigned
trial_mode(int trial)
{
	return (unsigned)trial * 0x01010101u + 1u;
}

/*
 * The illegal arm never writes +0x000, so the seed is still there and the
 * destructor would run `~V92Modulator` over it.  Nulling it is the fixture's
 * bookkeeping, and the check that it was still the seed happens first.
 */
static void
disarm_modulator(unsigned side)
{
	if (side != (unsigned)V92_MODEM_SIDE_DIGITAL
	    && side != (unsigned)V92_MODEM_SIDE_ANALOG) {
		slot_set_ptr(ours, OFF_MODULATOR, 0);
		slot_set_ptr(theirs, OFF_MODULATOR, 0);
	}
}

static int
run_ctor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor)
{
	unsigned char first[SLOT];
	int trial, moved = 0, distinct = 0;
	int saw_digital = 0, saw_analog = 0, saw_illegal = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned side = trial_side(trial);
		unsigned n = trial_samples(trial);
		unsigned mode = trial_mode(trial);
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes;

		seed(trial);
		harness_alloc_reset();

		our_ctor(ours, side, arg_mp_raw, n, arg_dil, mode);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;
		ref_ctor(theirs, side, arg_mp_raw, n, arg_dil, mode);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", (int)a_bytes,
			    (int)b_bytes, trial);
		diff_eq_int("no free of NULL (trial %ld)",
			    harness_alloc.free_null, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);

		/* The two arguments that are only stored. */
		diff_eq_int("+0xaa8 is the first argument (trial %ld)",
			    (long)(unsigned)((V92Modem *)(void *)ours)
			    ->modemSide == (long)side, 1, trial);
		diff_eq_int("+0xa9c is the fourth argument (trial %ld)",
			    slot_ptr(ours, OFF_DIL) == (void *)arg_dil, 1,
			    trial);

		/*
		 * THE 2,704 BYTES NOBODY WRITES.  If this ever stops holding
		 * it is either a constructor that overran a member or a
		 * fixture that stopped seeding, and both matter.
		 */
		diff_eq_int("the V92Ja span is untouched (trial %ld)",
			    memcmp(ours + OFF_JA, sown + OFF_JA,
				   V92_MODEM_JA_BYTES) == 0, 1, trial);
		diff_eq_int("and the blob leaves it alone too (trial %ld)",
			    memcmp(theirs + OFF_JA, sown + OFF_JA,
				   V92_MODEM_JA_BYTES) == 0, 1, trial);

		subobject_compare(trial);

		if (side == (unsigned)V92_MODEM_SIDE_DIGITAL) {
			saw_digital = 1;
			diff_eq_int("the digital side has no modulator "
				    "(trial %ld)",
				    slot_ptr(ours, OFF_MODULATOR) == 0, 1,
				    trial);
			diff_eq_int("nor has the blob's (trial %ld)",
				    slot_ptr(theirs, OFF_MODULATOR) == 0, 1,
				    trial);
		} else if (side == (unsigned)V92_MODEM_SIDE_ANALOG) {
			saw_analog = 1;
			modulator_arguments(n, trial);
		} else {
			saw_illegal = 1;
			diff_eq_int("the illegal arm left +0x000 at its seed "
				    "(trial %ld)",
				    memcmp(ours + OFF_MODULATOR,
					   sown + OFF_MODULATOR, 4) == 0, 1,
				    trial);
			diff_eq_int("and so did the blob's (trial %ld)",
				    memcmp(theirs + OFF_MODULATOR,
					   sown + OFF_MODULATOR, 4) == 0, 1,
				    trial);
		}

		compare(side, "after the constructor", trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(), 1, trial);

		if (memcmp(sown, ours, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours, SLOT);
		else if (memcmp(first, ours, SLOT) != 0)
			distinct = 1;

		disarm_modulator(side);
		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
		diff_eq_int("still no bad free (trial %ld)",
			    harness_alloc.bad_free, 0, trial);
	}

	diff_eq_int("the constructor changed the object", moved, 1, 0);
	diff_eq_int("the object is not the same on every trial", distinct, 1,
		    0);
	diff_eq_int("the digital arm was driven", saw_digital, 1, 0);
	diff_eq_int("the analog arm was driven", saw_analog, 1, 0);
	diff_eq_int("the illegal arm was driven", saw_illegal, 1, 0);

	return diff_end();
}

/*
 * The destructor over every one of the sixteen null/non-null combinations of
 * the four pointers it guards that CAN be nulled -- see the file comment for
 * the fifth.  The pointers that stay live are real: three of the four get a
 * destructor call that dereferences them.
 *
 * Each combination is destroyed twice, once as chosen and once over the
 * complement, so nothing leaks and `live == 0` afterwards is what says the
 * two halves covered all four exactly once.  The parameter block is released
 * by the first call unconditionally, so the second is given a FRESH one with
 * its ten arrays null -- the two deleters then free nothing and the guard
 * frees the block, which keeps the accounting honest without pretending the
 * unreachable branch was reached.
 */
static const unsigned owned4[4] = {
	OFF_MODULATOR, OFF_CP, OFF_PARAMETERS, OFF_PHASE2INFO
};
#define NOWNED4	4u

/*
 * The destructor's ONE store, spelled out: +0x008 becomes zero exactly when
 * it was not already, and nothing else moves.
 */
static void
check_single_store(const unsigned char *before, const unsigned char *after,
		   const char *who, long tag)
{
	unsigned char want[SLOT];

	memcpy(want, before, SLOT);
	if (slot_ptr(before, OFF_PHASE2INFO) != 0)
		memset(want + OFF_PHASE2INFO, 0, 4);
	diff_eq_int(who, memcmp(want, after, SLOT) == 0, 1, tag);
}

static int
run_dtor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor)
{
	unsigned char before_a[SLOT], before_b[SLOT];
	unsigned subset;
	int saw_null = 0, saw_live = 0, freed = 0, saw_nulled = 0;

	diff_begin(name);

	for (subset = 0; subset < (1u << NOWNED4); subset++) {
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;
		unsigned i, n = (subset % 23u) + 1u;
		void *fresh_a, *fresh_b;

		seed((int)(subset % 17u));
		harness_alloc_reset();

		/* The analog side, so that all four pointers are real. */
		our_ctor(ours, (unsigned)V92_MODEM_SIDE_ANALOG, arg_mp_raw, n,
			 arg_dil, subset);
		ref_ctor(theirs, (unsigned)V92_MODEM_SIDE_ANALOG, arg_mp_raw,
			 n, arg_dil, subset);
		memcpy(ctor_a, ours, SLOT);
		memcpy(ctor_b, theirs, SLOT);

		for (i = 0; i < NOWNED4; i++) {
			if ((subset >> i) & 1u) {
				slot_set_ptr(ours, owned4[i], 0);
				slot_set_ptr(theirs, owned4[i], 0);
				saw_null = 1;
			} else {
				saw_live = 1;
			}
		}
		memcpy(before_a, ours, SLOT);
		memcpy(before_b, theirs, SLOT);
		if (slot_ptr(before_a, OFF_PHASE2INFO) != 0)
			saw_nulled = 1;

		a_frees = harness_alloc.frees;
		a_null = harness_alloc.free_null;
		a_bad = harness_alloc.bad_free;
		our_dtor(ours);
		a_frees = harness_alloc.frees - a_frees;
		a_null = harness_alloc.free_null - a_null;
		a_bad = harness_alloc.bad_free - a_bad;

		b_frees = harness_alloc.frees;
		b_null = harness_alloc.free_null;
		b_bad = harness_alloc.bad_free;
		ref_dtor(theirs);
		b_frees = harness_alloc.frees - b_frees;
		b_null = harness_alloc.free_null - b_null;
		b_bad = harness_alloc.bad_free - b_bad;

		diff_eq_int("frees, subset 0x%02lx", a_frees, b_frees,
			    (long)subset);
		/* Drop a guard and this is the only counter that moves. */
		diff_eq_int("sysdep_free(NULL), subset 0x%02lx", a_null, 0,
			    (long)subset);
		diff_eq_int("ref sysdep_free(NULL), subset 0x%02lx", b_null, 0,
			    (long)subset);
		diff_eq_int("no bad free, subset 0x%02lx", a_bad, 0,
			    (long)subset);
		diff_eq_int("ref no bad free, subset 0x%02lx", b_bad, 0,
			    (long)subset);

		check_single_store(before_a, ours,
				   "the destructor's only store is +0x008, "
				   "subset 0x%02lx", (long)subset);
		check_single_store(before_b, theirs,
				   "and the blob's only store is +0x008, "
				   "subset 0x%02lx", (long)subset);

		compare((unsigned)V92_MODEM_SIDE_ANALOG, "after the destructor",
			(long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%02lx",
			    guard_intact(), 1, (long)subset);

		if (a_frees > 0)
			freed = 1;

		/*
		 * The complement, so that everything the first half left
		 * alive comes back.  The parameter block is gone, so both
		 * sides get a fresh empty one.
		 */
		memcpy(ours, ctor_a, SLOT);
		memcpy(theirs, ctor_b, SLOT);
		for (i = 0; i < NOWNED4; i++) {
			if (((subset >> i) & 1u) == 0) {
				slot_set_ptr(ours, owned4[i], 0);
				slot_set_ptr(theirs, owned4[i], 0);
			}
		}
		fresh_a = sysdep_malloc(PISZ);
		fresh_b = sysdep_malloc(PISZ);
		memset(fresh_a, 0, PISZ);
		memset(fresh_b, 0, PISZ);
		slot_set_ptr(ours, OFF_MAPPARAMS, fresh_a);
		slot_set_ptr(theirs, OFF_MAPPARAMS, fresh_b);
		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("the two halves freed everything, subset 0x%02lx",
			    harness_alloc.live, 0, (long)subset);
		diff_eq_int("and freed nothing twice, subset 0x%02lx",
			    harness_alloc.bad_free, 0, (long)subset);
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a non-null pointer was tried", saw_live, 1, 0);
	diff_eq_int("the destructor released something", freed, 1, 0);
	diff_eq_int("the nulling path was reached", saw_nulled, 1, 0);

	return diff_end();
}

/*
 * The same pair with the diagnostic channel live.
 *
 * FOUR CALL SITES ARE DEAD IN EVERY OTHER RUN HERE, because the level ships
 * at zero: the constructor's "V92Modem Construction (as %s Modem)" and its
 * "Illegal modemSide", and the destructor's "V92Modem Destruction" and its
 * own "Illegal modemSide".  A wrong string, a swapped `%s` arm or a gate
 * written `> 2` would be invisible to every check above.  So the pair is
 * driven again at level 2 with the harness's capture on, and the transcripts
 * are compared as text AND counted as lines: text alone can be filled by the
 * harness without a call site firing (finding F149).
 *
 * SIDE 0xffffffff IS THE ONE THAT MATTERS MOST HERE.  The destructor's range
 * test is unsigned, so 0xffffffff is illegal and prints; a reconstruction
 * that declared the enum signed would take it for -1, find it <= 1, and print
 * nothing.  No other check in this file can see that.
 */
static int
run_debug(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	  dtor_fn our_dtor, dtor_fn ref_dtor)
{
	int trial, saw = 0;

	diff_begin(name);

	for (trial = 0; trial < 6; trial++) {
		unsigned side = trial_side(trial);
		unsigned n = trial_samples(trial);
		unsigned lines_a, lines_b;
		const char *ta, *tb;

		seed(trial);
		harness_alloc_reset();
		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2u;
		ref_dsplibs_debug_level = 2u;

		our_ctor(ours, side, arg_mp_raw, n, arg_dil, trial_mode(trial));
		ref_ctor(theirs, side, arg_mp_raw, n, arg_dil,
			 trial_mode(trial));
		disarm_modulator(side);
		our_dtor(ours);
		ref_dtor(theirs);

		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;

		ta = dsplib_debug_capture_text(0);
		tb = dsplib_debug_capture_text(1);
		lines_a = dsplib_debug_capture_lines(0);
		lines_b = dsplib_debug_capture_lines(1);

		diff_eq_int("transcript line count, side 0x%lx",
			    (long)lines_a, (long)lines_b, (long)side);
		diff_eq_int("transcript text, side 0x%lx",
			    strcmp(ta, tb) == 0, 1, (long)side);
		diff_eq_int("we printed something, side 0x%lx", lines_a > 0, 1,
			    (long)side);
		diff_eq_int("the blob printed too, side 0x%lx", lines_b > 0, 1,
			    (long)side);
		diff_eq_int("nothing left allocated, side 0x%lx",
			    harness_alloc.live, 0, (long)side);
		if (lines_a > 0)
			saw = 1;
	}

	diff_eq_int("the diagnostic channel was exercised", saw, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------ */
/* reset and progress                                                   */
/* ------------------------------------------------------------------ */

/*
 * THE TWO RUN-TIME MEMBERS, and both are three-arm switches that forward to the
 * modulator.  What makes them testable here rather than in t_v92modstate.cpp is
 * that this fixture gives EACH SIDE ITS OWN GRAPH -- its own V92Modulator, its
 * own V92CP, its own 2,704-byte V92Ja -- so a store made through any of them is
 * two different bytes and compares.  The modulator's own fixture shares those
 * arguments deliberately and cannot see through them.
 *
 * `reset`'s ANALOG ARM WRITES THE V92Ja, which is the anti-vacuity guard the
 * constructor's own trials use the other way round: the constructor leaves all
 * 2,704 bytes at the seed and `reset` must not, because
 * `V92DILdescriptorPacker(dil, ja + 4, (int *)ja)` packs the descriptor into
 * it.  Both facts are checked, on the same span, in the same file.
 *
 * `progress` IS DRIVEN AFTER `reset`, so the analog side is in phase 3 with a
 * phase 3 modulator that has been reset over a packed `ja` -- which is the
 * only state any caller can reach it in.  The digital and illegal arms are
 * driven over a modem that has no modulator at all, which is the check that
 * neither arm dereferences +0x000.
 */
#define PROG_SAMPLES	48u
#define PROG_BITS	64u

static int prog_bits_a[PROG_BITS], prog_bits_b[PROG_BITS];
static float prog_out_a[PROG_SAMPLES + 16u], prog_out_b[PROG_SAMPLES + 16u];

#define PROG_WIPE	-3000.0f

static void
prog_fill(int trial)
{
	unsigned lfsr = 0x77abu + 0x9e37u * (unsigned)trial;
	unsigned i;

	for (i = 0; i < PROG_BITS; i++) {
		int v = (int)(lfsr_step(&lfsr) & 1u);

		prog_bits_a[i] = v;
		prog_bits_b[i] = v;
	}
	for (i = 0; i < PROG_SAMPLES + 16u; i++) {
		prog_out_a[i] = PROG_WIPE;
		prog_out_b[i] = PROG_WIPE;
	}
}

static int
run_reset_progress(void)
{
	int trial;
	int saw_digital = 0, saw_analog = 0, saw_illegal = 0;
	int ja_moved = 0, out_moved = 0;

	diff_begin("V92Modem::reset and ::progress against the blob");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned side = trial_side(trial);
		unsigned mode = trial_mode(trial);
		unsigned int na = 0xa5a5a5a5u, nb = 0xa5a5a5a5u;

		seed(trial);
		harness_alloc_reset();
		our_c1(ours, side, arg_mp_raw, PROG_SAMPLES, arg_dil, mode);
		ref_c1(theirs, side, arg_mp_raw, PROG_SAMPLES, arg_dil, mode);

		our_reset(ours);
		ref_reset(theirs);

		compare(side, "after reset", trial);
		subobject_compare(trial);
		diff_eq_int("reset stored nothing past the object (trial %ld)",
			    guard_intact(), 1, trial);

		if (side == (unsigned)V92_MODEM_SIDE_ANALOG) {
			V92Modem *m = (V92Modem *)(void *)ours;
			V92Modem *r = (V92Modem *)(void *)theirs;

			saw_analog = 1;
			/*
			 * The packer wrote the descriptor into the V92Ja: the
			 * span the constructor leaves alone entirely.  A `reset`
			 * that dropped the call, or aimed it at the wrong two
			 * offsets, leaves the seed here.
			 */
			diff_eq_int("reset packed the descriptor into the V92Ja "
				    "(trial %ld)",
				    memcmp(ours + OFF_JA, sown + OFF_JA,
					   V92_MODEM_JA_BYTES) != 0, 1, trial);
			ja_moved = 1;
			diff_eq_int("reset took the modulator to phase 3 "
				    "(trial %ld)", m->modulator->phase,
				    V92MOD_PHASE_3, trial);
			diff_eq_int("and the blob's went there too (trial %ld)",
				    r->modulator->phase, V92MOD_PHASE_3, trial);
			/*
			 * The modulator's SCALARS, in the two runs that hold
			 * them: +0x00..+0x0f is the three counts and the two
			 * flag bytes, +0x24..+0x3f is the phase machine and the
			 * pending resampler change.  Everything between and
			 * after is a pointer at a per-side allocation and can
			 * never agree -- t_v92modstate.cpp compares those by
			 * sharing the arguments instead.
			 */
			diff_eq_int("the modulators' counts agree (trial %ld)",
				    memcmp((const unsigned char *)m->modulator,
					   (const unsigned char *)r->modulator,
					   0x10) == 0, 1, trial);
			diff_eq_int("and their phase machines agree (trial %ld)",
				    memcmp((const unsigned char *)m->modulator
					   + 0x24,
					   (const unsigned char *)r->modulator
					   + 0x24, 0x1c) == 0, 1, trial);
		} else {
			/*
			 * NEITHER OTHER ARM TOUCHES THE V92Ja, and that is the
			 * whole of what the digital arm does: nothing.
			 */
			diff_eq_int("the other arms leave the V92Ja alone "
				    "(trial %ld)",
				    memcmp(ours + OFF_JA, sown + OFF_JA,
					   V92_MODEM_JA_BYTES) == 0, 1, trial);
			if (side == (unsigned)V92_MODEM_SIDE_DIGITAL)
				saw_digital = 1;
			else
				saw_illegal = 1;
		}

		/*
		 * `progress`, twice.  The SECOND call is the one that matters
		 * for the modulator underneath: the first moves the queue off
		 * the level `reset` primed it to, and the occupancy term is
		 * what the whole block size is computed from.
		 */
		prog_fill(trial);
		our_progress(ours, prog_bits_a, &na, prog_out_a, PROG_SAMPLES);
		ref_progress(theirs, prog_bits_b, &nb, prog_out_b,
			     PROG_SAMPLES);

		diff_eq_int("the bit count agrees (trial %ld)", (int)na, (int)nb,
			    trial);
		diff_eq_int("the samples agree (trial %ld)",
			    memcmp(prog_out_a, prog_out_b, sizeof(prog_out_a))
			    == 0, 1, trial);
		diff_eq_int("the input words agree (trial %ld)",
			    memcmp(prog_bits_a, prog_bits_b,
				   sizeof(prog_bits_a)) == 0, 1, trial);

		our_progress(ours, prog_bits_a, &na, prog_out_a, PROG_SAMPLES);
		ref_progress(theirs, prog_bits_b, &nb, prog_out_b,
			     PROG_SAMPLES);

		diff_eq_int("the bit count agrees on the second call "
			    "(trial %ld)", (int)na, (int)nb, trial);
		diff_eq_int("the samples agree on the second call (trial %ld)",
			    memcmp(prog_out_a, prog_out_b, sizeof(prog_out_a))
			    == 0, 1, trial);

		compare(side, "after progress", trial);
		subobject_compare(trial);
		diff_eq_int("progress stored nothing past the object "
			    "(trial %ld)", guard_intact(), 1, trial);

		if (side == (unsigned)V92_MODEM_SIDE_ANALOG) {
			unsigned i;

			/*
			 * SOMETHING CAME OUT.  The wipe is a value the chain
			 * cannot produce, so a `progress` that forwarded
			 * nothing leaves it in place and this fails -- which is
			 * the check the digital arm inverts.
			 */
			for (i = 0; i < PROG_SAMPLES; i++)
				if (prog_out_a[i] != PROG_WIPE)
					out_moved = 1;
			diff_eq_int("the analog arm filled the buffer "
				    "(trial %ld)", out_moved, 1, trial);
		} else {
			diff_eq_int("the other arms wrote no samples "
				    "(trial %ld)",
				    prog_out_a[0] == PROG_WIPE, 1, trial);
			diff_eq_int("nor did the blob's (trial %ld)",
				    prog_out_b[0] == PROG_WIPE, 1, trial);
			diff_eq_int("and left the bit count alone (trial %ld)",
				    na == 0xa5a5a5a5u, 1, trial);
		}

		disarm_modulator(side);
		our_d1(ours);
		ref_d1(theirs);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("the digital arm was driven", saw_digital, 1, 0);
	diff_eq_int("the analog arm was driven", saw_analog, 1, 0);
	diff_eq_int("the illegal arm was driven", saw_illegal, 1, 0);
	diff_eq_int("the V92Ja was packed at least once", ja_moved, 1, 0);
	diff_eq_int("samples came out at least once", out_moved, 1, 0);

	return diff_end();
}

/*
 * THE TRANSCRIPT OF THE TWO RUN-TIME MEMBERS, and it is not decoration.
 * `reset` calls `printTitle` and `printTitle` writes nothing but text, so a
 * `reset` that dropped the call is invisible to every state comparison in this
 * file -- exactly as it is for the constructor, which is why THAT has a
 * transcript run too.  The same trial covers the two "Illegal modemSide"
 * messages, which are the whole of what either member does on the third arm.
 */
static int
run_debug_rp(void)
{
	int trial, saw = 0, sawIllegal = 0;

	diff_begin("V92Modem::reset and ::progress at level 2");

	for (trial = 0; trial < 6; trial++) {
		unsigned side = trial_side(trial);
		unsigned int na = 0u, nb = 0u;
		unsigned lines_a, lines_b;
		const char *ta, *tb;

		seed(trial);
		harness_alloc_reset();
		our_c1(ours, side, arg_mp_raw, PROG_SAMPLES, arg_dil,
		       trial_mode(trial));
		ref_c1(theirs, side, arg_mp_raw, PROG_SAMPLES, arg_dil,
		       trial_mode(trial));

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2u;
		ref_dsplibs_debug_level = 2u;

		our_reset(ours);
		ref_reset(theirs);
		prog_fill(trial);
		our_progress(ours, prog_bits_a, &na, prog_out_a, PROG_SAMPLES);
		ref_progress(theirs, prog_bits_b, &nb, prog_out_b,
			     PROG_SAMPLES);

		dsplibs_debug_level = 0u;
		ref_dsplibs_debug_level = 0u;
		dsplib_debug_capture_on = 0;

		ta = dsplib_debug_capture_text(0);
		tb = dsplib_debug_capture_text(1);
		lines_a = dsplib_debug_capture_lines(0);
		lines_b = dsplib_debug_capture_lines(1);

		diff_eq_int("transcript line count, side 0x%lx", (long)lines_a,
			    (long)lines_b, (long)side);
		/*
		 * THE TEXT IS COMPARED ON EVERY SIDE BUT THE ANALOG ONE, and
		 * that exception is the fixture's arrangement rather than a
		 * difference in the code.  The analog `reset` reaches
		 * `V92DILdescriptorPacker`, which prints
		 * "## Debug: pParamObj address = %X" and two more lines like
		 * it; each side has its own allocations, so those three lines
		 * hold two different heap addresses and can never agree.  The
		 * LINE COUNT still does, and it is what the `printTitle` claim
		 * needs -- dropping that call removes seven lines whatever the
		 * side.
		 */
		if (side != (unsigned)V92_MODEM_SIDE_ANALOG)
			diff_eq_int("transcript text, side 0x%lx",
				    strcmp(ta, tb) == 0, 1, (long)side);
		diff_eq_int("we printed something, side 0x%lx", lines_a > 0, 1,
			    (long)side);
		diff_eq_int("the blob printed too, side 0x%lx", lines_b > 0, 1,
			    (long)side);
		if (lines_a > 0u)
			saw = 1;
		if (side != (unsigned)V92_MODEM_SIDE_DIGITAL
		    && side != (unsigned)V92_MODEM_SIDE_ANALOG)
			sawIllegal = 1;

		disarm_modulator(side);
		our_d1(ours);
		ref_d1(theirs);
	}

	diff_eq_int("something was printed at all", saw, 1, 0);
	diff_eq_int("the illegal arm was driven at level 2", sawIllegal, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;

	rc |= run_ctor("V92Modem::V92Modem (C1) against the blob",
		       our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_ctor("V92Modem::V92Modem (C2) against the blob",
		       our_c2, ref_c2, our_d2, ref_d2);
	rc |= run_dtor("V92Modem::~V92Modem (D1) over every null combination",
		       our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_dtor("V92Modem::~V92Modem (D2) over every null combination",
		       our_c2, ref_c2, our_d2, ref_d2);
	rc |= run_debug("V92Modem construction and destruction at level 2",
			our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_debug("V92Modem construction and destruction at level 2 "
			"(C2/D2)", our_c2, ref_c2, our_d2, ref_d2);
	rc |= run_reset_progress();
	rc |= run_debug_rp();

	return rc;
}
