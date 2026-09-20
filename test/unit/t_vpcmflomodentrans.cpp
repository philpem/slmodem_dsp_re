/*
 * t_vpcmflomodentrans.cpp -- `getUinfoValue`'s L2 diagnostic transcript, in
 * its own binary on purpose.
 *
 * The parent fixture `t_vpcmflomodem` compares each side's `edprintf`
 * transcript with a `strcmp` from its `compare_all` helper.  On the modern
 * tier one L2 value in the `getUinfoValue` group prints one unit in its last
 * decimal place, while the state comparison, the return value and every other
 * group's transcript agree -- a rounding-level difference in a diagnostic's
 * float formatting, not a changed decision.  Finding F11368 rules that the
 * modern-tier float tolerance must never reach a transcript `strcmp`, so the
 * split `t_v90connevalnan` used applies (issue #143; findings
 * F2157/F3002/F6000-F6002).
 *
 * This file re-includes the parent with `TRANSCRIPT_ONLY` defined;
 * `test/unit/transcript_split.h` no-ops every substantive `diff_eq_*`
 * comparison, so what runs is the fixture's setup, its group frames and the
 * transcript checks.  Only `VPcmFloModem::getUinfoValue` diverges, and
 * `tools/gccdiverge.json` declares exactly that group.
 *
 * `make period` passes this file with the object's own compiler and flags.  An
 * excused binary exits non-zero, so this binary has NO mutation suite
 * (findings F2157/F3002); `t_vpcmflomodem` keeps `vpcmflomodem`'s 51
 * mutations and stays green under both compilers.
 */

#define TRANSCRIPT_ONLY
#include "t_vpcmflomodem.cpp"
