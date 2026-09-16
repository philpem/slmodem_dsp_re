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
  while the signed symbol budget remains positive. The data state consumes
  one FIFO word per symbol. Some state handlers only transition and return zero.

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

Correction to batch 3: the V.29 TX budget counts symbols, NOT output samples.
The earlier description inferred units from dispatcher observations without
tracing the leaf generator and was wrong. TxHdxQuietV29 passes n to
TxNoCarrierV29 and subtracts that same n from budget. TxNoCarrierV29 writes
exactly n symbol-ring slots before FPM_PPS_filter returns the shaped sample
count. TxHdxDataV29 likewise subtracts the FIFO words passed to ModDataV29,
not the sample count returned by it. The header comment and batch 3 ledger
now reflect this evidence. This correction changes comments/documentation
only; no executable expression, callback type or layout changed. The existing
375/375 period gate and 655-object identity results precede this documentation
correction; they are not a new test run or proof of comment semantics.

## Batch 6: service callback member names

The multiline lexical scan in `naming-callback-candidates.tsv` records 56
explicit function-pointer declaration candidates under `include/dsplib/*.h`
before this batch. It recognizes `(*identifier)(...)` with non-nested argument
lists; typedef-based member declarations, other declarator syntax, source-local
callbacks and semantic quality are outside this scan. It is a candidate
snapshot, not a proof that all callbacks in the project have been audited.
Eight candidates have offset-only `fn_hex` declarator names, all in the
beep/voice group. Existing shared equalizer and datapump callbacks inspected
in this pass already have named parameters.

Nine member names are resolved in this batch: those eight offset-only names
and one misleading existing name. Registration evidence, rather than analogy,
settles their roles:

| Type | Old member | New member | Evidence |
|---|---|---|---|
| `voice_config` | `fn_04` | `get_sreg` | VOICE_create installs vce_get_sreg; receive setup queries settings through it. |
| `voice_config` | `fn_08` | `hook_on` | VOICE_create installs vce_hook_on. |
| `voice_config` | `fn_0c` | `hook_off` | VOICE_create installs vce_hook_off. |
| `beepgen_config` | `fn_04` | `hook_on` | voice_create maps voice_config's hook-on slot here. |
| `beepgen_config` | `fn_08` | `hook_off` | voice_create maps voice_config's hook-off slot here. |
| `beepgen_config` | `fn_0c` | `get_sreg` | voice_create maps the settings getter here. |
| `beepgen` | `fn_011c` | `hook_on` | Copied from the corresponding config slot; invoked when the flash marker starts. |
| `beepgen` | `hook_on_proc` | `hook_off` | Receives vce_hook_off through config; invoked when the flash marker ends. |
| `beepgen` | `fn_0124` | `get_sreg` | Used with register 24 to obtain flash timing. |

The original diagnostic string "Hook on proc" is deliberately preserved even
though its adjacent callback receives vce_hook_off. Debug strings are strong
naming evidence, not infallible evidence: the concrete registered callback
settles this discrepancy. Return signedness and callback casts remain as
reconstructed; naming is not an opportunity to change their contracts.

Known callback parameter names, runtime expressions, mutation labels and
mutation fault semantics remain unchanged. The earlier candidate snapshots
are retained as historical inventories rather than silently rewritten totals.

Batch 6 validation: the initial gate passed 373 tests and failed to compile
`t_beepgen` and `t_voicesvc` because fixture helper/member renames were
incomplete. Those references were corrected without changing trace tags or
checks. The deciding rerun, `build/structure-naming-service-callbacks-fixed/`
`gates.log`, passed all 375 period tests with structural checks OK.

Before/after SHA-256 results: 652 of 655 baseline objects identical. All 275
objects whose paths identify reconstruction `src/` files are identical; zero
source-object differences. The three different objects are the fixtures
`t_beepgen`, `t_voicesvc`, and `t_voiceapi`, whose callback helper identifiers
were renamed. No full-object or instruction-identity claim is made for those
three fixtures. The differential gate verifies their runtime checks still
pass. Full comparison: `object-identity.log` beside the deciding gate log.

## Batch 7: V.92 echo-canceller state fields

Two fields reviewed and named; no unresolved member in this bounded batch:

| Owner | Offset | Old name | New name | Evidence |
|---|---|---|---|---|
| `V92EchoCanceller` | 0x10 | `word_10` | `updateSampleCount` | process adds the block count and compares against updateDuration; setState clears it on a transition. The filter-only process arm does not advance it. |
| `V92EchoCanceller` | 0x18 | `word_18` | `filterLengthMinusOne` | Constructor stores unsigned length minus one; history sizing and process wrap limits read this cached quantity. |

These are usage-derived semantic names, not claims to recover original
identifiers. Missing original spelling alone is not an unresolved role.
The old header's assertion that +0x18 was only read during construction was
stale: process subtracts it from historyAlloc. Its independent storage, reads,
unsigned underflow at zero filter length, and all arithmetic are preserved.
Do not replace the cached field with a recomputation from filterLength.

Consumers, live offset assertions, fixtures and mutation find/replace anchors
use the new names. Mutation labels and fixture diagnostic strings remain
unchanged. Similarly named fields in other owners (including additionalCPinfo
and V92 phase-four state) are deliberately untouched.

Batch 7 validation: the first run passed all 375 differential tests but
rejected three stale mutation anchors. Their escaped-tab strings required
explicit token updates; mutation labels and injected faults were preserved.
The deciding rerun passed 375 tests with zero failures and all structural
checks OK. All 655 baseline period objects remained SHA-256-identical.
Artifacts: `build/structure-naming-v92-echo-fields-anchors/gates.log` and
`object-identity.log` in the same directory.

## Batch 8: demodulator/message parameters and unused arguments

Nine unnamed declaration slots named across six methods. Seven have
implementation-established roles; two are explicitly unused in their bodies:

| Method | Names | Evidence |
|---|---|---|
| `V90Demodulator::progress` | `out`, `nofOut`, `in`, `nofIn` | Existing definition names distinguish output bits/count from input samples/count. |
| `V90Demodulator::reset` | `quickConnectArg` | Existing definition's name; no new interpretation of its values. |
| `V92Jd::setJdPhase` | `jdPhase` | Definition scales the argument by 65536 before encoding phase-message bits. |
| `V92Jd::setRatesMask` | `mask` | Definition extracts rate-selection bits into the two message groups. |
| `ResamplerTiming::reset` | `unused` | Implementation does not read it; its historical meaning remains unspecified. |
| `ResamplerTimingOffset::timingCorrection` | `unusedSample` | Override ignores the per-output-sample argument and advances phase by its constant timingOffset instead. |

The two intentionally unused arguments are named consistently in declarations
and definitions; no dummy expression, signature change or removal is introduced.
V92Jd comments claiming these setters were undefined were stale and are corrected.
Other comments about staged reconstruction are not treated as live coverage data.

The audit of ordinary method declarations in V90Modulator and
V90Phase3Modulator found their parameters already named; constructors and
semantic quality of existing shorthand names were not exhaustively audited.
V90Demodulator::enterChannelVerification remains unresolved: the first
argument is unused/unnamed in the definition and the second is the offset-based
placeholder short414. No semantic roles were invented for either. The three
ResamplerTiming float declarations using documented implementation shorthand
`v` remain for a later consistent semantic rename rather than assigning better
names only in the header.

Documentation convention: concise Doxygen at the method/member is the primary
reader-facing explanation, including internal APIs. Record input/output roles,
units and intentionally ignored arguments where established. Keep extended
reconstruction evidence here rather than forcing readers through the audit
history to understand an interface. This batch also gives the two previously
named echo-canceller fields Doxygen member descriptions.

Batch 8 validation: the first run passed 375 differential tests but found one
mutation anchor still spelling an anonymous parameter. Only its find string
was synchronized; the injected argument-scaling fault and label were retained.
The final Doxygen-inclusive gate passed 375 tests, zero failures, and all
structural checks. All 655 baseline period objects remained SHA-256-identical.
Artifacts: `build/structure-naming-demodulator-parameters-doxygen/gates.log`
and `object-identity.log` beside it.

## Batch 9: concise callback Doxygen

After review and merge of #102 through #108, convert the nine previously
explained V17/V27/V29/V32 callback declarations to Doxygen. Keep the root
context, buffer roles, symbol/sample units, in/out counts and return semantics
beside each declaration. The longer derivations remain in earlier batches of
this ledger. This is a comments-only follow-up: no identifiers, declarations,
expressions or layout change. No new local gate or Doxygen rendering run is
claimed for this documentation-only batch.

The merge review found no blocking source, ABI, test or mutation-fault issues.
The five changed mutation manifests retained their labels and counts (67, 8,
34, 44 and 95 respectively), and each merged PR had green Gentoo CI checks.
The existing byte-identity and local gate measurements remain recorded per
batch above, not re-labelled as measurements of these comments.
