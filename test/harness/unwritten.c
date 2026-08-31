/*
 * unwritten.c -- bridges from unwritten callees' REAL names to the blob's
 * ref_ aliases, so a reconstructed caller can link and run before its
 * callees are reconstructed.
 *
 * WHY THIS EXISTS.  symmap.py renames every symbol the blob defines to
 * ref_*, so an unwritten callee's real name resolves to nothing in a test
 * binary: writing `CID_process`, which calls `cid_progress`
 * unconditionally, would otherwise fail 60+ links at once -- finding F217's
 * wall, which for `v34handshak`'s closure was answered by writing the
 * callees first (F215).  For the CID wrapper and the `prop_dp_*` aggregate
 * the callees are the whole FSK caller-ID receiver and the whole V.22 and
 * V.32 datapumps, far outside those functions' own batch, so this file is
 * F214's documented escape hatch at the harness tier instead: each bridge
 * forwards the real name to the blob's copy.
 *
 * WHAT IT MEANS FOR A TEST.  A bridged callee runs BLOB code whichever side
 * called it, so its stateful host callbacks (modem_dp_register,
 * dsplibs_debug_printf...) go through the ref_ imports and land on the
 * REFERENCE side's logs and transcripts even for our-side calls.  t_dpinit
 * and t_cid are written against exactly that split and say so.
 *
 * WHAT IT MEANS FOR src/.  Nothing.  The reconstruction declares these
 * callees weak (the DSPLIB_*_UNWRITTEN idiom) and calls them by their real
 * names; in the interop binaries, which link no blob and never reach these
 * paths, the weak references resolve to zero.  No ref_ name appears
 * anywhere in src/.
 *
 * WHEN A BRIDGED SYMBOL IS RECONSTRUCTED its definition collides with the
 * bridge and every link fails LOUDLY -- delete the bridge here, and the
 * test split it served collapses back to straight symmetry (t_dpinit's
 * header comment walks through it).
 */

/* --- the Caller ID receiver, span cid_modem.c (0x8fd60..) ------------- */

/*
 * ALL FIVE CID BRIDGES ARE GONE.  `cid_create`, `cid_delete`,
 * `cid_freq_sampl`, `cid_get_strings` and now `cid_progress` are every one of
 * them defined in `src/service/cid.c`, which is the "WHEN A BRIDGED SYMBOL IS
 * RECONSTRUCTED" paragraph above taken five times.
 *
 * `cid_progress` was the last, and with it the split that paragraph describes
 * has collapsed for the whole service: `CID_process` and everything under it
 * is ours on our side and the blob's on the reference side, so `t_cid` no
 * longer has a shared-callee caveat to state and `t_cidprog` drives
 * `cid_progress` straight against `ref_cid_progress` with no bridge in
 * between.
 */

/* --- BOTH registration pairs are gone, and this note is what is left ---
 *
 * V.22's pair (`dp_v22_init`/`dp_v22_exit`) and V.32's (0x4bb0/0x4bf0) were
 * both bridged here for `prop_dp_init`/`prop_dp_exit`.  `src/pump/v22/v22.c`
 * and `src/pump/v32/v32.c` now define all four themselves, so both bridges
 * are deleted rather than left to collide -- the "WHEN A BRIDGED SYMBOL IS
 * RECONSTRUCTED" paragraph above, taken twice.
 *
 * They were removed by two different agents on two branches, each deleting
 * only its own pair, so the merge conflicted with one bridge surviving on
 * each side and BOTH had to go.  A resolution that took either side entire
 * would have kept a bridge whose symbol is now defined in `src/`, and the
 * period link would have said `multiple definition`.
 * ------------------------------------------------------------------------ */
