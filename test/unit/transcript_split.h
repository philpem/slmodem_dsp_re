/*
 * transcript_split.h -- mechanics for lifting a diagnostic-transcript check
 * out of a differential fixture and into its own declared binary.
 *
 * WHY.  A few fixtures compare the object's `edprintf`/`dsplibs_debug_printf`
 * transcripts with a `strcmp`.  On the modern tier the printed text can differ
 * in the last decimal place of a float the diagnostic formats, because the
 * modern compiler keeps a different intermediate precision than GCC 3.4.2.
 * The substantive state comparison in the same fixture passes -- the decision
 * is identical -- but the transcript is a `strcmp`, and finding F11368 rules
 * that the modern-tier float tolerance must never reach a transcript compare.
 * The sanctioned remedy is the one `t_v90connevalnan` established (issue #143,
 * findings F2157/F3002/F6000-F6002): move the divergent checks into a binary
 * of their own, declare that binary in `tools/gccdiverge.json`, and leave the
 * parent green so its mutation suite stays scoreable.
 *
 * HOW, AND WHY NOT BY COPYING THE SETUP.  The precedent files duplicate the
 * fixture's setup.  That is a lot of copied apparatus and a second copy to
 * keep in step.  Instead the split-off binary `#define`s `TRANSCRIPT_ONLY` and
 * `#include`s the parent fixture; this header, included by the parent AFTER
 * `harness.h`, then turns every substantive `diff_eq_*` comparison into a
 * no-op.  What is left running is the fixture's setup, its `diff_begin` /
 * `diff_end` frame, and the transcript checks -- which are written to call
 * `diff_eq_int_` DIRECTLY rather than through the `diff_eq_int` macro, so they
 * survive the no-op.
 *
 * The group that fails in the split-off binary therefore contains the
 * transcript checks and nothing else, which is what makes the declaration
 * check-granular rather than test-granular.  The parent is unchanged in
 * behaviour: with `TRANSCRIPT_ONLY` undefined this header defines nothing.
 *
 * A check written as `diff_eq_int("...", strcmp(dsplib_debug_capture_text(0),
 * dsplib_debug_capture_text(1)) == 0, 1, input)` becomes
 * `diff_eq_int_(__FILE__, __LINE__, "...", strcmp(...) == 0, 1, input)`.
 */

#ifndef DSPLIB_TRANSCRIPT_SPLIT_H
#define DSPLIB_TRANSCRIPT_SPLIT_H

#ifdef TRANSCRIPT_ONLY

/*
 * The macros first, so a substantive call written with the macro spelling is
 * removed before the macro expands.
 */
#undef diff_eq_int
#undef diff_eq_float
#undef diff_eq_float_ulp
#undef diff_eq_float_abs
#undef diff_eq_obj
#undef diff_eq_double

#define diff_eq_int(...)	((void)0)
#define diff_eq_float(...)	((void)0)
#define diff_eq_float_ulp(...)	((void)0)
#define diff_eq_float_abs(...)	((void)0)
#define diff_eq_obj(...)	((void)0)
#define diff_eq_double(...)	((void)0)

/*
 * And the function names, because a fixture may call a `_`-suffixed form
 * directly to name its own file and line.
 */
#define diff_eq_obj_(...)		((void)0)
#define diff_eq_obj_float_(...)		((void)0)
#define diff_eq_float_(...)		((void)0)
#define diff_eq_double_(...)		((void)0)

/*
 * `diff_eq_int_` is deliberately NOT removed: the transcript checks call it
 * directly, and it is the one comparison the split-off binary exists to run.
 */

#endif /* TRANSCRIPT_ONLY */

#endif /* DSPLIB_TRANSCRIPT_SPLIT_H */
