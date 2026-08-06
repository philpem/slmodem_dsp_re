/*
 * t_v34hstx1.c -- ten arms of `v34handshak`'s per-sample transmit dispatch,
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
#define TX1_F358C	0x358c		/* 60 masks it with one            */
#define TX1_F25D0	0x25d0		/* the transmitted point, 32 bits  */
#define TX1_RATENOW	0x0228		/* rate_now, int                   */
#define TX1_RATEWANT	0x022c		/* rate_want, int                  */
#define TX1_TIMER	0x0238		/* the sample clock's running count*/
#define TX1_TIMERMARK	0x0248		/* where the span is measured from */
#define TX1_F2218	0x2218		/* int: selects the tail's arms    */
#define TX1_RATEIDX	0xaa98		/* short: the negotiated rate index*/
#define TX1_RXF21C	0x0480		/* receiver +0x21c, short          */
#define TX1_RXF220	0x0484		/* receiver +0x220, int            */

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

/* --- 60 TONE_AB ----------------------------------------------------------- */

/*
 * One bit of +0x358c chooses between `vect4[0]` and `vect4[2]`, and the
 * object gets there by scaling the masked bit by EIGHT over a table of
 * four-byte entries.  Three runs:
 *
 *   0x1234  bit 0 clear                -> vect4[0]
 *   0x1235  bit 0 set                  -> vect4[2]
 *   0x1236  bit 0 clear, BIT 1 SET     -> vect4[0]
 *
 * The third is not a third behaviour; it is the second reading of the first
 * one, and it is here to separate "bit 0" from "bit 1" and from "the whole
 * low nibble".  A mask that admitted bit 1 would send vect4[2] on it.  The
 * scale is separated by the second run alone: with a scale of four the odd
 * case sends vect4[1], which is a different point.
 *
 * `f25d0` and `f25d2` are seeded together as one 32-bit word that is neither
 * point, so the store is visible whichever branch runs.  The fill would
 * otherwise leave whatever it left and one of the two cases could pass with
 * no store at all.
 */
static struct tx1_poke tone_ab[] = {
	P16(TX1_F358C, 0x1234), P32(TX1_F25D0, 0x11223344)
};

static void
case_tone_ab(void)
{
	tone_ab[0].val = 0x1234;
	run_case(V34HS_TONE_AB, v34tx1_tone_ab, V34TX1_LOOP,
		 "60 TONE_AB, +0x358c even", 6000, tone_ab, NP(tone_ab));
	tone_ab[0].val = 0x1235;
	run_case(V34HS_TONE_AB, v34tx1_tone_ab, V34TX1_LOOP,
		 "60 TONE_AB, +0x358c odd", 6001, tone_ab, NP(tone_ab));
	tone_ab[0].val = 0x1236;
	run_case(V34HS_TONE_AB, v34tx1_tone_ab, V34TX1_LOOP,
		 "60 TONE_AB, +0x358c even with bit 1 set", 6002,
		 tone_ab, NP(tone_ab));
}

/* --- 18 SSEG -------------------------------------------------------------- */

/*
 * Two symbols and one tick.  `f25c0` is seeded away from both 0x3f and 0 so
 * that the counting run's increment and the completing run's clear are both
 * visible against what was there.
 *
 * The completing run is the one that moves `txstate` to SBARSEG, and it is
 * the only path through 0x66d11.  There is nothing to hold fixed for it
 * beyond the counter: the arm reads no other companion.
 */
static struct tx1_poke sseg[] = {
	P16(TX1_F25C0, 0x10), P32(TX1_F25D0, 0x11223344)
};

static void
case_sseg(void)
{
	sseg[0].val = 0x10;
	run_case(V34HS_SSEG, v34tx1_sseg, V34TX1_LOOP,
		 "18 SSEG, counting", 1800, sseg, NP(sseg));
	sseg[0].val = 0x3f;
	run_case(V34HS_SSEG, v34tx1_sseg, V34TX1_LOOP,
		 "18 SSEG, segment complete at 0x40", 1801, sseg, NP(sseg));
	/*
	 * PAST 0x40, which the object never reaches by counting but which is
	 * the only run that tells `== 0x40` from `>= 0x40`.  The count is
	 * seeded, so the state is reachable in the fixture whether or not the
	 * handshake can produce it, and the object's answer is that the
	 * segment does NOT end -- it counts on to the sixteen-bit wrap.
	 */
	sseg[0].val = 0x50;
	run_case(V34HS_SSEG, v34tx1_sseg, V34TX1_LOOP,
		 "18 SSEG, counting past 0x40", 1802, sseg, NP(sseg));
}

/* --- 70 DATAXMIT ---------------------------------------------------------- */

/*
 * `vect_idx` IS PINNED SMALL AND THAT IS NOT TIDINESS.  `modulatevector`
 * regenerates its eight points when the index is exactly 8 and otherwise maps
 * point `vect_idx` out of the vector it is holding; the fill leaves a
 * pseudorandom halfword there, so an unpinned run reads past an eight-point
 * array at most seeds and the case would be measuring the fill.  Three is a
 * point the vector has.
 *
 * Everything the conditional body writes is seeded to something else:
 *
 *   receiver +0x21c, +0x220     cleared, so they are seeded non-zero
 *   +0x248                      seeded apart from +0x238, or the copy is
 *                               invisible
 *   rate_now, rate_want         seeded apart from each other AND from the
 *                               index, so neither store can stand in for
 *                               the other
 *   +0x2218                     seeded to 3, so the store of 1 shows
 *
 * THE RATE INDEX IS DRIVEN NEGATIVE in the third run.  The object
 * sign-extends a short into two ints, and a rate index is small and positive
 * in every run that a fill or a real handshake would produce -- so a `movzwl`
 * reading of it agrees everywhere except here.
 */
#define DATAXMIT_SEED							\
	P16(TX1_VECTIDX, 3),						\
	P16(TX1_RXF21C, 0x5a5a),	P32(TX1_RXF220, 0x33445566),	\
	P32(TX1_TIMER, 0x0a0b0c0d),	P32(TX1_TIMERMARK, 0x0e0f1011),	\
	P32(TX1_RATENOW, 0x11111111),	P32(TX1_RATEWANT, 0x22222222),	\
	P32(TX1_F2218, 3)

static const struct tx1_poke dataxmit_off[] = {
	DATAXMIT_SEED, P16(TX1_F25C2, 0x1001), P16(TX1_RATEIDX, 0x0c)
};
static const struct tx1_poke dataxmit_on[] = {
	DATAXMIT_SEED, P16(TX1_F25C2, 0x1011), P16(TX1_RATEIDX, 0x0c)
};
static const struct tx1_poke dataxmit_neg[] = {
	DATAXMIT_SEED, P16(TX1_F25C2, 0x1011), P16(TX1_RATEIDX, -3)
};

static void
case_dataxmit(void)
{
	run_case(V34HS_DATAXMIT, v34tx1_dataxmit, V34TX1_LOOP,
		 "70 DATAXMIT, data path off", 7000,
		 dataxmit_off, NP(dataxmit_off));
	run_case(V34HS_DATAXMIT, v34tx1_dataxmit, V34TX1_LOOP,
		 "70 DATAXMIT, data path on", 7001,
		 dataxmit_on, NP(dataxmit_on));
	run_case(V34HS_DATAXMIT, v34tx1_dataxmit, V34TX1_LOOP,
		 "70 DATAXMIT, negative rate index", 7002,
		 dataxmit_neg, NP(dataxmit_neg));
}

/* --- 51 TX_L1 ------------------------------------------------------------- */

/*
 * The arm's own loop is selected by the MICROSTATE, which `apply()` sets to
 * PHASE1 through `v34hs_state`; poking +0x3592 afterwards is what moves it,
 * and both values are driven because the object carries the loop twice and a
 * reconstruction with one copy would pass on whichever the fixture picked.
 *
 * `f25d4` is the transmit scale and it is poked rather than left to the fill,
 * because the fill's value decides whether the doubling in the second copy is
 * visible at all: at a scale of zero every sample comes out zero and the two
 * copies agree.  0x16a1 is `v34pcmif.c`'s own value and is not a power of
 * two, so the shift is exercised rather than folded into the multiply.  One
 * run drives it NEGATIVE, which is the reading of `movswl` against `movzwl`.
 * The arithmetic shift needs no run of its own: `probe` is half negative, so
 * every run produces negative products.
 *
 * Six of the seven runs below are independent; the seventh is named where it
 * is not.
 */
#define TX1_MST		0x3592		/* the microstate halfword         */
#define TX1_F25D4	0x25d4		/* the transmit scale              */
#define TX1_SCALE	0x16a1

static struct tx1_poke tx_l1[] = {
	P16(TX1_MST, V34HS_PHASE1), P16(TX1_VECTIDX, 0x10),
	P16(TX1_F25D4, TX1_SCALE)
};

static void
run_tx_l1(short mst, short vect, short scale, const char *what, long tag)
{
	tx_l1[0].val = mst;
	tx_l1[1].val = vect;
	tx_l1[2].val = scale;
	run_case(V34HS_TX_L1, v34tx1_tx_l1, V34TX1_LOOP, what, tag,
		 tx_l1, NP(tx_l1));
}

static void
case_tx_l1(void)
{
	/* The two copies of the loop, on the same four indices. */
	run_tx_l1(V34HS_PHASE1, 0x10, TX1_SCALE,
		  "51 TX_L1, microstate elsewhere", 5100);
	run_tx_l1(V34HS_TX_L1, 0x10, TX1_SCALE,
		  "51 TX_L1, microstate TX_L1", 5101);

	/*
	 * The segment's end, and the two ways of missing it.  0x5fc reaches
	 * 0x600 exactly; 0x5fd steps over it to 0x601, which is what tells
	 * `== 0x600` from `>= 0x600`; and 0x5fc with the microstate elsewhere
	 * is the run that says the SECOND microstate test at 0x62cf5 is not
	 * decoration -- an arm that ended the segment unconditionally would
	 * move a machine that is not in this state.
	 */
	run_tx_l1(V34HS_TX_L1, 0x5fc, TX1_SCALE,
		  "51 TX_L1, segment ends at 0x600", 5102);
	run_tx_l1(V34HS_TX_L1, 0x5fd, TX1_SCALE,
		  "51 TX_L1, stepped over 0x600", 5103);
	run_tx_l1(V34HS_PHASE1, 0x5fc, TX1_SCALE,
		  "51 TX_L1, 0x600 with the microstate elsewhere", 5104);

	/*
	 * The mask.  0x3e..0x41 walks off the end of the table and back to
	 * the start, so a reconstruction that indexed without masking, or
	 * masked with 0x1f or 0x7f, reads four samples this run does not.
	 */
	run_tx_l1(V34HS_TX_L1, 0x3e, TX1_SCALE,
		  "51 TX_L1, the index wraps at 64", 5105);

	/* The scale's sign. */
	run_tx_l1(V34HS_TX_L1, 0x10, -TX1_SCALE,
		  "51 TX_L1, negative scale", 5106);
}

/*
 * `probe` IS DATA THIS TREE NOW CARRIES, so it is proved the way `vect4` is
 * -- against the blob's own copy, which `objcopy` renamed `ref_probe`.  This
 * is the only check in the file that does not go through the fixture, and it
 * is not redundant with the seven runs above, for two measured reasons.
 *
 * FIRST, THE RUNS READ TEN OF THE SIXTY-FOUR ENTRIES.  Four indices per run
 * and four starting points -- 0x10, 0x3c, 0x3d and 0x3e -- reach 0x00, 0x01,
 * 0x10 to 0x13 and 0x3c to 0x3f.  The other fifty-four are covered here and
 * nowhere else.
 *
 * SECOND, A SMALL ERROR IN AN ENTRY THE RUNS DO READ CAN STILL BE INVISIBLE.
 * Each sample is multiplied by 0x16a1 and shifted right by 14, so a
 * transcription off by one survives the arithmetic: entry 0 changed from
 * 13,027 to 13,028 leaves every emitted sample identical and fails ONLY the
 * memcmp.  That was run rather than argued.
 */
extern const short ref_probe[V34_PROBE_SAMPLES];

static void
case_probe_table(void)
{
	diff_eq_int("probe, 64 shorts at .rodata+0x2c00",
		    memcmp(probe, ref_probe, sizeof(probe)), 0, 5199);
}

int
main(void)
{
	dump = getenv("V34TX1_DUMP") != NULL;
	diff_begin("v34handshak table 1: ten per-sample transmit arms");

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

	case_tone_ab();
	case_sseg();
	case_dataxmit();
	case_probe_table();
	case_tx_l1();

	return diff_end();
}
