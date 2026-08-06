/*
 * t_v34hstx1.c -- thirteen arms of `v34handshak`'s per-sample transmit
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

int
main(void)
{
	dump = getenv("V34TX1_DUMP") != NULL;
	diff_begin("v34handshak table 1: thirteen per-sample transmit arms");

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

	return diff_end();
}
