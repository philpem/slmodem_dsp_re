/*
 * v34hs_state.h -- the handshake's three state words, their eighty-seven
 * names, and the read/print/store closure that moves them, as ONE textual
 * definition compiled into every translation unit that needs them.
 *
 * The blob defines NO `hs_get`, `hs_put` or `hs_setstate` symbol and its
 * `v34handshak` calls none of them: the object inlines the whole closure
 * into the function (finding F11506).  This tree defined all three as
 * GLOBALs in `V34hshak.c`, so `v34handshak` emitted 26 out-of-line calls
 * and the symbol surface carried three GLOBALs the blob does not define.
 *
 * A `static inline` definition in one home fixes both: `V34hshak.c` gets
 * its own internal-linkage copy to inline, and `t_v34hstx1.c` -- which
 * includes `v34hstx1_arms.h` and so must link the arms' `hs_setstate` calls
 * -- gets the same copy from here.  The bodies are the source that stood in
 * `V34hshak.c` (F11503's migration pattern); only their home and linkage
 * changed.
 *
 * FOUNDING FINDINGS.  F11502 resolved the "262 diagnostics" premise: the
 * closure is already emitted, out of line.  F11506 measured that inlining
 * it gives `v34handshak` zero hs calls and no hs symbol, which is the
 * object's own shape.
 */

#ifndef DSPLIB_V34HS_STATE_H
#define DSPLIB_V34HS_STATE_H

#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

/*
 * ---------------------------------------------------------------------------
 * The handshake's state names, and the three machines that use them.
 */

/*
 * `StateName`, eighty-seven string pointers at .data+0x6c00.
 *
 * NOT `const` and not in .rodata: `nm` gives it a lowercase `d`, so the
 * original declared an array of pointers to string literals in writable
 * storage.  Same reading, and the same reason, as `bpv22high` above -- the
 * storage class is what the object records, whatever the intent was.
 *
 * `static` because the symbol is LOCAL (`readelf` says so), which is also
 * what makes it untestable the ordinary way: `objcopy --redefine-syms`
 * renames a local symbol but cannot make it linkable, so there is no
 * `ref_StateName` to compare against the way the fifteen rate tables at the
 * top of this file are compared.  That is finding F173's trap a third time.
 *
 * SO THE TRANSCRIPT COMPARISON IS NOT A SUPPLEMENTARY CHECK ON THESE EIGHTY-
 * SEVEN STRINGS -- IT IS THE ONLY ONE.  `t_v34hshak.c` sweeps the three state
 * words over 0..86 with both debug levels raised so that every entry is
 * printed by both sides and the two transcripts compared.  Delete that sweep
 * and nothing in the tree checks this table at all.
 *
 * The names are the author's, gaps included: NOSTATE0, NOSTATE2, NOSTATE3 and
 * NOSTATE36 are placeholders, so eighty-three of the eighty-seven are real.
 * The indices are `include/dsplib/v34hshak.h`'s `V34HS_*`.
 */
static const char *StateName[V34HS_STATE_COUNT] = {
	"NOSTATE0",	"TXRENEG",	"NOSTATE2",	"NOSTATE3",
	"RECEIVE",	"SILENCE",	"ANSAM",	"TONE_2100",
	"TONE2225",	"AA_TX",	"CC_TX",	"AC_TX",
	"CA_TX",	"SXMIT",	"XMIT1",	"XMIT2",
	"XMIT3",	"XMITV22",	"SSEG",		"SBARSEG",
	"PPSEG",	"TRNSEG4",	"TRNSEG16",	"TX_JM_CM",
	"TX_DPSK",	"DET_2100",	"DET_2250",	"DET_2400",
	"DET_1200",	"DET_AC",	"DET_AC_RTN",	"DET_AC_END",
	"DET_AA",	"PHASE1",	"PHASE2",	"WAIT",
	"NOSTATE36",	"RECEIVE1",	"RECEIVE2",	"RECEIVEV22",
	"DET_CM_JM",	"DET_SYNC",	"DET_CJ",	"RX_DPSK",
	"DET_INFO",	"TONE_AB_ANS",	"TX_PHASE1_ANS","TX_PHASE2_ANS",
	"TX_PHASE3_ANS","RX_PHASE1_ANS","RX_PHASE2_ANS","TX_L1",
	"TX_L2",	"DET_AB",	"SILENCEINFO",	"TX_PHASE1_CALL",
	"TX_PHASE2_CALL","TX_PHASE3_CALL","RX_PHASE1_CALL","RX_PHASE2_CALL",
	"TONE_AB",	"TONE_AB_CALL",	"RX_PHASE3_CALL","INFODONE",
	"JTXMIT",	"XMIT0",	"TRNSEG4A",	"XMITMP",
	"J1TXMIT",	"EXMIT",	"DATAXMIT",	"TXLEVEL",
	"RX_L1",	"RX_L2",	"SILENCERETRAIN","RX_RETRAIN_CALL",
	"RX_RETRAIN_ANSWER","TX_RETRAIN_ANS","JaTXMIT","MOH_TONE",
	"MOH_TONE_DROP","MOH_SILENCE",	"MOH_ON_HOLD",	"MOH_FRR",
	"MOH_CLEARDOWN","K56JaTXMIT",	"TXMD"
};

/*
 * The three state words, and WHICH IS WHICH.
 *
 * +0x3592, +0x3594 and +0x3596 are three concurrent machines, not one, and
 * the assignment below is read off the format strings against their
 * arguments rather than guessed -- finding F171 is what guessing costs.  At
 * every one of the thirteen sites the slot holding a FIXED `StateName[k]`
 * carries the same k the site then assigns, which pins the word that is
 * changing; the two variable slots are then named by the format:
 *
 *   +0x3594 <- 4 at 0x5fd80, and 0x60118 prints "rxstate %s=>%s" with
 *              StateName[4] as the new value          => rxstate
 *   +0x3596 <- 18 at 0x5fd5a, and 0x60492 prints "txstate %s=>%s" with
 *              StateName[18] as the new value         => txstate
 *   +0x3592 <- 41 at 0x5fda6, and 0x6017b prints "microstate %s=>%s" with
 *              StateName[41] as the new value         => microstate
 *
 * and the other two slots agree in all three directions: the rxstate trace's
 * "tx %s" reads +0x3596, the txstate trace's "rx %s" reads +0x3594, and both
 * "mst %s" read +0x3592.
 *
 * A FOURTH, INDEPENDENT SIGN, which is the same argument finding F171 used to
 * settle `Uinfo`: +0x3596 only ever receives SSEG, SILENCEINFO and
 * SILENCERETRAIN, and +0x3594 only ever receives RECEIVE, WAIT and RX_DPSK.
 * Transposed, the RECEIVE machine would be the one entering SSEG.  The
 * table's own `TX_` and `RX_` prefixes say that is the wrong way round.
 */
#define HS_MICROSTATE	0x3592
#define HS_RXSTATE	0x3594
#define HS_TXSTATE	0x3596

/*
 * The two counters every trace prints as `[1]` and `[2]`.  `[1]` is the
 * second short of the pair at +0x2aa0, whose first `v34modeminit` sets to 6
 * and this function's tail sets to 0x10.
 */
#define HS_TRACE_2	0xaa78

static inline short
hs_get(const struct v34_object *obj, unsigned off)
{
	return *(const short *)((const char *)obj + off);
}

static inline void
hs_put(struct v34_object *obj, unsigned off, short v)
{
	*(short *)((char *)obj + off) = v;
}

/*
 * One state transition, with its diagnostic.
 *
 * All thirteen sites are this idiom -- compare, print, assign -- and each
 * prints its own change plus the other two machines' current values, so the
 * three format strings differ only in which word they call the subject.
 *
 * THE TWO CONTEXT SLOTS ARE NOT INTERCHANGEABLE and the order below is the
 * object's: "rxstate" prints (tx, mst), "txstate" prints (rx, mst) and
 * "microstate" prints (tx, rx).  Passing the right strings in the wrong
 * order leaves every byte of the object identical, which is why the fixture
 * sweeps the three words to three DIFFERENT values and not to one.
 *
 * NOT `static`, and neither are `hs_get` and `hs_put` above.  `v34handshak`
 * belongs to this translation unit and is being reconstructed one dispatch
 * arm at a time in files beside this one (v34hshak_t3mid.c is the first),
 * and its arms emit the same three transitions from the same three format
 * strings.  A second copy of this function next door is a second place for
 * the argument order above to be got wrong.  Finding F223's six functions
 * lost their `static` for the weaker reason that a test wanted to call them.
 */
static inline void
hs_setstate(struct v34_object *obj, unsigned off, short next)
{
	static const char *const fmt[3] = {
		/* HS_MICROSTATE */
		"V34HSHAKE: microstate %s=>%s(tx %s, rx %s, [1]%ld, [2]%ld)\n",
		/* HS_RXSTATE */
		"V34HSHAKE: rxstate %s=>%s(tx %s, mst %s, [1]%ld, [2]%ld)\n",
		/* HS_TXSTATE */
		"V34HSHAKE: txstate %s=>%s(rx %s, mst %s, [1]%ld, [2]%ld)\n"
	};
	short now = hs_get(obj, off);

	if (now == next)
		return;

	if (DSPLIB_DEBUG_ON()) {
		const char *ctx1;
		const char *ctx2;

		if (off == HS_MICROSTATE) {
			ctx1 = StateName[hs_get(obj, HS_TXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_RXSTATE)];
		} else if (off == HS_RXSTATE) {
			ctx1 = StateName[hs_get(obj, HS_TXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_MICROSTATE)];
		} else {
			ctx1 = StateName[hs_get(obj, HS_RXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_MICROSTATE)];
		}

		dsplibs_debug_printf(fmt[(off - HS_MICROSTATE) / 2],
				     StateName[now], StateName[next],
				     ctx1, ctx2,
				     (long)obj->vect_idx,
				     (long)hs_get(obj, HS_TRACE_2));
	}

	hs_put(obj, off, next);
}

#endif /* DSPLIB_V34HS_STATE_H */
