/*
 * t_v92unpck.c -- differential test of `V92setParamsInfoFromCPUnPck`.
 *
 * ONE SYMBOL, AND HALF OF IT IS UNREACHABLE AT LEVEL 0.  Twenty-six of the
 * function's branches are `dsplibs_debug_level > 1` gates, so a run at the
 * shipping level exercises every copy, every clamp and every loop bound and
 * NOT ONE format string.  That is the case finding F126 is about -- a wrong
 * conversion or a wrong argument in a diagnostic survives for ever if no test
 * raises the level -- so this fixture runs the same cases twice: once at
 * level 0 over lengths chosen to hit the clamps, and once at level 2 over
 * lengths small enough that both transcripts fit in the capture buffer, with
 * the two transcripts compared as strings.  The second run is what checks the
 * thirty-one format strings, the `%c%d.%06d` float formatting and the
 * argument order; the first is what checks the modem.
 *
 * WHAT IS SHARED AND WHAT IS NOT.  The CP block and the six constellation
 * arrays it points at are INPUT: one copy, pointed at by both sides, because
 * two separately seeded inputs would agree whatever was read out of them.
 * The parameter block and the ten arrays it owns are OUTPUT and are two of
 * everything.
 *
 * THE TEN POINTERS ARE POISONED BEFORE THE BLOCKS ARE COMPARED, because they
 * are two different arrays and always will be; what is compared instead is
 * the CONTENTS of all ten, in full, including the entries past the length so
 * that a loop running one element too far is a failure and not a rounding.
 * The function must not store to the ten slots at all, and that is asserted
 * separately rather than hidden by the poison.
 *
 * NOTHING IS ZERO-FILLED.  Both parameter blocks and all twenty arrays are
 * seeded with the same varied pseudorandom bytes before every trial, so a
 * store of zero that never happened cannot pass (findings F223, F224), and a
 * 64-byte guard past the block is compared against the seed so that a store
 * one byte past the end fails.
 *
 * WHY `prefilterGain` IS BOUNDED.  `gain` is `prefilterGain * 2^-18 * 4000`,
 * and the diagnostic prints its integer part.  Above 2^31 / 4000 * 2^18 the
 * conversion is out of `int` range, which is undefined in C and is 0x80000000
 * on both sides here -- agreeing for a reason that has nothing to do with the
 * modem.  The seed keeps it under 2^26, where the arithmetic is ordinary; the
 * clamps and the loop bounds are where the extremes are worth spending.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92CPUnPck.h"
#include "dsplib/debug.h"

extern void ref_V92setParamsInfoFromCPUnPck(struct V92ParamsInfo *p,
					    struct V92CPUnPck *cp);
extern unsigned int ref_dsplibs_debug_level;

#define GUARD		64
#define NCOEF		V92_CPUNPCK_COEFS	/* 384, the CP's own array */
#define NCONST		V92_PARAMSINFO_MAX_LC	/* 128, one constellation */

struct slot {
	struct V92ParamsInfo p;
	unsigned char guard[GUARD];
};

static struct slot pi[2];
static struct slot seedcopy;

/* Output: two of each. */
static float coefbuf[2][4][NCOEF];
static int constbuf[2][V92_PARAMSINFO_CONSTELLATIONS][NCONST];

/* Input: one of each, pointed at by both sides. */
static struct V92CPUnPck cp;
static int cpconst[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

/* Where the ten pointers live inside the block. */
static const size_t ptr_off[10] = {
	0x5c, 0x60, 0x64, 0x68,
	0x84, 0x88, 0x8c, 0x90, 0x94, 0x98
};

static unsigned int lfsr;

static unsigned char
nextbyte(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned int)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)(lfsr >> 3);
}

static void
fill(void *dst, size_t n)
{
	unsigned char *q = (unsigned char *)dst;
	size_t i;

	for (i = 0; i < n; i++)
		q[i] = nextbyte();
}

/*
 * The ten arrays the block owns.  Both sides get the SAME bytes, so an
 * element the function never writes compares equal for the right reason.
 */
static void
seed_outputs(unsigned int s)
{
	int i, j;

	lfsr = 0x2f19u + 0x9e37u * s;
	fill(&seedcopy, sizeof(seedcopy));
	memcpy(&pi[0], &seedcopy, sizeof(pi[0]));
	memcpy(&pi[1], &seedcopy, sizeof(pi[1]));

	for (i = 0; i < 4; i++) {
		fill(coefbuf[0][i], sizeof(coefbuf[0][i]));
		memcpy(coefbuf[1][i], coefbuf[0][i], sizeof(coefbuf[0][i]));
	}
	for (j = 0; j < V92_PARAMSINFO_CONSTELLATIONS; j++) {
		fill(constbuf[0][j], sizeof(constbuf[0][j]));
		memcpy(constbuf[1][j], constbuf[0][j], sizeof(constbuf[0][j]));
	}
}

static void
wire(int side)
{
	struct V92ParamsInfo *p = &pi[side].p;
	int i;

	p->z1 = coefbuf[side][0];
	p->p1 = coefbuf[side][1];
	p->z2 = coefbuf[side][2];
	p->p2 = coefbuf[side][3];
	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++)
		p->constellations[i] = constbuf[side][i];
}

/*
 * The CP block.  Everything is seeded, then the fields that decide how far
 * the function walks are overwritten with the case's own values -- and
 * `prefilterGain` is narrowed for the reason in the file comment.
 */
struct tcase {
	const char *name;
	int mep, ppp, cop;
	unsigned int lz1, lp1, lz2, lp2;
	unsigned int lc0, lc1, lc2, lc3, lc4, lc5;
};

static void
seed_cp(unsigned int s, const struct tcase *t)
{
	int j;

	lfsr = 0x7a11u + 0x4e6du * s;
	fill(&cp, sizeof(cp));
	for (j = 0; j < V92_PARAMSINFO_CONSTELLATIONS; j++)
		fill(cpconst[j], sizeof(cpconst[j]));

	cp.const1 = cpconst[0];
	cp.const2 = cpconst[1];
	cp.const3 = cpconst[2];
	cp.const4 = cpconst[3];
	cp.const5 = cpconst[4];
	cp.const6 = cpconst[5];

	/*
	 * Bounded for the reason in the file comment -- and forced to EXACTLY
	 * zero on one seed in five, because `gain` is `prefilterGain` times
	 * two positive constants and zero is the only non-positive value it
	 * can take.  Without that, `sign_of`'s `<` versus `<=` is unreachable.
	 *
	 * ONE IN FIVE AND NOT ONE IN SEVEN, and the difference is the whole
	 * point: the sign only shows up in a PRINT, so the zero has to land on
	 * a TRANSCRIPT seed, and those are 900..905.  A one-in-seven rule hit
	 * plenty of the level-0 seeds, where nothing prints, and none of the
	 * six transcript ones -- so both mutations survived a run that looked
	 * like it was covering them.  With `% 5 == 2` seed 902 is zero.
	 */
	cp.prefilterGain &= 0x03ffffffu;
	if ((s % 5u) == 2u)
		cp.prefilterGain = 0u;

	cp.modulosEncoderPresent = t->mep;
	cp.prefilterPrecoderPresent = t->ppp;
	cp.constellationPresent = t->cop;

	cp.lz1 = t->lz1;
	cp.lp1 = t->lp1;
	cp.lz2 = t->lz2;
	cp.lp2 = t->lp2;

	cp.LC[0] = t->lc0;
	cp.LC[1] = t->lc1;
	cp.LC[2] = t->lc2;
	cp.LC[3] = t->lc3;
	cp.LC[4] = t->lc4;
	cp.LC[5] = t->lc5;
}

/*
 * `p->constellationPresent` gates the copies, and the copies are what the
 * blocks are compared on -- but the FLAGS themselves are copied before they
 * are tested, so a run whose flags came out of the seed rather than the case
 * would be testing the seed.  They are set from the case above; this is the
 * assertion that the function then read what it wrote.
 */
static void
compare_run(const char *what, long tag)
{
	void *saved[2][10];
	int i, side;

	for (side = 0; side < 2; side++)
		for (i = 0; i < 10; i++) {
			void *want = (i < 4)
			    ? (void *)coefbuf[side][i]
			    : (void *)constbuf[side][i - 4];

			memcpy(&saved[side][i],
			       (unsigned char *)&pi[side].p + ptr_off[i],
			       sizeof(void *));
			diff_eq_int("pointer slot survives (%ld)",
				    (long)(saved[side][i] == want), 1,
				    tag * 100 + side * 10 + i);
		}

	/* Poison the ten, identically, so the block itself can be compared. */
	for (side = 0; side < 2; side++)
		for (i = 0; i < 10; i++) {
			void *poison = (void *)0;

			memcpy((unsigned char *)&pi[side].p + ptr_off[i],
			       &poison, sizeof poison);
		}

	diff_eq_obj_(__FILE__, __LINE__, what, "struct V92ParamsInfo",
		     &pi[0], &pi[1], sizeof(struct V92ParamsInfo), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the guard past the block",
		     pi[0].guard, seedcopy.guard, GUARD, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "the reference's guard",
		     pi[1].guard, seedcopy.guard, GUARD, tag);

	for (i = 0; i < 4; i++)
		diff_eq_obj_(__FILE__, __LINE__, what, "a coefficient bank",
			     coefbuf[0][i], coefbuf[1][i],
			     sizeof(coefbuf[0][i]), tag * 10 + i);
	for (i = 0; i < V92_PARAMSINFO_CONSTELLATIONS; i++)
		diff_eq_obj_(__FILE__, __LINE__, what, "a constellation",
			     constbuf[0][i], constbuf[1][i],
			     sizeof(constbuf[0][i]), tag * 10 + i);

	/* Restore, so a caller that wants to look at them still can. */
	for (side = 0; side < 2; side++)
		for (i = 0; i < 10; i++)
			memcpy((unsigned char *)&pi[side].p + ptr_off[i],
			       &saved[side][i], sizeof(void *));
}

/*
 * THE CLAMPS ARE WHERE THE EXTREMES GO.  0x148 and 0x149 straddle the filter
 * ceiling, 0x80 and 0x81 the constellation one, and 0xffffffff is what says
 * the comparison is unsigned: a signed `>` would leave -1 alone and the loop
 * that follows would then not run at all, which is exactly the shape a
 * `jle`-for-`jbe` defect has.
 */
static const struct tcase cases[] = {
 { "all three halves off",       0, 0, 0,   3,  4,  5,  6,   2, 2, 2, 2, 2, 2 },
 { "moduli only",                1, 0, 0,   3,  4,  5,  6,   2, 2, 2, 2, 2, 2 },
 { "prefilter/precoder only",    0, 1, 0,   3,  4,  5,  6,   2, 2, 2, 2, 2, 2 },
 { "constellations only",        0, 0, 1,   3,  4,  5,  6,   2, 2, 2, 2, 2, 2 },
 { "all three on, short",        1, 1, 1,   1,  2,  3,  4,   1, 2, 3, 4, 5, 6 },
 { "empty filters",              1, 1, 1,   0,  0,  0,  0,   3, 3, 3, 3, 3, 3 },
 { "empty constellations",       1, 1, 1,   7,  7,  7,  7,   0, 0, 0, 0, 0, 0 },
 { "one empty constellation",    1, 1, 1,   7,  7,  7,  7,   0, 4, 0, 4, 0, 4 },
 { "at the filter ceiling",      1, 1, 1, 0x148, 0x148, 0x148, 0x148,
							   2, 2, 2, 2, 2, 2 },
 { "one past the filter ceiling", 1, 1, 1, 0x149, 0x200, 0x149, 0x1000,
							   2, 2, 2, 2, 2, 2 },
 { "filter lengths all ones",    1, 1, 1, 0xffffffffu, 0xffffffffu,
					  0xffffffffu, 0xffffffffu,
							   2, 2, 2, 2, 2, 2 },
 { "at the constellation ceiling", 1, 1, 1,  5,  5,  5,  5,
					     0x80, 0x80, 0x80, 0x80, 0x80, 0x80 },
 { "one past the constellation ceiling", 1, 1, 1, 5, 5, 5, 5,
					     0x81, 0x100, 0x81, 0xffff, 0x81,
					     0x81 },
 { "LC all ones",                1, 1, 1,   5,  5,  5,  5,
					     0xffffffffu, 0xffffffffu,
					     0xffffffffu, 0xffffffffu,
					     0xffffffffu, 0xffffffffu },
 { "nonzero flags that are not 1", 0x5a5a5a5a, -1, 0x7fffffff, 4, 4, 4, 4,
							   3, 3, 3, 3, 3, 3 }
};
#define NCASE ((int)(sizeof(cases) / sizeof(cases[0])))

/*
 * The transcript cases: the same shapes, cut down so that both sides' output
 * fits in the 16 KB capture.  A truncated transcript would still compare
 * equal -- both sides truncate at the same place -- and would silently stop
 * testing anything past the cut, which is the failure mode this avoids.
 */
static const struct tcase tcases[] = {
 { "transcript, all on",         1, 1, 1,   2,  3,  1,  2,   1, 2, 3, 2, 1, 2 },
 { "transcript, empty filters",  1, 1, 1,   0,  0,  0,  0,   2, 2, 2, 2, 2, 2 },
 { "transcript, empty constels", 1, 1, 1,   3,  3,  3,  3,   0, 0, 0, 0, 0, 0 },
 { "transcript, one empty",      1, 1, 1,   2,  0,  2,  0,   0, 1, 0, 1, 0, 1 },
 { "transcript, flags off",      0, 0, 0,   2,  2,  2,  2,   2, 2, 2, 2, 2, 2 },
 { "transcript, moduli only",    1, 0, 0,   2,  2,  2,  2,   2, 2, 2, 2, 2, 2 }
};
#define NTCASE ((int)(sizeof(tcases) / sizeof(tcases[0])))

static void
run_one(const struct tcase *t, unsigned int s)
{
	seed_outputs(s);
	seed_cp(s, t);
	wire(0);
	wire(1);

	V92setParamsInfoFromCPUnPck(&pi[0].p, &cp);
	ref_V92setParamsInfoFromCPUnPck(&pi[1].p, &cp);
}

static int
run_level0(void)
{
	int c, rep;

	diff_begin("V92setParamsInfoFromCPUnPck, level 0");

	for (c = 0; c < NCASE; c++)
		for (rep = 0; rep < 3; rep++) {
			unsigned int s = (unsigned int)(c * 3 + rep) + 1u;

			run_one(&cases[c], s);
			compare_run(cases[c].name, (long)(c * 3 + rep));

			/*
			 * The three flags are copied unconditionally and are
			 * what everything else is gated on, so they are worth
			 * a verdict of their own rather than only inside the
			 * block comparison.
			 */
			diff_eq_int("the three flags landed (case %ld)",
				    (long)(pi[0].p.modulosEncoderPresent
					   == cases[c].mep
					   && pi[0].p.prefilterPrecoderPresent
					      == cases[c].ppp
					   && pi[0].p.constellationPresent
					      == cases[c].cop), 1, (long)c);
		}

	return diff_end();
}

static int
run_transcripts(void)
{
	int c;

	diff_begin("V92setParamsInfoFromCPUnPck, level 2 transcripts");

	for (c = 0; c < NTCASE; c++) {
		unsigned int s = 900u + (unsigned int)c;
		const char *o;
		const char *r;

		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		run_one(&tcases[c], s);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		o = dsplib_debug_capture_text(0);
		r = dsplib_debug_capture_text(1);

		diff_eq_int("transcript matches (case %ld)",
			    strcmp(o, r) == 0, 1, (long)c);
		diff_eq_int("line counts match (case %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)c);
		/*
		 * Anti-vacuity: the buffer being equal proves nothing if
		 * neither side printed.  Every case above reaches at least
		 * the eleven ungated-by-a-flag prints at the top.
		 */
		diff_eq_int("the reference actually printed (case %ld)",
			    (int)(dsplib_debug_capture_lines(1) >= 11), 1,
			    (long)c);
		/* And it must not have overflowed the capture. */
		diff_eq_int("transcript is not at the buffer limit (case %ld)",
			    (int)(strlen(r) < 15000u), 1, (long)c);

		if (getenv("DBGDIFF") && strcmp(o, r) != 0) {
			int k = 0;

			while (o[k] && o[k] == r[k])
				k++;
			while (k > 0 && o[k - 1] != '\n')
				k--;
			printf("=== %s: first divergence at %d\n",
			       tcases[c].name, k);
			printf("--- ours: %.300s\n", o + k);
			printf("--- ref : %.300s\n", r + k);
		}

		compare_run(tcases[c].name, 500 + (long)c);
	}

	return diff_end();
}

/* ----------------------------- V.92 Table 30 independent rate oracle */

/*
 * The runs above compare the reconstruction with the blob over the whole
 * unpacker, including its diagnostic text.  Agreement does not establish
 * that the CPd data signalling rate is interpreted as Table 30 requires, so
 * this layer judges each implementation separately.
 *
 * For a non-cleardown CPd, Table 30 defines DRn values 1 through 19.  Those
 * select 18 through 36 upstream data-rate units, and the mapping block's K is
 * twice that number: 36 through 72.  The literal table below is deliberately
 * not obtained from the production expression.  In particular, the debug
 * transcript's separately scaled and integer-formatted "Upstream rate" is a
 * diagnostic only and is not used as the standards oracle here.  DRn zero
 * (cleardown) and reserved/out-of-domain values 20 through 31 remain outside
 * these conformance groups.
 */

typedef void (*table30_unpack_fn)(struct V92ParamsInfo *,
				  struct V92CPUnPck *);

struct table30_subject {
	const char *name;
	table30_unpack_fn unpack;
};

static const int table30_k[19] = {
	36, 38, 40, 42, 44, 46, 48, 50, 52, 54,
	56, 58, 60, 62, 64, 66, 68, 70, 72
};

static struct V92CPUnPck table30_cp_before;
static int table30_const_before[V92_PARAMSINFO_CONSTELLATIONS][NCONST];

static int
run_table30_subject(const struct table30_subject *subject, unsigned int side)
{
	static const struct tcase dormant = {
		"Table 30", 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
	};
	int drn;

	diff_begin(subject->name);

	for (drn = 1; drn <= 19; drn++) {
		unsigned int seed = 1200u + side * 100u + (unsigned int)drn;

		seed_outputs(seed);
		seed_cp(seed, &dormant);
		wire((int)side);
		cp.drn = (signed char)drn;
		memcpy(&table30_cp_before, &cp, sizeof cp);
		memcpy(table30_const_before, cpconst, sizeof cpconst);

		/* No transcript participates in this standards verdict. */
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		subject->unpack(&pi[side].p, &cp);

		diff_eq_int("Table 30 K for DRn %ld", pi[side].p.K,
		    table30_k[drn - 1], (long)drn);
		diff_eq_int("destination guard survives DRn %ld",
		    memcmp(pi[side].guard, seedcopy.guard, GUARD) == 0,
		    1, (long)drn);
		diff_eq_int("CP input is unchanged for DRn %ld",
		    memcmp(&cp, &table30_cp_before, sizeof cp) == 0,
		    1, (long)drn);
		diff_eq_int("CP pointed-to inputs are unchanged for DRn %ld",
		    memcmp(cpconst, table30_const_before, sizeof cpconst) == 0,
		    1, (long)drn);
	}

	return diff_end();
}

static int
run_table30(void)
{
	static const struct table30_subject subjects[] = {
		{ "V.92 Table 30 CPd rate oracle, reconstruction",
		  V92setParamsInfoFromCPUnPck },
		{ "V.92 Table 30 CPd rate oracle, blob",
		  ref_V92setParamsInfoFromCPUnPck }
	};
	int rc = 0;
	unsigned int i;

	for (i = 0; i < sizeof subjects / sizeof subjects[0]; i++)
		rc |= run_table30_subject(&subjects[i], i);
	return rc;
}

int
main(void)
{
	int rc = 0;

	/* The map this fixture pokes at by offset has to be the header's. */
	diff_begin("the parameter block's map");
	diff_eq_int("sizeof(struct V92ParamsInfo) is %ld",
		    (long)sizeof(struct V92ParamsInfo), 0xb4, 0xb4);
	diff_eq_int("z1 is at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, z1), 0x5c, 0x5c);
	diff_eq_int("constellations are at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, constellations),
		    0x84, 0x84);
	diff_eq_int("LC is at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, LC), 0x6c, 0x6c);
	diff_eq_int("indexConstel is at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, indexConstel),
		    0x9c, 0x9c);
	diff_eq_int("m is at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, m), 0x1c, 0x1c);
	diff_eq_int("gain is at +0x%lx",
		    (long)offsetof(struct V92ParamsInfo, gain), 0x18, 0x18);
	/* The CP side, which nothing else in the tree pins. */
	diff_eq_int("CP: M is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, M), 0x18, 0x18);
	diff_eq_int("CP: lz1 is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, lz1), 0x48, 0x48);
	diff_eq_int("CP: z1 is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, z1), 0x58, 0x58);
	diff_eq_int("CP: p1 is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, p1), 0x358, 0x358);
	diff_eq_int("CP: LC is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, LC), 0xc58, 0xc58);
	diff_eq_int("CP: indexConstel is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, indexConstel),
		    0xc70, 0xc70);
	diff_eq_int("CP: const1 is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, const1), 0xc88, 0xc88);
	diff_eq_int("CP: const6 is at +0x%lx",
		    (long)offsetof(struct V92CPUnPck, const6), 0xc9c, 0xc9c);
	rc |= diff_end();

	rc |= run_level0();
	rc |= run_transcripts();
	rc |= run_table30();

	return rc;
}
