# AGC observed fourth-argument recovery

F11613. [Predeclared two-cell, twelve-TU domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951078263) at d3abfa0b.
The blob supplies nineteen explicit fourth slots in seventeen calling functions.
Seventeen are literal1. The second calls in v23FP_rx_progress (0x8710c) and
DemodDataB103 (0x8fc0a) pass the sign-extended original input count, not the
resampler output. B103 therefore retains that original short before count is
reassigned. The 566B callee reads only its first three parameters. An unread
slot does not establish an absent formal; the earlier behavioral rationale
(F8234/F8857/F8875/F8895/F9116) did not exhaust byte reconstruction.

Restore all observed slots consistently with an ignored int formal, conventional
rather than uniquely recovered original width/name/meaning. Preserve void and
all caller signal-field reloads. EAX consumption supports an independent return
hypothesis: neither frame reads nor this reconstruction's header establish the
original return type. Do not combine that axis with these argument controls.

Twenty-four valid complete compilations across twelve TUs, 126 functions and 24 named data objects.
RxHdxNoSignal207→223B is the sole complete EXACT gain, no losses. The callee's
complete object raw-merges; eighteen canonical bodies change, 108 unchanged.
All seventeen calling bodies change. Only noncalling bystander QualityDetectV27
changes: at unchanged246B, the transition to state33 moves from a jump through
a common store to a local word store and a jump past the common store. Same
field0x4f4a/value33/callee targets; record this control factoring, not merely
register renaming. Other caller sizes and scheduling are not uniformly closer.
All symbol names/types/binding/visibility/imports/exports and allocated nontext
contents/flags/alignment/relocation records/addends agree. Per-TU complete
object audits and changed-body disassemblies retained under
build/playbook-agc-unused-argument. Actual complete saved flags, mandatory
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header hashes and initial RTL are retained. Replay
tools/playbook_agc_unused_argument.py --domain <linked URL>.

Production API consumers are adapted; fixed reference probes with three
arguments remain separate measurement apparatus because the fourth is unread.
No fuzzing or mutation execution. All300 period objects rebuild; exactly eleven caller objects change, all
twelve selected TUs raw-match promoted complete cells (callee unchanged).
Census896→897/1852,90290→90513 exact bytes, one gain/no losses. Complete
same-order partial links remain DIFFERENT: positioned bytes68637→68786/943398,
allocated914382→914510, sections70/92 and symbols394/2907 unchanged,
relocations1021→1024/18317. Artifacts under
build/playbook-agc-unused-argument-adoption. Initial sandbox run could not
access Docker; elevated rerun passed385 period tests but was structurally red
on two stale AGC static anchors. Retarget their find/replace without changing
the wrong-buffer/wrong-object checks; a transient overly broad replacement
also touched ECC anchors and was reverted before the final gate. Preserve
initial red artifacts, do not reinterpret them as green. Final fixed make phase passes385 tests/0 failures and every structural
gate; static anchors285 suites/10038, zero detached/nonunique. No tests
weakened, no modern portability claim. Do not cite the historical V29
return probe as period evidence; its old period crash is a recorded limitation.
