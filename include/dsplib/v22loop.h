/*
 * v22loop.h -- V.22 / V.22bis: the local-loopback state of the protocol
 * machine.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_local_loop  .text 0x08a7a0  1,104 bytes
 *
 * It is index 3 of `V22_PROTOCOL`, the seven-entry table at `.rodata` + 0x8544
 * that `V22FP_modem` dispatches through, and it takes the same seven arguments
 * in the same order as the six handlers already reconstructed -- v22ans.h and
 * v22hdx.h carry the derivation of that argument list and it is not repeated
 * here.  Two of the sub-states are not states at all: they hand the whole
 * call, arguments unchanged, to `connect_1200` or `connect_2400`, which is why
 * v22conn.h records this function as one of only three callers of either.
 *
 * ---------------------------------------------------------------------------
 * THE DISPATCH IS A FOURTEEN-ENTRY JUMP TABLE AND ONLY TEN ENTRIES ARE LIVE
 *
 * `hdx->r0c` is loaded with `movswl` and range-checked against 13 with an
 * UNSIGNED `ja`, so a negative sub-state takes the default arm and not case 0.
 * The table at `.rodata` + 0x8580 has fourteen slots and four of them -- 4, 5,
 * 6 and 7 -- point at the function's own epilogue, i.e. at the default:
 *
 *     0        the entry node
 *     1        the entry node's other successor
 *     2        wait for the tone to stop
 *     3        the loop itself
 *     4..7     nothing at all, silently
 *     8..11    connect_2400, verbatim
 *     12,13    connect_1200, verbatim
 *
 * That 8..13 split is exactly v22conn.h's: `V22_NODE_2400A` .. `V22_NODE_2400D`
 * are 8..11 and `V22_NODE_1200_12`/`_13` are 12 and 13.  So the sub-state word
 * is shared between this function and the two connect subroutines rather than
 * being reused with different meanings, and this function is what hands over.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE FOUR OWN SUB-STATES DO, AND WHY THEY ARE NOT NAMED
 *
 * Nothing in the object names any of them.  `v22_local_loop` carries
 * TWENTY-SIX relocations and exactly one is not a call -- its own jump table
 * -- so there is not a single format string in it and the strongest evidence
 * v22conn.h had is unavailable here.  The names below are the sub-state's own
 * value, CLAUDE.md's "modelled, unnamed" tier, and the comments say only what
 * the instructions do:
 *
 *   0  zero the timer and both counters, report the status block, transmit a
 *      block of silence, and go to 1 or to 3 depending on ONE BIT of that
 *      report -- `st.flags2` bit 0, which v22status.h establishes as
 *      `params.flags` bit 10 and hence as `struct v22fp_cfg::f0c` bit 0.  The
 *      3 branch also drops both directions to `V22_RATE_1200`.
 *   1  transmit the tone `hdx->tone` holds for `V22_LOOP_TONE_MS`, then go to
 *      2 and drop both directions to `V22_RATE_1200`.
 *   2  transmit silence until `FPM_TONE_detect` reports `FPM_TONE_NOSIGNAL`,
 *      then go to 3.
 *   3  the loop: modulate, demodulate, and run the two detectors whose
 *      accumulators decide which of the two connect ladders to enter.
 *
 * ---------------------------------------------------------------------------
 * SUB-STATE 3'S TWO ACCUMULATORS ARE THE WHOLE FUNCTION
 *
 * `hdx->r08` and `hdx->r0a` both count milliseconds and each has its own
 * threshold and its own destination:
 *
 *   r08  += V22_LOOP_BLOCK_MS on every block where the S1 detector
 *           (`hdx->mtd_s1`) returns FPM_MTD_ABSENT, and is reset to zero on a
 *           block where it does not -- but ONLY while it is still at or below
 *           V22_LOOP_S1_HOLD_MS.  Above that a detection no longer resets it.
 *           Past V22_LOOP_S1_MS the machine goes to V22_NODE_2400A.
 *   r0a  += whatever `Detect_1s` returns for the DESCRAMBLED symbols, on the
 *           blocks where the same detector applied to the RAW symbols returned
 *           zero.  Past V22_LOOP_ONES_MS the machine goes to V22_NODE_1200_12.
 *
 * Neither threshold is ever compared the other way round and the two arms are
 * mutually exclusive in the object: r08 is tested first and its arm returns.
 *
 * THE HYSTERESIS IS THE ONE PART THAT IS EASY TO GET BACKWARDS.  The reset is
 * guarded by `ja` on `cmpw $0x3b`, so it is "reset unless r08 is ALREADY past
 * 59 ms", not "reset only once 59 ms have passed".
 *
 * ---------------------------------------------------------------------------
 * THE READINGS THIS FILE INHERITS RATHER THAN RE-ESTABLISHES
 *
 *   - Every `ReadGTimer` comparison is `jbe`, i.e. UNSIGNED, although the
 *     function is declared `int`.  Two sites here, and both are cast.
 *   - Every read of `hdx->r08` and `hdx->r0a` is `movzwl` feeding an unsigned
 *     compare, although v22fp.h declares both `short`.  Cast at the site;
 *     the header is not touched.
 *   - `Detect_1s` is declared `int` in v22det.h and the first of the two call
 *     sites here narrows the result to sixteen bits before testing it
 *     (`test %ax,%ax`).  The second does NOT -- its full 32-bit return is
 *     added to `r0a` and only the sum is truncated -- so the two spellings
 *     below differ on purpose.
 *   - `hdx->r04` is the node deadline in `ReadGTimer`'s milliseconds, and it
 *     is `params.r08` as `V22FP_create` copied it.
 *
 * ---------------------------------------------------------------------------
 * ONE PROTOTYPE IN THE TREE IS WRONG, AND IT IS NOT THIS ONE
 *
 * Sub-state 1 stores `FPM_TONE_generate`'s return into `*txcount`:
 *
 *     8a9be:  call FPM_TONE_generate
 *     8a9c3:  mov  %ax,(%esi)          <- %esi is txcount
 *
 * and `FPM_TONE_generate` itself loads `%eax` from the stack slot holding its
 * own sign-extended `count` argument at BOTH of its returns (0xaadf9 and
 * 0xaae15, from the slot written at 0xaad68).  So the object's
 * `FPM_TONE_generate` returns `count`, exactly as `FPM_TONE_generate_demod`
 * and `FPM_TONE_generate2` are already recorded as doing, and fpm_tone.h's
 * `void` is a defect.  Retyping it is not this file's to do, so the source
 * writes the constant and says why; see the note at the call site.
 */

#ifndef DSPLIB_V22LOOP_H
#define DSPLIB_V22LOOP_H

struct v22fp;

/*
 * ---------------------------------------------------------------------------
 * The four sub-states this function owns.  8..13 are v22conn.h's and are used
 * from there rather than respelled.
 */
#define V22_LOOP_NODE_0		0
#define V22_LOOP_NODE_1		1
#define V22_LOOP_NODE_2		2
#define V22_LOOP_NODE_3		3

/*
 * `fp->r1e[0]` (+0x1e).  OR-ed in on entry and read back by nothing
 * reconstructed; the same bit `v22_ans_rmloop2` sets in the same byte.
 */
#define V22_LOOP_R1E_SET	0x02

/*
 * `fp->flags` (+0x1d) bit 4.  Written on the two paths that also call
 * `SetTxRate(fp, V22_RATE_1200)` and `SetRxRate(fp, V22_RATE_1200)`, and by
 * nothing else reconstructed.  v22conn.h names bits 0, 1, 3 and 5 of the same
 * byte the same way -- by value -- and this is the fifth.
 */
#define V22FP_FLAG_1D_BIT4	0x10

/*
 * `fp->status` (+0x1c) on the sub-state 3 deadline.  22 is not one of the
 * seven values v22conn.h carries and no format string names it, so it keeps
 * its value for a name.  What IS observed is that it is written together with
 * `V22FP_FLAGS_TIMEOUT`, which v22conn.h records as the mask both of its
 * training-timeout exits write -- so this is a timeout exit of the same shape.
 */
#define V22_STATUS_16		0x16

/*
 * Sub-state 1's tone length, in the milliseconds `ReadGTimer` counts.  The
 * test is `ja` against the timer's value AFTER the tick, so the wait ends on
 * the first block whose timer exceeds it.
 */
#define V22_LOOP_TONE_MS	3300u	/* strictly more than this */

/*
 * The block, in milliseconds: what `ReadGTimer` adds per call and what
 * sub-state 3 adds to `hdx->r08`.  Same 20 as v22ans.h's V22_BLOCK_MS and
 * v22hdx.h's V22_HDX_TICK_MS.
 */
#define V22_LOOP_BLOCK_MS	20

/*
 * Sub-state 3's three `hdx` thresholds.  Every one is a `cmpw` against a bare
 * immediate followed by an unsigned branch.
 */
#define V22_LOOP_S1_MS		0x61	/* r08: past this -> V22_NODE_2400A  */
#define V22_LOOP_S1_HOLD_MS	0x3b	/* r08: past this, a hit stops
					 *      resetting it                 */
#define V22_LOOP_ONES_MS	0xe6	/* r0a: past this -> V22_NODE_1200_12 */

/*
 * What sub-state 3 hands `Detect_1s`, both times.  The rate is the literal
 * 1200 and NOT `params.bps` -- the object encodes `mov $0x4b0,%edx` at both
 * sites, so the detector is asked for the 1200 bit/s "one" whatever the modem
 * is running at.  The threshold is Q15, 27852/32768 = 0.85, and is the same
 * constant `v22_org_rmloop2` supplies (v22hdx.h's V22_RMLOOP2_ONES_Q15);
 * v22det.h records that `Detect_1s` is the only one of the three detectors
 * that takes a threshold from its caller.
 */
#define V22_LOOP_DET_BPS	1200
#define V22_LOOP_ONES_Q15	0x6ccc

/*
 * `MakeTxData`'s pattern in sub-state 3 is `params.bps2 == this`, overridden
 * to V22_TXDATA_ONES_1200 once `hdx->r08` is past V22_LOOP_S1_MS.  Note +0x04
 * and not +0x02: v22fp.h records that `V22FP_create` copies +0x02 to +0x04 on
 * every path, so the two are always equal and this is the one the object's
 * `cmpw` names -- at BOTH of the two sites that test it.
 */
#define V22_LOOP_BPS_1200	1200

/*
 * The local-loopback state.  See the header comment: ten live sub-states, of
 * which six are `connect_1200` and `connect_2400` under another name, and a
 * silent default that still performs the two stores at the top.
 */
void v22_local_loop(struct v22fp *fp, unsigned short *txsym, short *txout,
		    short *rxin, unsigned short *rxsym,
		    unsigned short *txcount, unsigned short *rxcount);

#endif /* DSPLIB_V22LOOP_H */
