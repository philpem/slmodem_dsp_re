/*
 * t_v90cdadjusttrans.cpp -- the four members' diagnostic transcripts, in their
 * own binary on purpose.
 *
 * The parent fixture `t_v90cdadjust` raises both sides' debug level and
 * compares the four `edprintf` transcripts with a `strcmp` in its `run_loud`
 * group.  On the modern tier 26 of those checks differ, all in a float printed
 * to its last decimal place (`real K after optimization = +20.06272` against
 * the object's `+20.06271`, and the like), while all four members' substantive
 * object comparisons and the returned values agree -- a rounding-level
 * difference in a diagnostic's float formatting, not a changed decision.
 * Finding F11368 rules that the modern-tier float tolerance must never reach a
 * transcript `strcmp`, so the split `t_v90connevalnan` used applies (issue
 * #143; findings F2157/F3002/F6000-F6002).
 *
 * This file re-includes the parent with `TRANSCRIPT_ONLY` defined;
 * `test/unit/transcript_split.h` no-ops every substantive `diff_eq_*`
 * comparison, so what runs is the fixture's setup, its group frame and the
 * transcript checks.  `tools/gccdiverge.json` declares the one group.
 *
 * `make period` passes this file with the object's own compiler and flags.  An
 * excused binary exits non-zero, so this binary has NO mutation suite
 * (findings F2157/F3002); `t_v90cdadjust` keeps `v90cdadjust`'s 36 mutations
 * and stays green under both compilers.
 */

#define TRANSCRIPT_ONLY
#include "t_v90cdadjust.cpp"
