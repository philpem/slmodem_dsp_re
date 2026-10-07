# Medium fax service values and factoring

Base a95a6c65. Four initial value-timing cells and seven follow-up factoring
cells in full fax.c:11 compilations,44 live canonical body verdicts,0 gains,
0 losses. Raw baseline reproduces in both packages. No production source/header
adoption. Complete object/source hashes and live canonical verdicts rechecked;
all four functions, imports/exports/type/binding/visibility and BSS reviewed.
Gentoo3.4.2-r2 complete retained flags, executed selected assembler2.15.92.0.2,
mandatory bugdefine appended last; preprocess/RTL/annotated assembly/logs saved.

| Source property | Command verdict | Constructor verdict |
| --- | --- | --- |
| retained | SIZE8 | SIZE3 |
| initialized extra | SIZE5 | SIZE3 |
| mode before S7 | SIZE8 | SIZE1 |
| both | SIZE5 | SIZE1 |
| explicit sixth command case | SIZE8 | SIZE3 |
| initialized extra + sixth case | SIZE12 | SIZE3 |
| common converter result | SIZE8 | SIZE48 |
| mode before S7 + common result | SIZE8 | SIZE32 |

These are length gaps, not differing-byte counts. No near-size candidate is
adopted; no local type/register/slot/order/profile search followed the misses.

Original command clears EBP before dispatch and sets0x50 only for FTM. Source
has `int extra;`, assigned only by FTM. GCC therefore emits80 for the fourth
argument on every successful command. Initializing the default restores the
original argument rule but not the whole body. This is an uninitialized caller
value/source-fidelity defect, not a measured operational modem failure:
original and reconstructed fax_class1_command consume arg4 only in TM, where
both callers already supply80. Other five successful cases ignore the value.
Existing t_faxcreate uses real constructed sessions across all six commands;
it compares return/state, not callee-entry arg4. Thus a passing public fixture
would not establish call-argument fidelity, and a synthetic replacement callee
reading an otherwise ignored argument would not prove public modem divergence.
No runtime claim made or partial source adopted.

Original stores mode in the local config before modem_get_sreg; current after.
The pre-call control reaches563 B versus original564 B, without exactness.
Original two converter-result branches converge on one store/test each; common
local result restored that source structure but expanded to612 B (596 B with
pre-call mode) versus564 B. Both finite constructor families are now closed
without another independently observed use/alias/CFG property.

Original dispatch table has six entries. Current source makes FRH default,
so GCC emits five entries plus a separate guard before the table. Explicit FRH
restores six entries but does not close the instruction body. It also changes
source-unchanged FAX_process, whose emitted body/table destinations are audited.
Mode-only constructor cells change source-unchanged FAX_class1_command without
restoring complete identity. Preserve these TU interactions instead of fitting
an unchanged sibling or flattening the result to an exact-count score.

Nontext review is explicit, not a blanket invariant exception. Initial extra
controls reorder .rodata.str1.1: flags0x32, alignment1, entrysize1,375 bytes,
identical full NUL-terminated literal multiset; audit records each literal's
before/after offset. Other noncode sections unchanged. Explicit six-case
controls extend relocation-covered .rodata from64 to68 bytes, adding exactly
one command-table slot:6 command destinations and11 FAX_process destinations,
versus5+11 baseline. Every relocation resolves to a named owner and real
instruction boundary through inspect(); full offset/target inventory saved.
No named data or BSS changes. Changes to existing block offsets remain reviewed
negative artifacts, not a claim that boundary validity alone proves semantic
jump-table equivalence or permits adoption.

Together with quality controls, this scoped pass has33 complete-TU cells and
208 live body verdicts,0 gains, two losing quality cells on EpochDetectV29.
Quality has its separate initial/CSE/allocation/peephole/renaming trace; no
runtime/fuzz/mutation runs, no source/header changes, no commit by this agent.

Replay:

    mkdir -p build
    python3 tools/fax_service_values_reproduce.py --domain docs/fax-service-values-domain.md --baseline-dir ../byteexact-x87-scheduling/build/tc_out
    python3 tools/fax_service_factoring_reproduce.py --domain docs/fax-service-values-domain.md --baseline-dir ../byteexact-x87-scheduling/build/tc_out
    python3 tools/fax_service_values_audit.py

Artifacts build/fax-service-values/results.json,
build/fax-service-factoring/results.json,build/fax-service-values-audit.json.
Source forms in tools are replay candidates only, not production corrections.
