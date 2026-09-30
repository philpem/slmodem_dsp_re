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
 * states share one function body).
 *
 * The arms themselves are DEFINED in `v34hstx1_arms.h`, which `V34hshak.c`
 * and `t_v34hstx1.c` each include as a `static` copy, so `-O3` can inline
 * them into `v34handshak` while the mutation driver still calls them
 * directly (findings F11443/F11502). This header keeps the one shared
 * definition of `enum v34tx1_exit` and the reading of the table.
 *
 * Each arm is a function that runs one pass of the per-sample transmit loop
 * for its state(s) and returns to let the loop test run again. Two entries
 * once looked like they left the loop through code this tree had not
 * reconstructed; both targets turned out to already be inside arms written
 * here, so every arm in fact rejoins the loop on every path (finding F340),
 * and `enum v34tx1_exit` below has a single value as a result.
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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSTX1_H */
