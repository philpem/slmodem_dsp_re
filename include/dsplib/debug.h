/*
 * debug.h -- the diagnostic hooks dsplibs.o imports.
 *
 * Three symbols, all undefined in the object and supplied by whatever links
 * it:
 *
 *     nm -u ../slmodemd/dsplibs.o | grep -E 'debug'
 *
 * They are separate from sysdep.h because they are a different kind of
 * dependency.  `sysdep_*` are libc wrappers the datapumps genuinely need;
 * these are pure diagnostics, every use is gated on `dsplibs_debug_level`,
 * and slmodemd ships with that at zero -- so on a working modem none of the
 * call sites do anything at all.
 *
 * WHY THE RECONSTRUCTION CARRIES THEM ANYWAY.  Two reasons, and the second is
 * the one that matters.  The gating comparison is real code with real
 * branches, and a reconstruction that dropped it would differ from the object
 * in its control flow even when the output matched.  More usefully, the
 * format strings are the original author's own words about what the code is
 * doing -- `Carrier Detection Time Out `, `Energy drop detected......` -- and
 * several findings in this project rest on them.  Discarding a call site
 * discards the annotation.
 *
 * The level is the object's own type: `cmpl $0x1,dsplibs_debug_level`
 * compares a full 32-bit word, and every gate in the object is `> 1`.
 */

#ifndef DSPLIB_DEBUG_H
#define DSPLIB_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int dsplibs_debug_level;

/*
 * EXPERIMENTAL KNOB, branch `improve/v34-training` only, default 0.
 *
 * Non-zero selects the least-squares tilt estimator in `probe_preemp` instead
 * of the object's two-point counter.  It is a runtime flag rather than a
 * compile-time one so a single hybrid build can serve both arms of an A/B and
 * the two arms cannot differ in anything else -- the pre-emphasis A/B runs so
 * far used two separate binaries, which leaves the compiler as an uncontrolled
 * variable.  Set from the environment by the caller; never set in library code.
 */
extern int dsplib_v34_fit_preemp;

/*
 * Dump all 25 probe DFT bins (V34PROBEBINS).  Default 0 for the same reason
 * the flag above exists, plus one the estimator does not have: this
 * instrumentation prints text the object never printed, and the differential
 * tests compare our debug transcript against the reference's CHARACTER FOR
 * CHARACTER.  Left unconditional at level 3 it fails
 * `t_v34hshak`'s "probeselect narrates every decision" on 150 checks -- which
 * is the differential tier doing exactly its job.  The bench sets this from
 * the environment; nothing in the library sets it.
 */
extern int dsplib_v34_dump_probe_bins;

/*
 * Deliberately degrade the equaliser's adaptation, to prove the replay harness
 * can tell two receivers apart.  Default 0; set only by tools/benchflags.c
 * from the environment, and that file is linked only into the bench hybrid.
 * See v34rx.c and finding 1906 -- this is a test instrument, not a knob.
 */
extern int dsplib_v34_seed_defect;

/*
 * Route the BAD-BLOCK arm of the retrain test to a V.34 §11.6 rate
 * renegotiation instead of a full retrain.  Default 0; branch experiment,
 * set only by tools/benchflags.c from DSPLIB_V34_RRN_ON_BADBLOCK.
 *
 * The far end's own retrain request is unaffected -- only the arm this modem
 * raises against itself.  Findings 1921, 1925, 1931.
 */
extern int dsplib_v34_rrn_on_badblock;

/*
 * Choose the pre-emphasis filter by matching the measured channel against all
 * eleven transmit-spectrum templates, instead of reducing it to a tilt and
 * indexing a counter that can only reach 6-10.  Set only by
 * tools/benchflags.c from DSPLIB_V34_SHAPE_PREEMP.  Findings 1956, 1957.
 */
extern int dsplib_v34_shape_preemp;

/*
 * Log the equaliser's tap energy, split centre-run against off-centre, every
 * 1024-symbol block.  Off-centre energy IS the work the equaliser is doing to
 * undo a channel the pre-emphasis failed to match, which `equerr` alone cannot
 * distinguish from a simply noisier line.  DSPLIB_V34_DUMP_EQ_TAPS.
 */
extern int dsplib_v34_dump_eq_taps;

int dsplibs_debug_printf(const char *fmt, ...);

/*
 * The modem core's data logger.  Declared here for completeness -- the object
 * imports it -- and not yet called by anything reconstructed.
 */
int modem_debug_log_data(void *m, unsigned id, const void *buf, int len);

/*
 * Almost every gate in the object is this comparison, spelled out here so a
 * module does not have to remember that it is `> 1` and not `>= 1`.
 */
#define DSPLIB_DEBUG_ON()	(dsplibs_debug_level > 1)

/*
 * ...but not every one.  `cadence_progress` gates three of its seven sites on
 * `cmpl $0x2` rather than `cmpl $0x1`, so they need level 3, not 2 -- the
 * three "CADENCE %s: CONDITION ... SATISFIED" messages, which fire once per
 * matched cycle and would drown the rest.
 *
 * This is exactly what finding 150 warned could not be seen: a site at the
 * wrong threshold produces a byte-identical transcript at level 2, and one
 * macro for every gate quietly flattens the distinction.  Sweeping the level
 * is what makes the two distinguishable, so any test that compares
 * transcripts must run at 1, 2 AND 3.
 */
#define DSPLIB_DEBUG_VERBOSE()	(dsplibs_debug_level > 2)

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DEBUG_H */
