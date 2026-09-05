/*
 * v22org.h -- V.22 / V.22bis: the ORIGINATE and ANSWER states of the protocol
 * machine.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_answer     .text 0x08abf0  1,789 bytes
 *   v22_originate  .text 0x08b2f0  2,655 bytes
 *
 * THEY ARE TWO OF THE SEVEN `V22_PROTOCOL` ENTRIES.  That table is at
 * `.rodata` + 0x8544 and its seven relocations resolve to `v22_data`,
 * `v22_originate`, `v22_answer`, `v22_local_loop`, `v22_org_rmloop2`,
 * `v22_ans_rmloop2` and `v22_retrain`; `V22FP_modem` indexes it with
 * `fp->hdx->protocol`, so index 1 is `v22_originate` and index 2 is `v22_answer`
 * (finding F8529).  Each takes the SAME seven arguments `connect_1200` and
 * `connect_2400` take -- see v22conn.h, which carries that derivation -- and
 * each dispatches on `fp->hdx->connect_substate` through a jump table of its own:
 * `.rodata` + 0x85b8 for `v22_answer` (fifteen entries, 0..14) and
 * `.rodata` + 0x85f4 for `v22_originate` (fourteen, 0..13).  Both tables end
 * the same way -- 8..11 hand over to `connect_2400` and 12..13 to
 * `connect_1200` -- so the two machines meet at the connect states.
 *
 * ---------------------------------------------------------------------------
 * THE NODE NAMES ARE THE AUTHOR'S OWN
 *
 * Every arm of both functions opens with a debug print, and eleven of them
 * name the node they are entering:
 *
 *     "V22_answer,NODE_0\n"                     answer, r0c == 0
 *     "V22_answer,NODE_1\n"                     answer, r0c == 1
 *     "V22_answer,NODE_3\n"                     answer, r0c == 3
 *     "V22_answer,NODE_4\n"                     answer, r0c == 4
 *     "V22_answer,NODE_SILENCE_AFTER_2100\n"    answer, r0c == 14
 *     "V22_originate,NODE_0\n"                  originate, r0c == 0
 *     "V22_originate, NODE_1\n"                 originate, r0c == 1
 *     "V22_originate, NODE_3\n"                 originate, r0c == 3
 *     "V22_originate, NODE_4\n"                 originate, r0c == 4
 *     "V22_originate, NODE_5\n"                 originate, r0c == 5
 *     "V22_originate, NODE_6\n"                 originate, r0c == 6
 *
 * -- so those values ARE named, by the strongest evidence there is.  The
 * remaining arms print "V22_answer, NODE %d\n " or "V22_originate, NODE %d\n "
 * with the sub-state as the operand and hand straight over to the connect
 * states, whose own names v22conn.h already established.  Values with a table
 * entry pointing at the epilogue -- 2, 5, 6 and 7 for the answer machine, 2
 * and 7 for the originate one -- return with the status byte set and nothing
 * else done, as does anything past the end of each table.
 *
 * THE TWO MACHINES ARE NOT MIRRORS AND THE NODE NUMBERS DO NOT CORRESPOND.
 * `v22_answer`'s NODE_4 is a short S1 burst before `connect_2400`;
 * `v22_originate`'s NODE_4 is the descrambled receive phase before its own
 * NODE_5.  Only NODE_0 (the reset), NODE_1 (the wait) and the shared 8..13
 * tail line up, and even NODE_0 differs: the answer machine's takes one of two
 * exits on a `V22_status` bit and the originate machine's ignores the report
 * it asks for entirely.
 *
 * NODE_SILENCE_AFTER_2100 IS WHAT NAMES THE ANSWER MACHINE'S TONE PHASE.
 * NODE_1 generates a tone for 3,300 ms and then moves to node 14, which is
 * called "SILENCE_AFTER_2100"; so the tone NODE_1 sends is the 2100 Hz answer
 * tone, and node 14 is the silence the recommendation puts after it.  That is
 * a reading of the author's word, not of the oscillator -- see the note on the
 * retune tails below, which set 2225 Hz and 1200 Hz rather than 2100.
 *
 * ---------------------------------------------------------------------------
 * `v22_answer` OWNS ONE FILE-LOCAL DATUM AND `v22_originate` OWNS NONE
 *
 * `iSilenceAfter2100`, 2 bytes at `.bss` + 0x380, LOCAL, referenced three
 * times by `v22_answer` and by nothing else in the whole object: node 1 clears
 * it on the way out, and node 14 increments it and compares it with 4.  It is
 * `static` in src/pump/v22/v22org.c for that reason.  The load is `movzwl`,
 * which is what makes it unsigned.
 *
 * At one 160-sample block per call, four blocks is 80 ms -- the
 * recommendation's 75 ms of silence rounded up to a block boundary -- which
 * corroborates the name without being needed for it.
 *
 * ---------------------------------------------------------------------------
 * TWO COUNTERS AND A GATE, AND THE FORMAT STRINGS THAT BOUND THEM
 *
 * The carrier hunt appears three times -- `v22_answer`'s NODE_3 and
 * `v22_originate`'s NODE_5 and NODE_6 -- and it always runs two independent
 * detectors over each received block:
 *
 *   `hdx->r08` counts, in milliseconds, the run of consecutive blocks in which
 *   `FPM_MTD_detect` on `hdx->mtd_s1` reported NOTHING.  A block that does
 *   report resets the counter unless the run has already passed 59 ms.
 *
 *   `hdx->r0a` accumulates `Detect_1s`'s answer, which v22det.c shows IS a
 *   duration in milliseconds -- `(count * V22_DET_SYMBOL_MS_Q14) >> 14`.
 *
 * `v22_answer`'s copy adds a reset of `r0a` on the same 59 ms rule that
 * `r08` gets; NEITHER of `v22_originate`'s does, and that asymmetry is the
 * object's.  The verdicts are then printed in the author's words: "Detected
 * V22bis Carrier\n" and "Detected V22 Carrier\n" in `v22_answer`, and
 * "V.22 %d modem det true\n" with a literal 640 in `v22_originate`'s NODE_6.
 * The first counter's verdict goes to the 2400 ladder and the second's to the
 * 1200 one, and that is what the strings say rather than what the code looks
 * like.
 *
 * `v22_originate`'s NODE_5 tracks the same two counters and reaches NO
 * verdict at all: it moves to NODE_6 on a plain deadline and leaves both
 * counters standing for NODE_6 to judge.
 *
 * ---------------------------------------------------------------------------
 * THE COMPARISONS ARE UNSIGNED AND THAT IS ENCODED
 *
 * Same finding as v22conn.h's.  Every `ReadGTimer` test in both functions is
 * `jbe`, all the `hdx->r08` and `hdx->r0a` tests are `seta`/`ja`/`jbe` on a
 * `cmpw`, the answer machine's NODE_3 tone gate is a `ja` on `hdx->gtimer`,
 * and `v22_originate`'s NODE_3 mean is a `div` -- an UNSIGNED divide, with
 * `%edx` zeroed first.  Every one of those fields is declared signed in
 * v22fp.h and no reachable value can tell the readings apart, so the source
 * casts at the comparison rather than renaming a field -- see the note at the
 * bottom of this file.
 *
 * ---------------------------------------------------------------------------
 * THE RETUNE TAILS, AND WHAT THEY SAY ABOUT `FPM_TONE_generate`
 *
 * The answer machine's NODE_0 and NODE_1 both end by rebuilding the tone
 * generator at 2225 Hz, and the originate machine's NODE_0 does the same at
 * 1200 Hz with the phase reversals switched off:
 *
 *     tone->cfg.freq  = 2225;   /_ or 1200, and cfg.rev_period = 0 _/
 *     tone->cfg.scale = (tone->cfg.scale * 23265) >> 15;
 *     FPM_TONE_create(tone, &tone->cfg);
 *
 * -- the object passes the SAME pointer as both arguments, which works because
 * `struct fpm_tone`'s first member is its own `cfg`.  23265/32768 is 0.71, so
 * the amplitude drops 3 dB each time.  2225 Hz is not 2100; nothing in the
 * object says why, and it is recorded rather than explained.
 *
 * `FPM_TONE_generate` IS DECLARED `void` IN fpm_tone.h AND THE OBJECT RETURNS
 * `count`.  Both of its call sites in `v22_answer` store `%ax` into
 * `*txcount`, and the blob's `FPM_TONE_generate` loads `0xc(%esp)` -- which
 * its prologue filled with `movswl` of its own `count` argument -- into `%eax`
 * on both of its exits.  So the object's store is `*txcount = count`, and
 * `count` is the literal 160 at both sites.  This file spells that as the
 * literal rather than as a return value, because changing fpm_tone.h's
 * declaration is a change to a differentially-tested module and is not this
 * file's to make.  It is recorded here so that the next reader of fpm_tone.h
 * knows the declaration understates the object.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS NOT RENAMED
 *
 * `hdx->r08`, `r0a`, `r0c`, `r04`, `r28`, `r2c`, `r30`, `r32`, `r34` and
 * `fp->r1e` keep v22fp.h's names.  Several of them are now read by something
 * -- `r08` and `r0a` are the two counters above, and `v22_originate`'s NODE_3
 * accumulates `FPM_rms` into `r2c` over `r32` blocks, divides to get the mean
 * and sets `r34` when that mean exceeds `r30` (9,300 from `V22FP_create`) --
 * and all of them are read UNSIGNED here while v22fp.h declares them signed.
 * A rename is a change to a shared header that other branches are editing; the
 * derivation is recorded and the rename is left to whoever lands them.
 *
 * NOTE THE DIVIDE BY `r32`.  `V22FP_create` leaves it zero and only the
 * `*rxcount != 0` path increments it, so entering NODE_3's mean calculation
 * with `r08` already past 135 and no block counted divides by zero.  That is
 * the object's, and a test must not construct it.
 */

#ifndef DSPLIB_V22ORG_H
#define DSPLIB_V22ORG_H

struct v22fp;

/*
 * ---------------------------------------------------------------------------
 * The sub-states, `struct v22fp_hdx::r0c`.  Named by the author's own debug
 * strings; 8..13 are v22conn.h's and are not repeated here.
 */
#define V22_ANS_NODE_0			0
#define V22_ANS_NODE_1			1
#define V22_ANS_NODE_3			3
#define V22_ANS_NODE_4			4
#define V22_ANS_NODE_SILENCE_AFTER_2100	14

#define V22_ORG_NODE_0			0
#define V22_ORG_NODE_1			1
#define V22_ORG_NODE_3			3
#define V22_ORG_NODE_4			4
#define V22_ORG_NODE_5			5
#define V22_ORG_NODE_6			6

/*
 * How many calls NODE_SILENCE_AFTER_2100 stays put before moving to NODE_3.
 * The object compares `iSilenceAfter2100` with 4 for equality.
 */
#define V22_ANS_SILENCE_BLOCKS		4

/*
 * ---------------------------------------------------------------------------
 * `struct v22fp::status` (+0x1c), a message code.  `V22_STATUS_01` and the
 * rest of the set live in v22conn.h; these four are new here, and each carries
 * the author's word because the debug print naming it sits in the same basic
 * block as the store.
 */
#define V22_MSG_ERROR1			17	/* originate NODE_1 gave up  */
#define V22_MSG_ERROR3			19	/* originate NODE_3 gave up  */
#define V22_MSG_ERROR4			20	/* originate NODE_6 gave up  */
#define V22_MSG_ERROR5			21	/* answer NODE_3 gave up     */

/*
 * ---------------------------------------------------------------------------
 * Bits OR-ed into `struct v22fp::flags` (+0x1d) and into `fp->r1e[0]` (+0x1e).
 *
 * WHAT ANY OF THEM MEANS IS NOT ESTABLISHED -- exactly as v22conn.h records
 * for the four bits it names.  They are named by BIT VALUE so that the source
 * and the object's immediates stay 1:1, and the comments claim only where each
 * is written.  v22conn.h's `V22FP_FLAGS_TIMEOUT` (0x02) is reused unchanged;
 * every "gave up" arm above writes it.
 *
 * THE `r1e` BIT IS NOT WRITTEN CONSISTENTLY and that is worth recording: the
 * answer machine writes it at BOTH of NODE_3's carrier verdicts, the originate
 * machine writes it at NODE_6's 1200 verdict and NOT at its 2400 one, and it
 * also writes it on the way into `connect_2400` from nodes 8..11 but not on
 * the way into `connect_1200` from 12..13.
 */
#define V22FP_FLAG_1D_BIT4	0x10	/* answer NODE_1's tone ended, and
					 * originate NODE_1 detected V.22   */
#define V22FP_R1E_BIT3		0x08	/* see the note above               */

/*
 * ---------------------------------------------------------------------------
 * The node deadlines, in milliseconds on `ReadGTimer`'s clock, except the one
 * that is read off `hdx->gtimer` directly.  Each is a bare immediate in its
 * own `cmp`.  The clock advances 20 ms per call, so the effective thresholds
 * are the next multiple of 20 above each.
 *
 * Two of the arms have no constant at all and compare against `hdx->node_deadline`, the
 * caller's own limit -- 60,000 ms as `v22_create` configures it: `v22_answer`'s
 * NODE_3 and `v22_originate`'s NODE_1 and NODE_3.
 */
#define V22_ANS_NODE_1_MS	3300u	/* the answer tone's length          */
#define V22_ANS_NODE_3_TONE_MS	1470u	/* NODE_3 switches its transmitter
					 * from data to tone past this; the
					 * one test on `gtimer` itself       */
#define V22_ANS_NODE_4_MS	97u	/* NODE_4 -> connect_2400's NODE_8   */
#define V22_ORG_NODE_4_MS	416u	/* NODE_4 -> NODE_5                  */
#define V22_ORG_NODE_5_MS	536u	/* NODE_5 -> NODE_6                  */
#define V22_ORG_NODE_6_MS	1926u	/* NODE_6 gives up                   */

/*
 * The carrier-hunt thresholds, in milliseconds.  59 gates BOTH counters
 * wherever the hunt appears; 230 is the 1200 verdict's alone; 135 is
 * `v22_originate`'s NODE_3, which counts the same `Detect_1s` milliseconds
 * into `r08` without any S1 detector beside it.
 */
#define V22_ORG_MIN_RUN_MS	59
#define V22_ORG_ONES_MS		230
#define V22_ORG_NODE_3_RUN_MS	135

/*
 * `Detect_1s`'s two constants at all six of its call sites.  v22det.h names
 * only 2400, because 2400 is the only value the callee tests for; 1200 is a
 * bare immediate here.  The threshold is Q15 -- 27852/32768 is 0.85.
 */
#define V22_ORG_DETECT_BPS	1200
#define V22_ORG_DETECT_THRESH	0x6ccc

/*
 * The block length handed to `FPM_TONE_generate` and to `FPM_rms`, and what
 * the former writes back into `*txcount`.
 */
#define V22_ORG_BLOCK		160

/*
 * The retune tails' constants.  The scale is Q15 and multiplies the
 * generator's existing gain; both tails apply the same one.
 */
#define V22_ANS_TONE_HZ		2225
#define V22_ORG_TONE_HZ		1200
#define V22_ORG_TONE_SCALE_Q15	0x5ae1

/*
 * `SetAdaptEqV22`'s mode at each site.  v22prc.h documents the modes; these
 * are the two the object passes here.
 */
#define V22_ORG_EQ_MODE_CARRIER	2	/* both carrier verdicts             */
#define V22_ORG_EQ_MODE_NODE_5	1	/* originate NODE_4 -> NODE_5        */

/*
 * The operand of "V.22 %d modem det true\n".  A literal in the object and
 * nothing says what it counts.
 */
#define V22_ORG_DET_TRUE_ARG	640

/*
 * ---------------------------------------------------------------------------
 * The handlers below take the same seven arguments `connect_1200` and
 * `connect_2400` take and in the same order -- v22conn.h carries the
 * derivation, and these two are read off the same stack slots. Both counts
 * are in/out and both symbol buffers are scratch the caller owns.
 *
 * Both are declared void and the object does not settle that: every exit is
 * a plain `ret` with a dead `%eax`. Same situation as `connect_1200` and
 * `TxClockSync`, and recorded for the same reason.
 */

/**
 * @brief V.22 protocol handler for the ANSWER station's connection sequence (index 2 of V22_PROTOCOL).
 *
 * A jump table of fifteen sub-states (`hdx->r0c`, `.rodata`+0x85b8): NODE_0
 * resets, NODE_1 sends the 2225 Hz answer tone for V22_ANS_NODE_1_MS then
 * moves to NODE_SILENCE_AFTER_2100, that node waits V22_ANS_SILENCE_BLOCKS
 * calls then moves to NODE_3, NODE_3 hunts for carrier over two detectors
 * and NODE_4 is a short S1 burst before handing over to `connect_2400`
 * (nodes 8..11) or `connect_1200` (12..13). See the file banner for the
 * full derivation, including the two carrier-hunt counters `hdx->r08`/`r0a`.
 *
 * @param fp       The V.22 datapump instance.
 * @param txsym    Transmit symbols to scramble and modulate.
 * @param txout    Output for the modulated transmit samples.
 * @param rxin     Received samples to demodulate.
 * @param rxsym    Output for the demodulated receive symbols.
 * @param txcount  In/out: transmit symbol/sample count.
 * @param rxcount  In/out: receive sample/symbol count.
 */
void v22_answer(struct v22fp *fp, unsigned short *txsym, short *txout,
		short *rxin, unsigned short *rxsym,
		unsigned short *txcount, unsigned short *rxcount);

/**
 * @brief V.22 protocol handler for the ORIGINATE station's connection sequence (index 1 of V22_PROTOCOL).
 *
 * A jump table of fourteen sub-states (`hdx->r0c`, `.rodata`+0x85f4):
 * NODE_0 resets and retunes the tone generator to 1200 Hz, NODE_1 waits for
 * the far end, NODE_3 measures the received RMS mean against a threshold,
 * NODE_4 is a descrambled receive phase leading to NODE_5, and NODE_5/NODE_6
 * run the same two carrier-hunt counters `v22_answer`'s NODE_3 does before
 * handing over to `connect_2400`/`connect_1200`. See the file banner for the
 * full derivation, including the asymmetry between this and `v22_answer`'s
 * counter resets.
 *
 * @param fp       The V.22 datapump instance.
 * @param txsym    Transmit symbols to scramble and modulate.
 * @param txout    Output for the modulated transmit samples.
 * @param rxin     Received samples to demodulate.
 * @param rxsym    Output for the demodulated receive symbols.
 * @param txcount  In/out: transmit symbol/sample count.
 * @param rxcount  In/out: receive sample/symbol count.
 */
void v22_originate(struct v22fp *fp, unsigned short *txsym, short *txout,
		   short *rxin, unsigned short *rxsym,
		   unsigned short *txcount, unsigned short *rxcount);

#endif /* DSPLIB_V22ORG_H */
