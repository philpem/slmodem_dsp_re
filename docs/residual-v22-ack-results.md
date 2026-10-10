# V.22 ACK lifetimes: no gain, an earlier bystander boundary

Pinned df4b23b9, Gentoo GCC 3.4.2-r2/assembler 2.15.92.0.2 and complete retained
flags including DSPLIB_REPRODUCE_BUGS. Domains recorded before compilation:
[square/verdict](residual-v22-ack-domain.md),
[count acquisition](residual-v22-ack-count-domain.md). Original ACK body209B.

| Explicit ideal square | Late verdict | Bytes | Strict verdict |
|---|---|---:|---|
| No | No |209| BYTES187 |
| Yes | No |189| SIZE20 |
| No | Yes |205| SIZE4 |
| Yes | Yes |185| SIZE24 |

Initial RTL demonstrates the intended lifetime change: the explicit square is
UID25 before both sample reads (UID41/68), rather than the existing square at
UID75 in the second loop. Verdict zero moves from UID31 to UID91. No original
RTL exists; these are retained compiler transformations checked against the
blob's final instruction witnesses, not original source proof.

The follow-up repeats the baseline and combined objects raw, then delays the
existing count read until ideal preparation is complete. Baseline with late
count stays209B/BYTES194; combined with late count stays185B/SIZE24, but its
complete object changes. In early RTL the count definition moves from UID13
to UID25, after idealEnergy UID22. No strict gain or exact loss in either cross.

Eight complete-TU cells /80 common body verdicts preserve the exact set4/10,
all bindings/imports/exports and metadata/allocated nontext/BSS/relocations.
Only ACK changes in individual controls; combined controls also change the
following nonexact Detect_1s. All other bodies stay identical. No source/header
adoption, type change, flag change, artificial scope or register fitting.
The finite domains are closed. Production remains1079/1852 EXACT.

Detect_1s has identical instruction patterns and UIDs in01.rtl and24.lreg;
twelve patterns differ at25.greg, eleven at27.flow2, thirteen at28.peephole2
and seventeen at30.rnreg. This is a global-allocation/reload boundary, earlier
than the previous constructor scratch examples. It does not identify the
responsible state or prove original allocation history. Next diagnostic, if
pursued: inspect global/reload decisions for the same unchanged function in
raw-reproducing baseline and combined TUs, rather than changing its source to
compensate. Distinguish the two passes before claiming allocator causality.

Replay tools: residual_v22_ack_reproduce.py, residual_v22_ack_count_reproduce.py,
residual_v22_ack_audit.py under tools/. Audit outputs
build/residual-v22-ack-audit.json and reports all eight cells. Its positive and
negative lifetime checks fire, and it checks the following detector's stage
boundary. New tools compile, reference and diff checks pass. No candidate
runtime validation or mutation/fuzzing execution is claimed. Unchanged production
source retains its previous period gate388/0.
