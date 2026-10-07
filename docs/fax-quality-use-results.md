# Quality monitor boundary controls: no exact gains

Base a95a6c65. No production source/header changes.22 full-TU cells,164 live
canonical body verdicts,0 gains. Two V29 split-tail/member-cached cells lose
untouched EpochDetectV29; no cells adopted. All symbol metadata/binding, named
and allocated noncode data, BSS, canonical nontext relocations agree with their
retained baseline. Three untouched full-TU raw controls reproduce. Every live
source/object hash and canonical verdict rechecked against saved JSON.

V17:5 distinct sources and5 distinct object emissions, target235 B baseline;
fixed owner238 B, direct tests263 B, split tails265 B versus original266 B.
V27:9 sources/5 objects, target246 B baseline; fixed owner249 B, membertests240 B,
split tails265 B, split+member249 B versus original266 B. Early AGC narrowing
always raw-merges with its corresponding no-narrowing cell, closing that axis
under retained flags. V29:8 sources/4 objects, target239 B baseline; direct
membertests237 B, split tails359 B, split+member327 B versus original266 B.
Early narrowing raw-merges here too. Size closeness is not a source preimage.
Counts sourced from the live comparator; no fuzz/mutation/runtime runs.

Fresh bystander evidence: EpochDetectV29 instruction patterns are unchanged
through26.postreload and27.flow2. At28.peephole2 UID38's new scratch load uses
CX in baseline versus AX in the losing source-control TU; UID39 compares that
scratch. At30.rnreg owner UID11 becomes DX versus CX, and candidate scratch
becomes DX. No independent allocation-stage difference: initial RTL/CSE,
combine/local/global reload patterns are identical. The source-unchanged
bystander changes at peephole scratch selection, then renaming, not an early
source type/field interpretation. Audit saves exact patterns with UID lists.
Do not reorder declarations or change EpochDetectV29 to restore the score.

Prior quality-owner tests are repeated only as fixed explanatory controls;
new axes are count-test uses and distinct averaging/judgement publication tails.
Both finite families now closed absent independent new body-stage evidence.
Original local signedness remains underdetermined where upper bits die; no
near265B local-type sweep justified. Null_process signed count/guard and
SDM_init were inspected but not compiled: null domain already tested; SDM_init
has the same aggregate dword+word copy and non-register operations, only colors
differ. That is no fresh source-factoring witness.

Reproduction:

    mkdir -p build
    python3 tools/fax_quality_use_reproduce.py --domain docs/fax-quality-use-domain.md --baseline-dir ../byteexact-x87-scheduling/build/tc_out
    python3 tools/fax_quality_use_audit.py

Complete per-cell compiler commands, hashes, saved preprocess/RTL/assembly/logs
in build/fax-quality-use; audit build/fax-quality-use-audit.json. Retained
Gentoo3.4.2-r2 complete flags, compiler-selected assembler2.15.92.0.2,
DSPLIB_REPRODUCE_BUGS appended last. Fresh worktree initially lacked build/;
first generator invocation failed before compiler execution. Directory created
and all22 valid cells subsequently compiled; no invalid compiled results used.
