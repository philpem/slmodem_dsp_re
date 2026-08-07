/*
 * t_v34hstx1.c -- seventeen arms of `v34handshak`'s per-sample transmit
 * dispatch, each compared against the blob on its own.
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
#define TX1_F25C	0x025c		/* the echo filter's lag base      */
#define TX1_F358E	0x358e		/* 19 clears it, 20 counts it to 6 */
#define TX1_F35A4	0x35a4		/* 19 scales it by 0x53            */
#define TX1_FAA7C	0xaa7c		/* 20 adds it into the counter     */
#define TX1_RTD		0xaa7e		/* 20 reloads vect_idx from it     */
#define TX1_FAA86	0xaa86		/* 20 accumulates into it          */
#define TX1_F25C8	0x25c8		/* the quadrant just chosen        */
#define TX1_F382	0x0382		/* receiver +0x11e: 69's selector  */
#define TX1_F25D6	0x25d6		/* 64/68's sixteen bits, two a pass*/
#define TX1_F25D8	0x25d8		/* 64/68 counts one per pass       */
#define TX1_F25DA	0x25da		/* 64 needs 2; 66ca8 needs non-zero*/
#define TX1_F35A2	0x35a2		/* 66ca8's other companion         */
#define TX1_FABF8	0x00abf8	/* byte: 24's message-dispatch     */
					/* one-shot.  Up here rather than  */
					/* with 24's other companions      */
					/* because `run_case_ex` reads it   */
#define TX1_FABF9	0x00abf9	/* byte: picks 24's message AND    */
					/* its hold tail's first way out   */

#define NP(a)	((int)(sizeof(a) / sizeof((a)[0])))

/*
 * A companion field to set before the arm runs.  `kind` is 0 for a halfword,
 * 1 for a 32-bit one and 2 for a POINTER, which has to be aimed per side --
 * the two objects are at two addresses, so one address written into both is
 * exactly the asymmetry findings 319-322 are about.
 */
struct tx1_poke {
	unsigned	off;
	int		val;
	int		kind;
};

#define P16(o, v)	{ (o), (v), 0 }
#define P32(o, v)	{ (o), (v), 1 }
#define PSELF(o, t)	{ (o), (int)(t), 2 }
#define P8(o, v)	{ (o), (v), 3 }

static int dump;
static unsigned char before[sizeof(struct v34_object)];

/*
 * A per-case fixup that runs after the bring-up and outside the object.
 * Only 54's completion needs one; it is NULL for every other case and the
 * ten arms that landed before this one never see it.  See `aim_session`.
 */
static void (*fixup)(void);

static void
apply(short txst, const struct tx1_poke *p, int np)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_TXSAMPLE, 1);
	v34hs_state(V34HS_PHASE1, V34HS_SILENCE, txst);
	for (i = 0; i < np; i++) {
		if (p[i].kind == 3)
			v34hs_poke_byte(p[i].off, (unsigned char)p[i].val);
		else if (p[i].kind == 2)
			v34hs_poke_self_ptr(p[i].off, (unsigned)p[i].val);
		else if (p[i].kind)
			v34hs_poke_int(p[i].off, p[i].val);
		else
			v34hs_poke_short(p[i].off, (short)p[i].val);
	}
	if (fixup != NULL)
		fixup();
}

/*
 * One case: the arm alone, then the arm against the blob.
 *
 * `want` is the exit the case is supposed to take.  When it is not
 * `V34TX1_LOOP` the arm has left the dispatch for a block this reconstruction
 * does not model, there is nothing to compare it against, and only the first
 * run happens -- named as a gap in finding 343 rather than left silent.
 *
 * `guard` IS THE SECOND ANTI-VACUITY GUARD, AND IT HAS THREE FORMS BECAUSE
 * ONE ARM HAS PATHS THAT DO NOT TRANSMIT.  What the guard is FOR is that side
 * A's `ref_v34handshak` must not go on to run the blob's own copy of the arm
 * we are checking and supply what our arm left out -- if it did, an arm that
 * did nothing would be compared against itself and pass.  Sixteen of the
 * seventeen arms stop that by leaving the queue's count at the block's limit,
 * which makes the blob skip the per-sample loop outright (0x62933).
 *
 * 24 `TX_DPSK` has four paths and only two of them reach `txmit`.  The two
 * that end the message move the TRANSMIT STATE instead, so the blob's loop
 * re-dispatches -- to a DIFFERENT arm, and the two sides converge because
 * side B's blob arm moved the state the same way.  `TX1_GUARD_STATE` is that,
 * and it is the same claim by the other route: nothing else in table 1 shares
 * 24's entry, and `case_tx_dpsk_entry` asserts that against the blob's own
 * `.rodata`.
 *
 * AND ONE OF 24'S PATHS MOVES NEITHER, which is the third form.  With
 * `moh_message` outside 0..3 the dispatch does NOTHING but clear its own
 * one-shot at +0xabf8, and clearing it is what stops the re-dispatch being a
 * repeat: the blob then takes the arm's OTHER branch (0x67c4e) rather than
 * the message dispatch again.  `TX1_GUARD_ONESHOT` asserts the clear, and it
 * is a real guard for the same reason the other two are -- an arm that did
 * nothing fails it.  It is 24's alone and the offset is named at the head of
 * this file for that reason.
 *
 * `run_case` passes `TX1_GUARD_QUEUE`, so the sixteen arms that landed before
 * this one are unchanged and no run of theirs silently moved to a weaker
 * form.
 */
enum tx1_guard {
	TX1_GUARD_QUEUE = 0,	/* the arm left the queue at the block limit */
	TX1_GUARD_STATE,	/* the arm moved the transmit machine        */
	TX1_GUARD_ONESHOT	/* 24 only: the arm cleared +0xabf8          */
};

static void
run_case_ex(short txst, int (*arm)(void *), int want, const char *what,
	    long tag, const struct tx1_poke *p, int np, int guard)
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
	if (guard == TX1_GUARD_QUEUE) {
		snprintf(msg, sizeof(msg),
			 "%s: the arm advanced the queue to the limit", what);
		diff_eq_int(msg, v34hs_peek_short(0, V34HS_TXCURSOR)
			    >= v34hs_peek_short(0, V34HS_TXLIMIT), 1, tag);
	} else if (guard == TX1_GUARD_STATE) {
		snprintf(msg, sizeof(msg),
			 "%s: the arm moved the transmit machine off %d",
			 what, (int)txst);
		diff_eq_int(msg,
			    v34hs_peek_short(0, V34HS_TXSTATE) != txst, 1, tag);
	} else {
		/*
		 * BOTH HALVES, and the second is the guard.  "+0xabf8 is
		 * zero" is true of an arm that did nothing whenever the fixture
		 * seeded it zero, which is a check that reads as a check and is
		 * not; what has to be asserted is that the arm CLEARED it.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: the arm cleared the one-shot at +0xabf8", what);
		diff_eq_int(msg,
			    before[TX1_FABF8] != 0 && o[TX1_FABF8] == 0, 1, tag);
	}

	apply(txst, p, np);
	rc = v34hs_step_case(arm);
	snprintf(msg, sizeof(msg), "%s: exit code, differential run", what);
	diff_eq_int(msg, rc, want, tag);
	v34hs_compare(what, tag);

	if (dump)
		printf("  %-38s tx %2d  arm wrote %4u  step wrote %4u\n",
		       what, txst, nd, v34hs_observed(0)->changed);
}

static void
run_case(short txst, int (*arm)(void *), int want, const char *what, long tag,
	 const struct tx1_poke *p, int np)
{
	run_case_ex(txst, arm, want, what, tag, p, np, TX1_GUARD_QUEUE);
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
	/*
	 * WHAT THE FILL LEAVES AT +0x358c IS NOT ASSERTED, because it is the
	 * fill's and every `V34HS_SEED` moves it: `v34handshakinit` clears
	 * the field only on the Modem-on-Hold path (v34hshak.c:1480) and the
	 * cases here are brought up in mode 0.  That is exactly why both
	 * values are POKED rather than one of them being reached by luck.
	 * `V34TX1_DUMP=1` prints what this fill happened to leave.
	 */
	if (dump) {
		v34hs_setup(0);
		printf("  +0x358c after the default bring-up: 0x%04x\n",
		       (unsigned short)v34hs_peek_short(0, TX1_F358C));
	}

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

/* --- 19 SBARSEG ----------------------------------------------------------- */

/*
 * Two symbols and one tick of the PAIR count, and then four ways out of the
 * completion tested in the object's own order.  Everything the arm writes is
 * seeded away from what it stores:
 *
 *   +0x25d0    to neither `vect4[2]` nor `vect4[1]`
 *   f25c0      to 7 on the completing runs, so the clear at 0x671f2 shows,
 *              and away from 7 on the counting ones
 *   +0x35a6    to a value that is neither zero nor `f35a4 * 0x53`, which is
 *              what makes the bit-15 run's `f25c0 = f35a6` visible AND the
 *              TXMD run's recomputation visible
 *   +0xaa78    to a value the counter arithmetic does not produce
 *   `vect_idx` and +0x358e non-zero, because 0x67236 clears both
 *
 * `f25c2` carries the two guard bits and is seeded with a third bit set that
 * neither path touches, so a reconstruction that assigned the word rather
 * than testing it is caught by the byte comparison.
 *
 * THE MODULATOR NEEDS NO RATE POKED.  All six arguments are literals in the
 * object -- 4800 baud, 2400 carrier, no pre-emphasis, `v90` zero -- so unlike
 * 86 there is no configuration to drive and no `v90` argument to compute
 * either way.  Finding 216's 3200-baud rule is about a rate this arm never
 * asks for.
 */
#define SBARSEG_SEED							\
	P32(TX1_F25D0, 0x11223344),	P16(TX1_SEGLEN, 0x0555),	\
	P16(TX1_COUNT, 0x0777),		P16(TX1_VECTIDX, 0x0123),	\
	P16(TX1_F358E, 0x0456)

static struct tx1_poke sbarseg[] = {
	SBARSEG_SEED, P16(TX1_F25C0, 0x10), P16(TX1_F25C2, 0x0101),
	P16(TX1_F35A4, 0x1234), P16(TX1_F25C, 0x0100)
};
#define SB_C0	5
#define SB_C2	6
#define SB_A4	7
#define SB_25C	8

static void
run_sbarseg(int c0, int c2, int a4, int f25c, const char *what, long tag)
{
	sbarseg[SB_C0].val = c0;
	sbarseg[SB_C2].val = c2;
	sbarseg[SB_A4].val = a4;
	sbarseg[SB_25C].val = f25c;
	run_case(V34HS_SBARSEG, v34tx1_sbarseg, V34TX1_LOOP, what, tag,
		 sbarseg, NP(sbarseg));
}

static void
case_sbarseg(void)
{
	/*
	 * Counting.  The second run is at f25c0 == 8, which is the run that
	 * tells the object's `== 8` after the increment from a `>= 8`: the
	 * count goes to 9 and the segment does NOT end.
	 */
	run_sbarseg(0x10, 0x0101, 0x1234, 0x0100,
		    "19 SBARSEG, counting", 1900);
	run_sbarseg(8, 0x0101, 0x1234, 0x0100,
		    "19 SBARSEG, counting past 8", 1901);

	/*
	 * The four completions, in the order the object tests them.  The
	 * first is bit 13 of f25c2 -- `test $0x20,%dh`, which is bit 5 of the
	 * HIGH byte -- and a reading of it as bit 5 of the word would take
	 * the wrong branch on 0x0121 as well as on 0x2101.
	 */
	run_sbarseg(7, 0x2101, 0x1234, 0x0100,
		    "19 SBARSEG, complete, f25c2 bit 13 -> TRNSEG4A", 1902);
	run_sbarseg(7, 0x0121, 0x1234, 0x0100,
		    "19 SBARSEG, complete, bit 13 clear with bit 5 set", 1903);
	run_sbarseg(7, 0x0101, 0, 0x0100,
		    "19 SBARSEG, complete, f35a4 zero -> PPSEG", 1904);
	run_sbarseg(7, 0x8101, 0x1234, 0x0100,
		    "19 SBARSEG, complete, f25c2 bit 15 -> PPSEG, f25c0 = f35a6",
		    1905);

	/*
	 * And the fourth, which is the only path that reaches the modulator
	 * and the counter.  Four values of +0x25c, because the counter is a
	 * signed division and each one separates a different misreading:
	 *
	 *   0x0100   an ordinary positive span
	 *   0x1388   1512 - 5000, NEGATIVE, and the quotient is not exact --
	 *            the object truncates toward zero where a floor would
	 *            give one less
	 *   0xb1e0   -20000 as a short: 1512 - (-20000) leaves a quotient
	 *            that does NOT fit a short, which is what the `movswl`
	 *            at 0x678e9 is for
	 *   0x7530   30000: negative span AND a quotient outside a short
	 */
	run_sbarseg(7, 0x0101, 0x1234, 0x0100,
		    "19 SBARSEG, complete -> TXMD, positive span", 1906);
	run_sbarseg(7, 0x0101, 0x1234, 5000,
		    "19 SBARSEG, complete -> TXMD, negative span", 1907);
	run_sbarseg(7, 0x0101, 0x1234, -20000,
		    "19 SBARSEG, complete -> TXMD, quotient past a short", 1908);
	run_sbarseg(7, 0x0101, 0x1234, 30000,
		    "19 SBARSEG, complete -> TXMD, both at once", 1909);

	/*
	 * AND THE DIVISOR ITSELF, which needs the widest span the field can
	 * hold.  The quotient is truncated to a short and then scaled down by
	 * 0x960/0x4000, so dividing by 9601 instead of 9600 changes the stored
	 * counter on almost no input: at +0x25c 0x100, 5000, -20000 and 30000
	 * the two divisors give the SAME answer, and only at 0x7fff -- a span
	 * of -31,255 -- do they part.  Measured rather than assumed; without
	 * this run the constant is untested and the suite looks green.
	 */
	run_sbarseg(7, 0x0101, 0x1234, 0x7fff,
		    "19 SBARSEG, complete -> TXMD, the widest span", 1911);

	/*
	 * The 0x53 scale, at a value whose product does not fit sixteen bits:
	 * 0x1234 * 0x53 is 0x5e71c and the object stores 0xe71c.
	 */
	run_sbarseg(7, 0x0101, 0x0203, 0x0100,
		    "19 SBARSEG, complete -> TXMD, small f35a4", 1910);
}

/* --- 20 PPSEG ------------------------------------------------------------- */

/*
 * `vect_idx` IS PINNED INTO 0..47 ON EVERY RUN, and for the reason 70's is:
 * the object indexes `vectpp` with a SIGN-EXTENDED `vect_idx` and no mask, so
 * an unpinned run reads outside a 192-byte table.  That is a fault rather
 * than a failure, and a fault has no offset in it.
 *
 * Everything the arm writes is seeded away from what it stores: the point at
 * +0x25d0 to a word that is no entry of `vectpp`; f25c0, +0x358e, +0xaa78 and
 * +0xaa86 to values none of the three paths produces.
 *
 * The three paths are told apart by f25c0 as much as by anything else -- the
 * two that do not end the segment bump it at 0x6430c and the one that does
 * leaves through 0x63da2, which writes nothing.
 */
#define PPSEG_SEED							\
	P32(TX1_F25D0, 0x11223344),	P16(TX1_F25C0, 0x1234),		\
	P16(TX1_COUNT, 0x0777),		P16(TX1_FAA86, 0x0555),		\
	P16(TX1_FAA7C, 0x0037),		P16(TX1_F25C, 0x0100)

static struct tx1_poke ppseg[] = {
	PPSEG_SEED, P16(TX1_VECTIDX, 0x10), P16(TX1_F358E, 2),
	P16(TX1_RTD, 0x0011), P16(TX1_RATECFG + 0x00, 3200),
	P16(TX1_F359C, 0x64)
};
#define PP_IDX	6
#define PP_58E	7
#define PP_RTD	8
#define PP_BAUD	9
#define PP_59C	10

static void
run_ppseg(int idx, int f358e, int rtd, int baud, int f359c, int f25c,
	  const char *what, long tag)
{
	ppseg[5].val = f25c;			/* the last of PPSEG_SEED */
	ppseg[PP_IDX].val = idx;
	ppseg[PP_58E].val = f358e;
	ppseg[PP_RTD].val = rtd;
	ppseg[PP_BAUD].val = baud;
	ppseg[PP_59C].val = f359c;
	run_case(V34HS_PPSEG, v34tx1_ppseg, V34TX1_LOOP, what, tag,
		 ppseg, NP(ppseg));
}

static void
case_ppseg(void)
{
	/*
	 * Inside a pass.  Two indices, because one point tells nothing about
	 * the scale: entry 0x10 is (-3238, 5609) and entry 0 is (6476, 0),
	 * and a reconstruction indexing by shorts rather than by four-byte
	 * points sends a different word for the first and the same for the
	 * second.
	 */
	run_ppseg(0x10, 2, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, inside a pass", 2000);
	run_ppseg(0, 2, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, the first point of the pass", 2001);
	run_ppseg(0x2e, 2, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, one before the last point", 2002);

	/*
	 * The pass ends at 48.  +0x358e counts, `vect_idx` is cleared, and
	 * f25c0 is still bumped.  The second run is at +0x358e == 6, which
	 * separates the object's `== 6` after the increment from a `>= 6`.
	 */
	run_ppseg(0x2f, 2, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, the pass ends, +0x358e counts", 2003);
	run_ppseg(0x2f, 6, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, the pass ends past six", 2004);

	/*
	 * The segment ends: one run per baud the object recognises, plus one
	 * it does not.  The unrecognised run is NOT a filler -- it is the one
	 * that says +0xaa86 keeps its own value through the switch and is
	 * still read three instructions later.
	 */
	run_ppseg(0x2f, 5, 0x11, 2400, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 2400 baud", 2005);
	run_ppseg(0x2f, 5, 0x11, 2800, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 2800 baud", 2006);
	run_ppseg(0x2f, 5, 0x11, 3000, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 3000 baud", 2007);
	run_ppseg(0x2f, 5, 0x11, 3200, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 3200 baud", 2008);
	run_ppseg(0x2f, 5, 0x11, 3429, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 3429 baud", 2009);
	run_ppseg(0x2f, 5, 0x11, 1234, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at an unknown baud", 2010);

	/*
	 * `f359c == 0x65` adds `rtd` into the counter a SECOND time, having
	 * already put it into `vect_idx`, so a run at 0x65 with the same
	 * `rtd` is what separates the two uses.
	 */
	run_ppseg(0x2f, 5, 0x11, 3200, 0x65, 0x0100,
		  "20 PPSEG, the segment ends, f359c 0x65", 2011);

	/*
	 * `rtd + 0x90` NEGATIVE, which is the `cwtl` at 0x6928e and friends:
	 * the scaled copy is an arithmetic shift of a sign-extended halfword,
	 * and a `movzwl` reading of it agrees on every non-negative value.
	 */
	run_ppseg(0x2f, 5, -0x200, 2400, 0x64, 0x0100,
		  "20 PPSEG, the segment ends, rtd negative at 2400", 2012);
	run_ppseg(0x2f, 5, -0x200, 3429, 0x64, 0x0100,
		  "20 PPSEG, the segment ends, rtd negative at 3429", 2013);

	/*
	 * A LARGE `rtd`, and it is what makes the five scales separable at
	 * all.  The scaled copy is `(v * m) >> 14`, so a one-count change in
	 * `m` moves the answer by `v / 16384` -- at v = 0xa1 that is zero and
	 * 0x12ab and 0x12ac give the same byte.  At 0x7000 they do not.
	 */
	run_ppseg(0x2f, 5, 0x7000, 2800, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 2800 baud, large rtd", 2016);
	run_ppseg(0x2f, 5, 0x7000, 3429, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 3429 baud, large rtd", 2017);
	run_ppseg(0x2f, 5, 0x7000, 3000, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 3000 baud, large rtd", 2018);
	run_ppseg(0x2f, 5, -0x7000, 2800, 0x64, 0x0100,
		  "20 PPSEG, the segment ends at 2800 baud, rtd far negative",
		  2019);

	/*
	 * And the counter's division, which is 19's with the baud in place of
	 * the shift: 0x1388 makes `0x5e8 - f25c` negative and inexact, so the
	 * object's truncation toward zero and a floor differ by one.
	 */
	run_ppseg(0x2f, 5, 0x11, 3200, 0x64, 5000,
		  "20 PPSEG, the segment ends, negative span", 2014);
	run_ppseg(0x2f, 5, 0x11, 3429, 0x64, -20000,
		  "20 PPSEG, the segment ends, large span", 2015);
}

/* --- 5 SILENCE, 54 SILENCEINFO, 74 SILENCERETRAIN ------------------------- */

/*
 * ONE TABLE ENTRY, THREE CASES, AND THEY ARE INDEPENDENT.  The brief this
 * batch was written to expected 54 and 74 to be "the same check under a
 * different index"; they are not.  `.rodata+0x2da0` gives indices 0, 49 and
 * 69 the identical address, and the shared prologue then RE-READS +0x3596 and
 * branches on it, so each of the three runs a different tail.  The entry's
 * identity is asserted below in `case_silence_entry`; the three behaviours
 * are asserted by driving them, and each of the three can fail while the
 * other two pass.
 */
#define TX1_FAA7A	0xaa7a		/* cleared by the shared prologue  */
#define TX1_FABE8	0xabe8		/* byte: the Modem-on-Hold flag    */
#define TX1_PTR_AA6C	0xaa6c		/* the self-pointer 54 aims        */
#define TX1_BLK_A94C	0xa94c		/* the record it is aimed at       */
#define TX1_BLK_A97C	0xa97c		/* somewhere else to aim it first  */
#define TX1_MSTATE	0x3592
#define TX1_RXSTATE	0x3594

/*
 * The twelve fields of the record at +0xa94c that 54's completion writes
 * directly, plus the three `V34SetINFO0aBits` fills.  Every one is seeded to
 * something the arm does not store, which is finding 345's rule: eleven of
 * task #16's mutations were uncatchable until the fixture stopped agreeing
 * with the arm by accident.
 */
#define A94C_SEED							\
	P16(TX1_BLK_A94C + 0x00, 0x0101), P16(TX1_BLK_A94C + 0x02, 0x0202), \
	P16(TX1_BLK_A94C + 0x04, 0x0303),				\
	P16(TX1_BLK_A94C + 0x14, 0x0404), P16(TX1_BLK_A94C + 0x16, 0x0505), \
	P16(TX1_BLK_A94C + 0x18, 0x0606), P16(TX1_BLK_A94C + 0x1a, 0x0707), \
	P16(TX1_BLK_A94C + 0x1c, 0x0808), P16(TX1_BLK_A94C + 0x1e, 0x0909), \
	P16(TX1_BLK_A94C + 0x20, 0x0a0a), P16(TX1_BLK_A94C + 0x22, 0x0b0b), \
	P32(TX1_BLK_A94C + 0x24, 0x0c0c0c0c),				\
	P16(TX1_BLK_A94C + 0x28, 0x0d0d), P16(TX1_BLK_A94C + 0x2a, 0x0e0e), \
	P32(TX1_BLK_A94C + 0x2c, 0x0f0f0f0f)

/* What 74's retrain clears, moves or sets, all seeded away from it. */
#define RETRAIN_SEED							\
	P16(TX1_F358C, 0x1234), P16(TX1_COUNT, 0x0777),			\
	P16(0xaae0, 0x1111), P16(0xaae2, 0x2222)

/*
 * THE SESSION, and it is here because `V34SetINFO0aBits` walks it.
 *
 * With `v90_receiver` non-zero the callee reaches
 * `*(session_ptr(sess, SESSION_CAPS) + 0x11)` (v34info.c:244) or the upstream
 * block at +9, and the fixture fills the session with pseudorandom bytes and
 * aims only its pointer to the PCM receiver -- so those two fields are wild
 * and the run would FAULT rather than fail, on both sides.  The two runs that
 * drive `v90_receiver` non-zero therefore aim both at one shared static.
 *
 * THAT KEEPS THE TWO SIDES CONGRUENT, which is the property findings 319-322
 * are about: a pointer into each side's own arena would make the session
 * blocks differ and the arena sweep would fail.  One address in both sessions
 * is the same four bytes in both, and the callee reads the same byte on each
 * side.  The variant word is pinned for the same reason a poke is preferred
 * to a fill anywhere else -- so the case does not depend on the seed.
 */
#define TX1_SESSPTR	0x3548
#define SESS_VARIANT	0x6120
#define SESS_CAPS	0x612c
#define SESS_UPSTREAM	0x1760

static unsigned char sess_stub[64];

static void
aim_session(void)
{
	int side;

	for (side = 0; side < 2; side++) {
		char *o = (char *)v34hs_object(side);
		char *s;
		const void *stub = sess_stub;

		memcpy(&s, o + TX1_SESSPTR, sizeof(s));
		memcpy(s + SESS_VARIANT, &(int){ 1 }, sizeof(int));
		memcpy(s + SESS_CAPS, &stub, sizeof(stub));
		memcpy(s + SESS_UPSTREAM, &stub, sizeof(stub));
	}
}

/*
 * THE FOUR THINGS A RUN VARIES COME FIRST, at indices 0 to 3.  They were at
 * the end, addressed as `NP(silence) - k`, and inserting one poke in the
 * middle moved every one of them by one -- so `vect_idx` was never poked, the
 * countdowns never completed, and both tails ran on no case at all.  The test
 * still passed; the mutation suite is what said otherwise, with every one of
 * 54's and 74's mutations uncaught in a run that reported green.
 */
#define SI_IDX	0
#define SI_59C	1
#define SI_V90	2
#define SI_E8	3

static struct tx1_poke silence[] = {
	P16(TX1_VECTIDX, 3), P16(TX1_F359C, 0x64), P32(TX1_V90RX, 0),
	P8(TX1_FABE8, 0),
	A94C_SEED, RETRAIN_SEED,
	P16(TX1_FAA7A, 0x3333), PSELF(TX1_PTR_AA6C, TX1_BLK_A97C),
	/*
	 * +0xabe9 IS SEEDED NON-ZERO AND IS NOT DECORATION.  The object reads
	 * the flag with `cmpb`, so the byte after it must hold something for a
	 * halfword reading of the same field to be distinguishable at all --
	 * left to the fill it is a coin toss and the check would depend on the
	 * seed.
	 */
	P8(0xabe9, 0x5a)
};

static void
run_silence(short txst, int idx, int f359c, int v90, int abe8,
	    const char *what, long tag)
{
	silence[SI_IDX].val = idx;
	silence[SI_59C].val = f359c;
	silence[SI_V90].val = v90;
	silence[SI_E8].val = abe8;
	fixup = v90 != 0 ? aim_session : NULL;
	run_case(txst, v34tx1_silence, V34TX1_LOOP, what, tag,
		 silence, NP(silence));
	fixup = NULL;
}

static void
case_silence(void)
{
	/*
	 * 5 SILENCE is the prologue and nothing else: four zero samples and
	 * the clear of +0xaa7a, which is seeded non-zero so the clear shows.
	 * It is the whole arm for that txstate, and driving 54 or 74 does not
	 * test it -- their tails run on top of the same prologue but the
	 * "wrote something" guard cannot separate the two contributions.
	 */
	run_silence(V34HS_SILENCE, 3, 0x64, 0, 0,
		    "5 SILENCE, the prologue alone", 500);

	/* 54: nine blocks of counting, then the tenth. */
	run_silence(V34HS_SILENCEINFO, 3, 0x64, 0, 0,
		    "54 SILENCEINFO, counting", 5400);
	run_silence(V34HS_SILENCEINFO, 0xa, 0x64, 0, 0,
		    "54 SILENCEINFO, counting past ten", 5401);
	run_silence(V34HS_SILENCEINFO, 9, 0x64, 0, 0,
		    "54 SILENCEINFO, the tenth block, f359c 0x64", 5402);
	/*
	 * The 0x1e store needs BOTH `f359c == 0x65` and `v90_receiver`
	 * non-zero, so three more runs: neither half of the conjunction can
	 * stand in for it.  It is `v90_receiver` ALONE and not the
	 * `+0x24c || +0x250` pair 86 computes, which is why there is no
	 * K56flex run here.
	 */
	run_silence(V34HS_SILENCEINFO, 9, 0x65, 0, 0,
		    "54 SILENCEINFO, the tenth block, f359c 0x65, no V.90",
		    5403);
	run_silence(V34HS_SILENCEINFO, 9, 0x65, 1, 0,
		    "54 SILENCEINFO, the tenth block, f359c 0x65 and V.90",
		    5404);
	run_silence(V34HS_SILENCEINFO, 9, 0x64, 1, 0,
		    "54 SILENCEINFO, the tenth block, V.90 without f359c",
		    5405);

	/*
	 * 74: 0xb3 blocks of counting, then the retrain.  +0xabe8 is read
	 * with `cmpb`, so it is poked as a BYTE -- a halfword poke would set
	 * +0xabe9 too and say nothing about the width the object reads.
	 */
	run_silence(V34HS_SILENCERETRAIN, 3, 0x64, 0, 0,
		    "74 SILENCERETRAIN, counting", 7400);
	run_silence(V34HS_SILENCERETRAIN, 0xb4, 0x64, 0, 0,
		    "74 SILENCERETRAIN, counting past 0xb4", 7401);
	run_silence(V34HS_SILENCERETRAIN, 0xb3, 0x64, 0, 0,
		    "74 SILENCERETRAIN, retrain -> TX_PHASE1_ANS", 7402);
	run_silence(V34HS_SILENCERETRAIN, 0xb3, 0x65, 0, 0,
		    "74 SILENCERETRAIN, retrain -> RX_PHASE1_CALL", 7403);
	run_silence(V34HS_SILENCERETRAIN, 0xb3, 0x64, 0, 1,
		    "74 SILENCERETRAIN, retrain -> MOH_TONE", 7404);
	/*
	 * NOT A FIFTH BEHAVIOUR, and named rather than counted: +0xabe8
	 * non-zero wins over `f359c`, so this run must produce exactly what
	 * 7404 produces.  It separates "the flag is tested first" from "the
	 * two are combined", and it cannot fail while 7404 passes.
	 */
	run_silence(V34HS_SILENCERETRAIN, 0xb3, 0x65, 0, 1,
		    "74 SILENCERETRAIN, retrain, the flag beats f359c", 7405);
}

/*
 * THE ENTRY IS SHARED, AND THAT IS A PROPERTY OF THE OBJECT AND NOT OF THIS
 * FILE.  `.rodata+0x2da0` is 0x1a0 bytes past `probe`, which the blob exports
 * at `.rodata+0x2c00`, and the three indices are `txstate - 5`.  Reading the
 * table this way rather than trusting a comment is what makes a later change
 * that gave 54 its own arm a FAILURE here rather than a silence.
 *
 * The entries are addresses inside `v34handshak`, so the second check pins
 * the target as well as the sharing: 0x640b4 - 0x628f0.
 */
extern const short ref_probe[V34_PROBE_SAMPLES];
extern void ref_v34handshak(void *obj);

/*
 * `volatile` so that the offset arithmetic below is done at run time.  With a
 * plain `const char *` the compiler knows `ref_probe` is 64 shorts long and
 * warns that 0x1a0 bytes past it is outside the array -- which is true and is
 * the point: the anchor names a place in `.rodata`, not an object to index.
 */
static const char *volatile rodata_2c00 = (const char *)ref_probe;

static void
case_silence_entry(void)
{
	const char *const *t1 = (const char *const *)
				(rodata_2c00 + (0x2da0 - 0x2c00));
	const char *base = (const char *)ref_v34handshak;

	diff_eq_int("table 1: txstates 5 and 54 share one entry",
		    t1[5 - 5] == t1[54 - 5], 1, 5490);
	diff_eq_int("table 1: txstates 5 and 74 share one entry",
		    t1[5 - 5] == t1[74 - 5], 1, 5491);
	diff_eq_int("table 1: that entry is v34handshak + 0x17c4",
		    (int)(t1[5 - 5] - base), 0x640b4 - 0x628f0, 5492);
}

/*
 * `vectpp` IS DATA THIS TREE ALREADY HAD AND DID NOT PROVE.  It was static in
 * v34rx.c, where `receiver` slices against it as ninety-six shorts; 20 PPSEG
 * reads the same bytes as forty-eight four-byte points, which is why the blob
 * exports it and why it is global now.  Proved against `ref_vectpp` the way
 * `probe` and `vect4` are: the fifteen PPSEG runs read four of the
 * forty-eight entries and the memcmp covers the other forty-four.
 */
extern const short ref_vectpp[2 * V34_VECTPP_POINTS];

static void
case_vectpp_table(void)
{
	diff_eq_int("vectpp, 96 shorts at .rodata+0x2c80",
		    memcmp(vectpp, ref_vectpp, sizeof(vectpp)), 0, 2099);
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
static void
case_probe_table(void)
{
	diff_eq_int("probe, 64 shorts at .rodata+0x2c00",
		    memcmp(probe, ref_probe, sizeof(probe)), 0, 5199);
}

/* --- 69 EXMIT ------------------------------------------------------------- */

/*
 * ONE HALFWORD OF THE RECEIVER DECIDES WHETHER HALF THE ARM RUNS AT ALL, and
 * that is why it is the first poke rather than an afterthought.  0x63858's
 * first instruction compares +0x11e against 0x89b0; the fixture's fill leaves
 * a pseudorandom halfword there, so without this poke the sixteen-point
 * continuation at 0x67031 -- 447 bytes, the `vect16` half, two scrambler
 * calls -- is never entered on any run, and `run_case`'s two guards cannot
 * see it because the four-point half writes plenty.  That is finding 422's
 * failure mode exactly, and the mutation suite is what would say so.
 *
 * Everything the arm writes is seeded away from what it stores:
 *
 *   f25cc      TX1_SRSEED, which separates the two generators (see 71)
 *   f25c6      NON-ZERO and inside 0..3, or `(q + f25c6) & 3` cannot be told
 *              from `q & 3`; two values, so "add f25c6" is not "add two"
 *   f25c8      to a word that is no quadrant, so the store shows
 *   +0x25d0    to a word that is neither a `vect4` nor a `vect16` entry
 *   vect_idx   set per run; the completion reloads it with 8
 *
 * `f25c2` carries the generator select in bit 0 -- NOT `f359c == 0x65`, which
 * is 71 and 86's -- and is seeded with other bits set so a reconstruction
 * that assigned the word rather than reading one bit of it is caught.
 */
#define EX_F382		0
#define EX_C2		1
#define EX_IDX		2
#define EX_C6		3

static struct tx1_poke exmit[] = {
	P16(TX1_F382, 0x1234), P16(TX1_F25C2, 0x1001), P16(TX1_VECTIDX, 4),
	P16(TX1_F25C6, 2),
	P32(TX1_F25CC, TX1_SRSEED), P16(TX1_F25C8, 0x0777),
	P32(TX1_F25D0, 0x11223344)
};

static void
run_exmit(int f382, int c2, int idx, int c6, const char *what, long tag)
{
	exmit[EX_F382].val = f382;
	exmit[EX_C2].val = c2;
	exmit[EX_IDX].val = idx;
	exmit[EX_C6].val = c6;
	run_case(V34HS_EXMIT, v34tx1_exmit, V34TX1_LOOP, what, tag,
		 exmit, NP(exmit));
}

static void
case_exmit(void)
{
	/*
	 * The four-point half: +0x11e is anything but 0x89b0.  Two generators
	 * and two previous quadrants, on an index that does not complete.
	 */
	run_exmit(0x1234, 0x1001, 4, 2,
		  "69 EXMIT, four points, generator A", 6900);
	run_exmit(0x1234, 0x1000, 4, 2,
		  "69 EXMIT, four points, generator B", 6901);
	run_exmit(0x1234, 0x1001, 4, 1,
		  "69 EXMIT, four points, a different last quadrant", 6902);

	/*
	 * The segment ends at `vect_idx == 0x14` exactly, tested after the
	 * advance -- so 0x12 reaches it by two and 0x13 steps over it to
	 * 0x15, which is what tells `== 0x14` from `>= 0x14`.
	 */
	run_exmit(0x1234, 0x1001, 0x12, 2,
		  "69 EXMIT, four points, the segment ends at 0x14", 6903);
	run_exmit(0x1234, 0x1001, 0x13, 2,
		  "69 EXMIT, four points, stepping over 0x14", 6904);

	/*
	 * The sixteen-point half.  0x89b0 is a sixteen-bit compare and the
	 * value is negative as a short, so it is poked as one word rather
	 * than assembled from a fill.
	 */
	run_exmit(0x89b0, 0x1001, 4, 2,
		  "69 EXMIT, sixteen points, generator A", 6905);
	run_exmit(0x89b0, 0x1000, 4, 2,
		  "69 EXMIT, sixteen points, generator B", 6906);
	run_exmit(0x89b0, 0x1001, 4, 1,
		  "69 EXMIT, sixteen points, a different last quadrant", 6907);

	/* Four samples a pass, so 0x10 completes and 0x11 steps over. */
	run_exmit(0x89b0, 0x1001, 0x10, 2,
		  "69 EXMIT, sixteen points, the segment ends at 0x14", 6908);
	run_exmit(0x89b0, 0x1001, 0x11, 2,
		  "69 EXMIT, sixteen points, stepping over 0x14", 6909);

	/*
	 * AND ONE RUN THAT IS NOT A BEHAVIOUR: 0x89b1 is one count away from
	 * the constant, so it must produce exactly what 6900 produces.  It
	 * separates "equal to 0x89b0" from a mask or a range and cannot fail
	 * while the four-point runs pass.
	 */
	run_exmit(0x89b1, 0x1001, 4, 2,
		  "69 EXMIT, one count off the selector", 6910);
	/*
	 * AND 0x12b0, WHICH IS THE RUN THAT SAYS THE COMPARE IS SIXTEEN BITS.
	 * Its low byte is the constant's, so a byte-wide reading of +0x11e
	 * takes the sixteen-point half here where the object takes the
	 * four-point one; 0x1234 and 0x89b1 both agree with a byte reading and
	 * the mutation went uncaught until this run existed.
	 */
	run_exmit(0x12b0, 0x1001, 4, 2,
		  "69 EXMIT, the selector's low byte alone", 6911);
}

/* --- 64 JTXMIT and 68 J1TXMIT --------------------------------------------- */

/*
 * ONE TABLE ENTRY, ONE BODY, TWO TAILS -- and unlike 5/54/74 the prologue does
 * NOT re-read `txstate`.  The compare at 0x636ff is inside the pass on which
 * `vect_idx` wraps to zero, so a run at 68 that does not wrap is 64's check
 * under a different index and a run at 68 that does wrap is a behaviour of its
 * own.  Both are here and each is labelled with which it is.
 *
 * THE FOUR THINGS EVERY RUN VARIES ARE AT LITERAL INDICES AT THE HEAD, which
 * is finding 422's rule: addressing a poke as `NP(a) - k` breaks silently the
 * moment one is inserted, and `run_case`'s guards do not notice because the
 * body still writes.
 *
 * Everything the arm writes is seeded away from what it stores:
 *
 *   +0x25d8    away from what the increment produces, and per run
 *   +0x25d6    0x1b4e, whose eight dibits are 2 3 0 1 3 2 1 0 -- all four
 *              values, so the shift by twice `vect_idx` is live and a wrong
 *              shift picks a different symbol.  0x899f is what the segment's
 *              end stores, so the seed is away from that too
 *   f25cc      TX1_SRSEED for the generators, and 68's tail clears it
 *   f25c6      non-zero and inside 0..3, and 68's tail clears it
 *   f25c8      to a word that is no quadrant
 *   f25c0      non-zero, because only 68's tail clears it
 *   +0x25d0    to a word that is no `vect4` entry
 *   +0xaa78    per run: 0 is "no countdown", 1 completes it, 2 leaves it
 *              running AND is what the segment's end needs to report
 *   f25c2      bit 0 the generator, bit 2 CLEAR so both echo-report blocks'
 *              one visible act is visible
 */
#define JT_IDX		0
#define JT_C2		1
#define JT_CNT		2
#define JT_59C		3
#define JT_D8		4
#define JT_DA		5
#define JT_A2		6
#define JT_FLAGS	7
#define JT_C6		8
#define JT_D6		9

static struct tx1_poke jtxmit[] = {
	P16(TX1_VECTIDX, 3), P16(TX1_F25C2, 0x1001), P16(TX1_COUNT, 0),
	P16(TX1_F359C, 0x64), P16(TX1_F25D8, 0x0070), P16(TX1_F25DA, 2),
	P16(TX1_F35A2, 0x0033), P16(TX1_RXFLAGS, 0), P16(TX1_F25C6, 2),
	P16(TX1_F25D6, 0x1b4e),
	P32(TX1_F25CC, TX1_SRSEED), P16(TX1_F25C8, 0x0777),
	P32(TX1_F25D0, 0x11223344),
	P16(TX1_F25C0, 0x1234)
};

/*
 * The defaults: a pass that does not wrap, no countdown, the generator with
 * bit 0 set, `f359c` away from 0x66, and 64's two companions already holding
 * what its tail wants so that a run reaching the wrap ends the segment unless
 * it says otherwise.
 */
static void
jt_reset(void)
{
	jtxmit[JT_IDX].val = 3;
	jtxmit[JT_C2].val = 0x1001;
	jtxmit[JT_CNT].val = 0;
	jtxmit[JT_59C].val = 0x64;
	jtxmit[JT_D8].val = 0x0070;
	jtxmit[JT_DA].val = 2;
	jtxmit[JT_A2].val = 0x0033;
	jtxmit[JT_FLAGS].val = 0;
	jtxmit[JT_C6].val = 2;
	jtxmit[JT_D6].val = 0x1b4e;
}

static void
run_jtxmit(short txst, const char *what, long tag)
{
	run_case(txst, v34tx1_jtxmit, V34TX1_LOOP, what, tag,
		 jtxmit, NP(jtxmit));
}

static void
case_jtxmit(void)
{
	/* The body: two generators, and the dibit the index selects. */
	jt_reset();
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, a pass, generator A", 6400);
	jt_reset();
	jtxmit[JT_C2].val = 0x1000;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, a pass, generator B", 6401);
	jt_reset();
	jtxmit[JT_C6].val = 1;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, a different last quadrant", 6402);

	/*
	 * The shift is by TWICE `vect_idx`.  At +0x25d6 == 0x1b4e these four
	 * indices -- 0, 1, 5 and 8 -- select dibits 2, 3, 2 and 0, so three
	 * distinct symbols and not four; index 5 repeats index 0's.  Index 8 is
	 * the one that shifts the halfword out entirely, a legal count of
	 * sixteen whose answer is zero, and it is also THE ONLY ONE that
	 * separates a shift by twice the index from a shift by the index: at
	 * 0, 1 and 5 the halved count lands on the same dibit.  That is a fact
	 * about this seed, so a different +0x25d6 would move it.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 0;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the first dibit", 6403);
	jt_reset();
	jtxmit[JT_IDX].val = 1;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the second dibit", 6404);
	jt_reset();
	jtxmit[JT_IDX].val = 5;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the sixth dibit", 6405);
	jt_reset();
	jtxmit[JT_IDX].val = 8;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the halfword shifted out", 6406);
	/*
	 * AND THE SAME INDEX WITH THE BIT SOURCE NEGATIVE, which is the ONLY
	 * run that separates the object's `movzwl` at 0x6360d from a `movswl`.
	 * Only two bits of the shifted value are consumed, so a sign extension
	 * is invisible until the shift pushes the sign INTO them -- which needs
	 * a count of at least sixteen, and index 8 is the smallest that gives
	 * one.  Unsigned the two bits are 0 and signed they are 3, so the two
	 * readings send different symbols.  0x899f is the value the segment's
	 * end reloads, and it is negative as a short.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 8;
	jtxmit[JT_D6].val = 0x899f;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the bit source negative", 6409);

	/*
	 * The countdown at the top, which is 78 and 85's `tx1_ja_common`: 0
	 * does nothing (6400 above), 2 decrements and leaves, 1 decrements TO
	 * zero and is the path that raises bit 2 of f25c2 and reports both
	 * cancellers before falling back into the body.
	 */
	jt_reset();
	jtxmit[JT_CNT].val = 2;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the counter 2 -> 1", 6407);
	jt_reset();
	jtxmit[JT_CNT].val = 1;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the counter 1 -> 0", 6408);

	/*
	 * NOT A BEHAVIOUR OF ITS OWN, and it is the point of the pair: a pass
	 * that does not wrap never reaches 0x636ff, so 68 runs 64's body and
	 * must produce exactly what 6400 produces.  It cannot fail while 6400
	 * passes, and it is what makes "one entry, one body" a measurement.
	 */
	jt_reset();
	run_jtxmit(V34HS_J1TXMIT, "68 J1TXMIT, a pass, the same body", 6800);

	/*
	 * 68's TAIL, and it is independent: at the wrap the arm clears f25cc,
	 * f25c6 and f25c0 and moves the transmit machine to TRNSEG4A.  All
	 * three are seeded non-zero; the second run is at the other generator
	 * so the clear of f25cc is measured against two different registers.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	run_jtxmit(V34HS_J1TXMIT, "68 J1TXMIT, the wrap ends the segment", 6801);
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_C2].val = 0x1000;
	run_jtxmit(V34HS_J1TXMIT,
		   "68 J1TXMIT, the wrap, generator B", 6802);

	/*
	 * 64's TAIL.  +0x25da must hold 2 or the pass ends where it stands;
	 * +0x25d8 must be PAST 0x80 after the increment, tested signed.
	 */
	/*
	 * +0x25d8 IS PAST 0x80 ON THIS RUN AND THAT IS DELIBERATE: with it
	 * short of the end the mutation that deletes the +0x25da test rejoins
	 * where the object rejoins and goes uncaught.  The two guards have to
	 * be separated one at a time.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_DA].val = 3;
	jtxmit[JT_D8].val = 0x0080;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the wrap with +0x25da wrong", 6410);
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x0060;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the wrap, the segment runs on",
		   6411);
	/*
	 * 0x7f increments to 0x80, which is NOT past it -- the object's `jle`
	 * -- so this is the run that tells `> 0x80` from `>= 0x80`.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x007f;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the wrap at exactly 0x80", 6412);
	/* And 0x80 increments to 0x81, which is. */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x0080;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the segment ends, no report", 6413);

	/*
	 * THE COMPARE IS SIGNED, and these two runs are the only ones that say
	 * so.  0x8000 and 0x7fff both leave a NEGATIVE halfword after the
	 * increment, which a `movzwl` reading calls larger than 0x80 and the
	 * object does not -- so an unsigned reconstruction ends the segment on
	 * both and the object ends it on neither.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x8000;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the wrap with +0x25d8 negative",
		   6414);
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x7fff;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the wrap, +0x25d8 wraps negative",
		   6415);

	/*
	 * The segment's end with the counter still running, which is the only
	 * path to the SECOND pair of echo reports: it raises bit 2 of f25c2
	 * and zeroes +0xaa78.  The two pairs are mutually exclusive on one
	 * pass -- reaching zero at the top leaves nothing here to do -- so
	 * 6416 is the second pair and 6417 is the first pair and no second.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x0080;
	jtxmit[JT_CNT].val = 2;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the segment ends, counter 2",
		   6416);
	/*
	 * AND 6417 IS NOT A THIRD OBSERVATION, which is worth saying because
	 * three paths give only two states: at counter 1 the countdown reaches
	 * zero and does the reporting, and the segment's end then finds the
	 * counter already zero and declines -- so the object ends where the
	 * counter-2 run ends it, +0xaa78 zero and bit 2 of f25c2 up, by the
	 * other route.  It cannot fail while 6416 passes.  It is kept because
	 * its MUTATION value differs: `the second pair is guarded the other
	 * way` fails 6416 alone, and only running both says which.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_D8].val = 0x0080;
	jtxmit[JT_CNT].val = 1;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, the segment ends, counter 1 -> 0",
		   6417);

	/*
	 * `f359c == 0x66` -- 0x66ca8, reachable from either txstate and tested
	 * BEFORE the wrap.  Four runs, because it is a three-test conjunction
	 * whose first arm short-circuits the other two, and only the arm that
	 * gets through writes: it moves the transmit machine to XMIT0 and
	 * clears `vect_idx` through 0x62d32.  The index is 3 on all four, so
	 * the body leaves 4 behind and the clear is visible.
	 */
	/*
	 * THE FLAG RUN HAS BOTH COMPANIONS ZERO, and that is what makes it a
	 * check: with them holding what 0x66ca8 wants anyway, a mutation
	 * reading the wrong flag bit falls through the other two tests and
	 * arrives at the same place.  Only the flag can get through here.
	 */
	jt_reset();
	jtxmit[JT_59C].val = 0x66;
	jtxmit[JT_FLAGS].val = 0x0400;		/* V34_RX_FLAG_DATA */
	jtxmit[JT_A2].val = 0;
	jtxmit[JT_DA].val = 0;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, f359c 0x66, the data flag", 6420);
	jt_reset();
	jtxmit[JT_59C].val = 0x66;
	jtxmit[JT_A2].val = 0;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, f359c 0x66, +0x35a2 zero", 6421);
	jt_reset();
	jtxmit[JT_59C].val = 0x66;
	jtxmit[JT_DA].val = 0;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, f359c 0x66, +0x25da zero", 6422);
	jt_reset();
	jtxmit[JT_59C].val = 0x66;
	run_jtxmit(V34HS_JTXMIT, "64 JTXMIT, f359c 0x66, both companions set",
		   6423);

	/*
	 * AND ONE THAT ORDERS THE TWO DECISIONS.  At txstate 68 with the index
	 * at the wrap, 0x65653 and 0x66ca8 want two different things; the
	 * object tests `f359c` first (0x636ea, before 0x636f0), so this run
	 * must move the transmit machine to XMIT0 and NOT to TRNSEG4A.  It is
	 * an independent check: a reconstruction testing the wrap first passes
	 * every other run here.
	 */
	jt_reset();
	jtxmit[JT_IDX].val = 7;
	jtxmit[JT_59C].val = 0x66;
	jtxmit[JT_FLAGS].val = 0x0400;
	run_jtxmit(V34HS_J1TXMIT, "68 J1TXMIT, f359c 0x66 beats the wrap",
		   6424);
}

/*
 * THE TWO SHARED ENTRIES OF THIS BATCH, read out of the blob's own `.rodata`
 * the way `case_silence_entry` reads 5, 54 and 74's.  64 and 68 hold one
 * entry; 69 holds a different one, and that is asserted too so that a later
 * blob folding the two together is a failure rather than a silence.
 */
static void
case_jtxmit_entry(void)
{
	const char *const *t1 = (const char *const *)
				(rodata_2c00 + (0x2da0 - 0x2c00));
	const char *base = (const char *)ref_v34handshak;

	diff_eq_int("table 1: txstates 64 and 68 share one entry",
		    t1[64 - 5] == t1[68 - 5], 1, 6490);
	diff_eq_int("table 1: that entry is v34handshak + 0xcdc",
		    (int)(t1[64 - 5] - base), 0x635cc - 0x628f0, 6491);
	diff_eq_int("table 1: txstate 69 has an entry of its own",
		    t1[69 - 5] != t1[64 - 5], 1, 6492);
	diff_eq_int("table 1: 69's entry is v34handshak + 0xf68",
		    (int)(t1[69 - 5] - base), 0x63858 - 0x628f0, 6493);
}

/*
 * `vect16` IS THE ONE TABLE 69's SIXTEEN-POINT HALF READS, and this file is
 * its first reader here.  It is already proved against `ref_vect16` in
 * t_v34hshak.c, t_v34k56.c and t_v90p34.cpp, so this is a FOURTH copy and not
 * an independent check -- it is here for the reason `probe` and `vectpp` are,
 * that a run reads one entry of sixteen and a transcription error in any
 * other survives every run in this file.
 */
extern const int ref_vect16[16];

static void
case_vect16_table(void)
{
	diff_eq_int("vect16, 16 points at .rodata",
		    memcmp(vect16, ref_vect16, sizeof(vect16)), 0, 6999);
}

/* --- 67 XMITMP ------------------------------------------------------------ */

/*
 * THE ARM ALWAYS MAPS A SYMBOL, so `run_case`'s two guards -- "the arm wrote
 * something" and "the cursor reached the limit" -- hold on EVERY run whatever
 * else did or did not happen.  Neither of them can see whether the bit loop
 * ran twice or four times, whether a checkpoint fired, whether the reader
 * refilled, or whether `initdigital` was reached.  That is finding 422's
 * per-arm/per-path gap at its widest in this file, and the only thing that
 * says a run went where its name says is `tools/mutate.py`.  So every field
 * that decides a path is poked, and every poke a run varies is at a LITERAL
 * index at the head of the array.
 *
 * WHERE THE MESSAGE READER LIVES IS THE CASE'S OWN AXIS.  The object aims
 * +0xaa6c at +0xaa3c (v34hshak.h, `getMPrecvdBits`) and 0x30 bytes of
 * `struct v34_bitsource` fit exactly between the two -- so in the object's own
 * configuration the halfword 0x645de tests at obj+0xaa3c and the one 0x647db
 * tests at `*(+0xaa6c)` are the SAME two bytes, and a reconstruction using
 * either for both passes every run.  The runs here aim the pointer at +0xa94c
 * instead and drive the two bits apart; the run with the record back at
 * +0xaa3c is a CONTROL and is named as one below.
 *
 * Everything the arm writes is seeded away from what it stores: `f25c8` to a
 * word that is no quadrant (the arm clears it first, so a reconstruction that
 * only ORed would keep the seed), `f25c6` non-zero and inside 0..3, `f25cc`
 * to TX1_SRSEED, +0x25d0 to a word that is no `vect4` or `vect16` entry,
 * +0x3590 away from 0x22, +0x3598 away from 1, +0x359e non-zero, and every
 * one of the reader's thirteen fields away from what 0x64635 reloads it with.
 */
#define TX1_F3590	0x3590		/* the next stuff point            */
#define TX1_F3598	0x3598		/* initdigital has run             */
#define TX1_F359E	0x359e		/* sequences sent                  */

#define MP_REC		0xa94c		/* where these runs aim +0xaa6c    */
#define MP_ALT		0xaa3c		/* and where the object aims it    */

/*
 * The reader's fields the runs do NOT vary, at both bases.  Every one is away
 * from what 0x64635 stores, which is what makes the reload visible at all.
 */
#define MP_TAIL_SEED(B)							\
	P16((B) + 0x14, 0x0123),	/* crc, away from 0xffff       */ \
	P16((B) + 0x1c, 0x0008),	/* wordbits, away from 0x10    */ \
	P16((B) + 0x1e, 0x0001),	/* idx, away from zero         */ \
	P16((B) + 0x22, 0x0044),	/* repeats, away from zero     */ \
	P16((B) + 0x2a, 0x0009),	/* avail0, away from 0x12      */ \
	P32((B) + 0x2c, 0x00012345)	/* acc0, away from 0x3fffe     */

/* The nine words after word[0]; word[0] itself is varied. */
#define MP_WORD_SEED(B)							\
	P16((B) + 0x02, 0x1357), P16((B) + 0x04, 0x2466),		\
	P16((B) + 0x06, 0x3575), P16((B) + 0x08, 0x4684),		\
	P16((B) + 0x0a, 0x5793), P16((B) + 0x0c, 0x68a2),		\
	P16((B) + 0x0e, 0x79b1), P16((B) + 0x10, 0x8ac0),		\
	P16((B) + 0x12, 0x9bcf)

/*
 * And the six the runs DO vary, at the alternate base, held at the defaults:
 * only the control run reads the record there, and it is a control.
 */
#define MP_ALT_SEED							\
	P32(MP_ALT + 0x24, 0x000000b0), P16(MP_ALT + 0x28, 8),		\
	P16(MP_ALT + 0x18, 0x0040), P16(MP_ALT + 0x1a, 0x0008),		\
	P16(MP_ALT + 0x16, 0), P16(MP_ALT + 0x20, 3)

#define XM_SEL		0
#define XM_FLAGS	1
#define XM_IDX		2
#define XM_3590		3
#define XM_359E		4
#define XM_3598		5
#define XM_AA3C		6
#define XM_W0		7
#define XM_BASE		8
#define XM_ACC		9
#define XM_AVAIL	10
#define XM_NBITS	11
#define XM_POS		12
#define XM_CRCON	13
#define XM_REPEAT	14
#define XM_C2		15
#define XM_C6		16

static struct tx1_poke xmitmp[] = {
	P16(TX1_F382, 0x1234),			/* XM_SEL    */
	P16(TX1_RXFLAGS, 0x0141),		/* XM_FLAGS  */
	P16(TX1_VECTIDX, 0x0010),		/* XM_IDX    */
	P16(TX1_F3590, 0x0700),			/* XM_3590   */
	P16(TX1_F359E, 5),			/* XM_359E   */
	P16(TX1_F3598, 3),			/* XM_3598   */
	P16(MP_ALT + 0x00, 0x5a5a),		/* XM_AA3C   */
	P16(MP_REC + 0x00, 0x2468),		/* XM_W0     */
	PSELF(TX1_PTR_AA6C, MP_REC),		/* XM_BASE   */
	P32(MP_REC + 0x24, 0x000000b0),		/* XM_ACC    */
	P16(MP_REC + 0x28, 8),			/* XM_AVAIL  */
	P16(MP_REC + 0x18, 0x0040),		/* XM_NBITS  */
	P16(MP_REC + 0x1a, 0x0008),		/* XM_POS    */
	P16(MP_REC + 0x16, 0),			/* XM_CRCON  */
	P16(MP_REC + 0x20, 3),			/* XM_REPEAT */
	P16(TX1_F25C2, 0x1001),			/* XM_C2     */
	P16(TX1_F25C6, 2),			/* XM_C6     */

	MP_WORD_SEED(MP_REC), MP_TAIL_SEED(MP_REC),
	MP_WORD_SEED(MP_ALT), MP_TAIL_SEED(MP_ALT), MP_ALT_SEED,

	P32(TX1_F25CC, TX1_SRSEED), P16(TX1_F25C8, 0x0777),
	P32(TX1_F25D0, 0x11223344)
};

/*
 * The defaults: the four-point selector, bit 5 of the flags word CLEAR so the
 * checkpoints are at 0xbb and 0xbc, an index far from all three of them, the
 * reader holding eight bits of 0xb0 -- which are 1, 0, 1, 1 in the order they
 * come out, so all four collected bits differ and neither dibit is the other.
 */
static void
xm_reset(void)
{
	xmitmp[XM_SEL].val = 0x1234;
	xmitmp[XM_FLAGS].val = 0x0141;
	xmitmp[XM_IDX].val = 0x0010;
	xmitmp[XM_3590].val = 0x0700;
	xmitmp[XM_359E].val = 5;
	xmitmp[XM_3598].val = 3;
	xmitmp[XM_AA3C].val = 0x5a5a;
	xmitmp[XM_W0].val = 0x2468;
	xmitmp[XM_BASE].val = MP_REC;
	xmitmp[XM_ACC].val = 0x000000b0;
	xmitmp[XM_AVAIL].val = 8;
	xmitmp[XM_NBITS].val = 0x0040;
	xmitmp[XM_POS].val = 0x0008;
	xmitmp[XM_CRCON].val = 0;
	xmitmp[XM_REPEAT].val = 3;
	xmitmp[XM_C2].val = 0x1001;
	xmitmp[XM_C6].val = 2;
}

static void
run_xmitmp(const char *what, long tag)
{
	run_case(V34HS_XMITMP, v34tx1_xmitmp, V34TX1_LOOP, what, tag,
		 xmitmp, NP(xmitmp));
}

static void
case_xmitmp(void)
{
	/*
	 * The four-point half: two bits, one scrambler step, one differential
	 * quadrant.  Two generators and two previous quadrants.
	 */
	xm_reset();
	run_xmitmp("67 XMITMP, four points, generator A", 6700);
	xm_reset();
	xmitmp[XM_C2].val = 0x1000;
	run_xmitmp("67 XMITMP, four points, generator B", 6701);
	xm_reset();
	xmitmp[XM_C6].val = 1;
	run_xmitmp("67 XMITMP, four points, a different last quadrant", 6702);

	/*
	 * The sixteen-point half: FOUR bits, and the two dibits go to two
	 * scrambler steps.  The seed makes them 1 and 3, so a reconstruction
	 * passing the same dibit twice, or the wrong one first, sends a
	 * different point.
	 */
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b0;
	run_xmitmp("67 XMITMP, sixteen points, generator A", 6703);
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b0;
	xmitmp[XM_C2].val = 0x1000;
	run_xmitmp("67 XMITMP, sixteen points, generator B", 6704);
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b0;
	xmitmp[XM_C6].val = 1;
	run_xmitmp("67 XMITMP, sixteen points, a different last quadrant", 6705);
	/*
	 * AND ONE WITH A HIGH DIBIT THAT IS NOT THREE, which is what the
	 * default seed cannot give: 0xb0's four bits are 1, 0, 1, 1, so the
	 * high dibit is 3 and passing the scrambler a literal three instead of
	 * `src >> 2` computes the same answer on every other run here.  0x90's
	 * are 1, 0, 0, 1 -- a low dibit of 1 and a high one of 2.
	 */
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b0;
	xmitmp[XM_ACC].val = 0x00000090;
	run_xmitmp("67 XMITMP, sixteen points, a high dibit that is not three",
		   6735);

	/*
	 * ONE COUNT OFF THE SELECTOR IS A CONTROL and must produce exactly
	 * what 6700 produces; 0x12b0 is not, because it shares the constant's
	 * low byte and is what says the compare is sixteen bits wide.  Both
	 * are 69's runs under a second arm -- the same halfword against the
	 * same constant.
	 */
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b1;
	run_xmitmp("67 XMITMP, one count off the selector", 6706);
	xm_reset();
	xmitmp[XM_SEL].val = 0x12b0;
	run_xmitmp("67 XMITMP, the selector's low byte alone", 6707);

	/*
	 * THE READER, three of its arms, through the arm's one call.  The
	 * refill run folds the CRC as well, so the record moves in five
	 * fields rather than one; the restart is the arm that does NOT inline
	 * in the object (0x6484c is a real `call getbit`); and the exhausted
	 * one returns -1, which is the only input that puts a value in `f25c8`
	 * no successful pass produces -- every bit of it set, and the mapper
	 * then reads a negative halfword.
	 */
	xm_reset();
	xmitmp[XM_AVAIL].val = 0;
	xmitmp[XM_CRCON].val = 1;
	run_xmitmp("67 XMITMP, the reader refills", 6708);
	xm_reset();
	xmitmp[XM_AVAIL].val = 0;
	xmitmp[XM_POS].val = 0x0040;
	run_xmitmp("67 XMITMP, the reader restarts", 6709);
	xm_reset();
	xmitmp[XM_AVAIL].val = 0;
	xmitmp[XM_POS].val = 0x0040;
	xmitmp[XM_REPEAT].val = 0;
	run_xmitmp("67 XMITMP, the reader is exhausted", 6710);
	xm_reset();
	xmitmp[XM_SEL].val = 0x89b0;
	xmitmp[XM_AVAIL].val = 0;
	xmitmp[XM_POS].val = 0x0040;
	xmitmp[XM_REPEAT].val = 0;
	run_xmitmp("67 XMITMP, sixteen points, the reader is exhausted", 6711);

	/*
	 * THE STUFF POINT, AND ITS TWO CONSTANTS ARE NOT ONE CONSTANT.  Bit 5
	 * of the flags word picks 0x55 or 0xbb, and the two do DIFFERENT
	 * things -- three bits pushed onto the reader or one (0x645a4 against
	 * 0x64750).  The index is started two short of the point so the
	 * checkpoint fires on the LAST bit of the pass and nothing else moves.
	 */
	xm_reset();
	xmitmp[XM_IDX].val = 0xb9;
	run_xmitmp("67 XMITMP, the stuff point at 0xbb", 6712);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0161;		/* bit 5 set */
	xmitmp[XM_IDX].val = 0x53;
	run_xmitmp("67 XMITMP, the stuff point at 0x55", 6713);
	/*
	 * And the two crossed, which is what says the pair is selected rather
	 * than tested together: neither of these stuffs anything.
	 */
	xm_reset();
	xmitmp[XM_IDX].val = 0x53;
	run_xmitmp("67 XMITMP, 0x55 with bit 5 clear", 6714);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0161;
	xmitmp[XM_IDX].val = 0xb9;
	run_xmitmp("67 XMITMP, 0xbb with bit 5 set", 6715);

	/*
	 * +0x3590, the third checkpoint: one bit pushed and the point moved on
	 * by 0x11.  The second run puts the stuff point AND +0x3590 on the
	 * same index, which is the run that says 0x6459b jumps past the
	 * +0x3590 test -- a reconstruction testing all three independently
	 * advances +0x3590 here and the object does not.
	 */
	xm_reset();
	xmitmp[XM_3590].val = 0x12;
	run_xmitmp("67 XMITMP, the stuff point at +0x3590", 6716);
	xm_reset();
	xmitmp[XM_IDX].val = 0xb9;
	xmitmp[XM_3590].val = 0xbb;
	run_xmitmp("67 XMITMP, +0x3590 on the stuff point itself", 6717);
	/*
	 * AND ONE WITH +0x3590 ALREADY BEHIND THE INDEX, which is the only run
	 * that tells the object's `==` from a `>=`.  Every other run here has
	 * the stuff point ahead of `vect_idx`, so the two answer alike.
	 */
	xm_reset();
	xmitmp[XM_3590].val = 0x0f;
	run_xmitmp("67 XMITMP, +0x3590 already behind the index", 6736);

	/*
	 * THE STUFF'S PLACE IN THE PASS, and it needs a reader that is about to
	 * run out.  Pushing zeros onto the END of the accumulator does not
	 * change the bits already in it, so with eight bits in hand a stuff one
	 * pass early is invisible -- the next bit is the same bit either way.
	 * With ONE bit in hand it is not: the object refills on the next pass
	 * and a reconstruction that stuffed early has three bits and does not.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0161;
	xmitmp[XM_IDX].val = 0x53;
	xmitmp[XM_AVAIL].val = 1;
	run_xmitmp("67 XMITMP, the stuff point with the reader nearly empty",
		   6737);

	/*
	 * THE SEQUENCE'S END, 0x645d0.  Bit 5 clear puts it at 0xbc, and the
	 * index starts one short so it fires on the FIRST bit -- the pass then
	 * collects its second bit out of the reader 0x64635 has just reloaded,
	 * which is what makes the reload's twelve stores visible in the same
	 * run that decides them.
	 *
	 * The first run has the flags word away from 0x10 in bits 3 and 4, so
	 * it reloads and does nothing else.
	 */
	xm_reset();
	xmitmp[XM_IDX].val = 0xbb;
	run_xmitmp("67 XMITMP, the sequence ends, the reload alone", 6718);
	/*
	 * With bit 5 SET the message is 0x30 bits rather than 0x90, which is
	 * the one field of the reload the flags word chooses.  The end is at
	 * 0x58 for the same reason the stuff point moved.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0161;
	xmitmp[XM_IDX].val = 0x57;
	run_xmitmp("67 XMITMP, the sequence ends, bit 5 set", 6719);

	/*
	 * The stamp at 0x647d3, which needs `flags & 0x18 == 0x10` and +0x359e
	 * past one.  Two runs for the record's own bit: clear, and +0x359e is
	 * zeroed as well as the bit raised; set, and only the bit is written --
	 * which writes nothing, so the run is what says +0x359e is NOT cleared
	 * on it.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0151;		/* & 0x18 == 0x10 */
	xmitmp[XM_IDX].val = 0xbb;
	run_xmitmp("67 XMITMP, the sequence ends, the record is stamped", 6720);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0151;
	xmitmp[XM_IDX].val = 0xbb;
	xmitmp[XM_W0].val = 0x2469;		/* bit 0 already up */
	run_xmitmp("67 XMITMP, the sequence ends, already stamped", 6721);
	/*
	 * And the boundary at 0x647cd, which is `> 1` and signed: +0x359e 0
	 * counts to one and takes 0x649d9, which reloads and stamps nothing;
	 * +0x359e 1 counts to two and does.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0151;
	xmitmp[XM_IDX].val = 0xbb;
	xmitmp[XM_359E].val = 0;
	run_xmitmp("67 XMITMP, the sequence ends, the first sequence", 6722);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0151;
	xmitmp[XM_IDX].val = 0xbb;
	xmitmp[XM_359E].val = 1;
	run_xmitmp("67 XMITMP, the sequence ends, +0x359e at one", 6723);

	/*
	 * THE EXIT AT 0x648bf, which is the only path that leaves the bit loop
	 * early.  It needs bit 0 at obj+0xaa3c, `flags & 0x90 == 0x90` and a
	 * fourth sequence; it moves the transmit machine to 69 EXMIT, clears
	 * `vect_idx` and drops bit 5 of the flags word -- which is why bit 5
	 * is SET on every run here.  The index starts two short of 0x58 so the
	 * pass collects both its bits and then leaves.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;		/* 0x90, bit 5, & 0x18 = 0x10 */
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;		/* bit 0 up */
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, the sequence ends, initdigital already done",
		   6724);
	/*
	 * AND THE SAME EXIT ON THE FIRST BIT OF THE PASS, which is the run that
	 * says it LEAVES the loop.  On 6724 the exit falls on the last bit, so
	 * a reconstruction that carried on would end the pass at the same place
	 * anyway; here the object stops with one bit in `f25c8` and one that
	 * carried on would collect a second out of the reader and advance
	 * `vect_idx` past the zero the exit just wrote.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x57;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, the exit on the first bit of the pass", 6738);
	/*
	 * THE RUN WITH +0x3598 CLEAR -- the one that calls `initdigital` --
	 * IS NOT HERE, AND THE REASON IS THE FIXTURE'S AND NOT THE ARM'S.
	 *
	 * It was written, run, and taken out again.  `initdigital` reaches
	 * `initV34` twice and each call stores its `coeff` argument at
	 * shell +0x24 -- and that argument is `obj + 0xe84` for the receive
	 * context and `obj + 0x2a68` for the transmit one, so what lands at
	 * +0x0a24 and +0x2604 is a pointer INTO EACH SIDE'S OWN OBJECT.  The
	 * two sides then hold two addresses of the same offset, which is
	 * exactly the class `holes[]` exists for and neither offset is in it,
	 * so `v34hs_compare`'s byte sweep reports them and the step signature
	 * differs with them.
	 *
	 * MEASURED, because it is worth having and it is not a claim this file
	 * makes: those were the only two bytes that differed OF THE BYTES THAT
	 * WERE COMPARED -- every shell field and the rate configuration both
	 * `initV34` calls wrote agreed.  The four pointer holes at +0x0a28,
	 * +0x0e48, +0x2608 and +0x2a28 were SKIPPED and not agreed: `initV34`
	 * aims those at library tables, so side A holds ours and side B the
	 * blob's, and neither the ordinary run nor `V34HS_REFINIT` can settle
	 * two addresses of two copies (findings 324 and 359).  That hazard is
	 * real here and is named rather than measured; it is just not what
	 * made the run fail.  The run is recorded rather than committed
	 * because closing it means adding two entries to a list three other
	 * tests assert every entry of is exercised, and they do not reach
	 * `initdigital`.
	 *
	 * WHAT IS THEREFORE UNTESTED IS TWO LINES, and finding 424 names
	 * them: the call itself and the store of one into +0x3598.  Both are
	 * in the suite as mutations and both go uncaught, which is finding
	 * 343's way of making a gap concrete rather than leaving it silent.
	 * 6724 still covers the guard, because it is the path where +0x3598
	 * is already set.
	 */

	/*
	 * The three ways of NOT taking that exit, one guard at a time.  Each
	 * has the other two guards satisfied, or a mutation of the one being
	 * tested falls through to the same answer as its neighbour.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0171;		/* 0x90 -> 0x10 */
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, the sequence ends, the flags word lacks 0x80",
		   6726);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 2;		/* counts to three */
	run_xmitmp("67 XMITMP, the sequence ends, only three sequences", 6727);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 3;		/* counts to four */
	run_xmitmp("67 XMITMP, the sequence ends, the fourth sequence", 6728);
	/*
	 * 0x80 without 0x10, which is the other half of the mask: every run
	 * above that has one has both, so a reconstruction testing either bit
	 * alone answers the same on all of them.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01c1;
	xmitmp[XM_IDX].val = 0xbb;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, the sequence ends, the flags word lacks 0x10",
		   6733);
	/*
	 * And bits 3 AND 4, which is the run that says 0x647b5's compare is
	 * `== 0x10` and not "bit 4 is set": the stamp does NOT happen here
	 * where it does on 6720, and every other run leaves bit 3 clear.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x0159;
	xmitmp[XM_IDX].val = 0xbb;
	run_xmitmp("67 XMITMP, the sequence ends, bits 3 and 4 both set", 6732);
	/*
	 * THE OBJECT HAS TWO COPIES OF THAT COMPARE, 0x64614 and 0x647b5, one
	 * per side of the +0xaa3c test, and 6732 reaches only the second.  This
	 * is the same run through the first: +0xaa3c set, bits 3 and 4 set, and
	 * three sequences so the exit above is not taken.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f9;
	xmitmp[XM_IDX].val = 0x57;		/* bit 5 set: the end is 0x58 */
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_359E].val = 2;		/* counts to three            */
	run_xmitmp("67 XMITMP, the sequence ends, bits 3 and 4 at 0x64614",
		   6734);

	/*
	 * AND THE TWO RUNS THAT SEPARATE obj+0xaa3c FROM THE RECORD'S OWN
	 * FIRST HALFWORD.  Everything else is 6724's, and the two bits are
	 * driven opposite ways: the object exits on the first and reloads on
	 * the second, and a reconstruction reading the record for 0x645de --
	 * or the object for 0x647db -- gets both the wrong way round.  There
	 * is no state a fill or a handshake produces in which they differ,
	 * because `getMPrecvdBits` aims the pointer at the field; a poke is
	 * the only way here.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5a;		/* clear */
	xmitmp[XM_W0].val = 0x2469;		/* and the record's is set */
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, +0xaa3c clear where the record's word is set",
		   6729);
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;		/* set */
	xmitmp[XM_W0].val = 0x2468;		/* and the record's is clear */
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, +0xaa3c set where the record's word is clear",
		   6730);

	/*
	 * THE RECORD AT +0xaa3c IS A CONTROL AND NOT A BEHAVIOUR.  It is the
	 * one configuration the object's own writer produces, and there the
	 * two halfwords above are the same two bytes -- so this run must agree
	 * with 6730 and cannot fail while 6729 and 6730 pass.  It is here
	 * because a reconstruction that had them the wrong way round would
	 * still pass it, which is the point of saying so.
	 */
	xm_reset();
	xmitmp[XM_FLAGS].val = 0x01f1;
	xmitmp[XM_IDX].val = 0x56;
	xmitmp[XM_AA3C].val = 0x5a5b;
	xmitmp[XM_BASE].val = MP_ALT;
	xmitmp[XM_359E].val = 4;
	run_xmitmp("67 XMITMP, the record where the object puts it", 6731);
}

/*
 * 67's entry, read out of the blob's own `.rodata` the way the other two
 * shared-entry checks are.  It is its own target -- nothing else in table 1
 * shares it -- and 69's, which it hands over to, is a different one.
 */
static void
case_xmitmp_entry(void)
{
	const char *const *t1 = (const char *const *)
				(rodata_2c00 + (0x2da0 - 0x2c00));
	const char *base = (const char *)ref_v34handshak;

	diff_eq_int("table 1: 67's entry is v34handshak + 0x10ab",
		    (int)(t1[67 - 5] - base), 0x6399b - 0x628f0, 6790);
	diff_eq_int("table 1: txstate 67 has an entry of its own",
		    t1[67 - 5] != t1[69 - 5] && t1[67 - 5] != t1[64 - 5], 1,
		    6791);
}

/* --- 24 TX_DPSK ----------------------------------------------------------- */

/*
 * ONE BIT OF A MESSAGE PER PASS AND FOUR WAYS OUT, and only TWO of the four
 * reach `txmit`.  The other two end the message and move the transmit machine
 * instead, so they are driven through `run_case_ex(..., 0)` and guarded by
 * the state rather than by the queue -- see the comment on `run_case_ex` for
 * why that is the same claim and not a weaker one.
 *
 * WHERE THE READER LIVES IS A POKE, as it is for 67.  The fixture aims
 * +0xaa6c at a block OUTSIDE the object, so left alone the reader's own
 * advance would be invisible to the "wrote something" guard and to finding
 * 323's byte count -- which is half of why 24 and 60 agree cold.  These runs
 * put the record INSIDE the object, at +0xaa0c, which is one whole 0x30-byte
 * slot of the five-record array (v34hshak.c's mode 2/3 blanks the same one)
 * and does not overlap +0xaa78, +0xaa7e or the pointer itself.
 *
 * Every field the arm writes is seeded away from what it stores (finding
 * 345): `vect_idx` non-zero against the hold's clear, +0xaa78 non-zero
 * against it too, +0x25d0 to a word that is no `vect4` entry, +0xabe2,
 * +0xabe4 and +0xabe6 away from one, +0xabf8 non-zero against the one-shot's
 * clear, the reader's own fields away from what a refill leaves, and the
 * record at +0xa94c away from all twelve of `tx1_moh_send`'s values through
 * `A94C_SEED`.
 *
 * AND THE TWO BYTES AFTER THE THREE `cmpb` SITES ARE SEEDED, for the reason
 * 74's +0xabe9 is: a halfword reading of a byte field must be distinguishable
 * from a byte one, and left to the fill that is a coin toss.
 */
#define TX1_FABE2	0xabe2		/* short: the clear-down's stamp    */
#define TX1_FABE4	0xabe4		/* short: raised by the clear-down  */
#define TX1_FABE6	0xabe6		/* short: raised by the retrain     */
#define TX1_MOHMSG	0xabf0		/* int: moh_message                 */
#define TX1_MOHRCV	0xabf4		/* int: moh_recvd                   */
/* +0xabf8 and +0xabf9 are at the head of this file; `run_case_ex` reads one. */

/*
 * TWO OF THE FIVE 0x30-BYTE MESSAGE RECORDS, chosen because nothing else in
 * this tree names either.  The array runs +0xa94c, +0xa97c, +0xa9ac, +0xa9dc,
 * +0xaa0c and +0xaa3c; `v34handshakinit` aims +0xaa70 at +0xa97c and +0xaa6c
 * at +0xa94c, `getMPrecvdBits` uses +0xaa3c and `settxlevel` reads +0xa9dc as
 * a transmit-level table (v34hshak.c's `v34setuptxmit`), which leaves +0xa9ac
 * and +0xaa0c.
 */
#define DP_REC		0xaa0c		/* where these runs put the reader  */
#define DP_ALT		0xa9ac		/* and somewhere else to put it     */

/* The reader's nine words after word[0]; word[0] itself is varied. */
#define DP_WORD_SEED(B)							\
	P16((B) + 0x02, 0x1357), P16((B) + 0x04, 0x2466),		\
	P16((B) + 0x06, 0x3575), P16((B) + 0x08, 0x4684),		\
	P16((B) + 0x0a, 0x5793), P16((B) + 0x0c, 0x68a2),		\
	P16((B) + 0x0e, 0x79b1), P16((B) + 0x10, 0x8ac0),		\
	P16((B) + 0x12, 0x9bcf)

#define DP_E8		0		/* +0xabe8, the Modem-on-Hold flag  */
#define DP_F8		1		/* +0xabf8, the one-shot            */
#define DP_F9		2		/* +0xabf9                          */
#define DP_MSG		3		/* moh_message                      */
#define DP_RCV		4		/* moh_recvd                        */
#define DP_IDX		5		/* vect_idx                         */
#define DP_RTD		6		/* rtd, which sets the threshold    */
#define DP_358C		7		/* the tone's flag                  */
#define DP_ACC		8		/* the reader's accumulator         */
#define DP_AVAIL	9		/* and how much of it is left       */
#define DP_NBITS	10
#define DP_POS		11
#define DP_CRCON	12
#define DP_REPEAT	13
#define DP_ACC0		14
#define DP_AVAIL0	15
#define DP_BASE		16		/* where +0xaa6c is aimed           */
#define DP_E2		17		/* +0xabe2                          */
#define DP_W0		18		/* the reader's word[0]             */

static struct tx1_poke dpsk[] = {
	P8(TX1_FABE8, 0),			/* DP_E8     */
	P8(TX1_FABF8, 0x5a),			/* DP_F8     */
	P8(TX1_FABF9, 0),			/* DP_F9     */
	P32(TX1_MOHMSG, 7),			/* DP_MSG    */
	P32(TX1_MOHRCV, 9),			/* DP_RCV    */
	P16(TX1_VECTIDX, 0x0040),		/* DP_IDX    */
	P16(TX1_RTD, 0x0100),			/* DP_RTD    */
	P16(TX1_F358C, 0x1234),			/* DP_358C   */
	P32(DP_REC + 0x24, 0x000000b0),		/* DP_ACC    */
	P16(DP_REC + 0x28, 8),			/* DP_AVAIL  */
	P16(DP_REC + 0x18, 0x0040),		/* DP_NBITS  */
	P16(DP_REC + 0x1a, 0x0008),		/* DP_POS    */
	P16(DP_REC + 0x16, 0),			/* DP_CRCON  */
	P16(DP_REC + 0x20, 0),			/* DP_REPEAT */
	P32(DP_REC + 0x2c, 0x00000030),		/* DP_ACC0   */
	P16(DP_REC + 0x2a, 8),			/* DP_AVAIL0 */
	PSELF(TX1_PTR_AA6C, DP_REC),		/* DP_BASE   */
	P16(TX1_FABE2, 0x0f0f),			/* DP_E2     */
	P16(DP_REC + 0x00, 0x2468),		/* DP_W0     */

	/* The reader's fields no run varies, at both bases. */
	P16(DP_REC + 0x14, 0x0123),	/* crc                              */
	P16(DP_REC + 0x1c, 0x0008),	/* wordbits                         */
	/*
	 * `idx` IS SEEDED AWAY FROM ZERO and it is the re-arm's only visible
	 * clear: with it at zero the hand re-arm's `b->idx = 0` writes the
	 * value already there and 2413's refill reads `word[0]` either way.
	 */
	P16(DP_REC + 0x1e, 0x0002),	/* idx                              */
	P16(DP_REC + 0x22, 0x0044),	/* repeats                          */
	DP_WORD_SEED(DP_REC),
	/*
	 * The alternate record is EXHAUSTED where the main one is varied: the
	 * one run that uses it is 2431, which needs the message to be over.
	 * `word[0]` is away from 0xbb, which is the byte
	 * `VPcmV34SetMohMessageBits` writes for `moh_message` three.
	 */
	P16(DP_ALT + 0x00, 0x1a2b), DP_WORD_SEED(DP_ALT),
	P16(DP_ALT + 0x14, 0x0123), P16(DP_ALT + 0x16, 0),
	P16(DP_ALT + 0x18, 0x0040), P16(DP_ALT + 0x1a, 0x0040),
	P16(DP_ALT + 0x1c, 0x0008), P16(DP_ALT + 0x1e, 0x0000),
	P16(DP_ALT + 0x20, 0), P16(DP_ALT + 0x22, 0x0044),
	P32(DP_ALT + 0x24, 0x000000b0), P16(DP_ALT + 0x28, 0),
	P16(DP_ALT + 0x2a, 8), P32(DP_ALT + 0x2c, 0x00000030),

	/* What the arm writes, all away from what it stores. */
	A94C_SEED,
	P16(TX1_FABE4, 0x0e0e), P16(TX1_FABE6, 0x0d0d),
	P16(TX1_COUNT, 0x0777), P32(TX1_F25D0, 0x11223344),
	P8(0xabe9, 0x5a), P8(0xabfa, 0x5a)
};

static void
dp_reset(void)
{
	dpsk[DP_E8].val = 0;
	dpsk[DP_F8].val = 0x5a;
	dpsk[DP_F9].val = 0;
	dpsk[DP_MSG].val = 7;
	dpsk[DP_RCV].val = 9;
	dpsk[DP_IDX].val = 0x0040;
	dpsk[DP_RTD].val = 0x0100;
	dpsk[DP_358C].val = 0x1234;
	dpsk[DP_ACC].val = 0x000000b0;
	dpsk[DP_AVAIL].val = 8;
	dpsk[DP_NBITS].val = 0x0040;
	dpsk[DP_POS].val = 0x0008;
	dpsk[DP_CRCON].val = 0;
	dpsk[DP_REPEAT].val = 0;
	dpsk[DP_ACC0].val = 0x00000030;
	dpsk[DP_AVAIL0].val = 8;
	dpsk[DP_BASE].val = DP_REC;
	dpsk[DP_E2].val = 0x0f0f;
	dpsk[DP_W0].val = 0x2468;
}

/* The message is over: `avail` gone, `pos` at `nbits`, no CRC and no repeat. */
static void
dp_exhaust(void)
{
	dpsk[DP_AVAIL].val = 0;
	dpsk[DP_POS].val = 0x0040;
	dpsk[DP_NBITS].val = 0x0040;
	dpsk[DP_CRCON].val = 0;
	dpsk[DP_REPEAT].val = 0;
}

static void
run_dpsk(const char *what, long tag)
{
	run_case(V34HS_TX_DPSK, v34tx1_tx_dpsk, V34TX1_LOOP, what, tag,
		 dpsk, NP(dpsk));
}

/* The paths that do not transmit; the state is the guard instead. */
static void
run_dpsk_nq(const char *what, long tag)
{
	run_case_ex(V34HS_TX_DPSK, v34tx1_tx_dpsk, V34TX1_LOOP, what, tag,
		    dpsk, NP(dpsk), TX1_GUARD_STATE);
}

/* And the one that moves neither: the one-shot is the guard. */
static void
run_dpsk_1s(const char *what, long tag)
{
	run_case_ex(V34HS_TX_DPSK, v34tx1_tx_dpsk, V34TX1_LOOP, what, tag,
		    dpsk, NP(dpsk), TX1_GUARD_ONESHOT);
}

/*
 * FINDING 323'S ONE COLLISION, MEASURED HERE RATHER THAN QUOTED, and it is
 * AGREEMENT ON ONE PATH and not identity.
 *
 * Cold, 24 and 60 write the same 69 bytes with the same signature.  The
 * reason has three parts and each is asserted: the fill leaves the
 * Modem-on-Hold flag clear, so there is no `vect_idx` tick; the reader hands
 * back a ZERO bit, so the store into +0x358c writes the value already there
 * and selects the entry 60 selects; and the message is NOT over, so the arm
 * does not move the transmit machine.  The last check runs both arms from the
 * identical cold object -- 60's reads no state word, so it can be dispatched
 * at txstate 24 -- and requires the two objects to agree byte for byte.
 *
 * `case_tx_dpsk_entry` is the other half: two entries at two addresses.  A
 * later blob that folded them together, or a fill that moved any of the three
 * conditions, is a failure here rather than a silence.
 */
static void
case_dpsk_cold(void)
{
	const unsigned char *o;
	short f358c;
	int point;

	apply(V34HS_TX_DPSK, NULL, 0);
	o = (const unsigned char *)v34hs_object(0);
	diff_eq_int("24 TX_DPSK cold: the fill leaves +0xabe8 clear",
		    o[TX1_FABE8], 0, 2490);
	memcpy(&f358c, o + TX1_F358C, sizeof(f358c));

	(void)v34tx1_tx_dpsk(v34hs_object(0));
	diff_eq_int("24 TX_DPSK cold: the message is not over",
		    v34hs_peek_short(0, V34HS_TXSTATE), V34HS_TX_DPSK, 2491);
	diff_eq_int("24 TX_DPSK cold: +0x358c is unchanged, so the bit was zero",
		    v34hs_peek_short(0, TX1_F358C), f358c, 2492);
	memcpy(&point, o + TX1_F25D0, sizeof(point));
	diff_eq_int("24 TX_DPSK cold: the tone is 60 TONE_AB's",
		    point, vect4[2 * (f358c & 1)], 2493);
	memcpy(before, o, sizeof(before));

	apply(V34HS_TX_DPSK, NULL, 0);
	(void)v34tx1_tone_ab(v34hs_object(0));
	diff_eq_obj("24 TX_DPSK and 60 TONE_AB leave the same object cold",
		    struct v34_object, (const struct v34_object *)before,
		    (const struct v34_object *)v34hs_object(0), 2494);

	/* And the same thing against the blob, which is the real check. */
	run_case(V34HS_TX_DPSK, v34tx1_tx_dpsk, V34TX1_LOOP,
		 "24 TX_DPSK, cold, no pokes at all", 2495, NULL, 0);
}

/*
 * 24's entry, read out of the blob's own `.rodata` the way the other
 * shared-entry checks are.  It is its own target, and 60's -- the arm it
 * agrees with cold -- is a different one.
 */
static void
case_tx_dpsk_entry(void)
{
	const char *const *t1 = (const char *const *)
				(rodata_2c00 + (0x2da0 - 0x2c00));
	const char *base = (const char *)ref_v34handshak;

	diff_eq_int("table 1: 24's entry is v34handshak + 0x22a6",
		    (int)(t1[24 - 5] - base), 0x62b96 - 0x628f0, 2480);
	diff_eq_int("table 1: 60's entry is v34handshak + 0x244d",
		    (int)(t1[60 - 5] - base), 0x62d3d - 0x628f0, 2481);
	diff_eq_int("table 1: 24 and 60 are two entries, not one",
		    t1[24 - 5] != t1[60 - 5], 1, 2482);
}

static void
case_tx_dpsk(void)
{
	/*
	 * THE TONE, and the XOR is an XOR.  The reader's eight bits of 0xb0
	 * come out 1, 0, 1, 1, so the first run takes a ONE and the second --
	 * eight bits of 0x30 -- takes a ZERO.  The third pairs the one bit
	 * with an ODD +0x358c, which sends the same point the zero-bit run
	 * sends from the opposite pair: a reconstruction in which the bit
	 * SELECTS the tone rather than toggling it passes exactly one of them.
	 */
	dp_reset();
	run_dpsk("24 TX_DPSK, a one bit and an even flag", 2400);
	dp_reset();
	dpsk[DP_ACC].val = 0x00000030;
	run_dpsk("24 TX_DPSK, a zero bit and an even flag", 2401);
	dp_reset();
	dpsk[DP_358C].val = 0x1235;
	run_dpsk("24 TX_DPSK, a one bit and an odd flag", 2402);

	/*
	 * THE MODEM-ON-HOLD CLOCK AT THE ENTRY.  +0xabe8 set adds one tick of
	 * `vect_idx` before anything else, and this run is otherwise 2400: the
	 * two differ in that one halfword and in nothing else.
	 */
	dp_reset();
	dpsk[DP_E8].val = 1;
	run_dpsk("24 TX_DPSK, the Modem-on-Hold clock ticks", 2403);

	/*
	 * THE READER, three of its arms through the arm's first call.  The
	 * refill folds the CRC as well, so the record moves in five fields
	 * rather than one; the restart is the arm that does NOT inline in the
	 * object (0x684bb is a real `call getbit`).  Exhaustion is 2410 below,
	 * because it is also the arm that ends the message.
	 */
	dp_reset();
	dpsk[DP_AVAIL].val = 0;
	dpsk[DP_POS].val = 0;
	dpsk[DP_CRCON].val = 1;
	run_dpsk("24 TX_DPSK, the reader refills", 2404);
	dp_reset();
	dp_exhaust();
	dpsk[DP_REPEAT].val = 1;
	run_dpsk("24 TX_DPSK, the reader restarts", 2405);

	/*
	 * THE MESSAGE IS OVER AND THE MODEM IS NOT ON HOLD: the transmit
	 * machine goes to 60 TONE_AB and the arm does nothing else at all --
	 * no tone, no `txmit`.  This is the first of the two runs the queue
	 * guard cannot cover.
	 */
	dp_reset();
	dp_exhaust();
	run_dpsk_nq("24 TX_DPSK, the message is over", 2410);

	/*
	 * ON HOLD WITH THE ONE-SHOT DOWN, 0x67c4e: the reader is re-armed by
	 * hand -- `getbit`'s restart arm with the `repeat` test taken out --
	 * and read again, and the bit that comes out is sent like any other.
	 *
	 * Three runs for the three shapes the re-armed reader can take, and
	 * the third is the one that says the re-arm CLEARS `pos`: the outer
	 * read was exhausted because `pos` had reached `nbits`, so a
	 * reconstruction that left `pos` alone is exhausted again and sends
	 * -1 where the object refills and sends a real bit.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	run_dpsk("24 TX_DPSK, on hold, the reader is re-armed", 2411);
	/*
	 * TWO MORE OF THE SAME PATH THAT EXIST FOR THE THRESHOLD ALONE, and
	 * both leave at 0x6431f like 2411 does.  2419's `vect_idx` is 512 --
	 * short of 1216 and past a threshold of 0x4b, which is the only way to
	 * tell the constant's three digits from its two.  2409's is -1 after
	 * the entry tick, which is the only way to tell 0x64ffa's `movswl`
	 * from a `movzwl`: unsigned it is 65,535 and the hold would be over.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0x0200;
	run_dpsk("24 TX_DPSK, on hold, halfway to the threshold", 2419);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = -2;			/* -1 after the entry tick */
	run_dpsk("24 TX_DPSK, on hold, the clock is negative", 2409);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_NBITS].val = 0;
	dpsk[DP_POS].val = 0;
	dpsk[DP_AVAIL0].val = 0;
	run_dpsk("24 TX_DPSK, on hold, the re-armed reader is empty too", 2412);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_AVAIL0].val = 0;
	run_dpsk("24 TX_DPSK, on hold, the re-armed reader refills", 2413);

	/*
	 * THE HOLD TAIL, 0x64fec, reached through that same path so that the
	 * tone and the `txmit` keep the queue guard.  Its first test is a
	 * TIME: `vect_idx` against `(rtd >> 4) + 1200`.  2411 is below it and
	 * these four are at or above.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0x0500;		/* 1280, past 1216 */
	dpsk[DP_F9].val = 0x33;
	run_dpsk("24 TX_DPSK, the hold is over, +0xabf9 up", 2414);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0x0500;
	dpsk[DP_MSG].val = 2;
	run_dpsk("24 TX_DPSK, the hold is over, clearing down", 2415);
	/*
	 * moh_message 3 takes the same way out as 2, which is what says
	 * 0x65025's `sub $2 ; cmp $1 ; ja` is a RANGE and not `== 2`.  It is
	 * an independent check of the compare and the SAME check of the
	 * clear-down that 2415 makes.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0x0500;
	dpsk[DP_MSG].val = 3;
	run_dpsk("24 TX_DPSK, the hold is over, message three", 2416);
	/*
	 * AND A NEGATIVE ROUND-TRIP DELAY, which is the only run that tells
	 * 0x65001's arithmetic `sar $0x4` from a logical one: with `rtd`
	 * positive the two agree on every value, and with it at -20000 the
	 * threshold is -50 where a `shr` reading puts it past 268 million.
	 * `vect_idx` is zero here, so the two answers are opposite.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0;
	dpsk[DP_RTD].val = -20000;
	dpsk[DP_MSG].val = 2;
	run_dpsk("24 TX_DPSK, the hold is over, a negative round trip", 2417);
	/*
	 * The third way out of the tail: neither +0xabf9 nor a message in
	 * 2..3, so the handshake is restarted.  `v34handshakinit(obj, 1)`
	 * runs INSIDE the step, which is finding 359's case -- and it also
	 * re-arms the loop that is dispatching this arm, setting +0x2aa0 to
	 * six and clearing `vect_idx`.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_F8].val = 0;
	dpsk[DP_IDX].val = 0x0500;
	dpsk[DP_MSG].val = 7;
	run_dpsk("24 TX_DPSK, the hold is over, the handshake restarts", 2418);

	/*
	 * ON HOLD WITH THE ONE-SHOT UP, 0x64e91: the message dispatch.  None
	 * of these transmits, so all of them are guarded by the state -- and
	 * every one of them is arranged so the state DOES move, which for the
	 * three arms that write nothing of their own means the tail has to.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 1;
	run_dpsk_nq("24 TX_DPSK, on hold, a message to send", 2420);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 2;
	run_dpsk_nq("24 TX_DPSK, on hold, clearing down", 2421);
	/*
	 * moh_message 3 is 0x64eb6's compare under a different index and the
	 * SAME clear-down; it is here because 0x64ea6's `cmp $3 ; ja` is what
	 * separates 2 and 3 from 4, and 2422 with 2423 is that boundary.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 3;
	run_dpsk_nq("24 TX_DPSK, on hold, message three", 2422);
	/*
	 * moh_message FOUR DOES NOTHING AT ALL, and the run has to prove a
	 * negative -- which is the one thing neither of the other two guards
	 * can carry.  The arm here writes ONE byte, the one-shot at +0xabf8,
	 * and the hold tail leaves at 0x6431f, so the transmit machine does
	 * not move and there is no queue to advance; `TX1_GUARD_ONESHOT` is
	 * what says the arm ran.  It is not a weaker check of the DISPATCH: a
	 * reconstruction that cleared down here leaves the transmit machine at
	 * MOH_CLEARDOWN, side A's `ref_v34handshak` then sends four silent
	 * samples where side B sends a tone, and the comparison says so.
	 *
	 * PUTTING THE TAIL PAST THE THRESHOLD WOULD HIDE IT, which is why this
	 * run does not: every way the tail can move the machine leaves exactly
	 * what a spurious clear-down leaves -- the retrain's own
	 * `v34handshakinit` clears +0xabe4 (v34hshak.c:1307) and rewrites both
	 * state words, and the tail's clear-down IS the clear-down.
	 *
	 * 2433 is the same shape with a NEGATIVE moh_message, which is the only
	 * value that separates 0x64ea6's unsigned `ja` from a signed compare.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 4;
	run_dpsk_1s("24 TX_DPSK, on hold, message four does nothing", 2423);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = -1;
	run_dpsk_1s("24 TX_DPSK, on hold, a negative message does nothing", 2433);
	/*
	 * AND THE SAME moh_message PAST THE THRESHOLD, which is a check of the
	 * TAIL and not of the dispatch: four is one past 0x65025's range, so it
	 * restarts the handshake where two and three clear down.  2423 cannot
	 * make that distinction and this run cannot make 2423's.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 4;
	dpsk[DP_IDX].val = 0x0500;
	run_dpsk_nq("24 TX_DPSK, the hold is over, message four restarts", 2434);

	/*
	 * moh_message zero hands over to `moh_recvd`, and its two "wait"
	 * values reach the same block through TWO SEPARATE COMPARES -- 0x6902d
	 * and 0x6903b -- so neither run stands in for the other.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 0;
	run_dpsk_nq("24 TX_DPSK, on hold, nothing received yet", 2424);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 4;
	run_dpsk_nq("24 TX_DPSK, on hold, an acknowledgement received", 2425);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 1;
	run_dpsk_nq("24 TX_DPSK, on hold, a message to answer", 2426);
	/*
	 * AND 0x6a3c6'S `cmp $5` IS EXACT: six takes the restart, five does
	 * not.  Without this run "anything above four" builds the message.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 6;
	run_dpsk_nq("24 TX_DPSK, on hold, an unknown message received", 2427);

	/*
	 * THE MESSAGE IS BUILT, 0x6a3cf.  +0xabf9 picks which one -- and, one
	 * block later, which way the tail goes out -- so the two runs differ
	 * in more than the message and the object couples them that way.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 5;
	dpsk[DP_F9].val = 0x33;
	dpsk[DP_IDX].val = 0x0500;
	run_dpsk_nq("24 TX_DPSK, on hold, a refusal received", 2428);
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 5;
	dpsk[DP_IDX].val = 0x0500;
	run_dpsk_nq("24 TX_DPSK, on hold, a refusal, +0xabf9 down", 2429);
	/*
	 * +0xabe2 ALREADY AT THREE, which is the one guard in that block: the
	 * stamp is NOT written, where every other run here writes one over the
	 * seed.  IT IS 2429'S RUN AND NOT 2428'S, and that is not a detail:
	 * with +0xabf9 up the HOLD TAIL stamps +0xabe2 with one four blocks
	 * later, and against that a message block that stamped
	 * unconditionally writes nothing of its own.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 5;
	dpsk[DP_IDX].val = 0x0500;
	dpsk[DP_E2].val = 3;
	run_dpsk_nq("24 TX_DPSK, on hold, a refusal already refused", 2430);
	/*
	 * AND THE RUN THAT SEPARATES THE OLD READER FROM THE NEW ONE.
	 * 0x6a42e reads +0xaa6c BEFORE the call and 0x6a448 writes it after,
	 * so `VPcmV34SetMohMessageBits` fills `word[0]` of the record the
	 * reader was on and the twelve stores re-arm +0xa94c.  In the object's
	 * own configuration those are one record (v34hshak.c:1451) and no fill
	 * or handshake can tell them apart; a poke is the only way.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 0;
	dpsk[DP_RCV].val = 5;
	dpsk[DP_F9].val = 0x33;
	dpsk[DP_IDX].val = 0x0500;
	dpsk[DP_BASE].val = DP_ALT;
	run_dpsk_nq("24 TX_DPSK, on hold, the message goes to the old reader",
		    2431);

	/*
	 * THE ENTRY TICK DECIDING A BRANCH.  Everywhere above, +0xabe8's
	 * `vect_idx += 1` shows only as one halfword; here `vect_idx` starts
	 * exactly one below the threshold, so the tick is what takes the hold
	 * past it.  Without the tick the tail leaves at 0x6431f, the transmit
	 * machine does not move, and the run's own guard fails -- which is the
	 * point of putting it on a state-guarded run.
	 */
	dp_reset();
	dp_exhaust();
	dpsk[DP_E8].val = 1;
	dpsk[DP_MSG].val = 4;
	dpsk[DP_IDX].val = 0x04bf;		/* 1215, one below 1216 */
	dpsk[DP_F9].val = 0x33;
	run_dpsk_nq("24 TX_DPSK, the clock tick crosses the threshold", 2432);
	dp_reset();
}

/* --- 21 TRNSEG4 ----------------------------------------------------------- */

/*
 * 21's companions.  The eight that are new here are the segment length, the
 * four fields the echo-adapt start clears, the halfword the completion sets
 * to one, and the two the RECEIVE side of the rate configuration is read out
 * of -- which are NOT `TX1_RATECFG`'s baud at +0xaa84 that the arm's third
 * compare uses.  The arm reads both and they are seeded apart on purpose.
 */
#define TX1_FA244	0xa244		/* int: the segment's length       */
#define TX1_F354C	0x354c		/* int: adaptecho's call counter   */
#define TX1_F3550	0x3550		/* the LMS step                    */
#define TX1_F3552	0x3552
#define TX1_F3560	0x3560		/* int: the accumulated energy     */
#define TX1_FAA80	0xaa80		/* the completion writes one here  */
#define TX1_RXBAUD	0xaa96		/* setupreceiver's rate switch     */
#define TX1_RXCARR	0xaaa8		/* setupreceiver's carrier switch  */
#define TX1_INFOREC	0xaa0c		/* the record the completion blanks*/
#define TX1_RXF1AC	0x0410		/* receiver +0x1ac, the rate switch */
#define TX1_RXF1AE	0x0412		/* receiver +0x1ae                 */
#define TX1_RXF1B0	0x0414		/* receiver +0x1b0                 */
#define TX1_RXF1BE	0x0422		/* receiver +0x1be                 */

/*
 * THE FOUR COMPARES ARE HELD APART BY CONSTRUCTION, and that is what makes a
 * mutation of one of them fail rather than fall through to the next one's
 * answer.  With `n` the value of `f25c0` AFTER the arm's increment:
 *
 *     TRN_SPAN     0x100    so the middle point is 0x80 and the end 0x100
 *     TRN_BAUD     3200     so the period point is 4800
 *     TRN_N        0x10     which is none of the three
 *
 * The one run that does NOT hold them apart is the middle-point run, and it
 * is deliberate: `VPcmV34ReportMiddleOfEchoAdapt` only prints, so at debug
 * level 0 taking that arm and falling through it write the same object.  That
 * run therefore makes the middle point and the PERIOD point the same number,
 * so a reconstruction that dropped the middle test raises 0x400 in `f25c2`
 * and fails.  Nothing else here can see the difference.
 */
#define TRN_SPAN	0x0100
#define TRN_BAUD	3200
#define TRN_N		0x0010

enum {
	T4_F359C = 0, T4_F25CC, T4_F25C0, T4_F25C2, T4_COUNT, T4_FAA86,
	T4_A244, T4_BAUD, T4_RTD, T4_V90, T4_K56,
	T4_F354C, T4_F3550, T4_F3552, T4_F3560,
	T4_F25C6, T4_F25D6, T4_VECTIDX, T4_FAA80,
	T4_RXBAUD, T4_RXCARR,
	T4_F1AC, T4_F1AE, T4_F1B0, T4_F1BE, T4_N_POKE
};

/*
 * LITERAL INDICES AND NO COMPUTED ONES, which is finding 420's rule: a poke
 * array addressed as `NP(a) - k` moves every entry when one is inserted, and
 * three families of runs once tested nothing while the test said PASS.
 */
static struct tx1_poke trn[T4_N_POKE] = {
	P16(TX1_F359C, 0x64),			/* generator A            */
	P32(TX1_F25CC, TX1_SRSEED),
	P16(TX1_F25C0, TRN_N - 1),		/* the arm increments it  */
	P16(TX1_F25C2, (short)0x8100),		/* bit 15 set, 8 set, 2 and 10 clear */
	P16(TX1_COUNT, TRN_N + 1),		/* not the start point    */
	P16(TX1_FAA86, TRN_N + 1),		/* not reached            */
	P32(TX1_FA244, TRN_SPAN),
	P16(TX1_RATECFG, TRN_BAUD),
	P16(TX1_RTD, 8),
	P32(TX1_V90RX, 0),
	P32(TX1_K56RX, 0),

	/* What the arm writes, all seeded away from what it stores. */
	P32(TX1_F354C, 0x11223344),
	P16(TX1_F3550, 0x1234),
	P16(TX1_F3552, 0x5678),
	P32(TX1_F3560, 0x0badf00d),
	P16(TX1_F25C6, 0x4321),
	P16(TX1_F25D6, 0x1111),
	P16(TX1_VECTIDX, 0x0033),
	P16(TX1_FAA80, 0x2222),

	/*
	 * A rate and a carrier `setupreceiver` recognises, so both switches
	 * take a body rather than their (real) default -- and the rate is NOT
	 * `TX1_RATECFG`'s 3200, because the arm reads both fields and a
	 * reconstruction that took the period point off the wrong one is
	 * invisible while they agree.
	 */
	P16(TX1_RXBAUD, 2400),
	P16(TX1_RXCARR, 1800),

	/*
	 * THE FOUR TIMING CONSTANTS THE RATE SWITCH WRITES, SEEDED AWAY FROM
	 * 3200's.  The bring-up has already run `setupreceiver` once, so
	 * against a cold object all four already hold what the switch would
	 * store and the whole rate arm is invisible -- the first version of
	 * this case measured four bytes of difference between a recognised
	 * rate and an unrecognised one, all four of them the carrier POINTER,
	 * which is finding 345 exactly.
	 */
	P16(TX1_RXF1AC, 0x0111), P16(TX1_RXF1AE, 0x0222),
	P16(TX1_RXF1B0, 0x0333), P16(TX1_RXF1BE, 0x0444)
};

static void
trn_reset(void)
{
	trn[T4_F359C].val = 0x64;
	trn[T4_F25CC].val = TX1_SRSEED;
	trn[T4_F25C0].val = TRN_N - 1;
	trn[T4_F25C2].val = (short)0x8100;
	trn[T4_COUNT].val = TRN_N + 1;
	trn[T4_FAA86].val = TRN_N + 1;
	trn[T4_A244].val = TRN_SPAN;
	trn[T4_BAUD].val = TRN_BAUD;
	trn[T4_RTD].val = 8;
	trn[T4_V90].val = 0;
	trn[T4_K56].val = 0;
	trn[T4_RXBAUD].val = 2400;
	trn[T4_RXCARR].val = 1800;
}

/*
 * THE V.90 Ja TAIL NEEDS A DEMODULATOR AND THE FIXTURE HAS NONE.
 *
 * `indicateJaTransmission` with `v90_receiver` above one calls
 * `VPcmFloModem::enterPhase3`, which ends in `modem.demodulator->enterPhase3()`
 * -- and the session's demodulator pointer is one the fixture never aims, so
 * the run faults inside `V90Demodulator::enterPhase3` before any comparison
 * happens.  The stub below is the smallest thing that makes the call safe
 * WITHOUT inventing behaviour: `V90Demodulator::enterPhase3` returns at once
 * when `inPhase3` is EXACTLY one (VPcmFloModem is the only caller and the
 * object's test is `cmpl $0x1`), so a forty-byte block with one there is a
 * demodulator already in phase 3 and the call writes nothing.
 *
 * ONE STUB FOR BOTH SIDES, and that is safe here for the reason `sess_stub`
 * is: nothing writes to it.  What the call DOES write -- twenty-one constants
 * and the packer's output -- lands in the session, which is inside the arena
 * and is compared.
 *
 * The offsets are VPcmFloModem.h's and V90Demodulator.h's: the V90Modem is
 * embedded at +0x1758 and its `demodulator` at +0x04.
 */
#define SESS_DEMOD	0x175c		/* VPcmFloModem + 0x1758 + 4       */
#define DEMOD_INPHASE3	0x0034		/* V90Demodulator::inPhase3        */

static unsigned char demod_stub[0x40];

static void
aim_demod(void)
{
	int side;

	*(int *)(demod_stub + DEMOD_INPHASE3) = 1;
	for (side = 0; side < 2; side++) {
		char *o = (char *)v34hs_object(side);
		char *s;
		const void *d = demod_stub;

		memcpy(&s, o + TX1_SESSPTR, sizeof(s));
		memcpy(s + SESS_DEMOD, &d, sizeof(d));
	}
}

/*
 * THE +0xaa0c RECORD, SEEDED.  `v34handshakinit` blanks the same twelve
 * fields on its Modem-on-Hold path, so against a cold object the arm's twelve
 * stores would write the values already there and every one of them would be
 * invisible -- finding 345's failure exactly.  Fourteen halfwords cover the
 * twelve stores, two of which are 32-bit.
 */
static void
seed_inforec(void)
{
	unsigned k;

	for (k = 0x14; k < 0x30; k += 2)
		v34hs_poke_short(TX1_INFOREC + k, (short)(0x4100 + k));
	aim_demod();
}

static void
run_trn(const char *what, long tag)
{
	run_case(V34HS_TRNSEG4, v34tx1_trnseg4, V34TX1_LOOP, what, tag,
		 trn, NP(trn));
}

/*
 * 21's entry, read out of the blob's own `.rodata`.  Here the claim is
 * EXCLUSIVITY and not sharing -- finding 354a says a shared entry is never
 * evidence of one behaviour, and the same argument run backwards says an
 * entry believed to be one txstate's alone has to be measured too.  All
 * eighty-two entries are swept.
 */
static void
case_trnseg4_entry(void)
{
	const char *const *t1 = (const char *const *)
				(rodata_2c00 + (0x2da0 - 0x2c00));
	const char *base = (const char *)ref_v34handshak;
	int i, shared = 0;

	diff_eq_int("table 1: 21's entry is v34handshak + 0x1a49",
		    (int)(t1[21 - 5] - base), 0x64339 - 0x628f0, 2190);
	for (i = 0; i < 82; i++)
		if (i != 21 - 5 && t1[i] == t1[21 - 5])
			shared++;
	diff_eq_int("table 1: 21's entry is reached by no other txstate",
		    shared, 0, 2191);
}

static void
case_trnseg4(void)
{
	fixup = seed_inforec;

	/*
	 * The head is 71's and 86's, so both scrambler generators are driven:
	 * the object carries the loop twice and a reconstruction using one tap
	 * for both passes on whichever the fill happens to select.
	 */
	trn_reset();
	run_trn("21 TRNSEG4, generator A, counting", 2100);
	trn_reset();
	trn[T4_F359C].val = 0x65;
	run_trn("21 TRNSEG4, generator B, counting", 2101);

	/*
	 * THE ECHO-ADAPT START IS NOT AN EXIT.  0x67613 clears four fields and
	 * bit 2 of `f25c2` and then jumps BACK into the compare chain at
	 * 0x643f8, so this run must still reach the same rejoin the two above
	 * do.  `f25c2` carries bit 2 here and not in any other run.
	 */
	trn_reset();
	trn[T4_COUNT].val = TRN_N;
	trn[T4_F25C2].val = (short)0x8104;
	run_trn("21 TRNSEG4, the echo-adapt start point", 2102);

	/*
	 * AND THE SAME START POINT WITH SOMETHING LEFT TO DO BELOW IT.  The
	 * run above cannot tell an arm that rejoins the chain from one that
	 * leaves it, because nothing further down fires; this one clears bit
	 * 2 at 0x67613 and then raises bit 8 at 0x64429, and an arm that left
	 * at 0x67613 stops short of the second.
	 */
	trn_reset();
	trn[T4_COUNT].val = TRN_N;
	trn[T4_F25C2].val = (short)0x8004;
	trn[T4_FAA86].val = TRN_N;
	run_trn("21 TRNSEG4, the start point, then the period flag", 2107);

	/*
	 * The period flag, both ways.  It is guarded on bit 8 being CLEAR as
	 * well as on the count, so the runs above -- which hold bit 8 set --
	 * never reach it and a mutation of either half is visible in exactly
	 * one of these two.
	 */
	trn_reset();
	trn[T4_F25C2].val = (short)0x8004;
	trn[T4_FAA86].val = TRN_N;
	run_trn("21 TRNSEG4, the period is reached", 2103);
	trn_reset();
	trn[T4_F25C2].val = (short)0x8004;
	trn[T4_FAA86].val = TRN_N + 1;
	run_trn("21 TRNSEG4, the period is one short", 2104);

	/*
	 * THE MIDDLE POINT, and it is the one run where the four compares are
	 * deliberately NOT held apart.  300 / 2 is 150 and 100 + 100/2 is 150,
	 * so if the middle test were dropped the period test below it would
	 * fire and raise 0x400; taking the middle arm writes nothing at all.
	 */
	trn_reset();
	trn[T4_A244].val = 300;
	trn[T4_BAUD].val = 100;
	trn[T4_F25C0].val = 149;
	run_trn("21 TRNSEG4, the echo-adapt middle point", 2105);

	/*
	 * The period point, which clears bit 15 as well as raising bit 10 --
	 * `and $0x7fff ; or $0x400`, two halves of one store and `f25c2` is
	 * seeded with bit 15 set so both are visible.
	 */
	trn_reset();
	trn[T4_A244].val = 0x1000;
	trn[T4_F25C0].val = TRN_BAUD + TRN_BAUD / 2 - 1;
	run_trn("21 TRNSEG4, the period point ends the segment", 2106);

	/*
	 * PAST THE END WITHOUT ENDING.  The segment's test is `!=` and not
	 * `>=`: with +0xa244 at 0x20 and the count at 0x40 the arm counts on,
	 * where an arm testing `<` would complete.
	 */
	trn_reset();
	trn[T4_A244].val = 0x20;
	trn[T4_F25C0].val = 0x3f;
	run_trn("21 TRNSEG4, the count is past +0xa244 and does not end", 2108);

	/*
	 * +0xa244 IS AN INT.  Its low halfword is 0x100 and the count reaches
	 * 0x100, so an arm reading it as a halfword ends the segment here and
	 * one reading it as an int counts on.  The middle point of the int is
	 * 0x08000080, which no `short` can reach.
	 */
	trn_reset();
	trn[T4_A244].val = 0x10000100;
	trn[T4_F25C0].val = 0xff;
	run_trn("21 TRNSEG4, +0xa244's high half is part of the compare", 2109);

	/*
	 * ---------------------------------------------------------------
	 * THE COMPLETION.  `f25c0` reaches +0xa244 exactly; the middle point
	 * (0x80) and the period point (4800) are both away from it, so a
	 * mutation of either of the two earlier compares takes a different
	 * exit and fails here rather than agreeing by accident.
	 *
	 * `rtd` chooses the counter three ways and the two PCM receivers
	 * choose the tail three more, and THE TWO TESTS ARE NOT THE SAME
	 * TEST: the counter takes `rtd` whole when either receiver is
	 * non-zero and the tail needs one of them ABOVE ONE.  The two runs at
	 * exactly one are what separate them.
	 */
	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RTD].val = 2;
	run_trn("21 TRNSEG4, the segment ends, rtd at the boundary", 2110);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RTD].val = -1;
	run_trn("21 TRNSEG4, the segment ends, rtd negative", 2111);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RTD].val = 9;
	run_trn("21 TRNSEG4, the segment ends, the counter is halved", 2112);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_V90].val = 1;
	run_trn("21 TRNSEG4, v90 at one: whole counter, neither tail", 2113);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_K56].val = 1;
	run_trn("21 TRNSEG4, k56 at one: whole counter, neither tail", 2114);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_V90].val = 2;
	run_trn("21 TRNSEG4, the V.90 Ja tail", 2115);

	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_K56].val = 2;
	run_trn("21 TRNSEG4, the K56flex Ja tail", 2116);

	/*
	 * BOTH RECEIVERS ABOVE ONE, which is what says the two tests are an
	 * `else if` and in this order rather than two independent ones.
	 */
	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_V90].val = 2;
	trn[T4_K56].val = 2;
	run_trn("21 TRNSEG4, both receivers: the V.90 tail wins", 2117);

	/*
	 * A NEGATIVE COUNT, which is where two sign questions meet.  `f25c0`
	 * is `movswl`-sign-extended before the three compares (0x64aeb) and
	 * compared SIGNED against +0xaa86 (0x64423), so at -2 against a
	 * +0xaa86 of one the period flag stays down and the segment ends;
	 * read either of them unsigned and the run takes a different exit.
	 * -2 is chosen because +0xa244 >> 1 is then -1 and not -2, which keeps
	 * the middle point out of the way.
	 */
	trn_reset();
	trn[T4_A244].val = -2;
	trn[T4_F25C0].val = -3;
	trn[T4_F25C2].val = (short)0x8004;
	trn[T4_FAA86].val = 1;
	run_trn("21 TRNSEG4, the segment ends at a negative count", 2120);

	/* rtd at three, one past the boundary the arm tests. */
	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RTD].val = 3;
	run_trn("21 TRNSEG4, the segment ends, rtd one past the boundary", 2121);

	/*
	 * The completion again with a rate and carrier `setupreceiver` does
	 * NOT recognise, so both of its switches take their default and the
	 * six timing constants and the carrier table are left alone.  That is
	 * a real arm of the inlined function and not an oversight.
	 */
	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RXBAUD].val = 2599;
	trn[T4_RXCARR].val = 1601;
	run_trn("21 TRNSEG4, the segment ends, no rate and no carrier", 2118);

	/*
	 * And once at a DIFFERENT recognised rate and carrier, so the two
	 * switches are shown to depend on what they read rather than to write
	 * one set of constants whatever it is.  2743 is the rate finding D35
	 * records as reachable only from outside `setfinalrate`.
	 */
	trn_reset();
	trn[T4_F25C0].val = TRN_SPAN - 1;
	trn[T4_RXBAUD].val = 2743;
	trn[T4_RXCARR].val = 1959;
	run_trn("21 TRNSEG4, the segment ends, a second rate", 2119);

	trn_reset();
	fixup = NULL;
}

int
main(void)
{
	dump = getenv("V34TX1_DUMP") != NULL;
	diff_begin("v34handshak table 1: seventeen per-sample transmit arms");

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
	case_sbarseg();
	case_ppseg();
	case_vectpp_table();
	case_silence_entry();
	case_silence();
	case_dataxmit();
	case_probe_table();
	case_tx_l1();

	case_jtxmit_entry();
	case_vect16_table();
	case_exmit();
	case_jtxmit();

	case_xmitmp_entry();
	case_xmitmp();

	case_trnseg4_entry();
	case_trnseg4();

	case_tx_dpsk_entry();
	case_dpsk_cold();
	case_tx_dpsk();

	return diff_end();
}
