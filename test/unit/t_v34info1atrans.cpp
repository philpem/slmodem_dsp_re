/*
 * t_v34info1atrans.cpp -- `V34SetINFO1aBits`'s `getUinfoValue` diagnostic
 * transcript, in its own binary on purpose.
 *
 * The parent fixture `t_v34info1a` compares each side's `edprintf` transcript
 * with a `strcmp` from its block helper.  On the modern tier 96 of the
 * transcript checks differ: the `getUinfoValue` L2 line prints one unit in its
 * last decimal place (`L2[15] = -0.17762` against the object's `-0.17761`, and
 * so on down the array), while the whole-block state comparison, the arena
 * comparison and the return value all agree.  That is a rounding-level
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
 * (findings F2157/F3002); `t_v34info1a` keeps `v34info1a`'s 47 mutations and
 * stays green under both compilers.
 */

#define TRANSCRIPT_ONLY
#include "t_v34info1a.cpp"
