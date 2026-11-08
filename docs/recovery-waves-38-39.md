# Waves 38–39: layout stages and missing visual histories

Base: `6f3e8988b82492a60e2cce87867b9dc86cbb61cd` (PR #415). Issue
[#409](https://github.com/philpem/slmodem_dsp_re/issues/409) carries both
predeclared contracts. Neither investigation produces a production source,
flag or maintained fixture change.

## Wave 38: two candidate layout decisions

The unchanged full-TU scratch build reproduces complete production object bytes.
It uses the actual period configuration, shared helper's final bug-reproduction
define, Gentoo G++ 3.4.2-r2 and executed assembler 2.15.92.0.2 (20040927).
Identical preprocessed inputs and raw baseline identity license the dump path.

Native selector-body order is `5,7,8,0,1,2,3,4,6`. Candidate stages show:

| Stages | Selector order | Emitted RTL returns |
|---|---|---:|
| `.01.rtl` through `.26.postreload` | `0,1,2,3,4,5,6,8,7` | 0 |
| `.27.flow2` through `.30.rnreg` | same source-arm order | 1 |
| `.31.bbro` through `.33.sched2` | `8,7,0,1,2,3,4,5,6` | 2 |
| `.34.stack` and `.35.mach` | `2,7,8,0,1,3,4,5,6` | 2 |

The `.34.stack` dump contains cleanup/cross-jumping and a second STC reorder.
Its name does not prove that x87 conversion itself chooses the final layout.
Block identifiers change; the ledger follows selector labels and edges. Original
RTL is unavailable: this is a candidate trace compared with native final topology.

After the baseline diagnosis, one `-fno-reorder-blocks` control was declared and
compiled. The domain is **two successful objects / six function verdicts**, zero
rejections. The control stays constellation-first, has one return and does not
recover native near-echo-first layout. All 26 pre-bbro dumps agree after removing
only diagnostic host/block-note pointers, printed assembler-label serials and
GCSE diagnostic hashes. UIDs, operands, constants and CFG references remain;
the first instruction/CFG difference between these candidates is `.31.bbro`.
The option affects both invocations, so it does not isolate either one.

| Function | Native bytes | Retained | No-reorder control |
|---|---:|---:|---:|
| `VPcmV34GetSNR` | 99 | 99, EXACT | 101, SIZE |
| `VPcmV34GetDiagnostics` | 821 | 823 | 787 |
| `VPcmV34GetVisualDiagnostics` | 1023 | 1006 | 902 |

All three bodies change. No exact/grade-0-or-1 gains occur; the sole loss is SNR.
Symbol identities/bindings/defining sections remain unchanged. Raw relocations
change 40→39 because two digital diagnostic paths share one debug-print call.
Jump-table addends and string packing change; constants/data/bss remain unchanged.
Full raw ledgers are retained rather than replaced by count-only assertions.

All nine switch selectors preserve their syntactically reachable call sets in
native, baseline and control: **27 rows, zero unsupported/ambiguous**. Swapping
selector-0/2 mapping fails **2/9 rows**. This calibrates the mapping check, not
feasible modem histories. Previous local boundary classes remain in the control.

No candidate or flag is adopted. The next discriminator is zero-result-tail
predecessor/value provenance: native DFE failure joins `0x72cb`, while candidate
failure can reach the primary epilogue with its initially zero result. Compare
those paths with pre-bbro probabilities and both STC decisions before another
source/pass experiment. The completed two-cell option domain remains closed.

## Wave 39: constructor session type is not live status

The probe uses registered constructors for ids 34/92, caller 1, rate 9600 and
fragment 48 with supported host parameters, then exported retrain 92. No live
status, role, receiver, count, coefficient, cursor, callback or history is planted.

| Id / observation | Status | Role | Progress | V90 receiver | K56 receiver | PCM type |
|---|---:|---:|---:|---:|---:|---:|
| 34 / constructor | 0 | `0x65` | 0 | 0 | 0 | 0 |
| 34 / retrain92 | 0 | `0x65` | 7 | 0 | 0 | 0 |
| 92 / constructor | 4 | `0x66` | 10 | 1 | 0 | 0 |
| 92 / retrain92 | 0 | `0x66` | 7 | 1 | 0 | 1 |

Native quick-connect overrides explain the constructor difference; the wrapper's
caller argument alone does not license role `0x65`. Retrain selects configuration
but unconditionally clears live status at `0x68e9`. Progress at owner+4 is not
status at owner+0; nearby stores of 2/3 to progress are not status producers.

Reviewed status-2/3 stores at `0xc30d`/`0xc346` and delayed transitions at
`0xcf54`/`0xcf85` require actual phase-2 progress and negotiated receiver greater
than 1. Those input/event histories were not established. The native inventory is
**31 selected bodies / 31 successful captures**, not an impossibility proof.

PCM echo storage is genuinely constructed and allocated. Its reset, ARMA-filtered
history, processing windows and output-sentinel contract were audited. Storage
and caller DSP inputs do not establish enclosing status 2. Selector 8 additionally
needs status 3/caller role; its K56 callee at `0x10240` is `xor eax,eax; ret`.
Unchanged output cannot distinguish that call from a guard. A future valid
history needs call-target observation as well as output comparison.

The final green/control/restored packet executes **three invocations, 12 graphs,
24 observations and 66 explicit lifecycle calls**, attempting 183 assertions.
Both green runs pass **61/61**. Observer-only progress+4 instead of status+0
fails **6/61**, without changing live state. This validates the status observer,
not an unimplemented visual fixture's output sensitivity. No width claim follows
from a count of 180. New maintained diagnostic calls/assertions: **0/0**.

The fixture remains byte-identical, with **47,561 assertions**, including the
previous 278 constructed assertions and 3600 synthetic grid trials. Focused baseline
and final period runs each pass **1/0**. Initial apparatus path failure, incorrect
constructor expectations and allocator-fill observations are preserved and
excluded from final claims; production source/live state were not changed to
accommodate them. Neither missing visual path is claimed reached.

## Evidence and continuation

Artifacts under `/mnt/zfs/dev/slmodem/scratch/independent-recovery/` include
`w38-layout/REPORT.md`, contract, raw objects, stages, complete-TU/ELF ledgers and
mapping controls; `w39-history/notes.md`, native captures, `probe-final.log`,
unchanged fixture identities and hashes. Independent read-only review checked
the packets; the parent additionally inspected the diagnostic substitutions.

Continue from native zero-tail/value provenance or genuine phase-2/receiver
histories. Do not repeat the no-reorder control, manufacture statuses, or count
an unobserved zero-return call as coverage. The handover records parent gate
and delivery details.

Final parent `make phase J=8` passes **394 period differential binaries, zero
failures**, with all structural checks passing (`w38-39-phase.log`, lines 528/530).
This is the unchanged production/fixture gate, not validation of an adopted
experimental flag or new maintained coverage.
