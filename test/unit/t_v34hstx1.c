/*
 * t_v34hstx1.c -- six arms of `v34handshak`'s per-sample transmit dispatch,
 * each compared against the blob on its own.
 *
 * ---------------------------------------------------------------------------
 * HOW ONE ARM IS COMPARED WHEN THE FUNCTION AROUND IT IS NOT WRITTEN.
 *
 * `v34handshak` has no reconstruction and will not have one until all four
 * dispatches are done, so `V34HS_OURS` -- the fixture's one-line swap that
 * puts our function on side A -- cannot be defined yet: it is one switch over
 * one shared harness object and it would demand all forty-three cases at
 * once.  What makes a single arm testable anyway is the loop's own guard,
 * read off the prologue at 0x62933:
 *
 *     cmp %dx,0x221c(%ebx)     ; queue count against the block's limit
 *     jge 629ed                ; at or above: skip the loop entirely
 *
 * So `ref_v34handshak` with the count already at the limit runs the
 * ONCE-PER-BLOCK HALF AND NOTHING ELSE.  Every arm here ends by rejoining the
 * loop test with the count advanced, so `v34hs_step_case` runs our arm first
 * on side A and the blob then contributes exactly the tail:
 *
 *     side A   our arm         + the blob's tail
 *     side B   the blob's arm  + the blob's tail
 *
 * on two congruent arenas (finding 319).  A difference is our arm's, because
 * the tail is the same code on both sides reading the same object -- if our
 * arm left the object right.
 *
 * ---------------------------------------------------------------------------
 * THE VACUITY THIS INVITES, AND THE TWO GUARDS AGAINST IT.
 *
 * If an arm did nothing, side A's count would still be below the limit, side
 * A's step would run THE BLOB'S ARM, and the comparison would pass having
 * tested nothing at all.  So every case is run TWICE:
 *
 *   - once with the arm alone, which is where the count reaching the limit
 *     and the arm having written anything can still be seen.  After the
 *     blob's tail has run on top, neither can;
 *   - once through `v34hs_step_case`, which is the differential comparison.
 *
 * The fixture is deterministic, so the two runs start from the same object.
 * `test/mutations/v34hstx1.json` is the empirical form of the same question.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS HELD FIXED, and it has to be stated because the separation results
 * in finding 323 are properties of the fill as much as of the object:
 *
 *   - one object fill, `v34hs_setup(0)`, the fixture's default seed;
 *   - route TXSAMPLE with a budget of ONE sample;
 *   - microstate PHASE1 and rxstate SILENCE, so neither of the other two
 *     machines contributes to the tail;
 *   - THE DIAGNOSTICS ARE OFF.  Every table-1 arm prints zero lines cold
 *     (finding 323), but three of these six have a trace on the path where
 *     they change `txstate` or reach zero, and this fixture cannot compare
 *     it: our code and the blob's log to two different capture channels and
 *     a side running both would reach only one of them.  The traces are not
 *     reconstructed either; finding 341 records both halves as one gap.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34hstx1.h"

/* Companion fields, by offset in the object.  See src/pump/v34/v34hstx1.c. */
#define TX1_RXFLAGS	0x0386		/* receiver +0x122                 */
#define TX1_F359C	0x359c		/* scrambler generator select      */
#define TX1_SEGLEN	0x35a6		/* 86's reconfiguration point      */
#define TX1_COUNT	0xaa78		/* 78, 85 count down; 86 counts up */
#define TX1_VECTIDX	0x2aa2		/* symbols emitted in this segment */
#define TX1_RATECFG	0xaa84		/* struct v34_ratecfg              */
#define TX1_F25C0	0x25c0		/* symbols in the current segment  */
#define TX1_F25C2	0x25c2		/* the transmitter's flags word    */
#define TX1_F25C6	0x25c6		/* the previous quadrant           */
#define TX1_F25CC	0x25cc		/* the scrambler's shift register  */
#define TX1_V90RX	0x024c		/* v90_receiver                    */
#define TX1_K56RX	0x0250		/* k56flex_receiver                */

#define NP(a)	((int)(sizeof(a) / sizeof((a)[0])))

/* A companion field to set before the arm runs; `wide` selects a 32-bit one. */
struct tx1_poke {
	unsigned	off;
	int		val;
	int		wide;
};

#define P16(o, v)	{ (o), (v), 0 }
#define P32(o, v)	{ (o), (v), 1 }

static int dump;
static unsigned char before[sizeof(struct v34_object)];

static void
apply(short txst, const struct tx1_poke *p, int np)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, 1);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);
	for (i = 0; i < np; i++) {
		if (p[i].wide)
			v34hs_poke_int(p[i].off, p[i].val);
		else
			v34hs_poke_short(p[i].off, (short)p[i].val);
	}
}

/*
 * One case: the arm alone, then the arm against the blob.
 *
 * `want` is the exit the case is supposed to take.  When it is not
 * `V34TX1_LOOP` the arm has left the dispatch for a block this reconstruction
 * does not model, there is nothing to compare it against, and only the first
 * run happens -- named as a gap in finding 343 rather than left silent.
 */
static void
run_case(short txst, int (*arm)(void *), int want, const char *what, long tag,
	 const struct tx1_poke *p, int np)
{
	const unsigned char *o;
	char msg[192];
	unsigned i, nd = 0;
	int rc;

	apply(txst, p, np);
	o = (const unsigned char *)v34hs_object(0);
	memcpy(before, o, sizeof(before));
	rc = arm(v34hs_object(0));

	snprintf(msg, sizeof(msg), "%s: exit code", what);
	diff_eq_int(msg, rc, want, tag);

	for (i = 0; i < sizeof(before); i++)
		if (o[i] != before[i])
			nd++;
	snprintf(msg, sizeof(msg), "%s: the arm wrote something", what);
	diff_eq_int(msg, nd != 0, 1, tag);

	if (rc != V34TX1_LOOP) {
		if (dump)
			printf("  %-38s tx %2d  arm wrote %4u  exit %d\n",
			       what, txst, nd, rc);
		return;
	}

	/*
	 * The guard that stops the differential run below passing for free:
	 * the blob's own loop test must now be false on side A, or side A
	 * runs the blob's copy of the arm we are trying to check.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: the arm advanced the queue to the limit", what);
	diff_eq_int(msg, v34hs_peek_short(0, V34HS_TXCURSOR)
		    >= v34hs_peek_short(0, V34HS_TXLIMIT), 1, tag);

	apply(txst, p, np);
	rc = v34hs_step_case(arm);
	snprintf(msg, sizeof(msg), "%s: exit code, differential run", what);
	diff_eq_int(msg, rc, want, tag);
	v34hs_compare(what, tag);

	if (dump)
		printf("  %-38s tx %2d  arm wrote %4u  step wrote %4u\n",
		       what, txst, nd, v34hs_observed(0)->changed);
}

/* --- 65 XMIT0 ------------------------------------------------------------- */

/*
 * Bit 3 of the receiver's flags word is the arm's only branch, and the fill
 * leaves it clear, so both runs are needed to reach both halves.  With it set
 * the arm raises 0x2000 in `f25c2`, moves the transmit machine to SSEG and
 * clears three counters.
 */
static void
case_xmit0(void)
{
	short flags;
	struct tx1_poke p[4] = {
		P16(TX1_RXFLAGS, 0),
		/*
		 * THE THREE COUNTERS ARE SEEDED NON-ZERO, and without that
		 * the three clears below the `if` are invisible: the
		 * bring-up leaves all three at zero, so clearing them writes
		 * nothing and a reconstruction that skipped them would pass.
		 * The same seeds run on the flag-CLEAR case too, where they
		 * check the opposite -- that the clears do not happen.
		 */
		P16(TX1_F25C6, 0x5678),
		P16(TX1_F25C0, 0x1234),
		P32(TX1_F25CC, 0x11223344)
	};

	v34hs_setup(0);
	flags = v34hs_peek_short(0, TX1_RXFLAGS);

	p[0].val = (short)(flags & ~8);
	run_case(V34HS_XMIT0, v34tx1_xmit0, V34TX1_LOOP,
		 "65 XMIT0, segment flag clear", 6500, p, NP(p));

	p[0].val = (short)(flags | 8);
	run_case(V34HS_XMIT0, v34tx1_xmit0, V34TX1_LOOP,
		 "65 XMIT0, segment flag set", 6501, p, NP(p));
}

/* --- 71 TXLEVEL ----------------------------------------------------------- */

/*
 * `f359c == 0x65` picks the second scrambler generator.  Both values are
 * driven because the object carries the loop twice, and a reconstruction
 * using one tap for both would pass on whichever the fill happens to select.
 */
/*
 * +0x25cc IS SEEDED, and it decides whether the two generators can be told
 * apart at all.  They differ in one tap -- bit 26 against bit 13 -- so a
 * register in which those two agree gives both the same two bits and the
 * select is untestable.  0x04000000 has bit 26 set and bit 13 clear, and the
 * two then diverge on the first bit: q is 2 for generator A and 3 for B.
 */
#define TX1_SRSEED	0x04000000

static const struct tx1_poke gen_a[] = {
	P16(TX1_F359C, 0x64), P32(TX1_F25CC, TX1_SRSEED)
};
static const struct tx1_poke gen_b[] = {
	P16(TX1_F359C, 0x65), P32(TX1_F25CC, TX1_SRSEED)
};

static void
case_txlevel(void)
{
	run_case(V34HS_TXLEVEL, v34tx1_txlevel, V34TX1_LOOP,
		 "71 TXLEVEL, generator A", 7100, gen_a, NP(gen_a));
	run_case(V34HS_TXLEVEL, v34tx1_txlevel, V34TX1_LOOP,
		 "71 TXLEVEL, generator B", 7101, gen_b, NP(gen_b));
}

/* --- 78 JaTXMIT and 85 K56JaTXMIT ----------------------------------------- */

/*
 * Three counter values, because the arm has three paths: zero does nothing,
 * two decrements and leaves, and one decrements TO zero and is the only path
 * that raises bit 2 of `f25c2` and calls `V34EchoReportCoeff` twice.
 */
static void
case_ja(short txst, int (*arm)(void *), const char *name, long tag)
{
	/*
	 * `f25c2` is seeded with bit 2 CLEAR, which is what makes the
	 * completion's one visible act visible: the bring-up leaves the bit
	 * already set, and against that a reconstruction raising it on every
	 * step rather than only at zero writes nothing and passes.
	 */
	struct tx1_poke p[2] = { P16(TX1_COUNT, 0), P16(TX1_F25C2, 0x1001) };
	char what[96];
	int k;
	static const short counts[3] = { 0, 2, 1 };
	static const char *const names[3] = {
		"counter already zero", "counter 2 -> 1", "counter 1 -> 0"
	};

	for (k = 0; k < 3; k++) {
		p[0].val = counts[k];
		snprintf(what, sizeof(what), "%s, %s", name, names[k]);
		run_case(txst, arm, V34TX1_LOOP, what, tag + k, p, NP(p));
	}
}

/* --- 81 MOH_SILENCE ------------------------------------------------------- */

/*
 * The wrap at 0xc0 leaves for 0x66d85, which is not reconstructed, so that
 * run checks the exit code and stops: there is no oracle past a transfer this
 * file does not model.  Finding 343.
 */
static const struct tx1_poke moh_run[] = { P16(TX1_VECTIDX, 0x10) };
static const struct tx1_poke moh_wrap[] = { P16(TX1_VECTIDX, 0xbf) };

static void
case_moh_silence(void)
{
	run_case(V34HS_MOH_SILENCE, v34tx1_moh_silence, V34TX1_LOOP,
		 "81 MOH_SILENCE, counting", 8100, moh_run, 1);
	run_case(V34HS_MOH_SILENCE, v34tx1_moh_silence, V34TX1_MOH_WRAP,
		 "81 MOH_SILENCE, wrap at 0xc0", 8101, moh_wrap, 1);
}

/* --- 86 TXMD -------------------------------------------------------------- */

/*
 * `vect_idx` is compared against +0xaa78 first and +0x35a6 second, so all
 * three are set together in every run and none of them relies on the fill.
 *
 * The reconfiguration runs are the ones that reach `V34SetupModulator`, and
 * there are two so that its fifth argument -- one when either PCM receiver at
 * +0x24c or +0x250 is non-zero -- is computed both ways.  The rate
 * configuration is set to a rate the modulator accepts rather than to
 * whatever the fill left, so the run exercises the setup and not its
 * rejection path.
 */
static const struct tx1_poke txmd_a[] = {
	P16(TX1_VECTIDX, 0x10), P16(TX1_COUNT, 0x40),
	P16(TX1_SEGLEN, 0x50), P16(TX1_F359C, 0x64),
	P32(TX1_F25CC, TX1_SRSEED)
};
static const struct tx1_poke txmd_b[] = {
	P16(TX1_VECTIDX, 0x10), P16(TX1_COUNT, 0x40),
	P16(TX1_SEGLEN, 0x50), P16(TX1_F359C, 0x65),
	P32(TX1_F25CC, TX1_SRSEED)
};
static const struct tx1_poke txmd_done[] = {
	P16(TX1_VECTIDX, 0x3f), P16(TX1_COUNT, 0x40),
	P16(TX1_SEGLEN, 0x50), P16(TX1_F359C, 0x64),
	P32(TX1_F25CC, TX1_SRSEED)
};

/*
 * 3200 BAUD IS NOT A ROUND NUMBER PICKED FOR TIDINESS.  It is the one rate
 * whose modulator setup reads the `v90` argument at all -- 0x40 taps from
 * `tx3200c1_for_v90` against 0x20 from `tx3200c1_for_v34` -- so it is the
 * only rate at which the two PCM-receiver runs below differ by anything.
 * With 2400 baud they are the same call twice and the argument is untested;
 * finding 216 and v34filters.c's 3200 case.
 *
 * `f25c0` and `f25c2` are seeded for the same reason as 65's counters: the
 * reconfiguration clears one and raises 0x8004 in the other, and both are
 * invisible against a zero and against a word that already holds bit 2.
 */
#define TXMD_SETUP							\
	P16(TX1_VECTIDX, 0x4f), P16(TX1_COUNT, 0x40),			\
	P16(TX1_SEGLEN, 0x50), P16(TX1_F359C, 0x64),			\
	P32(TX1_F25CC, TX1_SRSEED),					\
	P16(TX1_F25C0, 0x1234), P16(TX1_F25C2, 0x1001),			\
	P16(TX1_RATECFG + 0x00, 3200),		/* baud            */	\
	P16(TX1_RATECFG + 0x10, 1829),		/* carrier         */	\
	P16(TX1_RATECFG + 0x06, 0)		/* pre-emphasis    */

static const struct tx1_poke txmd_setup_nopcm[] = {
	TXMD_SETUP, P32(TX1_V90RX, 0), P32(TX1_K56RX, 0)
};
static const struct tx1_poke txmd_setup_pcm[] = {
	TXMD_SETUP, P32(TX1_V90RX, 0), P32(TX1_K56RX, 1)
};
/* The other half of the OR, so that neither receiver alone can stand in. */
static const struct tx1_poke txmd_setup_v90[] = {
	TXMD_SETUP, P32(TX1_V90RX, 1), P32(TX1_K56RX, 0)
};

static void
case_txmd(void)
{
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_LOOP,
		 "86 TXMD, generator A", 8600, txmd_a, NP(txmd_a));
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_LOOP,
		 "86 TXMD, generator B", 8601, txmd_b, NP(txmd_b));
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_TXMD_DONE,
		 "86 TXMD, segment complete", 8602, txmd_done, NP(txmd_done));
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_LOOP,
		 "86 TXMD, reconfigure, no PCM", 8603,
		 txmd_setup_nopcm, NP(txmd_setup_nopcm));
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_LOOP,
		 "86 TXMD, reconfigure, K56flex receiver", 8604,
		 txmd_setup_pcm, NP(txmd_setup_pcm));
	run_case(V34HS_TXMD, v34tx1_txmd, V34TX1_LOOP,
		 "86 TXMD, reconfigure, V.90 receiver", 8605,
		 txmd_setup_v90, NP(txmd_setup_v90));
}

int
main(void)
{
	dump = getenv("V34TX1_DUMP") != NULL;
	diff_begin("v34handshak table 1: six per-sample transmit arms");

	/*
	 * The diagnostics stay OFF; see the head of this file.  It is stated
	 * rather than left to the default because it is part of what the
	 * comparison holds fixed.
	 */
	v34hs_debug(0);

	case_xmit0();
	case_txlevel();
	case_ja(V34HS_JaTXMIT, v34tx1_jatxmit, "78 JaTXMIT", 7800);
	case_ja(V34HS_K56JaTXMIT, v34tx1_k56jatxmit, "85 K56JaTXMIT", 8500);
	case_moh_silence();
	case_txmd();

	return diff_end();
}
