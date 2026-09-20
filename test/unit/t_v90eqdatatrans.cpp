/*
 * t_v90eqdatatrans.cpp -- `enterDataPhase`'s diagnostic transcript, in its
 * own binary on purpose.
 *
 * The parent fixture `t_v90eqdata` compares the object's `edprintf` transcript
 * with a `strcmp` alongside every substantive state comparison.  On the modern
 * tier the DFE `coefs sum` line prints one unit in its last decimal place
 * (`+0.005850` against the object's `+0.005849`) -- the state comparison and
 * the return value both agree, so this is a rounding-level difference in a
 * diagnostic's float formatting and not a changed decision.  Finding F11368
 * rules that the modern-tier float tolerance must never reach a transcript
 * `strcmp`, so the sanctioned remedy is the split `t_v90connevalnan` used
 * (issue #143; findings F2157/F3002/F6000-F6002).
 *
 * This file re-includes the parent with `TRANSCRIPT_ONLY` defined;
 * `test/unit/transcript_split.h` then no-ops every substantive `diff_eq_*`
 * comparison, leaving the fixture's setup, its group frame and the transcript
 * checks.  The failing group therefore contains the transcript checks and
 * nothing else, and `tools/gccdiverge.json` declares exactly that group.
 *
 * `make period` passes this file with the object's own compiler and flags, so
 * the source is the object's and the modern build is what cannot reproduce it.
 * An excused binary exits non-zero, so this binary has NO mutation suite
 * (findings F2157/F3002); `t_v90eqdata` keeps `v90eqdata`'s 11 mutations and
 * stays green under both compilers.
 */

#define TRANSCRIPT_ONLY
#include "t_v90eqdata.cpp"
