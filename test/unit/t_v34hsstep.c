/*
 * t_v34hsstep.c -- the per-dispatch-case harness for `v34handshak`, proved.
 *
 * This test does NOT reconstruct any dispatch case, and there is nothing here
 * that could: `v34handshak` has no reconstruction.  What it proves is that
 * the fixture in test/harness/v34hsstep.c can put the object into a chosen
 * dispatch case, step the function once, and tell that case apart from its
 * neighbours -- which is the precondition for #56, #57 and #58 being sixteen
 * committable pieces rather than one 61,541-byte sitting.
 *
 * Three claims, in the order they have to hold:
 *
 *   1  THE FIXTURE IS SOUND.  Two objects at different addresses, brought up
 *      by different code -- this tree's `v34handshakinit` on side A and the
 *      blob's on side B -- step to byte-identical results and identical
 *      transcripts.  That is a real differential comparison even with the
 *      blob on both sides of the step itself: it fails if the fixture leaves
 *      anything unseeded, if the step depends on an address, or if our
 *      bring-up and the blob's have drifted apart.
 *
 *   2  THE ROUTE KNOBS WORK.  The four guards select table 1, table 2 or the
 *      rxstate chain, and each is confirmed by what the step DID rather than
 *      by reasoning about the guard.
 *
 *   3  THE CASES SEPARATE.  Distinct dispatch targets leave distinct
 *      signatures.  Asserted as NAMED PAIRS rather than over all pairs,
 *      because the tables alias by design -- twenty-four microstates share
 *      one three-instruction arm -- so an all-pairs claim would be red from
 *      its first run, which is the shape finding 134 says nobody keeps.
 *      The pairs that are equal are asserted equal, for the same reason: a
 *      collision that is the object's own structure is evidence too, and it
 *      is what tells the next agent its case needs companion fields.
 *
 * `V34HS_DUMP=1` in the environment prints the signature of every state word
 * driven.  That is how the sets below were chosen and how a per-case agent
 * finds out whether its case does anything at all from a cold object.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* --- recording ------------------------------------------------------------ */

struct rec {
	short		state;
	unsigned	changed;
	unsigned	first;
	unsigned	hash;
	unsigned	lines;
	/*
	 * WHERE EACH MACHINE ENDED, RELATIVE TO WHERE IT STARTED.  -1 means
	 * "did not move".  The raw value cannot go in the signature: it is
	 * the value the case was ENTERED with, so a comparison including it
	 * calls every case distinct from every other by construction, and
	 * sixteen cases that all did nothing would still "separate".  That is
	 * exactly the vacuous check this test exists to avoid, and it passed
	 * for a whole afternoon before it was noticed.
	 */
	short		mst, rxst, txst;
	int		progress;
};

static int dump;

static void
record(struct rec *r, short state, short in_mst, short in_rx, short in_tx,
       const char *what)
{
	const struct v34hs_obs *o = v34hs_observed(0);

	r->state = state;
	r->changed = o->changed;
	r->first = o->first;
	r->hash = o->hash;
	r->lines = o->lines;
	r->mst = o->mst == in_mst ? -1 : o->mst;
	r->rxst = o->rxst == in_rx ? -1 : o->rxst;
	r->txst = o->txst == in_tx ? -1 : o->txst;
	r->progress = o->progress;

	if (dump)
		printf("  %-9s %3d  wrote %5u B  first +0x%05x  sig %08x  "
		       "lines %2u  -> mst %3d rx %3d tx %3d  progress %d\n",
		       what, state, r->changed,
		       r->first == ~0u ? 0 : r->first,
		       r->hash, r->lines, r->mst, r->rxst, r->txst,
		       r->progress);
}

/*
 * WHAT COUNTS AS THE SAME BEHAVIOUR: what the step wrote, what it printed,
 * and which of the three machines MOVED -- never the state it was entered
 * with.  See the comment on `struct rec`.
 */
static int
same(const struct rec *x, const struct rec *y)
{
	return x->changed == y->changed && x->hash == y->hash
	       && x->lines == y->lines && x->progress == y->progress
	       && x->mst == y->mst && x->rxst == y->rxst
	       && x->txst == y->txst;
}

static void
differ(const char *what, const struct rec *x, const struct rec *y)
{
	char msg[160];

	snprintf(msg, sizeof(msg), "%s: %d and %d take different paths",
		 what, x->state, y->state);
	diff_eq_int(msg, same(x, y), 0, (long)x->state * 1000 + y->state);
}

static void
agree(const char *what, const struct rec *x, const struct rec *y)
{
	char msg[160];

	snprintf(msg, sizeof(msg), "%s: %d and %d share one arm",
		 what, x->state, y->state);
	diff_eq_int(msg, same(x, y), 1, (long)x->state * 1000 + y->state);
}

/* --- the three routes ----------------------------------------------------- */

/*
 * A microstate case.  rxstate is V34HS_RX_DPSK because that is the only value
 * that reaches table 3, and txstate is held at SSEG throughout so that a
 * difference between two runs is the microstate's and nothing else's.
 *
 * SSEG is not arbitrary: most of table 3's arms end by jumping to the
 * once-per-block txstate dispatch at 0x62af1 (finding 288), so the txstate a
 * microstate case is driven with is part of the fixture and has to be stated.
 */
#define MICRO_TXSTATE	V34HS_SSEG

static void
run_micro(struct rec *r, short mst, long tag)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_RX_DPSK, MICRO_TXSTATE);
	v34hs_step();
	v34hs_compare("microstate step", tag);
	record(r, mst, mst, V34HS_RX_DPSK, MICRO_TXSTATE, "microstate");
}

/*
 * A table-2 case: cursor at the limit and receiver +0x00 <= 5, so the
 * once-per-block transmit dispatch runs.  The microstate is held outside
 * table 3's 41..80 window and rxstate outside the chain's three values, so
 * neither can contribute.
 */
static void
run_txblock(struct rec *r, short txst, long tag)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXBLOCK, 0);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);
	v34hs_step();
	v34hs_compare("txstate block step", tag);
	record(r, txst, V34HS_PHASE1, V34HS_SILENCE, txst, "txblock");
}

/*
 * A table-1 case: one pass of the per-sample loop.
 *
 * `samples` is an explicit budget rather than a step count because the loop
 * bottom at 0x629e0 IS the dispatch's default arm (finding 287): a txstate
 * with no case of its own never advances the cursor and spins forever, so
 * only a state with a real target may be driven here.
 *
 * THIS ROUTE USED NOT TO COMPARE EQUAL, and finding 289 and D60 recorded it
 * as a property of the object: three of its nineteen targets left the two
 * sides differing in the modulator at +0x2078..+0x25d1, and WHICH three moved
 * when unrelated code in the fixture changed.
 *
 * It was the fixture.  The two sides' memory images were not congruent -- two
 * objects at unrelated addresses with unrelated neighbours -- and this loop is
 * the one route that can tell.  One arena per side, laid out identically and
 * copied byte for byte (finding 319), closes it: all nineteen compare, over
 * twenty-four object fills, ten placements and eight neighbourhoods
 * (finding 322).  The route is in the default sweep now.
 */
static void
run_txsample(struct rec *r, short txst, short samples, long tag)
{
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, samples);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);
	v34hs_step();
	v34hs_compare("txstate sample step", tag);
	record(r, txst, V34HS_PHASE1, V34HS_SILENCE, txst, "txsample");
}

/* --- table 3: the microstate machine, which is #57 ------------------------ */

/*
 * One representative per distinct target of the table at .rodata+0x3000, read
 * with its relocations attached (finding 286), plus one state outside the
 * table's 41..80 window for the default arm.
 *
 * 42 stands for the twenty-four states that share the three-instruction arm
 * at 0x6590b and 47 for the pair 47/56 at 0x66834; the aliasing groups are in
 * docs/v34handshak.md.
 */
static const short micro_reps[] = {
	41,	/* 0x669a4  DET_SYNC          */
	42,	/* 0x6590b  the shared arm     */
	44,	/* 0x668c0  DET_INFO           */
	46,	/* 0x65d6d  TX_PHASE1_ANS      */
	47,	/* 0x66834  TX_PHASE2_ANS      */
	48,	/* 0x65d30  TX_PHASE3_ANS      */
	49,	/* 0x66a0d  RX_PHASE1_ANS      */
	50,	/* 0x664b8  RX_PHASE2_ANS      */
	51,	/* 0x65c47  TX_L1              */
	55,	/* 0x65b72  TX_PHASE1_CALL     */
	58,	/* 0x66003  RX_PHASE1_CALL     */
	59,	/* 0x662b0  RX_PHASE2_CALL     */
	62,	/* 0x65c7a  RX_PHASE3_CALL     */
	63,	/* 0x6591e  INFODONE           */
	79,	/* 0x657ca  MOH_TONE           */
	80,	/* 0x656e0  MOH_TONE_DROP      */
	33	/* outside 41..80: the default arm at 0x65329 */
};
#define NMICRO	((int)(sizeof(micro_reps) / sizeof(micro_reps[0])))

/* One representative per distinct target of table 2, .rodata+0x2ee8. */
static const short block_reps[] = {
	5,	/* 0x64480  SILENCE                       */
	6,	/* 0x62a40  the default arm               */
	18,	/* 0x64518  SSEG, SBARSEG                 */
	20,	/* 0x64509  PPSEG, TRNSEG4, JTXMIT, J1TXMIT */
	24,	/* 0x644c9  TX_DPSK, TX_L1, ...           */
	66,	/* 0x644fa  TRNSEG4A, XMITMP, EXMIT       */
	70	/* 0x644d8  DATAXMIT                      */
};
#define NBLOCK	((int)(sizeof(block_reps) / sizeof(block_reps[0])))

/*
 * Table 1, one representative per distinct target -- WITHOUT the default,
 * which does not terminate (finding 287).
 */
static const short samp_reps[] = {
	5, 18, 19, 20, 21, 24, 51, 60, 64, 65,
	66, 67, 69, 70, 71, 78, 81, 85, 86
};
#define NSAMP	((int)(sizeof(samp_reps) / sizeof(samp_reps[0])))

static struct rec micro[NMICRO], block[NBLOCK], samp[NSAMP], spare;

/* A representative by its state value, so the assertions below read as the
 * state numbers the jump table is written in. */
static const struct rec *
by_state(const struct rec *tab, int n, short state)
{
	int i;

	for (i = 0; i < n; i++)
		if (tab[i].state == state)
			return &tab[i];
	return &tab[0];
}

#define M(s)	by_state(micro, NMICRO, (s))
#define B(s)	by_state(block, NBLOCK, (s))
#define S(s)	by_state(samp, NSAMP, (s))

static int
groups(const struct rec *tab, int n)
{
	int i, j, g = 0;

	for (i = 0; i < n; i++) {
		int dup = 0;

		for (j = 0; j < i; j++)
			if (same(&tab[i], &tab[j])) {
				dup = 1;
				break;
			}
		if (!dup)
			g++;
	}
	return g;
}

static int
did_something(const struct rec *tab, int n)
{
	int i, k = 0;

	for (i = 0; i < n; i++)
		if (tab[i].changed != 0 || tab[i].lines != 0)
			k++;
	return k;
}

int
main(void)
{
	int i;

	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak per-dispatch-case harness");

	/*
	 * THE ALARM, PROVED TO FIRE.  Finding 287 says table 1's default arm
	 * is the loop bottom and so does not terminate, and finding 249's
	 * standard is that a guard is demonstrated before it is committed.
	 * This cannot run inside the sweep -- it never returns -- so it is a
	 * mode of its own:
	 *
	 *     V34HS_HANG=1 ./build/test/t_v34hsstep    -> exit 3, and says why
	 *
	 * txstate 6 ANSAM is one of the fifty-seven entries that are the
	 * default; the cursor is below the limit, so the loop re-tests a
	 * cursor nothing advances.
	 */
	if (getenv("V34HS_HANG")) {
		printf("driving txstate 6, which has no case: "
		       "the alarm should fire\n");
		v34hs_setup(0);
		v34hs_route(V34HS_ROUTE_TXSAMPLE, 1);
		v34hs_state(V34HS_PHASE1, V34HS_SILENCE, V34HS_ANSAM);
		v34hs_step();
		printf("IT RETURNED -- finding 287 is wrong\n");
		return 1;
	}

	/*
	 * The diagnostics are on for every case below.  They are the cheapest
	 * discriminator the object offers -- `v34handshak` and
	 * `v34handshakinit` index `StateName` 533 times between them -- and
	 * they are the half of the evidence a byte comparison cannot give,
	 * because two cases that write the same fields still announce
	 * themselves differently.  Every state word driven here is in 0..86,
	 * which D42 requires while this is on.
	 */
	v34hs_debug(1);

	/* --- table 3, the microstate machine ------------------------------ */

	if (dump)
		printf("table 3  microstate  .rodata+0x3000  "
		       "(rxstate=43, txstate=18)\n");
	for (i = 0; i < NMICRO; i++)
		run_micro(&micro[i], micro_reps[i], 3000 + micro_reps[i]);

	/* --- table 2, the once-per-block transmit supervisor --------------- */

	if (dump)
		printf("table 2  txstate  .rodata+0x2ee8\n");
	for (i = 0; i < NBLOCK; i++)
		run_txblock(&block[i], block_reps[i], 2000 + block_reps[i]);

	/* --- table 1, the per-sample transmit loop, on request only -------- */

	if (dump)
		printf("table 1  txstate  .rodata+0x2da0  (one sample)\n");
	for (i = 0; i < NSAMP; i++)
		run_txsample(&samp[i], samp_reps[i], 1, 1000 + samp_reps[i]);

	/*
	 * ANTI-VACUITY.  A separation claim over cases that all did nothing
	 * would hold by having nothing to separate, so each sweep must have
	 * moved the object in most of its cases.
	 */
	diff_eq_int("microstate cases that did something",
		    did_something(micro, NMICRO) >= 12, 1,
		    did_something(micro, NMICRO));
	diff_eq_int("table-2 cases that did something",
		    did_something(block, NBLOCK) >= 5, 1,
		    did_something(block, NBLOCK));
	diff_eq_int("table-1 cases that did something",
		    did_something(samp, NSAMP) >= 18, 1,
		    did_something(samp, NSAMP));

	/*
	 * THE SEPARATION CLAIM, as a count and then as named pairs.  The
	 * counts are the summary; the pairs are what fails informatively.
	 */
	/*
	 * Seven behaviours from seventeen targets, and four from seven: the
	 * numbers are MEASURED, and their gap from 17 and 7 is the useful
	 * half.  A microstate target that cannot be told from its neighbour
	 * cold is one whose case reads a companion field the fixture has not
	 * set, and docs/v34handshak.md lists which those are because that is
	 * the per-case agent's starting point.
	 */
	diff_eq_int("distinct microstate behaviours",
		    groups(micro, NMICRO), 7, NMICRO);
	diff_eq_int("distinct table-2 behaviours",
		    groups(block, NBLOCK), 4, NBLOCK);
	/*
	 * TABLE 1 IS THE ONE THAT SEPARATES.  Eighteen behaviours from
	 * nineteen targets -- the per-sample loop runs a modulator, so almost
	 * every arm leaves a different sample behind, where table 2's arms
	 * mostly set a flag and leave.  The single collision is 24 and 60,
	 * which the table gives two entries and which agree cold.
	 */
	diff_eq_int("distinct table-1 behaviours",
		    groups(samp, NSAMP), 18, NSAMP);

	/*
	 * AND THE PAIRS, which is what fails informatively.  Every one of
	 * these was measured before it was asserted; the `agree` pairs are
	 * cases that genuinely share an arm or genuinely do nothing cold, and
	 * they are asserted rather than omitted because "this case needs a
	 * companion field before it does anything" is exactly what the next
	 * agent needs told, and an omitted claim tells it nothing.
	 */
	differ("microstate", M(41), M(47));
	differ("microstate", M(41), M(48));
	differ("microstate", M(41), M(42));
	differ("microstate", M(44), M(47));
	differ("microstate", M(47), M(59));
	differ("microstate", M(59), M(62));
	differ("microstate", M(62), M(79));
	differ("microstate", M(48), M(79));
	differ("microstate", M(42), M(79));

	/* 41 and 44 are two different targets that agree cold; 48..58 are six
	 * timers that all bump the same counter at +0xaa78; 79 and 80 are the
	 * two Modem-on-Hold arms; 42, 46, 63 and 33 all reach the txstate
	 * dispatch with nothing of their own. */
	agree("microstate", M(41), M(44));
	agree("microstate", M(48), M(49));
	agree("microstate", M(49), M(50));
	agree("microstate", M(50), M(51));
	agree("microstate", M(51), M(55));
	agree("microstate", M(55), M(58));
	agree("microstate", M(79), M(80));
	agree("microstate", M(42), M(63));
	agree("microstate", M(42), M(33));

	/*
	 * TABLE 1'S NAMED PAIRS.  Each of these is a pair of the twenty
	 * targets at .rodata+0x2da0 that the step tells apart by what it wrote
	 * into the object -- which is the standard finding 290 held the
	 * microstate targets to, and the thing #56 needs before a per-case
	 * reconstruction of any of them can be believed.  `S(24)` and `S(60)`
	 * are the one pair that agrees cold, and they are asserted equal so
	 * that a future change separating them is a failure and not a silence.
	 */
	differ("table 1", S(5), S(18));
	differ("table 1", S(18), S(19));
	differ("table 1", S(20), S(21));
	differ("table 1", S(21), S(24));
	differ("table 1", S(51), S(65));
	differ("table 1", S(66), S(67));
	differ("table 1", S(69), S(70));
	differ("table 1", S(71), S(78));
	differ("table 1", S(81), S(85));
	differ("table 1", S(85), S(86));
	agree("table 1", S(24), S(60));

	differ("table 2", B(5), B(18));
	differ("table 2", B(18), B(66));
	differ("table 2", B(5), B(6));
	differ("table 2", B(24), B(66));
	agree("table 2", B(5), B(24));
	agree("table 2", B(18), B(20));
	agree("table 2", B(66), B(70));

	/*
	 * And every pointer the comparison skips was reached, so the
	 * thirty-five offsets sixteen agents are going to trust cannot go
	 * stale unnoticed.
	 */
	v34hs_holes_check();

	/*
	 * SECOND PASS, WITH THE BLOB'S BRING-UP ON BOTH SIDES.
	 *
	 * Twelve of the thirty-five skipped pointers land in the program image
	 * -- a library table or a function -- and on the ordinary run side A
	 * holds ours where side B holds the blob's, so which one each SELECTS
	 * is not a question an address comparison can answer.  Brought up by
	 * the same code, they must select the identical address, and this is
	 * the pass in which `check_self_ptr` says so.  It is here rather than
	 * behind `V34HS_REFINIT=1` because a check `make phase` never runs is
	 * finding 249's shape.
	 *
	 * The separation assertions are not repeated: they are about the
	 * object, not the bring-up, and `v34hs_holes_check` does not apply
	 * here at all -- eleven of the skips legitimately never differ once
	 * both sides install the same table.
	 */
	v34hs_refinit(1);
	for (i = 0; i < NMICRO; i++)
		run_micro(&spare, micro_reps[i], 4000 + micro_reps[i]);
	for (i = 0; i < NBLOCK; i++)
		run_txblock(&spare, block_reps[i], 5000 + block_reps[i]);
	for (i = 0; i < NSAMP; i++)
		run_txsample(&spare, samp_reps[i], 1, 6000 + samp_reps[i]);
	v34hs_refinit(0);

	if (dump)
		printf("groups: microstate %d/%d, table 2 %d/%d, "
		       "table 1 %d/%d\n",
		       groups(micro, NMICRO), NMICRO,
		       groups(block, NBLOCK), NBLOCK,
		       groups(samp, NSAMP), NSAMP);

	return diff_end();
}
