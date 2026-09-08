/*
 * t_v92mod.cpp -- differential test of V92Modulator's constructor and
 * destructor, all four symbol variants.
 *
 * C1 and C2, D1 and D2.  The constructor builds an ELEVEN-PIECE GRAPH and six
 * of the pieces are objects that build graphs of their own, so this fixture
 * has to say more than "the two objects agree": almost every byte that could
 * be wrong is a heap address, and heap addresses can never agree.
 *
 * WHAT IS COMPARED DIRECTLY.  +0x00..+0x43 -- the two derived counts, the two
 * flag bytes, the five cleared words, the float and all seven argument
 * pointers -- plus the scrambler's `tailLength` at +0x70.  Those are the
 * bytes where a wrong argument order, a missing clear or a wrong scrambler
 * tap shows up, and they are compared byte for byte with nothing skipped.
 *
 * WHAT IS POISONED, AND WHAT STANDS IN FOR IT.  +0x44..+0x6f and
 * +0x74..+0x8f are eleven owned pointers and the scrambler's seven, two
 * different addresses each.  Three separate things cover them:
 *
 *   - `harness_alloc.allocs` and `.bytes`, compared against the reference.
 *     A wrong size anywhere in the graph -- `(blockSize + 10) * 2` written
 *     any other way, `3 * blockSize` written as `3 * nSamples`, a missing
 *     allocation -- moves one of them.  This is the only witness the five
 *     raw buffers have, and it is a differential one: the blob is the oracle.
 *   - The pieces are DEREFERENCED and their own scalars checked absolutely:
 *     the bit-to-symbol stage's element count against `3 * blockSize`, the
 *     queue's slot count against `MODULATOR_QUEUE_LENGTH + 1`, the transmit
 *     filter's tap and buffer lengths against 36 and 36 + 99.  Those are the
 *     four constructor arguments that are computed rather than copied.
 *   - Every pointer is checked non-null and distinct from the others.
 *
 * THE SEVEN ARGUMENTS ARE SEVEN SHARED OBJECTS, one instance each, pointed at
 * by both sides -- so the ADDRESS is the witness for which field each lands
 * in, and a constructor that filed argument five where argument six belongs
 * fails.  Separately seeded blocks per side would agree whatever was read.
 *
 * THE PARAMETER BLOCK IS SEEDED SMALL, POSITIVE AND VARIED, and that is not
 * tidiness.  `MODULATOR_QUEUE_LENGTH` is a SIGNED `int` in V92Parameters.h;
 * `reset` shifts it right arithmetically and then uses the result as the
 * UNSIGNED bound of the loop that primes the queue.  A negative value would
 * ask for about two billion iterations (finding F1284).  The queue's own
 * length is also an allocation size.
 *
 * TWO WORDS ARE NEVER WRITTEN.  +0x24 and +0x3c are in the compared region
 * and both sides carry the seed there, so they agree BECAUSE nobody wrote
 * them -- and a reconstruction that cleared either fails.  That is why the
 * seed is varied and never zero.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92CP.h"
#include "dsplib/Queue.h"
#include "dsplib/FloatFIR.h"

extern "C" {
void our_c1(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("_ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptor"
	    "P5V92CPP16V92MappingParamsP13V92Parameters");
void our_c2(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("_ZN12V92ModulatorC2EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptor"
	    "P5V92CPP16V92MappingParamsP13V92Parameters");
void our_d1(void *) asm("_ZN12V92ModulatorD1Ev");
void our_d2(void *) asm("_ZN12V92ModulatorD2Ev");
void ref_c1(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("ref__ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescri"
	    "ptorP5V92CPP16V92MappingParamsP13V92Parameters");
void ref_c2(void *, unsigned, void *, void *, void *, void *, void *, void *)
	asm("ref__ZN12V92ModulatorC2EjP13V92Phase2InfoP5V92JaP19tagV90DILdescri"
	    "ptorP5V92CPP16V92MappingParamsP13V92Parameters");
void ref_d1(void *) asm("ref__ZN12V92ModulatorD1Ev");
void ref_d2(void *) asm("ref__ZN12V92ModulatorD2Ev");

extern float v92TxPreFilter[];
extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;
}

/* Both sides' levels move together, or they take different branches for
 * reasons that have nothing to do with the modulator. */
static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

typedef void (*ctor_fn)(void *, unsigned, void *, void *, void *, void *,
			void *, void *);
typedef void (*dtor_fn)(void *);

#define GUARD	64
#define OBJSZ	((unsigned)sizeof(V92Modulator))
#define SLOT	(OBJSZ + GUARD)
#define CPSZ	((unsigned)sizeof(V92CP))
#define PARAMSZ	((unsigned)sizeof(V92Parameters))

static unsigned char ours[SLOT] __attribute__((aligned(8)));
static unsigned char theirs[SLOT] __attribute__((aligned(8)));
static unsigned char sown[SLOT];
static unsigned char cmp_a[SLOT];
static unsigned char cmp_b[SLOT];
static unsigned char ctor_a[SLOT];
static unsigned char ctor_b[SLOT];

static unsigned char arg_params[PARAMSZ] __attribute__((aligned(8)));
/*
 * The four opaque arguments are as big as a V92CP even though nothing reads
 * through them, so that a MUTATION which hands one of them to the phase 4
 * modulator in the V92CP's place stores into its own storage rather than off
 * the end of a 64-byte array.  A mutation tier that corrupts the fixture is
 * not measuring the mutation.
 */
static unsigned char arg_p2[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_ja[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_dil[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_mp[CPSZ] __attribute__((aligned(8)));
static unsigned char arg_cp[CPSZ] __attribute__((aligned(8)));

/*
 * Two runs of heap addresses: +0x44..+0x6f is the four owned objects and the
 * scrambler's seven pointers; +0x74..+0x8f is the queue, the filter and the
 * five raw buffers.  `tailLength` at +0x70 is a COUNT and is compared.
 */
struct region { unsigned off, len; };
static const struct region skip[] = {
	{ 0x44, 0x2c },
	{ 0x74, 0x1c },
};
#define NSKIP	((unsigned)(sizeof(skip) / sizeof(skip[0])))

/*
 * The eleven the destructor guards, in the order it guards them.  The
 * scrambler's own buffer at +0x54 is Scrambler's business and is not in the
 * subset sweep; it is released on every one of these runs.
 */
static const unsigned owned[11] = {
	0x44, 0x48, 0x4c, 0x50, 0x74, 0x78, 0x7c, 0x80, 0x88, 0x84, 0x8c
};
#define NOWNED	11u

static unsigned
lfsr_step(unsigned *s)
{
	*s = (*s >> 1) ^ (-(int)(*s & 1u) & 0xb400u);
	return *s;
}

/* What `MODULATOR_QUEUE_LENGTH` is set to on a given trial: small, positive
 * and varying, for the reason in the file comment. */
static int
queue_length(int trial)
{
	return 6 + 2 * (trial % 13);
}

static void
seed(int trial)
{
	unsigned lfsr = 0x1234u + 0x9e37u * (unsigned)trial;
	unsigned i;
	V92Parameters *pp;

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
	for (i = 0; i < CPSZ; i++) {
		arg_p2[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
		arg_ja[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
		arg_dil[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
		arg_mp[i] = (unsigned char)(lfsr_step(&lfsr) >> 3);
	}
	for (i = 0; i < CPSZ; i++)
		arg_cp[i] = (unsigned char)((lfsr_step(&lfsr) >> 3) | 1u);

	/*
	 * The parameter block: every slot a small positive int, varied by the
	 * trial, so nothing read out of it is a wild count or a wild divisor.
	 */
	for (i = 0; i < PARAMSZ / 4; i++) {
		int v = (int)(4u + ((i * 7u + (unsigned)trial) & 0x3fu));

		memcpy(arg_params + i * 4, &v, 4);
	}
	pp = (V92Parameters *)arg_params;
	pp->MODULATOR_QUEUE_LENGTH = queue_length(trial);
}

static int
guard_intact(void)
{
	return memcmp(ours + OBJSZ, sown + OBJSZ, GUARD) == 0
	    && memcmp(theirs + OBJSZ, sown + OBJSZ, GUARD) == 0;
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

/*
 * THE SCRAMBLER'S THREE ARGUMENTS, which nothing else here can see.  All
 * seven of its words are heap addresses the comparison has to poison, and the
 * near tap reaches only `pInitTap1`; the DISTANCES between them are not
 * addresses, and they are the three arguments back again.  `T` is `int` for
 * this instantiation, so a distance in bytes is four times the argument.
 */
#define SCRAM_ELEM	((int)sizeof(int))

static void
scram_geometry(long trial)
{
	const unsigned char *lim = (const unsigned char *)slot_ptr(ours, 0x54);
	const unsigned char *out = (const unsigned char *)slot_ptr(ours, 0x58);
	const unsigned char *t1 = (const unsigned char *)slot_ptr(ours, 0x5c);
	const unsigned char *t2 = (const unsigned char *)slot_ptr(ours, 0x60);
	const unsigned char *rlim =
	    (const unsigned char *)slot_ptr(theirs, 0x54);
	const unsigned char *rout =
	    (const unsigned char *)slot_ptr(theirs, 0x58);
	const unsigned char *rt1 =
	    (const unsigned char *)slot_ptr(theirs, 0x5c);
	const unsigned char *rt2 =
	    (const unsigned char *)slot_ptr(theirs, 0x60);

	/* V.92 section 6.3 selects GPA: literal expectations must not share
	 * the owner's initializer macros.  Slack 99 is the production layout. */
	diff_eq_int("the near tap is 5 ints up (trial %ld)", (int)(t1 - out),
		    5 * SCRAM_ELEM, trial);
	diff_eq_int("the far tap is 23 ints up (trial %ld)", (int)(t2 - out),
		    23 * SCRAM_ELEM, trial);
	diff_eq_int("the restart point is 99 ints above the base (trial %ld)",
		    (int)(out - lim), 99 * SCRAM_ELEM, trial);
	diff_eq_int("ref near tap is 5 ints up (trial %ld)", (int)(rt1 - rout),
		    5 * SCRAM_ELEM, trial);
	diff_eq_int("ref far tap is 23 ints up (trial %ld)", (int)(rt2 - rout),
		    23 * SCRAM_ELEM, trial);
	diff_eq_int("ref restart point is 99 ints above the base (trial %ld)",
		    (int)(rout - rlim), 99 * SCRAM_ELEM,
		    trial);
	diff_eq_int("reconstruction GPA history has 23 elements (trial %ld)",
		    ((V92Modulator *)ours)->scrambler.tailLength, 23, trial);
	diff_eq_int("blob GPA history has 23 elements (trial %ld)",
		    ((V92Modulator *)theirs)->scrambler.tailLength, 23, trial);
	diff_eq_int("pOut is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x64) == (void *)out, 1, trial);
	diff_eq_int("pTap1 is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x68) == (void *)t1, 1, trial);
	diff_eq_int("pTap2 is at its initial value (trial %ld)",
		    slot_ptr(ours, 0x6c) == (void *)t2, 1, trial);
}

/*
 * THE QUEUE'S CURSORS, for the same reason: the queue is behind a poisoned
 * pointer, so the priming loop's TRIP COUNT is invisible in the modulator's
 * own bytes.  `wr - buf` is how many floats were written and `rd - buf` is
 * zero, and both are compared against the loop's bound.
 */
static void
queue_cursors(const V92Modulator *m, int want, long trial)
{
	const unsigned char *q = (const unsigned char *)m->queue;
	const unsigned char *buf, *rd, *wr;

	memcpy(&buf, q + 0x00, sizeof(buf));
	memcpy(&rd, q + 0x08, sizeof(rd));
	memcpy(&wr, q + 0x0c, sizeof(wr));

	diff_eq_int("the queue was primed with queuePrime floats (trial %ld)",
		    (int)((wr - buf) / (int)sizeof(float)), want, trial);
	diff_eq_int("and nothing was read back out of it (trial %ld)",
		    (int)(rd - buf), 0, trial);
}

/*
 * THE SUB-OBJECTS' OWN BYTES.  Everything the constructor computes and hands
 * downward lands inside a piece of the graph, and every one of those pieces
 * is behind a pointer the modulator's own comparison has to poison.  So each
 * one is compared between the sides in its own right, with only ITS pointers
 * skipped -- which is what carries the resampler's six literal arguments and
 * the phase 4 modulator's four.
 */
static void
subobject_compare(const V92Modulator *a, const V92Modulator *b, long trial)
{
	unsigned char ra[0x4c], rb[0x4c];
	unsigned char pa[0x1cc], pb[0x1cc];
	unsigned taps, phases;

	/*
	 * ResamplerTimingOffset, 0x4c bytes: the vptr at +0x00 and the two
	 * buffers at +0x04 and +0x08 are the only things that cannot agree.
	 * `phases` at +0x30 and `taps` at +0x34 are the two literals the
	 * constructor passes that nothing else here can see.
	 */
	memcpy(ra, a->resampler, sizeof(ra));
	memcpy(rb, b->resampler, sizeof(rb));
	memset(ra, 0x77, 12);
	memset(rb, 0x77, 12);
	diff_eq_obj_(__FILE__, __LINE__, "the resampler it built",
		     "ResamplerTimingOffset", ra, rb, sizeof(ra), trial);
	memcpy(&phases, ra + 0x30, sizeof(phases));
	memcpy(&taps, ra + 0x34, sizeof(taps));
	diff_eq_int("the resampler has 120 phases (trial %ld)", (int)phases,
		    V92MOD_RS_PHASES, trial);
	diff_eq_int("the resampler has 16 taps (trial %ld)", (int)taps,
		    V92MOD_RS_TAPS, trial);

	/*
	 * V92Phase4Modulator, 0x1cc bytes: its scrambler's seven pointers at
	 * +0x4c and its mapper at +0x70 are the two runs that cannot agree.
	 * Its four stored pointers ARE the four arguments this constructor
	 * chose, so they carry the whole of that call.
	 */
	memcpy(pa, a->phase4Modulator, sizeof(pa));
	memcpy(pb, b->phase4Modulator, sizeof(pb));
	memset(pa + 0x4c, 0x77, 7 * sizeof(void *));
	memset(pb + 0x4c, 0x77, 7 * sizeof(void *));
	memset(pa + 0x6c, 0x77, 2 * sizeof(void *));
	memset(pb + 0x6c, 0x77, 2 * sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, "the phase 4 modulator it built",
		     "V92Phase4Modulator", pa, pb, sizeof(pa), trial);
	diff_eq_int("the phase 4 modulator got our bit-to-symbol stage "
		    "(trial %ld)",
		    slot_ptr((const unsigned char *)a->phase4Modulator, 0x6c)
		    == (void *)a->bitsToSymbol, 1, trial);
	diff_eq_int("the phase 4 modulator got our V92CP (trial %ld)",
		    slot_ptr((const unsigned char *)a->phase4Modulator, 0x74)
		    == (void *)arg_cp, 1, trial);
	diff_eq_int("the phase 4 modulator got our mapping parameters "
		    "(trial %ld)",
		    slot_ptr((const unsigned char *)a->phase4Modulator, 0x48)
		    == (void *)arg_mp, 1, trial);
	diff_eq_int("the phase 4 modulator got our parameters (trial %ld)",
		    slot_ptr((const unsigned char *)a->phase4Modulator, 0x1c8)
		    == (void *)arg_params, 1, trial);
}

/*
 * The scrambler's history, which lives at two different addresses and holds
 * the same bytes.  `reset(0)` fills 1 + 23 elements from `pInitOut + 1` and
 * the whole 1 + 23 + 99 is allocated, so the whole allocation is compared:
 * anything the constructor seeds it with differently shows up here and
 * nowhere else.
 */
static void
scram_history(long trial)
{
	const unsigned char *a = (const unsigned char *)slot_ptr(ours, 0x54);
	const unsigned char *b = (const unsigned char *)slot_ptr(theirs, 0x54);
	unsigned n = (1 + V92MOD_SCRAM_TAP2 + V92MOD_SCRAM_SLACK)
		     * (unsigned)sizeof(int);

	diff_eq_int("the scrambler's history is seeded the same (trial %ld)",
		    memcmp(a, b, n) == 0, 1, trial);
}

static void
compare(const char *what, long trial)
{
	unsigned i;

	memcpy(cmp_a, ours, OBJSZ);
	memcpy(cmp_b, theirs, OBJSZ);
	for (i = 0; i < NSKIP; i++) {
		memset(cmp_a + skip[i].off, 0x77, skip[i].len);
		memset(cmp_b + skip[i].off, 0x77, skip[i].len);
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Modulator", cmp_a, cmp_b,
		     (size_t)OBJSZ, trial);
}

#define NTRIAL	26

/*
 * The constructor.  `n` is the first argument and it drives four of the five
 * buffer sizes; it is kept small so a wrong multiplier asks for a wrong
 * allocation rather than an impossible one.
 */
static int
run_ctor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor)
{
	unsigned char first[SLOT];
	int trial, moved = 0, distinct = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		int a_allocs, b_allocs;
		unsigned a_bytes, b_bytes, i, j;
		unsigned n = (unsigned)(trial * 7 + 1);
		V92Modulator *m = (V92Modulator *)ours;
		unsigned blk;

		seed(trial);
		harness_alloc_reset();

		our_ctor(ours, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);
		a_allocs = harness_alloc.allocs;
		a_bytes = harness_alloc.bytes;

		ref_ctor(theirs, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_bytes = harness_alloc.bytes - a_bytes;

		/*
		 * The reference is the oracle for the whole graph's size and
		 * shape.  This is what stands in for the five raw buffers,
		 * whose pointers the comparison has to poison.
		 */
		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs,
			    trial);
		diff_eq_int("bytes allocated (trial %ld)", (int)a_bytes,
			    (int)b_bytes, trial);
		diff_eq_int("no free of NULL (trial %ld)",
			    harness_alloc.free_null, 0, trial);
		diff_eq_int("no bad free (trial %ld)", harness_alloc.bad_free,
			    0, trial);

		blk = m->blockSize;

		/*
		 * `blockSize` against an EXACT INTEGER form of the same
		 * rounding: floor(n * 5/6 + 1/2) == floor((5n + 3) / 6).  The
		 * two can only disagree where n * 5/6 lands exactly on a half
		 * -- n congruent to 3 mod 6 -- and there the float's own
		 * rounding decides, so those trials are left to the
		 * comparison against the reference.
		 */
		if (n % 6u != 3u)
			diff_eq_int("blockSize is round(5n/6) (trial %ld)",
				    (int)blk, (int)((5u * n + 3u) / 6u),
				    trial);
		diff_eq_int("blockRemaining is blockSize (trial %ld)",
			    (int)m->blockRemaining, (int)blk, trial);
		diff_eq_int("queuePrime is the parameter halved (trial %ld)",
			    (int)m->queuePrime, queue_length(trial) >> 1,
			    trial);

		/* The four arguments that are computed rather than copied,
		 * read back out of the pieces they were passed to. */
		diff_eq_int("the bit-to-symbol stage got 3 * blockSize "
			    "(trial %ld)", (int)m->bitsToSymbol->nSymbols,
			    (int)(3u * blk), trial);
		diff_eq_int("the bit-to-symbol stage got our params "
			    "(trial %ld)",
			    m->bitsToSymbol->params
			    == (V92Parameters *)arg_params, 1, trial);
		/* `Queue`'s members are private and this fixture does not get
		 * to change that, so its slot count is read at +0x10, which
		 * is where its own header records it. */
		{
			unsigned qsize;

			memcpy(&qsize, (const unsigned char *)m->queue + 0x10,
			       sizeof(qsize));
			diff_eq_int("the queue got MODULATOR_QUEUE_LENGTH "
				    "(trial %ld)", (int)qsize,
				    queue_length(trial) + 1, trial);
		}
		diff_eq_int("the filter got the table's tap count (trial %ld)",
			    (int)m->txFilter->taps, 36, trial);
		diff_eq_int("the filter got 99 samples of slack (trial %ld)",
			    (int)m->txFilter->bufferLength, 36 + 99, trial);
		diff_eq_int("the filter got the transmit table (trial %ld)",
			    m->txFilter->coefficients == v92TxPreFilter, 1,
			    trial);

		/* The seven arguments that are only stored. */
		diff_eq_int("+0x10 is the second argument (trial %ld)",
			    slot_ptr(ours, 0x10) == (void *)arg_p2, 1, trial);
		diff_eq_int("+0x14 is the third argument (trial %ld)",
			    slot_ptr(ours, 0x14) == (void *)arg_ja, 1, trial);
		diff_eq_int("+0x18 is the fourth argument (trial %ld)",
			    slot_ptr(ours, 0x18) == (void *)arg_dil, 1, trial);
		diff_eq_int("+0x1c is the sixth argument (trial %ld)",
			    slot_ptr(ours, 0x1c) == (void *)arg_mp, 1, trial);
		diff_eq_int("+0x20 is the fifth argument (trial %ld)",
			    slot_ptr(ours, 0x20) == (void *)arg_cp, 1, trial);
		diff_eq_int("+0x40 is the seventh argument (trial %ld)",
			    slot_ptr(ours, 0x40) == (void *)arg_params, 1,
			    trial);

		for (i = 0; i < NOWNED; i++) {
			diff_eq_int("owned pointer +0x%02lx is not null",
				    slot_ptr(ours, owned[i]) != 0, 1, owned[i]);
			diff_eq_int("ref owned pointer +0x%02lx is not null",
				    slot_ptr(theirs, owned[i]) != 0, 1,
				    owned[i]);
			for (j = 0; j < i; j++)
				diff_eq_int("+0x%02lx is its own allocation",
					    slot_ptr(ours, owned[i])
					    != slot_ptr(ours, owned[j]), 1,
					    owned[i]);
		}

		scram_geometry(trial);
		scram_history(trial);
		queue_cursors(m, queue_length(trial) >> 1, trial);
		subobject_compare(m, (const V92Modulator *)theirs, trial);

		compare("after the constructor", trial);
		diff_eq_int("nothing stored past the object (trial %ld)",
			    guard_intact(), 1, trial);

		if (memcmp(sown, ours, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours, SLOT);
		else if (memcmp(first, ours, SLOT) != 0)
			distinct = 1;

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

	return diff_end();
}

/*
 * The destructor over every one of the 2,048 null/non-null combinations of
 * the eleven pointers it guards.  The pointers that stay live are REAL, not
 * wild: six of the eleven get a destructor call that dereferences them.
 *
 * Each combination is destroyed twice -- once as chosen, once over the
 * complement -- so nothing leaks and `live == 0` afterwards is what says the
 * two halves covered all eleven exactly once.  The scrambler's own buffer is
 * released by the first call, so it is nulled before the second; that is the
 * fixture's bookkeeping and not a claim about the object.
 */
static int
run_dtor(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	 dtor_fn our_dtor, dtor_fn ref_dtor)
{
	unsigned char before[SLOT];
	unsigned subset;
	int saw_null = 0, saw_live = 0, freed = 0;

	diff_begin(name);

	for (subset = 0; subset < (1u << NOWNED); subset++) {
		int a_frees, b_frees, a_null, b_null, a_bad, b_bad;
		unsigned i, n = (unsigned)(subset % 61u) + 1u;

		seed((int)(subset % 17u));
		harness_alloc_reset();

		our_ctor(ours, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);
		ref_ctor(theirs, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);
		memcpy(ctor_a, ours, SLOT);
		memcpy(ctor_b, theirs, SLOT);

		for (i = 0; i < NOWNED; i++) {
			if ((subset >> i) & 1u) {
				slot_set_ptr(ours, owned[i], 0);
				slot_set_ptr(theirs, owned[i], 0);
				saw_null = 1;
			} else {
				saw_live = 1;
			}
		}
		memcpy(before, ours, SLOT);

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

		diff_eq_int("frees, subset 0x%03lx", a_frees, b_frees,
			    (long)subset);
		/* Drop a guard and this is the only counter that moves. */
		diff_eq_int("sysdep_free(NULL), subset 0x%03lx", a_null, 0,
			    (long)subset);
		diff_eq_int("ref sysdep_free(NULL), subset 0x%03lx", b_null, 0,
			    (long)subset);
		diff_eq_int("no bad free, subset 0x%03lx", a_bad, 0,
			    (long)subset);
		diff_eq_int("ref no bad free, subset 0x%03lx", b_bad, 0,
			    (long)subset);

		/* Nothing is nulled after a free; our own state before the
		 * call is what proves it. */
		diff_eq_int("the destructor stored nothing, subset 0x%03lx",
			    memcmp(before, ours, SLOT) == 0, 1, (long)subset);

		compare("after the destructor", (long)subset);
		diff_eq_int("nothing stored past the object, subset 0x%03lx",
			    guard_intact(), 1, (long)subset);

		if (a_frees > 0)
			freed = 1;

		memcpy(ours, ctor_a, SLOT);
		memcpy(theirs, ctor_b, SLOT);
		for (i = 0; i < NOWNED; i++) {
			if (((subset >> i) & 1u) == 0) {
				slot_set_ptr(ours, owned[i], 0);
				slot_set_ptr(theirs, owned[i], 0);
			}
		}
		/* The scrambler's buffer is gone already. */
		slot_set_ptr(ours, 0x54, 0);
		slot_set_ptr(theirs, 0x54, 0);
		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("the two halves freed everything, subset 0x%03lx",
			    harness_alloc.live, 0, (long)subset);
		diff_eq_int("and freed nothing twice, subset 0x%03lx",
			    harness_alloc.bad_free, 0, (long)subset);
	}

	diff_eq_int("a null pointer was tried", saw_null, 1, 0);
	diff_eq_int("a non-null pointer was tried", saw_live, 1, 0);
	diff_eq_int("the destructor released something", freed, 1, 0);

	return diff_end();
}

/*
 * The same constructor with the diagnostic channel live.
 *
 * The two `dsplibs_debug_level > 1` sites in this constructor -- its own
 * "V92Modulator constraction" and the "V92Modulator reset" belonging to the
 * `reset` it inlines -- are DEAD in every other run here, because the level
 * ships at zero.  A wrong string, a swapped pair or a gate written
 * `> 2` would then be invisible to every check in this file.  So the pair is
 * driven again at level 2 with the harness's debug capture on, and the two
 * transcripts are compared as text and counted as lines: text alone can be
 * filled by the harness without a call site firing (finding F149).
 *
 * The object comparison is not repeated -- the runs above cover it, and this
 * run's subject is the two lines.
 */
static int
run_debug(const char *name, ctor_fn our_ctor, ctor_fn ref_ctor,
	  dtor_fn our_dtor, dtor_fn ref_dtor)
{
	int trial, saw = 0;

	diff_begin(name);
	set_level(2);
	dsplib_debug_capture_on = 1;

	for (trial = 0; trial < 4; trial++) {
		unsigned n = (unsigned)(trial * 11 + 5);
		unsigned lines;

		seed(trial);
		harness_alloc_reset();
		dsplib_debug_capture_reset();

		our_ctor(ours, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);
		ref_ctor(theirs, n, arg_p2, arg_ja, arg_dil, arg_cp, arg_mp,
			 arg_params);

		lines = dsplib_debug_capture_lines(0);
		diff_eq_int("the two transcripts agree (trial %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		diff_eq_int("both sides printed the same number of lines "
			    "(trial %ld)", (int)lines,
			    (int)dsplib_debug_capture_lines(1), trial);
		/*
		 * THREE lines, and each one is placed: "V92Modulator
		 * constraction" at the head, "V92Modulator reset" from the
		 * `reset` this constructor inlines -- which is the running
		 * evidence for finding F1283 -- and the phase 3 modulator's
		 * own "TRN1u state length set to ..." from the `reset` ITS
		 * constructor calls.  Two of the three come out of this file
		 * and the third proves the sub-object was really built.
		 */
		diff_eq_int("three diagnostic lines, not one (trial %ld)",
			    (int)lines, 3, trial);
		if (lines > 0)
			saw = 1;

		our_dtor(ours);
		ref_dtor(theirs);
		diff_eq_int("nothing left allocated (trial %ld)",
			    harness_alloc.live, 0, trial);
	}

	diff_eq_int("something was printed at all", saw, 1, 0);

	dsplib_debug_capture_on = 0;
	set_level(0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctor("V92Modulator::V92Modulator (C1)", our_c1, ref_c1,
		       our_d1, ref_d1);
	rc |= run_ctor("V92Modulator::V92Modulator (C2)", our_c2, ref_c2,
		       our_d2, ref_d2);
	rc |= run_debug("V92Modulator::V92Modulator (C1), diagnostics live",
			our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_debug("V92Modulator::V92Modulator (C2), diagnostics live",
			our_c2, ref_c2, our_d2, ref_d2);
	rc |= run_dtor("V92Modulator::~V92Modulator, all 2048 null "
		       "combinations (D1)", our_c1, ref_c1, our_d1, ref_d1);
	rc |= run_dtor("V92Modulator::~V92Modulator, all 2048 null "
		       "combinations (D2)", our_c2, ref_c2, our_d2, ref_d2);

	return rc;
}
