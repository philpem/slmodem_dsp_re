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

## Batch 3: V.29 callback member contracts

Two function-pointer members reviewed, eight previously unnamed parameters
named, zero unresolved parameters in this bounded batch:

- `v29_rx_detector::handler(modem, in, out, count)` receives the owning
  `v29_rx`, not its detector. The caller advances its input pointer by the
  difference between the incoming and remaining count, and its output pointer
  by the callback return. The return is a produced-word count, not a status.
- `v29_tx_params::handler(modem, in, out, budget)` receives the owning
  `v29_tx_root`, not its parameter block. The caller passes its input-word
  pointer unchanged, advances output by the callback return, and repeats
  while the signed output-sample budget remains positive. The budget is not
  an input count. Some state handlers only transition and return zero.

Names match the assigned `RxHdx*V29` and `TxHdx*V29` definitions. Contract
comments come from `V29RX_modem`/`V29TX_modem` dispatch and handler assignments,
not from a generic assumption about the last pointer argument. Member names
remain `handler`: each is explicitly documented as its active state handler.
No signature, member offset, return type or source expression changed.

Callback typedefs and other function-pointer members remain in scope for #100;
this batch does not claim the earlier single-line inventory covered them all.

Batch 3 validation: Gentoo GCC 3.4.2-r2 `make phase` passed 375 tests,
zero failures; all structural checks OK. All 655 baseline top-level period
objects remained SHA-256-identical after rebuilding. Logs are in
`build/structure-naming-v29-callbacks/` (`gates.log`, `object-identity.log`).

## Batch 4: V.32 callback typedefs

Three typedefs reviewed, twelve previously unnamed parameters named, zero
unresolved parameters in this bounded batch. Function-pointer types and all
consumers remain unchanged.

- `v32_txhdx_fn(modem, data, out, left)`: the caller passes the root modem,
  seeds `left` from `hdx->symbol_len`, advances the output pointer by the
  returned sample count, and repeats until the symbol budget is exhausted.
- `v32_rxhdx_fn(modem, in, out, count)`: the dispatcher forwards the root
  modem and arguments unchanged. `RxHdxData` overwrites the input count with
  the demodulated and clamped output count. This is not V.29's remaining-input
  convention; the callback has no return value.
- `v32_encoder_fn(smc, out, in, count)`: the registered absolute, differential
  and trellis symbol encoders append to the output ring. The trellis encoder
  modifies input words, so naming does not introduce a const qualifier.

Evidence: `V32TxHdxModem`, `V32RxHdxModem`, `RxHdxData`, the encoder assignments
in `V32.c`, and the symbol encoder definitions. Names match the implementations.
These typedefs feed `v32_hdx::tx_state`/`rx_state` and `v32_fp::encoders`;
those existing role-based member names do not need a cosmetic rename.

Batch 4 validation: Gentoo GCC 3.4.2-r2 `make phase` passed 375 tests,
zero failures, with all structural checks OK. All 655 baseline period object
files remained SHA-256-identical. Logs: `build/structure-naming-v32-callbacks/`
(`gates.log` and `object-identity.log`).

## Batch 5: V.17/V.27 callback members

Four state-handler members reviewed. V.17 gains eight parameter names;
V.27's eight member parameters were already named and are retained. Existing
named callback typedefs are not duplicated or relocated. No callback types,
member names, offsets, expressions or function definitions change.

The names `modem`, `in`, `out`, `count`/`budget` match assigned state functions.
The first argument is always the root modem, not the private/shared/source
subobject that stores the function pointer. Comments now distinguish:

- RX callback count: available input samples on entry, unconsumed input samples
  on return. The short return counts produced data words. Each top-level
  dispatcher replaces its caller's count with total output only after looping.
- TX callback budget: remaining symbols/data words, NOT output samples. The
  short return instead counts shaped output samples and advances the caller's
  output pointer. Some states only transition and return zero.

Evidence is independently traced for each mode: V17RX_modem/V17TX_modem,
TxHdxSilenceV17 and TxNoCarrierV17; V27RX_modem/V27TX_modem, assigned RxHdx/TxHdx
states, ModDataV27 and TxNoCarrierV27. A shape-compatible function pointer does
not establish compatible count units; trace through the leaf generator.

Batch 5 validation: Gentoo GCC 3.4.2-r2 `make phase` passed 375 tests,
zero failures, with structural checks OK. All 655 baseline period objects
remain SHA-256-identical. Logs: `build/structure-naming-v17-v27-callbacks/`.

Follow-up caution: batch 3 described the V.29 TX budget as output samples
based on dispatcher observations, without tracing its leaf generator's input
units. The V.17/V.27 evidence demonstrates why that inference is insufficient.
V.29's budget-unit wording must be audited before relying on it. This is a
pending documentation concern, not a measured V.29 behavioral failure.
