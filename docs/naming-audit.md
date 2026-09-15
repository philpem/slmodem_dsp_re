# Structure and parameter naming audit (#100)

This is a semantic naming follow-up to the owner reconstruction in #99.
Names must be supported by uses or stronger evidence, not invented to make
an offset-only name disappear. No layout or algorithm change is intended.

## Initial candidate inventory

`naming-offset-candidates.txt` records 178 matching declaration lines in
`include/dsplib/*.h` before the first naming batch. The lexical scan selected
scalar/pointer/array declarations of primitive types whose first declarator
matches `[rf][0-9a-fA-F]{2,}`. It is a triage list, NOT a complete field census:
multiple declarators, other placeholder spellings and source-local types need
separate review. Line numbers describe the pre-batch snapshot.

`naming-parameter-candidates.txt` records 89 lines selected by a broad
single-line prototype/parameter heuristic. Known true candidates include
`V92EchoCanceller::setEchoDelay(unsigned int)` and
`V90Phase4Modulator::setSessionFlag(unsigned int)`. Known false positives
include `(void)` no-argument declarations and `for` loops in `Queue.h`.
These 89 lines are NOT a count of unnamed parameters. Multiline declarations,
definitions, function pointers and placeholder names still require review.

## Batch 1: V.32 symbol-mapping coder

Three fields reviewed and semantically named; zero unresolved in this batch.
Evidence is encoder usage, not recovered original identifier spelling.

| Owner | Offset | Old name | New name | Evidence |
|---|---|---|---|---|
| `v32_smc` | 0x0e | `f0e` | `trellis_diff_state` | Persistent index/result of `TrellisEncodeDifTable` in `SMCv32_encoder_tcm`. |
| `v32_smc` | 0x10 | `f10` | `trellis_state` | Persistent index/result of `TrellisTransitionTable`; prior state also selects the extra coded bit. |
| `v32_smc` | 0x14 | `f14` | `uncoded_bits` | Defines the low-bit mask and shift separating uncoded input bits from the differential/trellis-coded part. |

Consumers, initialization, fixtures and the existing mutation anchor retain
their expressions, widths and ordering. No function parameters changed in
this batch; the encoder declarations already name every parameter.

## Reviewed but unresolved: V.32 configuration/status

The bounded header/consumer audit found no safe new semantic names for the
two offset-only `v32fp_cfg` fields or eleven offset-only `v32_status` fields.
These 13 fields remain unresolved, not completed by cosmetic renaming.
`v32fp_cfg::r10` supplies a configuration option bit, but the diagnostic
letter and default do not establish its meaning. `r16` duplicates a rate in
one caller without a proven reader. Status copies establish provenance but
not necessarily meaning; `r06` and `r1c` are derived values, not direct copies.
Future work should trace original diagnostic strings and external consumers.

The `v32fp_ctl` examples `r02` and `r04` also need evidence beyond their
template values (9600 and 120000). Those defaults alone do not justify naming
them a receive rate and timeout. They remain in the uncompleted inventory.

## Next batches

Review unnamed parameters against definitions/callers, then continue V.32
configuration/status and V.22 owners. Keep unknown modelled fields distinct
from unmodelled padding. Name flags only when their meanings are established.
Every naming batch must pass the Gentoo period gate and a before/after
code-generation comparison; preserve all mutation faults and ABI signatures.

## Batch 1 measured result

Gentoo GCC 3.4.2-r2 `make phase`: 375 passed, 0 failed; structural checks
all OK. All 655 pre-batch top-level `build/period/*.o` files passed SHA-256
identity comparison after rebuilding, with zero differences. This includes
source and fixture objects and establishes that these renames did not change
those period object bytes. Logs: `build/structure-naming-v32-trellis/gates.log`
and `build/structure-naming-v32-trellis/object-identity.log`.

## Batch 2: C++ parameter declarations

Six previously unnamed declaration parameters reviewed and named, none
unresolved in this batch. Signatures, types and calling conventions stay
unchanged. Existing definition names are reused for `delay`, `flag` and
`next`; the abbreviated mapping parameters become `mapping` in declarations
and definitions together.

| Method | Parameter | Evidence |
|---|---|---|
| `V92EchoCanceller::setEchoDelay` | `delay` | Stored in `echoDelay`, adjusts `echoLength` by the delay delta, and printed as echoDelay. |
| `V90Phase4Modulator::setSessionFlag` | `flag` | Direct assignment to `sessionFlag`; also the caller chain's name. |
| `V90Phase4Modulator::setRdRtSymbols` | `mapping` | `V90MappingParams` source of the six constellation levels used to build Rd/Rt symbols. |
| `V90Phase4Modulator::setRfSymbols` | `mapping` | Same typed constellation source used to build the twelve Rf symbols. |
| `V90Phase4Modulator::setNextStateAfterTRN2d` | `next` | Typed state stored directly in `nextStateAfterTRN2d`. |
| `V90Phase4Modulator::setMappingParams` | `mapping` | Null-checked constellation passed to `bitsToSymbol->reset`; diagnostic names mappingParams. |

The mapping names replace `m`/`mp` without renaming the unrelated `V90MP`
member called `mp`. Mutation anchors retain their original injected faults.
The initial candidate lists remain historical snapshots, not live counts;
these six parameters are the first completed parameter batch, not a claim
that either header or the tree has been exhaustively audited.

Batch 2 validation: Gentoo GCC 3.4.2-r2 `make phase` passed 375 tests,
zero failures, with all structural checks OK. All 655 baseline top-level
period object files remained byte-identical by SHA-256 after rebuilding.
Artifacts: `build/structure-naming-cpp-parameters/gates.log` and
`build/structure-naming-cpp-parameters/object-identity.log`.
