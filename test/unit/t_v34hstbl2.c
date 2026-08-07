/*
 * t_v34hstbl2.c -- `v34handshak`'s once-per-block transmit dispatch, table 2,
 * against the blob.
 *
 * This is the first tier-1 differential test of any part of `v34handshak`.
 * Side A runs `v34handshak_txblock` from src/pump/v34/v34hshak.c and side
 * B runs `ref_v34handshak`, the whole 61,541-byte blob function, on an
 * object the fixture has steered into this dispatch and no other.  Over that
 * domain the two are the same function: the guards read three halfwords and
 * branch, and nothing else in the blob's function runs.  See
 * docs/v34handshak.md for the fixture and include/dsplib/v34hshak.h for
 * what the reconstruction claims.
 *
 * SIDE A USED TO BE A SECOND RECONSTRUCTION and is now the same dispatch
 * every microstate arm calls.  `src/pump/v34/v34hstxblock.c` held a third
 * reading of table 2 with all seven arms and a tail missing the reload at
 * 0x62b5f; finding 591 collapsed it onto `t3m_txblock`/`t3m_tail`, which is
 * the reading whose signature can hold both readings of the tail's `%cx`.
 * Every case below is unchanged by that, which is the point: this file's
 * claims are about the object and not about which file answers them.
 *
 * WHAT IS BEING SEPARATED, AND WHAT IS NOT.
 *
 * Finding 290 measured four distinct behaviours from table 2's seven targets
 * and asserted the three collisions as `agree` pairs.  That is reproduced
 * here -- COLD, on this fixture's fill, all seven driven with no companion
 * field touched -- and then taken apart: each of the three collisions is a
 * companion field the fill happens to leave in the value that makes two arms
 * answer alike, and each is broken by one poke.  So the answer to "how many
 * of the seven are distinct" is BOTH four and seven, and this file asserts
 * both halves, because a collision that stops being a collision when a field
 * moves is evidence about the fill and not about the object.  Finding 364.
 *
 * WHAT THE TRANSCRIPT AXIS CONTRIBUTES HERE: nothing.  The whole closure
 * holds no `call` and no diagnostic site, so every case prints zero lines on
 * both sides and `v34hs_compare`'s transcript check passes by construction.
 * It is named as a gap rather than counted as evidence (finding 362); the
 * evidence here is bytes written, and it is every byte of a 44,096-byte
 * object plus the five blocks it points at plus the padding around them.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

static void
call_ours(void *obj)
{
	v34handshak_txblock((struct v34_object *)obj);
}

/* The fields these cases steer, by offset, so the test reads as the object. */
#define O_F0004		0x0004
#define O_FLOOR		0x0230	/* rx_energy_floor, int                    */
#define O_F0234		0x0234	/* the tail's counter, int                 */
#define O_F0238		0x0238	/* int, compared UNSIGNED against +0x23c   */
#define O_F023C		0x023c
#define O_RXFLAGS	0x0386	/* receiver +0x122, bit 3                   */
#define O_AGCLEVEL	0x0398	/* receiver +0x134, SIGNED short            */
#define O_F1D2		0x0436	/* receiver +0x1d2, written on one path     */
#define O_F0E4C		0x0e4c	/* unsigned short, txstate 70's companion   */
#define O_F2218		0x2218	/* int, selects four of the tail's arms     */
#define O_VECTIDX	0x2aa2	/* short, signed against 0x3c               */
#define O_F359C		0x359c	/* short, three arms' companion             */
#define O_FAA96		0xaa96	/* short, tripled into the receiver         */

/* --- the three routes into this dispatch ---------------------------------- */

/*
 * All three reach 0x62af1 with the same two registers holding the same two
 * values, so the reconstruction cannot tell them apart -- it starts at the
 * txstate read.  What the three-way comparison below is worth is therefore
 * not a statement about our code but about the BLOB's: a case that gives the
 * same answer through all three says the blob's three approaches really do
 * converge, and it says it by the same byte-for-byte comparison as everything
 * else.  Finding 361.
 */
enum { R_BLOCK, R_CHAIN_LOW, R_CHAIN_HIGH };

static const char *const route_name[] = {
	"[0x264] <= 5", "rxstate below 43", "rxstate above 43"
};

/* --- a case --------------------------------------------------------------- */

struct poke {
	unsigned	off;
	int		sz;	/* 2 or 4; 0 ends the list */
	int		v;
};

#define EXP_SAME	INT_MIN		/* the step must leave +0x0004 alone */
#define EXP_ANY		(INT_MIN + 1)	/* no oracle, and no pinning either  */

struct rec {
	unsigned	changed, first, hash, lines;
	short		mst, rxst, txst;
	int		progress;
	int		used;
};

static int dump;

static int
f0004_of(int side)
{
	int v;

	memcpy(&v, (char *)v34hs_object(side) + O_F0004, sizeof(v));
	return v;
}

static int
peek_int(int side, unsigned off)
{
	int v;

	memcpy(&v, (char *)v34hs_object(side) + off, sizeof(v));
	return v;
}

/*
 * THE TAIL, HELD STILL.
 *
 * Every arm falls into the tail at 0x62a40, and the tail can overwrite what
 * the arm just decided -- four times, on fields no arm reads.  Left at the
 * fill, whether it does is a property of the FILL: at `V34HS_SEED=10` and 11
 * the object comes up with the tail's counter already past 0x257f, the tail
 * writes 9 over every arm's answer, and all seven targets become one
 * behaviour.  Measured, not feared.
 *
 * So every case below pins the tail's five inputs first, and a case that is
 * ABOUT the tail overrides them afterwards.  That makes each arm's answer a
 * property of the object rather than of the seed, and finding 290's
 * four-from-seven becomes a claim that holds at every fill instead of at this
 * one.  +0x0004 itself is pinned to -1 for the same reason: an arm writing
 * the value that happened to be there already would write no bytes at all,
 * and the signature would silently lose a case.
 */
static const struct poke pin[] = {
	{ O_F0004, 4, -1 },	/* no arm's answer collides with the fill  */
	{ O_F2218, 4, 0 },	/* not 1, not 2 or 3, not 4 or 5           */
	{ O_F0238, 4, 0 },	/* 0 > 0 is false, so no 8                 */
	{ O_F023C, 4, 0 },
	{ O_AGCLEVEL, 2, 0 },	/* at the floor: the counter is cleared    */
	{ O_FLOOR, 4, 0 },
	{ O_F0234, 4, 0x1234 },	/* and clearing it always writes four bytes */
	{ 0, 0, 0 }
};

/*
 * Drive one case: bring both objects up, select the route, write the three
 * state words, pin the tail, apply the case's own pokes to BOTH sides, step,
 * and compare everything.
 *
 * `expect` is what the BLOB must leave in +0x0004 -- an oracle read off the
 * disassembly, not off our code.  It is what makes each case a claim about
 * which arm ran rather than only a claim that the two sides agree: two arms
 * that both do nothing would agree perfectly.  `EXP_ANY` drops the oracle for
 * the one group that deliberately runs unpinned.
 */
static void
run(struct rec *r, const char *name, int route, short mst, short rxst,
    short txst, const struct poke *pk, int expect, long tag)
{
	const struct v34hs_obs *o;
	const struct poke *p;
	int pre, post;
	char msg[160];

	v34hs_setup(0);
	v34hs_route(route == R_BLOCK ? V34HS_ROUTE_TXBLOCK
				     : V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, rxst, txst);
	if (expect != EXP_ANY)
		for (p = pin; p->sz != 0; p++) {
			if (p->sz == 2)
				v34hs_poke_short(p->off, (short)p->v);
			else
				v34hs_poke_int(p->off, p->v);
		}
	for (p = pk; p != NULL && p->sz != 0; p++) {
		if (p->sz == 2)
			v34hs_poke_short(p->off, (short)p->v);
		else
			v34hs_poke_int(p->off, p->v);
	}

	pre = f0004_of(1);
	v34hs_step();
	post = f0004_of(1);

	v34hs_compare(name, tag);

	if (expect != EXP_ANY) {
		snprintf(msg, sizeof(msg),
			 "%s: the blob leaves +0x0004 at %d", name,
			 expect == EXP_SAME ? pre : expect);
		diff_eq_int(msg, post, expect == EXP_SAME ? pre : expect, tag);
	}

	o = v34hs_observed(0);
	if (r != NULL) {
		r->changed = o->changed;
		r->first = o->first;
		r->hash = o->hash;
		r->lines = o->lines;
		r->mst = o->mst == mst ? -1 : o->mst;
		r->rxst = o->rxst == rxst ? -1 : o->rxst;
		r->txst = o->txst == txst ? -1 : o->txst;
		r->progress = o->progress;
		r->used = 1;
	}
	if (dump)
		printf("  %-34s tx %3d %-17s wrote %2u B  sig %08x  "
		       "lines %u  +0x0004 %d\n", name, txst, route_name[route],
		       o->changed, o->hash, o->lines, o->progress);
}

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

	snprintf(msg, sizeof(msg), "%s take different paths", what);
	diff_eq_int(msg, same(x, y), 0, 0);
}

static void
agree(const char *what, const struct rec *x, const struct rec *y)
{
	char msg[160];

	snprintf(msg, sizeof(msg), "%s share one behaviour", what);
	diff_eq_int(msg, same(x, y), 1, 0);
}

/* --- the case tables ------------------------------------------------------ */

#define NOPOKE	((const struct poke *)NULL)
#define PK_END	{ 0, 0, 0 }

/*
 * The seven targets and all sixteen of their aliases, with the tail pinned
 * and no arm's own companion field touched.  This is finding 290's
 * measurement -- four behaviours from seven targets -- reproduced against a
 * reconstruction rather than against the blob twice, and made a property of
 * the object rather than of the seed by the pin.
 */
static const short cold_tx[] = { 5, 18, 19, 20, 21, 24, 51, 54, 60, 64,
				 66, 67, 68, 69, 70, 74, 6 };
#define NCOLD	((int)(sizeof(cold_tx) / sizeof(cold_tx[0])))
/*
 * What each must leave in +0x0004, in the same order.  Written out of the
 * disassembly by hand and WRONG at 68 on the first run -- 68 shares 0x64509
 * with 20, 21 and 64 and answers 2, and this table said 3.  The differential
 * comparison passed on that case; only the oracle failed, which is the shape
 * an independent oracle is supposed to have.
 */
static const int cold_exp[NCOLD] = { 0, 2, 2, 2, 2, 0, 0, 0, 0, 2,
				     3, 3, 2, 3, 3, 0, EXP_SAME };
static struct rec cold[NCOLD];

static const struct rec *
by_tx(short tx)
{
	int i;

	for (i = 0; i < NCOLD; i++)
		if (cold_tx[i] == tx)
			return &cold[i];
	return &cold[0];
}

int
main(void)
{
	static struct rec warm5, warm24, warm18, warm20, warm66, warm70;
	static struct rec chain[3][NCOLD];
	struct poke pk[8];
	int i, k, groups, moved;
	char msg[160];

	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak table 2, the once-per-block transmit dispatch");

	/*
	 * The diagnostics are on because every other test against this
	 * fixture has them on and because a case that unexpectedly traced
	 * would then be a failure rather than a silence.  Nothing in this
	 * dispatch traces; that is the point of the assertion at the end.
	 */
	v34hs_debug(1);
	v34hs_side_a(call_ours);

	/* --- the seven targets, no companion field touched ---------------- */

	if (dump)
		printf("table 2 cold, .rodata+0x2ee8\n");
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg), "cold txstate %d", cold_tx[i]);
		run(&cold[i], msg, R_BLOCK, V34HS_PHASE1, V34HS_SILENCE,
		    cold_tx[i], NOPOKE, cold_exp[i], 2000 + cold_tx[i]);
	}

	/*
	 * AND THE SAME SEVEN WITH NOTHING PINNED EITHER, which is the object
	 * exactly as `v34handshakinit` and the fill leave it.  No oracle: what
	 * the tail then does is a property of the seed, and at
	 * `V34HS_SEED=10` it writes 9 over all seven.  The differential
	 * comparison still applies and is the point -- the reconstruction has
	 * to be right on the fill it was not tuned for.
	 */
	if (dump)
		printf("table 2 unpinned\n");
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg), "unpinned txstate %d", cold_tx[i]);
		run(NULL, msg, R_BLOCK, V34HS_PHASE1, V34HS_SILENCE,
		    cold_tx[i], NOPOKE, EXP_ANY, 2400 + cold_tx[i]);
	}

	/*
	 * ANTI-VACUITY, before any separation claim.  Every case must have
	 * moved the object, or "these two differ" is a comparison of two
	 * silences.
	 */
	moved = 0;
	for (i = 0; i < NCOLD; i++)
		if (cold[i].changed != 0)
			moved++;
	diff_eq_int("cold cases that wrote something", moved, NCOLD, NCOLD);

	/*
	 * FOUR BEHAVIOURS FROM SEVEN TARGETS, COLD -- finding 290's number,
	 * reproduced.  The seventeen states above are the seven targets and
	 * their aliases; the count is over the seven representatives.
	 */
	{
		static const short reps[] = { 5, 18, 20, 24, 66, 70, 6 };
		const struct rec *r[7];

		for (i = 0; i < 7; i++)
			r[i] = by_tx(reps[i]);
		groups = 0;
		for (i = 0; i < 7; i++) {
			int dup = 0;

			for (k = 0; k < i; k++)
				if (same(r[i], r[k]))
					dup = 1;
			if (!dup)
				groups++;
		}
		diff_eq_int("distinct table-2 behaviours, cold", groups, 4, 7);
	}

	/*
	 * AND THE ALIASES WITHIN EACH TARGET, which are the same check under
	 * a different index and are asserted as such: five states share
	 * 0x644c9, four share 0x64509, three share 0x644fa and two share
	 * 0x64518, and cold they must be indistinguishable.
	 */
	agree("24 and 51", by_tx(24), by_tx(51));
	agree("24 and 54", by_tx(24), by_tx(54));
	agree("24 and 60", by_tx(24), by_tx(60));
	agree("24 and 74", by_tx(24), by_tx(74));
	agree("20 and 21", by_tx(20), by_tx(21));
	agree("20 and 64", by_tx(20), by_tx(64));
	agree("20 and 68", by_tx(20), by_tx(68));
	agree("66 and 67", by_tx(66), by_tx(67));
	agree("66 and 69", by_tx(66), by_tx(69));
	agree("18 and 19", by_tx(18), by_tx(19));

	/* The three cold collisions ACROSS targets, and the four groups. */
	agree("5 and 24 cold", by_tx(5), by_tx(24));
	agree("18 and 20 cold", by_tx(18), by_tx(20));
	agree("66 and 70 cold", by_tx(66), by_tx(70));
	differ("5 and 18 cold", by_tx(5), by_tx(18));
	differ("18 and 66 cold", by_tx(18), by_tx(66));
	differ("5 and 66 cold", by_tx(5), by_tx(66));
	differ("5 and the default cold", by_tx(5), by_tx(6));
	differ("18 and the default cold", by_tx(18), by_tx(6));
	differ("66 and the default cold", by_tx(66), by_tx(6));

	/* --- txstate 5's three branches out ------------------------------ */

	if (dump)
		printf("txstate 5, 0x64480 and its three branches\n");

	pk[0].off = O_F359C; pk[0].sz = 2; pk[0].v = 0x66;
	pk[1] = (struct poke)PK_END;
	run(&warm5, "5 with microstate 63 and +0x359c 0x66", R_BLOCK,
	    V34HS_INFODONE, V34HS_SILENCE, 5, pk, 1, 2101);
	run(NULL, "5 with microstate 33 and +0x359c 0x66", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 5, pk, 0, 2102);

	pk[0].v = 0x65;
	run(NULL, "5 with microstate 63 and +0x359c 0x65", R_BLOCK,
	    V34HS_INFODONE, V34HS_SILENCE, 5, pk, 0, 2103);
	pk[0].v = 0x64;
	run(NULL, "5 with microstate 63 and +0x359c 0x64", R_BLOCK,
	    V34HS_INFODONE, V34HS_SILENCE, 5, pk, 0, 2104);

	/* rxstate 4 -> 0x655c9, which wants microstate 44 and +0x359c 0x65 */
	pk[0].v = 0x65;
	run(NULL, "5 with rxstate 4, microstate 44, +0x359c 0x65", R_BLOCK,
	    V34HS_DET_INFO, V34HS_RECEIVE, 5, pk, 1, 2105);
	pk[0].v = 0x66;
	run(NULL, "5 with rxstate 4, microstate 44, +0x359c 0x66", R_BLOCK,
	    V34HS_DET_INFO, V34HS_RECEIVE, 5, pk, 0, 2106);
	pk[0].v = 0x65;
	run(NULL, "5 with rxstate 4, microstate 33, +0x359c 0x65", R_BLOCK,
	    V34HS_PHASE1, V34HS_RECEIVE, 5, pk, 0, 2107);
	run(NULL, "5 with rxstate 4, microstate 63, +0x359c 0x65", R_BLOCK,
	    V34HS_INFODONE, V34HS_RECEIVE, 5, pk, 0, 2108);

	/*
	 * rxstate 35 -> 0x67d48, and reaching its body is the round trip:
	 * microstate 63 sends the arm to 0x6778b FIRST, +0x359c 0x65 fails
	 * the test there and returns to 0x64498, and only then does the
	 * rxstate chain reach 0x67d48, whose own test is +0x359c == 0x65.
	 * A reconstruction that took the two constants the other way round
	 * passes every other case in this file.
	 */
	run(NULL, "5 with rxstate 35, microstate 63, +0x359c 0x65", R_BLOCK,
	    V34HS_INFODONE, V34HS_WAIT, 5, pk, 1, 2109);
	pk[0].v = 0x66;
	run(NULL, "5 with rxstate 35, microstate 63, +0x359c 0x66", R_BLOCK,
	    V34HS_INFODONE, V34HS_WAIT, 5, pk, 1, 2110);
	pk[0].v = 0x65;
	run(NULL, "5 with rxstate 35, microstate 33, +0x359c 0x65", R_BLOCK,
	    V34HS_PHASE1, V34HS_WAIT, 5, pk, 0, 2111);
	run(NULL, "5 with rxstate 35, microstate 44, +0x359c 0x65", R_BLOCK,
	    V34HS_DET_INFO, V34HS_WAIT, 5, pk, 0, 2112);

	/* 24 shares none of that: same pokes, and it still answers 0. */
	pk[0].v = 0x66;
	run(&warm24, "24 with microstate 63 and +0x359c 0x66", R_BLOCK,
	    V34HS_INFODONE, V34HS_SILENCE, 24, pk, 0, 2113);

	/* --- txstate 18 and 19, 0x64518 ---------------------------------- */

	if (dump)
		printf("txstates 18 and 19, 0x64518 and 0x6780f\n");

	pk[0].off = O_F359C; pk[0].sz = 2; pk[0].v = 0x64;
	pk[1].off = O_RXFLAGS; pk[1].sz = 2; pk[1].v = 0x0008;
	pk[2] = (struct poke)PK_END;
	run(&warm18, "18 with +0x386 bit 3 set", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 18, pk, 3, 2120);
	run(NULL, "19 with +0x386 bit 3 set", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 19, pk, 3, 2121);
	run(&warm20, "20 with +0x386 bit 3 set", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 20, pk, 2, 2122);

	pk[1].v = (int)0xfff7;	/* every bit but 3 */
	run(NULL, "18 with +0x386 bit 3 alone clear", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 18, pk, 2, 2123);
	pk[1].v = (int)0xffff;
	run(NULL, "18 with +0x386 all ones", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 18, pk, 3, 2124);

	/* and 0x6780f, which wins over the flags word and gives the same 2 */
	pk[0].v = 0x65; pk[1].v = 0x0008;
	run(NULL, "18 with +0x359c 0x65 and bit 3 set", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 18, pk, 2, 2125);
	pk[0].v = 0x66;
	run(NULL, "18 with +0x359c 0x66 and bit 3 set", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 18, pk, 3, 2126);

	/* --- txstate 70, 0x644d8 ----------------------------------------- */

	if (dump)
		printf("txstate 70, 0x644d8\n");

	pk[0].off = O_F0E4C; pk[0].sz = 2; pk[0].v = 0;
	pk[1] = (struct poke)PK_END;
	run(NULL, "70 with +0xe4c zero", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE,
	    70, pk, 3, 2130);
	pk[0].v = 1;
	run(&warm70, "70 with +0xe4c one", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 70, pk, 4, 2131);
	pk[0].v = (int)0x8000;
	run(NULL, "70 with +0xe4c 0x8000", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 70, pk, 4, 2132);
	pk[0].v = (int)0xffff;
	run(NULL, "70 with +0xe4c 0xffff", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 70, pk, 4, 2133);
	pk[0].v = 1;
	run(&warm66, "66 with +0xe4c one", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 66, pk, 3, 2134);

	/* --- ALL SEVEN ARE DISTINCT once the companions are set ---------- */

	differ("5 and 24 with microstate 63 and +0x359c 0x66",
	       &warm5, &warm24);
	differ("18 and 20 with +0x386 bit 3 set", &warm18, &warm20);
	differ("66 and 70 with +0xe4c non-zero", &warm66, &warm70);

	/* --- the shared tail at 0x62a40 ---------------------------------- */

	if (dump)
		printf("the shared tail, 0x62a40\n");

	/* +0x2218 == 1: the branch to 0x62b45 and back. */
	pk[0].off = O_F2218; pk[0].sz = 4; pk[0].v = 1;
	pk[1].off = O_FAA96; pk[1].sz = 2; pk[1].v = 0x1234;
	pk[2] = (struct poke)PK_END;
	run(NULL, "the default arm with +0x2218 one", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, 4, 2140);
	diff_eq_int("+0x2218 one tripled +0xaa96 into the receiver",
		    (int)v34hs_peek_short(1, O_F1D2), (short)(3 * 0x1234), 2140);
	pk[1].v = -2;
	run(NULL, "the default arm with +0x2218 one and +0xaa96 -2", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, 4, 2141);
	diff_eq_int("a negative +0xaa96 is tripled signed",
		    (int)v34hs_peek_short(1, O_F1D2), -6, 2141);
	pk[1].v = 0x2aab;	/* 3x overflows a short: 0x8001 */
	run(NULL, "the default arm with +0x2218 one and +0xaa96 0x2aab",
	    R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 6, pk, 4, 2142);
	diff_eq_int("the tripling is truncated to a short",
		    (int)v34hs_peek_short(1, O_F1D2), (short)0x8001, 2142);

	/* +0x2218 in {4,5}: 6.  In {2,3}: only txstate 74 does anything. */
	pk[1] = (struct poke)PK_END;
	pk[0].v = 4;
	run(NULL, "the default arm with +0x2218 four", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, 6, 2143);
	pk[0].v = 5;
	run(NULL, "the default arm with +0x2218 five", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, 6, 2144);
	pk[0].v = 3;
	run(NULL, "the default arm with +0x2218 three", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, EXP_SAME, 2145);
	pk[0].v = 6;
	run(NULL, "the default arm with +0x2218 six", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, EXP_SAME, 2146);
	pk[0].v = 0;
	run(NULL, "the default arm with +0x2218 zero", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 6, pk, EXP_SAME, 2147);
	pk[0].v = 4;
	run(NULL, "txstate 74 with +0x2218 four", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 74, pk, 6, 2148);

	/*
	 * 0x64884 -- and this is why txstate 74 is not just another alias of
	 * 0x644c9.  It shares that arm, and then the tail singles it out.
	 */
	pk[0].v = 2;
	pk[1].off = O_VECTIDX; pk[1].sz = 2; pk[1].v = 100;
	pk[2] = (struct poke)PK_END;
	run(NULL, "74 with +0x2218 two and +0x2aa2 above 60", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 74, pk, 0, 2150);
	pk[1].v = 61;
	run(NULL, "74 with +0x2218 two and +0x2aa2 61", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 74, pk, 0, 2151);
	pk[1].v = 60;
	run(NULL, "74 with +0x2218 two and +0x2aa2 60", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 74, pk, 7, 2152);
	pk[1].v = -1;	/* signed: below 60 */
	run(NULL, "74 with +0x2218 two and +0x2aa2 -1", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 74, pk, 7, 2153);
	pk[0].v = 3; pk[1].v = 60;
	run(NULL, "74 with +0x2218 three and +0x2aa2 60", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 74, pk, 7, 2154);
	pk[0].v = 2; pk[1].v = 60;
	run(NULL, "24 with +0x2218 two and +0x2aa2 60", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 24, pk, 0, 2155);
	run(NULL, "the default arm with +0x2218 two and +0x2aa2 60", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2156);

	/* +0x238 against +0x23c, UNSIGNED. */
	pk[0].off = O_F0238; pk[0].sz = 4; pk[0].v = 100;
	pk[1].off = O_F023C; pk[1].sz = 4; pk[1].v = 50;
	pk[2] = (struct poke)PK_END;
	run(NULL, "the default arm with +0x238 above +0x23c", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, 8, 2160);
	pk[0].v = 50; pk[1].v = 100;
	run(NULL, "the default arm with +0x238 below +0x23c", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2161);
	pk[0].v = 50; pk[1].v = 50;
	run(NULL, "the default arm with +0x238 equal to +0x23c", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2162);
	/*
	 * THE SIGNEDNESS DISCRIMINATOR.  -1 against 1 is above unsigned and
	 * below signed, and 1 against -1 is the other way round, so these two
	 * cases fail for a `>` that got the type wrong -- in opposite
	 * directions, which is what makes them two checks and not one.
	 */
	pk[0].v = -1; pk[1].v = 1;
	run(NULL, "the default arm with +0x238 -1 and +0x23c 1", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, 8, 2163);
	pk[0].v = 1; pk[1].v = -1;
	run(NULL, "the default arm with +0x238 1 and +0x23c -1", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2164);

	/*
	 * The counter at +0x234 against the receiver's level and the floor.
	 * At or above the floor it is cleared; below it, it advances, and
	 * past 0x257f the tail says so.
	 */
	pk[0].off = O_AGCLEVEL; pk[0].sz = 2; pk[0].v = 10;
	pk[1].off = O_FLOOR; pk[1].sz = 4; pk[1].v = 5;
	pk[2].off = O_F0234; pk[2].sz = 4; pk[2].v = 0x2000;
	pk[3] = (struct poke)PK_END;
	run(NULL, "level above the floor clears the counter", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2170);
	diff_eq_int("the counter was cleared", peek_int(1, O_F0234), 0,
		    2170);
	pk[0].v = 5;
	run(NULL, "level equal to the floor clears the counter", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2171);
	diff_eq_int("equality clears it too", peek_int(1, O_F0234), 0, 2171);
	pk[0].v = 4;
	run(NULL, "level below the floor advances the counter", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2172);
	diff_eq_int("the counter advanced", peek_int(1, O_F0234), 0x2001,
		    2172);
	/*
	 * AND IT IS SIGNED, on a sign-extended halfword: -1 against a floor
	 * of 1 advances, where an unsigned or a 16-bit comparison would read
	 * 0xffff and clear.
	 */
	pk[0].v = -1; pk[1].v = 1; pk[2].v = 0x257f;
	run(NULL, "a negative level below a positive floor advances", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, 9, 2173);
	diff_eq_int("and it passed 0x257f", peek_int(1, O_F0234),
		    0x2580, 2173);
	pk[2].v = 0x257e;
	run(NULL, "one short of 0x257f is not past it", R_BLOCK,
	    V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2174);
	pk[0].v = -1; pk[1].v = -1; pk[2].v = 0x257f;
	run(NULL, "a negative level equal to a negative floor clears",
	    R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 6, pk, EXP_SAME, 2175);

	/* --- the three states above the table, 82, 83 and 84 -------------- */

	if (dump)
		printf("txstates 82, 83 and 84, above the table's range\n");

	run(NULL, "txstate 82", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 82,
	    NOPOKE, 0xd, 2180);
	run(NULL, "txstate 83", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 83,
	    NOPOKE, 0xf, 2181);
	run(NULL, "txstate 84", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 84,
	    NOPOKE, 0x10, 2182);
	run(NULL, "txstate 81", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 81,
	    NOPOKE, EXP_SAME, 2183);
	run(NULL, "txstate 85", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 85,
	    NOPOKE, EXP_SAME, 2184);
	run(NULL, "txstate 86", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 86,
	    NOPOKE, EXP_SAME, 2185);
	run(NULL, "txstate 75, just above the table", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 75, NOPOKE, EXP_SAME, 2186);
	run(NULL, "txstate 4, just below the table", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 4, NOPOKE, EXP_SAME, 2187);
	run(NULL, "txstate 0", R_BLOCK, V34HS_PHASE1, V34HS_SILENCE, 0,
	    NOPOKE, EXP_SAME, 2188);

	/* And the tail's own arms win over the 82/83/84 chain, or lose. */
	pk[0].off = O_F2218; pk[0].sz = 4; pk[0].v = 4;
	pk[1] = (struct poke)PK_END;
	run(NULL, "txstate 82 with +0x2218 four", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 82, pk, 0xd, 2189);
	run(NULL, "txstate 85 with +0x2218 four", R_BLOCK, V34HS_PHASE1,
	    V34HS_SILENCE, 85, pk, 6, 2190);

	/* --- the three routes in, compared ------------------------------- */

	/*
	 * The same seventeen cold cases through the two rxstate routes.  Our
	 * side cannot tell them apart -- it starts at the txstate read -- so
	 * what this measures is the blob: three approaches to 0x62af1 that
	 * must leave the object in the same state.  rxstate 5 is below 43 and
	 * neither 4 nor 35; rxstate 44 is above 43 and neither 53 nor 72, and
	 * that third route runs through the chain at 0x62b71 which
	 * docs/v34handshak.md does not mention.
	 */
	if (dump)
		printf("the same cases through the two rxstate routes\n");
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg), "rxstate-5 route, txstate %d",
			 cold_tx[i]);
		run(&chain[R_CHAIN_LOW][i], msg, R_CHAIN_LOW, V34HS_PHASE1,
		    V34HS_SILENCE, cold_tx[i], NOPOKE, cold_exp[i],
		    2200 + cold_tx[i]);
		snprintf(msg, sizeof(msg), "rxstate-44 route, txstate %d",
			 cold_tx[i]);
		run(&chain[R_CHAIN_HIGH][i], msg, R_CHAIN_HIGH, V34HS_PHASE1,
		    V34HS_DET_INFO, cold_tx[i], NOPOKE, cold_exp[i],
		    2300 + cold_tx[i]);
	}
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg),
			 "txstate %d through [0x264] <= 5 and rxstate 5",
			 cold_tx[i]);
		agree(msg, &cold[i], &chain[R_CHAIN_LOW][i]);
		snprintf(msg, sizeof(msg),
			 "txstate %d through [0x264] <= 5 and rxstate 44",
			 cold_tx[i]);
		agree(msg, &cold[i], &chain[R_CHAIN_HIGH][i]);
	}

	/*
	 * NOTHING IN THIS DISPATCH TRACES, said as a check rather than as a
	 * comment.  The diagnostics are on; if an arm ever grows a debug site
	 * this fails and the transcript axis starts meaning something.
	 */
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg), "cold txstate %d printed nothing",
			 cold_tx[i]);
		diff_eq_int(msg, (int)cold[i].lines, 0, cold_tx[i]);
	}

	/*
	 * EVERY RECORD THE SEPARATION CLAIMS READ WAS ACTUALLY WRITTEN.
	 *
	 * `by_tx` returns `&cold[0]` for a state it cannot find, so a typo in a
	 * `differ` or `agree` argument compares txstate 5 against itself and
	 * reads as a passing check.  `used` is what stops that, and it is
	 * asserted rather than only written: an array filled and never read,
	 * under a comment promising it cannot go stale, is exactly what
	 * finding 290 records this fixture doing to `saw_hole`.
	 */
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg), "record for txstate %d was filled",
			 cold_tx[i]);
		diff_eq_int(msg, cold[i].used, 1, cold_tx[i]);
	}
	diff_eq_int("the warm records were filled",
		    warm5.used + warm24.used + warm18.used + warm20.used
		    + warm66.used + warm70.used, 6, 0);
	for (i = 0; i < NCOLD; i++) {
		snprintf(msg, sizeof(msg),
			 "both route records for txstate %d were filled",
			 cold_tx[i]);
		diff_eq_int(msg, chain[R_CHAIN_LOW][i].used
			    + chain[R_CHAIN_HIGH][i].used, 2, cold_tx[i]);
	}

	/*
	 * And every pointer the comparison skips was reached, so the
	 * thirty-five offsets this fixture excludes cannot go stale unnoticed.
	 */
	v34hs_holes_check();
	v34hs_side_a(NULL);

	return diff_end();
}
