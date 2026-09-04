/*
 * v34hstx1.h -- v34handshak's per-sample transmit dispatch, one function per
 * arm.
 *
 * `v34handshak` is 61,541 bytes and is not one unit; it is reconstructed and
 * tested piece by piece (see docs/v34handshak.md). Table 1, the transmit
 * jump table at `.rodata+0x2da0`, is read inside the per-sample loop and
 * indexed by `txstate - 5`. Of its 82 entries, only 19 have a target of
 * their own -- the rest just fall through to the loop bottom (finding
 * F287). Those 19 targets cover 23 distinct `txstate` values (a handful of
 * states share one function body) and are the 17 functions declared below.
 *
 * Each one is an "arm": a function that runs one pass of the per-sample
 * transmit loop for its state(s) and returns to let the loop test run
 * again. Two entries once looked like they left the loop through code this
 * tree had not reconstructed; both targets turned out to already be inside
 * arms written here, so every arm in fact rejoins the loop on every path
 * (finding F340), and `enum v34tx1_exit` below has a single value as a
 * result.
 *
 * Several arms serve more than one `txstate`, in three different shapes:
 *   - v34tx1_jatxmit() and v34tx1_k56jatxmit() are two separate functions
 *     built over one shared static helper.
 *   - v34tx1_silence() covers three states (SILENCE, SILENCEINFO,
 *     SILENCERETRAIN) as one function that re-reads `txstate` itself and
 *     branches, because the object does the same.
 *   - v34tx1_jtxmit() covers two states (JTXMIT, J1TXMIT) as one function
 *     with a shared body and a single internal branch that separates the
 *     two tails.
 *   - v34tx1_moh_silence() covers four states (81-84) as one function with
 *     one behaviour.
 *
 * WHAT IS NOT HERE, and it is a real gap rather than an omission: every arm
 * that changes `txstate`, plus both counter arms, has a diagnostic behind
 * `dsplibs_debug_level > 1` that this tree has not reconstructed. These
 * functions are faithful at debug level 0, which is what the library ships
 * and what t_v34hstx1.c tests against (finding F341).
 */

#ifndef DSPLIB_V34HSTX1_H
#define DSPLIB_V34HSTX1_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Where a transmit-dispatch arm leaves control.
 *
 * The only value: every arm rejoins the per-sample loop's own test on
 * every path (finding F340). The type is kept as a return value, rather
 * than reduced to `void`, because that is what every arm's signature is in
 * the object.
 */
enum v34tx1_exit {
	V34TX1_LOOP = 0		/* back to the per-sample loop test  */
};

/**
 * @brief SILENCE / SILENCEINFO / SILENCERETRAIN transmit-dispatch arm
 * (txstates 5, 54, 74).
 *
 * One function for three states: the shared prologue re-reads `txstate`
 * and gives each of the three a different tail, matching the object.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_silence(void *obj);

/**
 * @brief JTXMIT / J1TXMIT transmit-dispatch arm (txstates 64, 68).
 *
 * One function, one shared body, two tails. The body -- a countdown, two
 * status bits, the differential quadrant and one `vect4` point -- is the
 * same for both states; only on the pass where `vect_idx` wraps to zero
 * does it re-read `txstate` and separate them (JTXMIT counts up to a limit
 * and then becomes J1TXMIT; J1TXMIT ends the segment).
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_jtxmit(void *obj);

/**
 * @brief TX_DPSK transmit-dispatch arm (txstate 24).
 *
 * Sends one bit of the Modem-on-Hold message per pass, XORed into the
 * message state and sent as one of two constellation points -- the same
 * two points and scaling as TONE_AB, which is why the two agree numerically
 * and are still separate arms. When the message runs out, hands the
 * transmit machine to TONE_AB, or, on hold, re-arms the message reader and
 * works through the hold clear-down.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_tx_dpsk(void *obj);

/**
 * @brief SSEG transmit-dispatch arm (txstate 18).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_sseg(void *obj);

/**
 * @brief SBARSEG transmit-dispatch arm (txstate 19).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_sbarseg(void *obj);

/**
 * @brief PPSEG transmit-dispatch arm (txstate 20): sends the PP training
 * sequence (see vectpp in v34rx.h).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_ppseg(void *obj);

/**
 * @brief TX_L1 transmit-dispatch arm (txstate 51).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_tx_l1(void *obj);

/**
 * @brief TONE_AB transmit-dispatch arm (txstate 60).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_tone_ab(void *obj);

/**
 * @brief XMIT0 transmit-dispatch arm (txstate 65).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_xmit0(void *obj);

/**
 * @brief EXMIT transmit-dispatch arm (txstate 69).
 *
 * A receiver status bit chooses, per pass, between two symbols out of
 * `vect4` or four out of `vect16`; `vect_idx` advances by two or four to
 * match.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_exmit(void *obj);

/**
 * @brief DATAXMIT transmit-dispatch arm (txstate 70).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_dataxmit(void *obj);

/**
 * @brief TRNSEG4 transmit-dispatch arm (txstate 21).
 *
 * Sends one scrambled `vect4` point per pass, sharing its head with
 * v34tx1_txlevel() (TXLEVEL) and v34tx1_txmd() (TXMD). Counts the segment
 * against one of four limit fields, chosen by which of the two PCM
 * receivers is active; three of the four give the usual loop exit and the
 * fourth is the segment's completion, which inlines `setupreceiver` and
 * clears a twelve-field record ahead of the next phase.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_trnseg4(void *obj);

/**
 * @brief XMITMP transmit-dispatch arm (txstate 67).
 *
 * Reads two or four bits per symbol from the message reader (an inlined
 * `getbit`, with a real call only on the restart path) and maps them
 * through `vect4` or `vect16`, chosen the same way v34tx1_exmit() (EXMIT)
 * chooses.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_xmitmp(void *obj);

/**
 * @brief TXLEVEL transmit-dispatch arm (txstate 71).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_txlevel(void *obj);

/**
 * @brief JaTXMIT transmit-dispatch arm (txstate 78).
 *
 * A separate function from v34tx1_k56jatxmit() (K56JaTXMIT), but built over
 * a static helper the two share.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_jatxmit(void *obj);

/**
 * @brief MOH_SILENCE transmit-dispatch arm (txstates 81-84).
 *
 * One function, one behaviour, for all four states.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_moh_silence(void *obj);

/**
 * @brief K56JaTXMIT transmit-dispatch arm (txstate 85).
 *
 * See v34tx1_jatxmit() (JaTXMIT) for the helper the two share.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_k56jatxmit(void *obj);

/**
 * @brief TXMD transmit-dispatch arm (txstate 86).
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_txmd(void *obj);

/**
 * @brief TRNSEG4A transmit-dispatch arm (txstate 66).
 *
 * The largest single run of straight-line code in the table. Sends one
 * symbol out of `vect4` or `vect16`, chosen the way v34tx1_exmit() (EXMIT)
 * chooses, with the scrambler's generator chosen by role as in
 * v34tx1_txlevel() (TXLEVEL) and v34tx1_txmd() (TXMD); counts the segment
 * up against a length derived from the rate configuration. Its fourth exit,
 * distinct from the usual loop rejoin, is the segment's completion, which
 * rebuilds the receive half of that configuration.
 *
 * @param obj  The V.34 modem object.
 * @return #V34TX1_LOOP.
 */
int v34tx1_trnseg4a(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSTX1_H */
