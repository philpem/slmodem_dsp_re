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

## Batch 10: remaining scalar and stub parameters

Named 24 previously unnamed parameter slots across 14 methods/functions:
seven active arguments have names supported by their definitions, and seventeen
ignored ABI arguments have explicit `unused` names rather than invented roles.
This covers V90Phase3Demodulator, V90ConnectionEvaluator,
V90AutoDigitalImpDetector, V90Phase4Demodulator, V92Phase3Modulator, V92CP,
and the K56flex stubs in NoK56Flex.cpp. Signature-based mutation anchors follow
the declaration changes; injected faults and labels are unchanged.

The K56flex constructor, rate setter, demodulator stub and allocation wrapper
ignore their arguments. Their comments deliberately make no claim about the
missing implementation's argument meanings. The phase-four known-symbol
routine likewise ignores its short argument and obtains the known symbol from
its local modulator.

### Remaining scope estimate

The pre-batch lexical header inventory found 175 primitive `rXX`/`fXX`
declaration lines, 447 other non-padding offset/neutral declaration lines,
and 102 padding-only lines. These are triage counts, not a complete field
census: source-local types, multiline declarations and other spellings are not
fully covered. Historical candidate lists remain historical snapshots.

Allow roughly 20-40 owner-level evidence batches for the field audit, plus
further parameter cleanup. This is a work estimate, not a completion
percentage. Each candidate needs either a supported name with a concise local
Doxygen explanation or an explicit evidence-based unresolved disposition.
Unreviewed fields cannot be declared unresolved merely to close issue #100.

Validation for this batch is pending the Gentoo period reconstruction gate
and comparison against the pre-edit period-object hashes.

Batch 10 measured result: Gentoo period differential **375 passed, 0 failed**;
all **655/655** baseline period objects are byte-identical. The overall
`make phase` result is **FAIL**, because two `v92cpcrc` mutation anchors still
use the former `setSUV` argument spelling: "setSUV stores fifteen rather than
sixteen" and "setSUV stores its argument in both slots". No commit is ready
until those anchors are synchronized without changing their injected faults
and the structural gate is green.

Next bounded owner audit: V90ConnectionEvaluator has ten proposed semantic
names and one neutral disposition. The object's status text directly supports
rateUpCounter, rateDownCounter, retrainCounter and fadeCounter. Access patterns
support dataDurationCounter, debugPeriodCounter, phase3FallbackDuration,
phase4FallbackDuration and silenceRrnRequested. The diagnostic "FORCED rate
down on silence rrn" supports forceRateDownOnSilenceRrn. These proposals are
not yet applied. Keep word_b8 neutral: its observed multiplication of avePdsnr
and assignment from a local scale do not establish its full semantic role.

Batch 10 correction and final validation: both `v92cpcrc` anchors and their
replacement expressions now use `value`, preserving the same injected faults
and labels. The corrected Gentoo `make phase` exited 0: **375 passed, 0 failed**,
with all structural checks green. The repeated hash comparison found **655/655
period objects byte-identical** to the pre-edit baseline. The initial failed
run is retained above rather than represented as a successful gate.

## Batch 11: V90ConnectionEvaluator field names

Audited eleven remaining neutral field candidates in this owner: ten received
semantic names; one remains explicitly unresolved. This is an owner-scoped
candidate denominator, not a claim that the project-wide inventory is complete.

| Previous name | Disposition | Evidence |
| --- | --- | --- |
| word_10 | rateUpCounter | Original status diagnostic |
| word_14 | rateDownCounter | Original status diagnostic |
| word_18 | retrainCounter | Original status diagnostic |
| word_1c | dataDurationCounter | Accumulates symbol counts against both minimum data-duration thresholds; usage inference |
| word_20 | fadeCounter | Original status diagnostic |
| word_24 | debugPeriodCounter | Accumulates symbols and resets at the debug-period comparison; usage inference |
| word_64 | phase3FallbackDuration | Phase-3 counter comparison; distinct from the Phase-4 control |
| word_68 | phase4FallbackDuration | Phase-4 counter comparison; mutation controls exercise the distinction |
| word_90 | silenceRrnRequest | Parameter request value and external silence-RRN consumers; not assumed Boolean |
| word_98 | forceRateDownOnSilenceRrn | External demand code 5 and original forced-rate-down diagnostic |
| word_b8 | Retain neutral | Multiplies avePdsnr after assignment from a local scale; full semantic scope remains unknown |

Production consumers, offset assertions, fixtures and mutation find/replacement
identifiers follow the owner names. Diagnostic strings, mutation labels and
injected faults are retained. Header Doxygen records bounded interpretations.
Validation pending the Gentoo period gate and pre-edit object hash comparison.

Batch 11 final validation: Gentoo `make phase` exited 0, **375 passed, 0 failed**,
all structural checks green, and **655/655 baseline period objects byte-identical**.
The final names at +0x64/+0x68 are `phase3FallbackDuration` and
`phase4FallbackDuration`: their thresholds cause fallback, even though the
counter they compare is printed as `rateUpCounter` by the original diagnostic.

The first two gate attempts failed on missed consumers and damaged mutation
JSON. The two affected manifests were rebuilt independently from their
committed originals, transforming decoded find/replacement strings only.
Independent boundary-aware inverse comparison established that both retain
exactly their original content apart from the intended identifier renames.
The first successful gate left one test object different because assertion
labels had changed; restoring those labels and rerunning the full gate produced
the 655/655 final result. No failed run is counted as a successful gate.

Method note: retain owner scope when renaming common offset names; a matching
offset in another class is not the same field. Transform JSON property values,
not surrounding syntax, and preserve mutation labels and deliberate faults.
Decode escaped strings before identifier matching, and use identifier boundaries
for inverse comparisons so `retrainCounter` does not alter
`retrainCounterFadeCount`. A green anchor-count check does not validate identifiers
introduced only by a mutant replacement; review those independently.

## Batch 12: V90AutoDigitalImpDetector field dispositions

Audited seventeen still-neutral declarations in this owner, correcting the
older lexical estimate of eighteen. Five receive semantic names; twelve retain
neutral names with explicit constraints. This is not a project-wide census.

| Previous name | Disposition | Evidence |
| --- | --- | --- |
| byte_280c | Retain neutral | Rough-mapping consumers interpret nonzero as suspect, while QC mapping treats it as a usable phase verdict; no single Boolean meaning is established |
| byte_a96a | suspectedPhaseCount | Incremented exactly when a phase is marked suspect |
| byte_a954 | originalMaxUcode | Original diagnostic names the pre-floor maximum code |
| short_a97a | minMaxUcode | Floor for the usable-code scan and selected maximum; usage inference |
| float_a970 | altRbsVarianceThresholdFactor | Reset selects factor from altRbsExpected; consumed by getAltVarThresh |
| float_a97c | padGainSearchScale | Mapping projection scale in findPadGain; usage inference |
| short_a948 | Retain neutral | Reset-cleared, later-set latch without an established read-side role |
| float_a950 | Retain neutral | Reciprocal source for QC-study threshold setup; underlying quantity unknown |
| float_a974 | Retain neutral | Reset writes 5.0f; no established read-side role |
| short_a978 | Retain neutral | Connection-type-selected configuration word; represented condition unknown |
| float_a980 | Retain neutral | Variance-derived multiplier; exact policy role unestablished |
| int_a98c, int_a990, int_a994, int_a998, int_a99c, int_a9a0 | Retain six neutral members | Independently copied study parameters; five timing consumers and one unread slot do not establish original semantic roles |

Local Doxygen records these constraints. Widths, signedness and storage remain
unchanged. Validation and independent scope/mutation review are pending.

Batch 12 final validation: Gentoo `make phase` exited 0, **375 passed, 0 failed**,
all structural checks green, **655/655 period objects byte-identical**.
Independent review preserved mutation labels, counts and injected faults under
boundary-aware inverse comparison. It also rejected the proposed name
`suspectedPhaseFlag`: +0x280c has incompatible consumer interpretations, so
`byte_280c` and its existing warning remain. Final denominator: **17 reviewed,
5 semantically named, 12 explicitly unresolved**. A no-op visit to the mutation
snapshot did not change its content and is not part of the batch.

## Batch 13: V90CP field names

Owner-scoped pass over `V90CP`, following the sibling `V90MP` rather than
re-deriving roles. Seven members are renamed; the rest of the class keeps its
existing offset-anchored names for the reasons already recorded in
`include/dsplib/V90CP.h`.

`V90MP`'s `rxState`, `onesRun`, `zerosRun`, `bitIndex`, `groupSize`,
`seqLength` and `bodyLength` were named from its own driver and diagnostics in
commit `22fa07e2` (finding F10181). `V90CP`'s `bitsToInfo`/`evaluateInfo` pair
had already established the same roles and `calcSequenceLength` already
described the sequence-length fields in those terms, so the sibling name is
the strongest available evidence (evidence order 2, a sibling that types the
same role). No new wire meaning is claimed.

| Previous name | New name | Evidence |
| --- | --- | --- |
| word_ca4 | rxState | Sibling `V90MP::rxState` (22fa07e2, F10181); the decoder's state, documented at the field |
| byte_ca9 | onesRun | Sibling `V90MP::onesRun`; run length of ones in `bitsToInfo` |
| byte_caa | zerosRun | Sibling `V90MP::zerosRun`; run length of zeros in `bitsToInfo` |
| word_cac | bitIndex | Sibling `V90MP::bitIndex`; the cursor `bitsToInfo` stores at |
| word_3ba8 | groupSize | Sibling `V90MP::groupSize`; `calcSequenceLength`'s divisor |
| word_3bac | seqLength | Sibling `V90MP::seqLength`; the reported sequence length |
| word_3bb0 | bodyLength | Sibling `V90MP::bodyLength`; `calcSequenceLength`'s rounded input |

Retained neutral, with the reason each stays an offset:

| Retained name | Reason |
| --- | --- |
| word_00 | Polarity is opposite the sibling's `Type`; `V90CP` shortens on nonzero, so the name is not carried across (documented in V90CP.h) |
| word_04, word_08, word_0c | Whole-word long-form block flags; only `infoToBits` establishes them and their wire meaning is unstated |
| byte_10 | Five-bit signed header value; no string or caller names it |
| byte_11 | Two-bit four-arm switch; four arms written out but no enumeration is named in the object |
| byte_12 | One bit copied into `bits[0x1d]`; no consumer states its meaning |
| byte_13 | Raised/lowered by `V90Phase4Modulator`, but what bits[0x21] means on the wire is not stated |
| word_14 | Sixteen-bit header value; neither direction names it |
| word_18 | Six frames of signed pairs; contents not established anywhere in the object |
| short_58 | Four short lists; contents not established |
| word_c70 | Six four-bit values; position is the only structure |
| word_ca0 | Short-form payload; role is bounded but not semantic |
| byte_ca8 | Width settled by two accesses; nothing else about it |
| word_cb0 | Receive counter within the current block; role bounded, no agreed wire name |
| word_cb4 | The read cursor, mirror of `bitIndex`; role bounded, no agreed wire name |
| word_3bbc | Hold-off counter; what it holds off is not stated |

Production consumers in `src/pump/v90/`, the external `cp->` accesses in
`V90Modulator.cpp`, `V90Phase4Modulator.cpp` and `V90Phase4Demodulator.cpp`,
nine unit tests, the twelve affected mutation manifests and the
`tools/fieldcorrelate.py` usage example follow the new names. The
`V90CP_OFF` assertion labels in `src/pump/v90/V90CP.cpp` were updated to match
with their offsets unchanged. Mutation `find`/`replace` values were decoded
and transformed structurally; labels, `why` prose and every injected fault are
unchanged. The stale paragraph in `V90CP.h` that read "The sibling V90MP ...
kept byte_19, byte_1a, byte_1b and word_14, so this is the precedent" was
corrected.

Batch 13 final validation: Gentoo `make phase` exited 0, **375 passed, 0 failed**,
all structural checks green, **655/655 baseline period objects byte-identical**.
`anchorcheck` revalidated all 10041 mutation anchors against the renamed
source (0 matching other than exactly once) and `refcheck` resolved all 13936
references. Scope was checked token-by-token: apart from the intended
`V90CP.h` prose correction, every source/test edit is an identifier-only
substitution, and every mutation manifest differs only in decoded
`find`/`replace` identifier values, verified by a token-level comparison of
all non-`find`/`replace` properties and a boundary-aware inverse of the
renamed strings.

## Batch 14: V92CP field names

Owner-scoped pass over `V92CP`. Thirteen members are renamed and the
`bitsToInfo` declaration gains its parameter name. The header paragraph that
claimed the message fields were offset-named "because nothing written
establishes what any of them holds" was STALE and is rewritten below.

### Evidence

`src/pump/v90/V90MappingParamsInt.cpp`'s `setV92CPpckFromParamsInfo` (around
line 606) copies the fields directly from author-named `V90MappingParams`
members -- `cp->word_08 = params->shaperSR`, `cp->word_0c = params->shaperId`,
`cp->flt_14/18/1c/20 = params->shaperA1/A2/B1/B2` -- so a destination holding a
source's value is that source, not an adjacency guess. The same file's
`getDataBitRate`/`setDataBitRate` pair shows `char_02` is the data bit rate
(`setDataBitRateInline(params, (int)cp->char_01, (int)cp->char_02)`), and the
object's own symbols `getConstellationsIndex`, `getConstellationMask` and
`getCodecConstellationMask` fill `word_28`, `short_42` and `short_a2`
respectively. `byte_119`/`byte_11a`/`word_11c` are the roles the siblings
`V90CP`/`V90MP` already name `onesRun`/`zerosRun`/`bitIndex`, and `V92CP.h`
itself said `byte_11a` is "the same shape V90CP's `zerosRun` has".

| Previous name | New name | Evidence |
| --- | --- | --- |
| word_08 | shaperSR | `setV92CPpckFromParamsInfo`: `cp->word_08 = params->shaperSR`; `V90MappingParams::shaperSR` is author-named |
| word_0c | shaperId | Same function: `cp->word_0c = params->shaperId` |
| flt_14 | shaperA1 | Same function: `cp->flt_14 = params->shaperA1` |
| flt_18 | shaperA2 | Same function: `cp->flt_18 = params->shaperA2` |
| flt_1c | shaperB1 | Same function: `cp->flt_1c = params->shaperB1` |
| flt_20 | shaperB2 | Same function: `cp->flt_20 = params->shaperB2` |
| char_02 | dataBitRate | `setDataBitRateInline(params, (int)cp->char_01, (int)cp->char_02)`; `getDataBitRate` reads the same field |
| word_28 | distinctIndex | `getConstellationsIndex(params, cp->word_28)` fills it with group numbers; `setParamsInfoFromV92CPUnPck` copies it to `params->distinctIndex` |
| short_42 | constellationMask | `getConstellationMask(params, (int)i, cp->short_42[i])` |
| short_a2 | codecConstellationMask | `getCodecConstellationMask(params, (int)i, cp->short_a2[i])` |
| byte_119 | onesRun | Sibling `V90CP::onesRun`/`V90MP::onesRun`; the run of ones in `bitsToInfo` |
| byte_11a | zerosRun | Sibling `V90CP::zerosRun`/`V90MP::zerosRun`; `V92CP.h` says "the same shape V90CP's `zerosRun` has" |
| word_11c | bitIndex | Sibling `V90CP::bitIndex`/`V90MP::bitIndex`; the cursor `bitsToInfo` stores at and `infoToBits` writes through |

`bitsToInfo(unsigned char)` becomes `bitsToInfo(unsigned char bit)`, matching
the definition in `V92CP.cpp`.

Retained neutral, with the reason each stays an offset:

| Retained name | Reason |
| --- | --- |
| byte_00 | `bits[18]` stored whole and tested against ONE; picks short vs long form, no name stated |
| char_01 | Signed header byte; two bits leave and `<= 1` picks the long form; no name stated |
| byte_03 | `bits[35]` stored whole; only `infoToBits`/`evaluateInfo` touch it |
| byte_04 | `bits[33]`; raises `word_110`; other classes write it, but its meaning is still unstated |
| flt_10 | Sixteen magnitude entries; no consumer or string names it |
| byte_24 | `bits[128]` and the second-mask gate; the role is bounded, the name is not |
| word_104 | `setSUV` stores 16 before its argument and five bits go out; what the sixteen counts is unstated |
| word_10c | Group count; unbounded (D570) and no consumer names it |
| word_110 | Raised when `byte_04` is non-zero; what it holds is unstated |
| byte_118 | A whole copy of `char_01` that nothing written reads |
| word_124 | The read cursor; role bounded but no agreed name |
| word_914 | Hold-off counter over `bitsToInfo`'s answer; usage inference only (F6606) |

### Cross-owner classification and scoping

Every occurrence of `word_08`/`word_0c` was classified by the owning object
before any edit. Renamed: the `V92CP` members only -- `V92CP.h`,
`V92CP.cpp`, `V90MappingParamsInt.cpp`'s two `setParamsInfoFrom*UnPck`
bodies and the V.92 unit tests/manifests. NOT renamed: `V90CP`
(`V90CP.h`/`V90CP.cpp`, tests `t_v90cpinfo`, `t_v90cpb2i`, `t_v90modprog`,
`t_v90p4mgen`), `V90Mapper` (`V90Mapper.h`/`.cpp`, `t_v90modchain`,
`t_v90equproc`), `V90Demapper` (`V90Demapper.h`/`.cpp`, `t_v90demap`,
`t_v90demapctor`, `t_v90p4ddec`), and `tagV90AdditionalCPinfo`'s own
`word_0c` (`V90Demodulator.cpp`, `V90CPpck.cpp`, `V90CPUnPck.h` comment,
`t_v90cmask`, `t_vpcmrunpcm`, `t_v90rundemod`). `short_42`/`short_a2` are not
declared by any other class in this tree (the `V90CPUnPck.h` mention is
prose); `word_28` is a `V92Phase4Modulator` member and a
`V90SpectralVerifier` member, and those occurrences are untouched.

The `V92CP_OFF` field argument in `V92CP.cpp` was updated to the new name; the
third (`tag`) argument is left offset-based, matching the existing
`V92CP_OFF(rxState, 0x114, word114)` and
`V92CP_OFF(stateBitCount, 0x120, word120)` precedent. Every offset is
unchanged.

### The V.90 twin collision, and the re-anchored mutations

`V90CPUnPck` ALREADY uses these names, and its
`setParamsInfoFromCPUnPck` sits in the same file as
`setParamsInfoFromV92CPUnPck` with a line-for-line identical body over the
scalar copies and the two mask loops. Renaming V.92's fields therefore made
textually identical lines where the offset names had kept them apart. Twelve
`v92mpunpck` anchors that had been unique only because they carried
`cp->word_28`/`cp->short_42`/`cp->short_a2` became ambiguous, and three
`v90unpck` anchors that named the V.90 twin's lines became ambiguous in the
other direction. `anchorcheck.py` -- which `make phase` runs through the
`refs` tier and which FAILS on any anchor matching other than exactly once --
caught all fifteen. Each was re-anchored to a window containing a V.92-only or
V.90-only token (the `(int)` cast on `shaperSR`, `V90_CONSTELLATIONS` vs
`V90_CPUNPCK_CONSTELS`, `cp->byte_24` vs `cp->codecConstellationPresent`),
with the same line mutated inside the window, so the injected fault and every
label are byte-identical. This is the only deviation from an
identifier-only transform of the manifests, and it is required by the
exactly-once rule itself.

### Verification

A token-aware forward substitution of `HEAD` reproduces the working tree
exactly for the thirteen changed source/test files (strings and character
literals excluded, as the headers demand). The only file that differs is
`V92CP.h`, and it differs only by (a) the rewritten stale paragraph and (b)
`bitsToInfo`'s new parameter name -- both intended. Every remaining old
identifier in changed code is either a string literal or a protected
other-class member (`info->word_0c`, `ourInfo/theirInfo.word_0c`,
`o->word_28`). The manifests were transformed by decoding each `find`/`replace`
string value, substituting identifiers and re-encoding at the token boundary,
so every `label`, `why`, `note`, `equivalent`, `fn` and blank line is
preserved; the only values widened are the fifteen re-anchored finds above.
`anchorcheck.py` then passed the whole register (272 suites, 10041 mutations,
0 matching other than exactly once) and `refcheck.py` resolved all 13936
references.

Batch 14 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`. All 655 `build/period/*.o` hashes are
**byte-identical** to the pre-edit snapshot (`/home/philpem/slmodem/tmp/
issue100-v92cp-before.sha256`, 655/655). `anchorcheck` passed the full register
(272 suites, 10041 mutations, 0 matching other than exactly once) and
`refcheck` resolved all 13938 references.

## Batch 15: V92Phase4Modulator field names

Owner-scoped pass over `V92Phase4Modulator`. Two members are renamed and four
stale comments corrected. `word_1c0` is unique to the class; `word_2c` is ALSO
a `V90Phase3Demodulator` member, so every occurrence of that one was classified
by owning object before any edit.

### Evidence

| Previous name | New name | Evidence |
| --- | --- | --- |
| word_2c | silenceRrnRequest | Assigned from the already-named `V90ConnectionEvaluator::silenceRrnRequest` in `VpcmFloModem::runPcmModem`'s two RRN arms (`VpcmFloModem.cpp:1735`, `:1746`) and passed to `V92CP::setSUV(unsigned int)` in `generateSymbol` (`V92Phase4Modulator.cpp:1260`, `:1538`), which stores it at `V92CP::suv` |
| word_1c0 | cpReceived | Set by `recivedCP` (`:783`) and `recivedCPtag`'s first-tag path (`:919`); cleared by the constructor (`:207`), `resetBeforRRN` (`:538`), `resetRRNSecondSection` (`:644`) and `reset` (`:1671`); read as the `recivedSUVtag` guard (`:818`) and the `recivedCPtag` first-tag test (`:915`). Usage inference, CLAUDE.md's weakest tier; no format string prints it. |

### Cross-owner classification and scoping

`word_2c` is declared by two classes. Renamed: `V92Phase4Modulator` only --
`V92Phase4Modulator.h`, `V92Phase4Modulator.cpp`, `VpcmFloModem.cpp`'s two
`phase4Modulator->word_2c` stores, the four V.92 unit tests (`t_v92p4gen`,
`t_v92p4reset`, `t_v92p4sym`, `t_vpcmrunpcm`) and the
`v92p4gen`/`v92p4reset`/`v92p4sym`/`vpcmrunpcm`/`v92p4mod` manifests. NOT
renamed: `V90Phase3Demodulator` (`V90Phase3Demodulator.h`/`.cpp`),
`V90Equalizer.cpp`, `V90Demodulator.cpp`, `v34diag.cpp`, tests `t_v92dec`,
`t_v90p3ddec`, `t_v90p3dreset`, `t_v90p4ddec`, and manifests
`v90p3ddec.json`, `v90demprog.json`. Those occurrences are byte-for-byte
unchanged; the guard in verification found 0 changed lines naming that class.
`word_1c0` is declared by no other class in this tree.

The `V92P4M_OFF` first argument was updated to the new name; the third (`tag`)
argument is left offset-based (`word2c`, `word1c0`), matching the existing
`V92P4M_OFF(eventCode, 0x00c, word0c)` and `suvLimit`/`word44` precedent. Every
offset is unchanged.

### Stale comments corrected (no code change)

- `word_2c` (now `silenceRrnRequest`): the old "nothing written assigns it
  anything else" was false -- `VpcmFloModem.cpp:1735`/`:1746` assign it.
- `word_38`: the old "Nothing written sets it" and its implication that `reset`
  clears it were both false -- `VpcmFloModem.cpp:1826`/`:1834` set it from
  `cp->word_ca0`, and `reset` does NOT clear it. The name stays neutral: what
  the two sites compute is not established.
- `word_1c0`: "Cleared by the constructor." was stale; replaced by the latch
  role described above.
- `word_1c4`: "Cleared by the constructor." was stale; replaced by the
  once-per-SUV latch role (written by `recivedSUV` `:747`,
  `recivedPartTwoSilenceRrnSUV` `:776` and the `SUVu`->`CPu` arm `:1298`,
  cleared by constructor/`resetBeforRRN`/`resetRRNSecondSection`/`reset`, read
  as the early-out guard in both `recivedSUV` handlers). The name stays
  neutral: the three writers do not agree on one semantic.

Method-doc references to `word_1c0`/`word_2c` were renamed to the new names.
Retained neutral, unchanged from the class's existing dispositions:
`word_18`, `byte_1c`, `flag_20`, `word_24`, `word_28`, `word_30`, `word_34`,
`word_38`, `flag_3c`, `byte_42`, `word_1b0`, `word_1b8`, `word_1c4`.

### Verification

A token-aware forward substitution of `HEAD` reproduces the working tree
exactly for the six changed cpp/test files. The only file that differs is
`V92Phase4Modulator.h`, and it differs by exactly three blocks: the rewritten
`word_2c`, `word_38` and `word_1c0`/`word_1c4` comments -- all intended. No
changed line names `V90Phase3Demodulator`. The manifests were transformed by
decoding each `find`/`replace` string value, substituting identifiers at the
token boundary and re-encoding with the file's own `indent=1` formatting, so
every `label`, `why`, `note`, `equivalent`, `fn` and blank line is preserved
byte-for-byte; the old names remain only in retained labels and one `why`
(`v92p4gen`: labels 30/55/88; `v92p4reset`: labels 4/9; `v92p4sym`: `why` 2 and
labels 71/125; `snapshot.json`'s seven labels, which are unchanged). The
token-count check found 0 structural problems and each old->new pair balanced.
`anchorcheck` passed all five changed suites (395 mutations, 0 matching other
than exactly once), and `refcheck` resolved all 13938 references.

Batch 15 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and structural
checks all OK`. All 655 `build/period/*.o` hashes are **byte-identical** to the
pre-edit snapshot (`/home/philpem/slmodem/tmp/issue100-v92p4m-before.sha256`,
655/655).

## Batch 16: V90Phase3Demodulator field names

Owner-scoped pass over `V90Phase3Demodulator`. Three members are renamed and
three stale or partial comments corrected. `byte_3f8` and `word_420` are
declared by no other class in this tree; `word_2c` also appears as prose in
`v34diag.cpp` and as a `V92Phase4Modulator` mutation label, so every occurrence
of that one was classified by owning object before any edit.

### Evidence

| Previous name | New name | Evidence |
| --- | --- | --- |
| byte_3f8 | dilMaxUcode | Rank-1: `V90Demodulator` prints it with the author's own format string `"V90Demodulator: Dil max ucode = %d\n"` (`V90Demodulator.cpp:1568`) and stores 0x74 into it from both AGC arms (`:1548`, `:1564`); `V90Phase3Demodulator.cpp:2371` passes it as the only argument to the typed callee `V90AutoDigitalImpDetector::determineMaxUcode(short)`. |
| word_420 | trn1dDdLength | Both decision functions load it from the author-named `params->TRN1_QC_DD_LENGTH` (+0x4a0) when `quickConnect` is set and `params->TRN1D_DD_LENGTH` (+0x2fc) when it is not, then compare the state counter against it to leave state 4 (`getV90Decision`) / state 5 (`getV92Decision`): it is the TRN1d data-directed stage length, and the name follows that role. Finding F2118. |
| word_2c | samplesInState | Incremented once per sample at the top of both decision functions and zeroed on every state transition, so it holds the number of samples the current state has run. Read externally as a duration: `V90Equalizer.cpp:2382..2410` compares it against the `*_FREEZE_DURATION` constants and `V90Demodulator.cpp:1512` against `AGC_ADAPTATION_DURATION`. Usage inference (no format string or typed callee spells it), strengthened by the sibling `V90Demodulator::samplesInPhase` at `include/dsplib/V90Demodulator.h:389`. |

### Cross-owner classification and scoping

- `byte_3f8` -- declared only by `V90Phase3Demodulator`. Renamed in the header,
  `V90Phase3Demodulator.cpp`, `V90Demodulator.cpp`, `t_v90p3ddec.cpp`, and the
  `v90p3ddec`/`v90demprog` manifests.
- `word_420` -- declared only by `V90Phase3Demodulator`. It is ALSO a field of
  the local fixture `struct trial_args` in `test/unit/t_v92dec.cpp`, which
  mirrors this class field-for-field, so that mirror field was renamed with it.
- `word_2c` -- after Batch 15 no other class declares it. Renamed in the header,
  `V90Phase3Demodulator.cpp` (uses and method-doc comments),
  `V90Demodulator.cpp:1512`, `V90Equalizer.cpp:2382..2410`, the tests
  `t_v90p3ddec.cpp`, `t_v90p3dreset.cpp`, `t_v90p4ddec.cpp` and the
  `t_v92dec.cpp` `trial_args` mirror, and the `v90p3ddec`/`v90demprog`
  manifests. **NOT renamed:** `src/pump/v34/v34diag.cpp:140` is prose about what
  `V92Modulator`'s field was called before it was named `phase` -- not this
  class; and the mutation `label`/`why` prose in `v92p4reset.json`,
  `v92p4sym.json` and `snapshot.json` names `V92Phase4Modulator`'s field, not
  this one. Those occurrences are byte-for-byte unchanged.

The `P3D_OFF` first arguments were updated to the new names; the third (`tag`)
arguments are left offset-based (`word2c`, `byte3f8`, `word420`), matching the
existing `V92P4M_OFF(silenceRrnRequest, 0x02c, word2c)` precedent. Every offset
is unchanged and the `sizeof == 0x42c` assertion still holds.

### Stale comments corrected (no code change)

- `byte_3f8` (now `dilMaxUcode`): the old "Nothing writes this byte in anything
  written so far and no format string prints it ... not enough" was false. The
  rank-1 format string and both 0x74 stores are in `V90Demodulator.cpp`, and
  `determineMaxUcode` is a typed callee; the comment now records all three.
- `word_2c` (now `samplesInState`): the old comment only described it as
  `reset`'s fourth argument. It now keeps that role and adds the per-state
  sample counter and the external `*_FREEZE_DURATION` /
  `AGC_ADAPTATION_DURATION` readers, with the sibling-class precedent.
- `word_420` (now `trn1dDdLength`): the existing unit caveat (samples vs
  symbols, unsettled) is kept; the comment now also says where the name comes
  from.

Method-doc references to `word_2c` were renamed to `samplesInState`
(`enterWaitForANSpcmDrop`'s doc, the `timeoutBase` and `eventCode` field
comments, and the `getV92Decision` / `P3D_CHECK_TERMINATED` comments in the
`.cpp`).

Retained neutral, unchanged from the class's existing dispositions:
`word_3cc`, `word_3f4`, `byte_3f9`, `word_3fc`, `short_400`, `word_408`,
`short_414`, `float_418`, `byte_424`.

### Verification

A token-boundary forward substitution of `HEAD` reproduces the working tree
exactly for every changed source/test/manifest file except two intended
deviations: the three rewritten comment blocks in `V90Phase3Demodulator.h`, and
the preserved string literal `"word_2c stored (%ld)"` in `t_v90p3dreset.cpp`
(the assertion's field reference was renamed; the message text was kept because
the batch rule preserves string literals). No changed line names
`V90Phase4Modulator`'s or `V92Phase4Modulator`'s `word_2c`. The manifests were
transformed by decoding each `find`/`replace` value, substituting at the token
boundary and re-encoding with `indent=1`; every `label`, `why`, `note`,
`equivalent`, `fn`, `_` and blank line is preserved byte-for-byte (checked by
per-entry property equality). The token-count check found each old->new pair
balanced and 0 non-name identifier changes in every file except the header,
whose three intended prose rewrites add one `samplesInState` mention (5 removed
-> 6 added) and change only English words. `anchorcheck` passed all 272 suites
(10041 mutations, 0 matching other than exactly once) and `refcheck` resolved
all 13938 references.

Batch 16 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and structural
checks all OK`. All 655 `build/period/*.o` hashes are **byte-identical** to the
pre-edit snapshot (`/home/philpem/slmodem/tmp/issue100-v90p3d-before.sha256`,
655/655).

## Batch 17: v22org (v22fp_hdx) field names

Owner-scoped pass over `struct v22fp_hdx` (declared in `include/dsplib/v22fp.h`).
Five members are renamed and eight stale or incorrect prose passages in
`include/dsplib/v22org.h` are corrected. `r08` is RETAINED NEUTRAL (genuinely
multi-role), and `struct v22fp::r34` and `struct v22fp::r1e[2]` are retained.

### Evidence

| Previous name | New name | Evidence |
| --- | --- | --- |
| r0a | ones_detect_ms | Every read and accumulation is `Detect_1s`'s millisecond answer for the 1200-baud ones detector, thresholded by an `*_ONES_MS` constant: `v22org.c:207,252,275`, `v22mod.c:361,366,387`, `v22loop.c:199,215`. Reset in `V22.c:239`, `v22org.c:40,271,283`, `v22mod.c:235,382,392`, `v22loop.c:62,212,218`, `v22hdx.c:118`, `v22ans.c:113`. One role across every reconstructed user. |
| r28 | rx_rms | `v22org.c:102` `hdx->rx_rms = FPM_rms(rxin, V22_ORG_BLOCK);` and `:110` adds it into `rms_accum`. Its only user is `v22_originate`; init `V22.c:241`. |
| r2c | rms_accum | `v22org.c:110` RMS sum, `:127` divides by `rms_blocks` to become the mean, `:129` compares it to `rms_threshold`. Only `v22_originate`; init `V22.c:247`. |
| r30 | rms_threshold | `v22org.c:130` is the only reader; `V22.c:242` inits 0x2454 (9,300). |
| r32 | rms_blocks | `v22org.c:115` count, `:128` divisor; `V22.c:243` inits 0. |

### Cross-owner classification and scoping

The five members are declared only by `struct v22fp_hdx`. Same-named members of
other owners were classified and left byte-for-byte unchanged:

- `struct v22fp_dsp::r28`/`r2c` -- `V22.c:209,215,220,225,281,290`,
  `v22rate.c:51,68`, `v22stc.c:33`, the `v22fp.h:144` comment about the
  `(r2c, r2e)` DSP pair, the `v22fp.h:355/:363` field definitions, and the
  `V22FP_ASSERT_OFF(d_r28, struct v22fp_dsp, r28, 0x28)` assertion.
- `struct v22fp_params::r08` (`params.r08`), `struct v22fp::r34` (`fp->r34`)
  and the `struct v22fp::r1e[2]` mask.
- `v17data.h:226` `r0a`, `v17fax.h:615` `r28`, `v32fpstat.h:93` `r0a`,
  `v32fp.h:174` `r2c`, `b103fp.h:220` `r2c`, and the V.32 locals in
  `V32.c:364`, `V32stc.c:239..292`, `t_v32fptab.c`/`t_v32fpdisp.c`.

Renamed: the `v22fp_hdx` members in `v22fp.h`, `V22.c`, `v22org.c`, `v22mod.c`,
`v22loop.c`, `v22hdx.c`, `v22ans.c`; the test-local mirror fields that hold
these counters (`struct scenario` in `t_v22org.c`, `struct poke` in
`t_v22loop.c`); and the method-doc comments in `v22loop.h`. String-literal
labels (`"N3 r0a boundary -"`, `"O NODE_3 mean above r30 (%ld)"`,
`"r0a accumulated (%ld)"`, ...) are preserved unchanged, as are derived test
identifiers (`r0a_before`, `pre_r0a`, `saw_n3_r0a_inc`, `r32_before`).

The `V22FP_ASSERT_OFF(h_r30, struct v22fp_hdx, rms_threshold, 0x30)` field
argument was updated; the `h_r30` tag is left offset-based, matching the
existing `p_r18`/`h_r3c` precedent. Every offset is unchanged.

### Stale or incorrect prose corrected (no code change)

- **S1** banner node table: `r0c == N` -> `connect_substate == N`
  (+0x0c is now `connect_substate`).
- **S2** "WHAT IS NOT RENAMED" rewritten: `r0c`, `r34`, `r0a`, `r28`, `r2c`,
  `r30`, `r32` have since been named, and `r04` is not an hdx field at all
  (+0x04 is `node_deadline`). Only `r08` and `fp->r1e` remain offset-based.
- **S3** "sets `r34` when that mean exceeds `r30`" was wrong; `v22org.c:131`
  sets `fp->hdx->rx_shift`.
- **S4** `iSilenceAfter2100` is `static` in `src/pump/v22/v22mod.c`, where
  `v22_answer` is defined -- not in `v22org.c`. The `.bss`+0x380 and
  three-reference facts are kept.
- **S5** the "left to whoever lands them" status was stale; the renames have
  landed.
- **S6** the divide-by-zero warning was false for the reconstructed flow:
  `rms_accum`/`ones_detect_ms`/`rms_blocks` move under the same
  `if (nsym != 0)` guard and `ones_detect_ms` is zeroed at NODE_0 and at the
  NODE_3 transition, so `rms_blocks == 0` implies `ones_detect_ms == 0` and the
  `> 135` test cannot pass. Corrected to state the object's guard. (The
  `docs/remaining.md:656` note is a separate historical finding and was not
  touched.)
- **S7** "Two of the arms" listed three; now "Three of the arms".
- **S8** the unsigned-read claim made precise: it holds for `r08`,
  `ones_detect_ms`, `rx_rms`, `rms_accum`, `rms_threshold` and `rms_blocks`,
  but `fp->r1e` is an `unsigned char` bit mask with no signedness to invert.
- `v22fp.h`: five new Doxygen field comments, the `r08` multi-role retention
  note, and the banner's named-field count updated. `v22loop.h` comments and
  `V22_LOOP_ONES_MS` updated to the new name.

### Verification

A token-aware forward substitution of `HEAD` reproduces the working tree
exactly for all six code/test files except the intended deviations: the two
rewritten header prose blocks (`v22fp.h`, `v22org.h`), the `h_r30` assertion
field argument on `V22.c`, and the single `v22org.c` comment naming `rms_blocks`
(the identifier substitution there is paired with a reflowed comment). No
protected `v22fp_dsp`/`v22fp_params`/`v22fp::r34`/`r1e` code occurrence changed.
No mutation manifest value contains any of the five old identifiers (a
decoded-JSON scan of all 274 files found 0), so 0 `find`/`replace` values were
changed and every anchor still matches its renamed source; the gate's
`anchorcheck` passed 272 suites / 10041 mutations with 0 matching other than
exactly once, and `refcheck` resolved 13940 references with 0 unresolved.

Batch 17 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and structural
checks all OK`. All 655 `build/period/*.o` hashes are **byte-identical** to the
pre-edit snapshot (`/home/philpem/slmodem/tmp/issue100-v22org-before.sha256`,
655/655).

## Batch 18: issue #119 Tier A names

Owner-scoped pass over seven members whose "retain" disposition in the
acceptance inventory (`docs/naming-inventory.md`) went stale because the reader
or sibling that settles the role has since been reconstructed. All seven take
the name their sibling already carries; offsets, widths, signedness and layout
are unchanged. The owners are `V90Phase4Modulator`, `V90CP`, `V90Mapper` and
the two datapump wrappers whose +0x08 is the shared `struct dp` header.

### Evidence

| Owner | Previous name | New name | Evidence |
| --- | --- | --- | --- |
| `V90Phase4Modulator` | `word_2f9c` | `cpReceived` | Set to 1 by `recivedCP` (`V90Phase4Modulator.cpp:618`) and by `recivedCPtag`'s first-tag path (`:742`, alongside `cp->word_00 = 1`), and read as a gate at `recivedSUVtag` (`:632`) and at `recivedCPtag` (`:719`, the first/later discriminator). The identical latch is the sibling `V92Phase4Modulator::cpReceived` (`V92Phase4Modulator.h:890`), whose own `recivedCPtag` takes the same first-tag branch (`V92Phase4Modulator.cpp:918`). |
| `V90CP` | `word_cb0` | `stateBitCount` | Cleared per block, incremented per bit and compared to each block's length -- `0xf`, `3`, `0x44`, `0x55`, `0x11`, or the message-carried `alpha`/`beta` (`V90CP.cpp:1060-1236`). Unsigned, forced by `bitsToInfo`'s `cmp $0x1`/`jb` switch. The sibling `V92CP::stateBitCount` (`V92CP.h:489`) is the same per-block role. |
| `V90Mapper` | `uint_6f8` | `primeFrames` | Seeded from `mp->shaperId` when the shaper runs and 0 otherwise (`V90Mapper.cpp:248-250`); `process` tests it, subtracts `signBitGroups` and completes a short frame (`:444-454`). The blocker named in the old comment -- `V90SpectralShaper::primeFrames` (`V90SpectralShaper.h:269`) -- is now written, and this is the same priming countdown. |
| `call_dp` | `f08` | `status` | The shared datapump header's +0x08; `struct dp` types it `unsigned status` (`dp.h:38`). Declaration-only here -- `call.c` never touches it. Kept `int`, as declared; only the role is named. |
| `v8_dp` | `f08` | `status` | Same ABI sibling as `call_dp` and `struct dp`. |
| `v8_dp` | `f20` | `idle_timer` | Set to `modem_get_param(..., 5) + 0x2a0` when a datapump change is requested and counted down by each call's `count` until the window closes and the change is forced (`v8.c:201-224`); the unit test already diagnoses it "idle timer" (`t_v8dp.c:366`). |
| `v8_dp` | `f2c` | `last_status` | The previous `V8Process` status: `v8.c:212-216` updates it only when the status changes, the same one-layer-up change detector `V8Process` runs on its own `prev_status`. `b103.h:34` names the sibling `last_status`. |

### Cross-owner classification and scoping

`word_2f9c`, `word_cb0` and `uint_6f8` are each declared by one owner only
(verified by `git grep` over `src`/`include`/`test`). `f08`, `f20` and `f2c`
are short and collide across many structs, so only the `struct call_dp` and
`struct v8_dp` members were renamed. Classified and left byte-for-byte
unchanged:

- `class1.h:528` (`struct class1` context `f08`);
- `fpm_agc.h:49` and `fpm_tone.h:36` (`f08`), and `smc.h`'s `f20`/`f24`
  oscillator-config output;
- `v22.h` (`cfg.f08` comment), `v22fp.h:114`/`:153` (`f08`/`r08`),
  `v29data.h` (`f20` comment);
- the `f08` initialisers/readers in `src/dsp/fpm_tone.c`,
  `src/fax/V17rxtab.c`, `V27rxtab.c`, `V29rxtab.c`, `class1.c`, `fax.c`,
  `v21cfg.c`, `src/pump/b103/B103tab.c`, `src/pump/v22/V22.c`, `v22.c`,
  `v22rxtab.c`, `v22txtab.c`, `src/pump/v23/bwchdem.c`, `v23rx.c`, `v23tx.c`,
  `src/pump/v32/V32.c`, `v32cfg.c`, `v32nsorg.c`;
- the unit-test readers `t_class1create.c`, `t_v17cfg.c`, `t_v21cfg.c`,
  `t_v22ans.c`, `t_v22conn.c`, `t_v22ctl.c`, `t_v22fpcreate.c`, `t_v22fpdel.c`,
  `t_v22hdx.c`, `t_v22loop.c`, `t_v22modem.c`, `t_v22org.c`, `t_v22rate.c`,
  `t_v22status.c`, `t_v22tab.c`, `t_v27cfg.c`, `t_v27fax.c`, `t_v29cfg.c`,
  `t_v32cfg.c`, `t_v32nsorg.c`, `t_v92info.cpp`.

### Comment corrections (no code change)

- `V90CP.h`: the field-list paragraph at :124 now records that the per-block
  count is named from the other sibling, `V92CP::stateBitCount`, and the
  field's own comment says the same.
- `V90Phase4Modulator.h`: `word_2f9c`'s comment now says the sibling
  `V92Phase4Modulator::cpReceived` names the role, and which reads
  discriminate first from later tags.
- `V90Mapper.h`: `uint_6f8`'s comment now records that the blocker
  (`V90SpectralShaper::primeFrames`) is written.
- `call.h`/`v8dp.h`: the `f08` comments name the shared datapump header's
  `status` (`dp.h:38`), keep the declared `int`, and say the sibling there is
  `unsigned`. `v8dp.h`'s `idle_timer`/`last_status` field comments are added.
- `v8.c`: the idle-timer comment is reflowed around `idle_timer`, and the
  change-detector comment names `last_status`.

### Verification

A token-aware forward substitution of `HEAD` reproduces every changed
non-doc file exactly except the intended deviations: the five rewritten header
prose blocks, the two rewritten `v8.c` comments, and the preserved
`"f2c (%ld)"` string literal in `t_v8dp.c` (only the identifiers beside it
changed). Ten mutation manifests were edited by DECODING JSON and
substituting only `find`/`replace` values -- the raw files contain literal
`\t`, which defeats a naive `\b` match (the known trap). The same trap hid
four `word_2f9c` manifests (`v90p4mctor`, `v90p4mreset`, `v90p4mtab`,
`v90p4msym`) from the initial `\b`-scoped `git grep` enumeration; the first
gate run's anchor check is what caught them (7 detached anchors), and the
substring search over the filesystem is what completed the list. A
decoded-JSON re-derivation of `HEAD` reproduces every changed
`find`/`replace` value exactly with 0 other leaf strings changed. The manifest
token balance is clean for all ten: `word_2f9c` 15+1+3+8+1 = 28/28,
`word_cb0` 22+11+5 = 38/38, `uint_6f8` 14/14, `f20` 6/6 and `f2c` 5/5, with
no non-`{old,new}` identifier count moved. `make refs` is green: 272 suites,
10041 mutations, 0 anchors matching other than exactly once. A
`git diff` scan of every changed line carrying `f08`/`f20`/`f2c` shows all of
them in the four target files (`call.h`, `v8dp.h`, `v8.c`, `t_v8dp.c`) and the
one target manifest (`v8dp.json`); no other owner's occurrence changed.

A first pass wrapped the reworked `diff_eq_int("f2c (%ld)", ...)` call across
two lines and the period object changed: `diff_eq_int` embeds `__LINE__`
(`test/harness/harness.h:323`), so adding a line shifts every later `__LINE__`
in `t_v8dp.c` and moves `test_unit_t_v8dp.o`. The call was restored to one
line, restoring the object. Every other edited test file kept its line count
and stayed byte-identical.

Batch 18 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and **`phase boundary: period differential and
structural checks all OK`**; `make refs` reported 272 suites / 10041 mutations
with 0 anchors matching other than exactly once. All 655 `build/period/*.o`
hashes are **byte-identical** to the pre-edit snapshot
(`/home/philpem/slmodem/tmp/issue119-tier-a-before.sha256`, 655/655).

## Batch 19: issue #119 Tier B names

Owner-scoped pass over nine members whose "retain" disposition in the
acceptance inventory (`docs/naming-inventory.md`) went stale because the
reader or sibling that settles the role has since been reconstructed. Offsets,
widths, signedness and layout are unchanged; only the names, the reader
comments and the fixture identifiers move.

### Evidence

| Owner | Previous name | New name | Evidence |
| --- | --- | --- | --- |
| `V90Phase4Modulator` | `word_0018` | `repeatCpCount` | Read in `generateV92Symbol`'s SUVd arm (`P4M_STATE_SUVD`): `if (repeatCpEnable) repeatCpCount++` and `if (repeatCpCount > suvLimit + 0x320) enterRepeatedCPd()` (`V90Phase4Modulator.cpp:1869-1876`, +0x2ee7c). The same counter is `V92Phase4Modulator::repeatCpCount`, whose SUV arm runs `suvLimit + 800`. |
| `V90Phase4Modulator` | `byte_001c` | `repeatCpEnable` | The enable for that counter -- it gates the increment and is set to 1 by `generateV92Symbol`'s CPd termination (`:1919-1922`), exactly as the V.92 sibling's CPU termination does. |
| `V90Phase4Modulator` | `word_0040` | `suvLimit` | `reset`'s fifth argument, stored verbatim (`:351`) and read only by the same SUVd arm as `repeatCpCount > suvLimit + 0x320`. The sibling `V92Phase4Modulator::suvLimit` (+0x44) is the same field, filled the same way by `reset` and read against `+ 800`. |
| `V92Phase4Modulator` | `word_18` | `repeatCpCount` | `generateSymbol`'s state 5 arm: `if (repeatCpEnable) repeatCpCount++; if (repeatCpCount > suvLimit + 800) enterRepeatedCP()` (`V92Phase4Modulator.cpp:1275-1282`). The counter is the identical V.90 sibling's. |
| `V92Phase4Modulator` | `byte_1c` | `repeatCpEnable` | The enable above; set to 1 by `generateSymbol`'s CPU termination (`:1356-1357`), the twin of the V.90 CPd termination. |
| `V90AutoDigitalImpDetector` | `short_a948` | `altRbsInUse` | Set to 1 after the study's second update, beside `adid_recheckAltRbs` (`V90AutoDigitalImpDetector.cpp:2059-2061`), cleared by `reset` (`:490`). Read by `V90Phase3Demodulator` at six sites -- the `P3D_DEMOD_BIT` macro and `twoLevelDemod`, plus the four TRN1d arms (`V90Phase3Demodulator.cpp:569, 736, 772, 822, 878, 2501`) -- every one as `altRbsInUse != 0 && isAltRbs(...)`, choosing `linMappAlt` over `linMapp`. |
| `V90Demapper` | `short_1ea4` | `studyRunFinished` | Set on every completed run by `linearMappingStudy` (`V90Demapper.cpp:1143`, 0x315b6); read by `V90Equalizer::process` as the `&& flag_144` gate on a mean-error statistics update (`V90Equalizer.cpp:2215`). |
| `V90Demapper` | `short_1ea6` | `secondStudyRunFinished` | Set only when `completedRunCount == 2` (`V90Demapper.cpp:1140`, 0x3170f); read by `V90Equalizer::process` as the `&& flag_146` gate (`V90Equalizer.cpp:2223`). |
| `callprog` | `f1c` | `blind_dial` | Written from `cfg->w0` by `CALLPROG_Create` (`Callprog.c:577`), read by `CALLPROG_Dial` to suppress dial-tone listening and enter blind dialling (`:981`). `call_create` derives `w0` from S56; `docs/callprog_states.md:114-131` names and measures the behaviour. |

The `V92Phase4Modulator::word_18` / `byte_1c` rename reverses a Batch 15
deliberate retain. Batch 15 kept them offset-named because the V.92 side alone
did not establish the role; the V.90 sibling's SUVd arm is now reconstructed
and proves the identical counter and enable, so the twins share one name for
one role.

### Cross-owner classification and scoping

- `word_18` collides across owners. Renamed only for `V92Phase4Modulator`.
  Left byte-for-byte unchanged: `V90CP::word_18[12]` (`V90CP.h:369`,
  `V90CP.cpp` and its unit tests `t_v90conneval`, `t_v90cpb2i`, `t_v90cpinfo`,
  `t_v90leaves`, `t_v90modprog`), `v27fax::word_18` (`v27fax.h:182`,
  `src/fax/v27.c:1518`, `test/mutations/v27status.json`), the
  `t_v90modchain.cpp:3842` and `t_v90p4mgen.cpp:276` V90CP objects, the
  `v92ec.json` label, and the `snapshot.json` verdict keys.
- `byte_1c` is unique to `V92Phase4Modulator` (verified across `src`,
  `include`, `test`); no other owner carries it.
- `f1c` collides across owners. Renamed only for `struct callprog`. Left
  unchanged: `struct v8_dp` (`v8dp.h:62`, `src/v8/v8.c:224`, `t_v8dp.c`),
  `struct v8` (`v8.h:175`), the `fpm_tone.h:47` comment, and the local `float`
  in `t_v92info.cpp`.
- `short_a948`, `short_1ea4` and `short_1ea6` are each unique to their owner.

### Comment corrections (no code change)

- `V90Phase4Modulator.h`: `reset`'s `@param arg5` now names `suvLimit` and its
  reader; the +0x0018/+0x001c block replaces the false "read by neither those
  nor anything else in this file" with the `generateV92Symbol` SUVd-arm reader
  and the shared V.92 sibling role; the +0x0040 block names `suvLimit`, its
  reader and the sibling.
- `V92Phase4Modulator.h`: the +0x18/+0x1c blocks state the shared V.90 sibling
  role and that the rename reverses the earlier retain; the +0x44 `suvLimit`
  block cites the V.90 sibling and drops the stale "nothing written
  establishes that".
- `V90AutoDigitalImpDetector.h`: `altRbsInUse`'s comment cites the six
  `V90Phase3Demodulator` readers and the `isAltRbs`/`linMappAlt` selection.
- `V90Demapper.h`: `studyRunFinished`/`secondStudyRunFinished`'s comment cites
  `V90Equalizer::process`, the store conditions and the two clears, replacing
  the "names stay neutral / function nobody has written" text.
- `callprog_state.h`: `blind_dial` gains a field comment citing S56, the
  `CALLPROG_Dial`/`CALLPROG_Progress` behaviour and `docs/callprog_states.md`;
  the `callprog_cfg::w0` comment now points at `cp->blind_dial`.

### Verification

A token-aware forward substitution of `HEAD` reproduces every changed non-doc
file exactly except the five intended prose rewrites above
(`V90Phase4Modulator.h`, `V92Phase4Modulator.h`, `V90AutoDigitalImpDetector.h`,
`V90Demapper.h`, `callprog_state.h`). Sixteen files are pure
identifier-boundary substitutions, including all the `.cpp` and `test/unit`
sources; no non-target `word_18`, `f1c`, `byte_1c` or `word_0040` occurrence
changed. String literals that name a field are preserved byte-for-byte (the
`"f1c"`, `"short_1ea4 (%ld)"`, `"short_1ea6 tracks the second run"`,
`"word_0040 took argument five (%ld)"` and `"the byte_1c latch ..."` strings);
only the identifiers beside them moved.

A token-count check over every machine-edited source/test file reports 0
non-`{old,new}` identifier count differences for all sixteen pure files. Each
old->new pair is balanced there (for example `V90Phase4Modulator.cpp`:
`word_0018` 11->0 / `repeatCpCount` 0->11, `byte_001c` 9->0 /
`repeatCpEnable` 0->9, `word_0040` 4->0 / `suvLimit` 0->4). The only
old-token residues are the preserved string literals (`t_v90modchain.cpp`
`word_0040` 7->1, `t_v92p4sym.cpp` `byte_1c` 7->2, `t_v90demap.cpp`
`short_1ea4` 9->4 and `short_1ea6` 8->3, `t_callprog_create.c` `f1c` 3->1),
and the only new>old excess is the added prose in the five rewritten header
comments.

Eleven mutation manifests were edited by DECODING JSON and substituting only
`find`/`replace` values -- the raw files contain literal `\t`, which defeats a
naive `\b` match. A decoded-JSON re-derivation of `HEAD` reproduces every
changed `find`/`replace` value exactly, with 0 other leaf strings changed:
labels, `why`, `description`, `note`, `equivalent` and the file-level `_`
descriptions (including `v90p4mreset.json`'s `word_0040` mention) are
byte-for-byte identical, and the blank-line formatting of `v90p4mreset.json`
is preserved. `v27status.json` and the `v92ec.json` label were left untouched.

### Gate

Batch 19 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and **`phase boundary: period differential and
structural checks all OK`**. All 655 `build/period/*.o` hashes are
**byte-identical** to the pre-edit snapshot
(`/home/philpem/slmodem/tmp/issue119-tier-b-before.sha256`, 655/655).

## Batch 20: issue #119 Tier C -- V90Parameters BLL durations

The four QUICK-CONNECT BLL transition thresholds in `V90Parameters` were
`unnamed_*`. They are compared against `bllSamples` in `V90Demodulator`'s
BLL state machine, each gating exactly one transition, so each is named by
that transition in the `<FROM>_TO_<TO>_DURATION` shape the two non-QC
thresholds above already use (`BLL_TRN1D_INITIAL_TO_FAST_DURATION`,
`BLL_TRN1D_FAST_TO_SLOW_DURATION`). Usage inference -- the object prints no
name -- but the constant, the comparison and the state change are one site.

| Previous | New | Gate |
| --- | --- | --- |
| `unnamed_100` | `BLL_TRN1D_SLOW_TO_SLOW2_DURATION` | `V90_BLL_SLOW` -> `V90_BLL_SLOW2` (`V90Demodulator.cpp:1493`) |
| `unnamed_104` | `BLL_TRN1_QC_INITIAL_TO_FAST_DURATION` | `V90_BLL_TRN1_QC_INITIAL` -> `_FAST` (`:1496`) |
| `unnamed_108` | `BLL_TRN1_QC_FAST_TO_MEDIUM_DURATION` | `V90_BLL_TRN1_QC_FAST` -> `_MEDIUM` (`:1499`) |
| `unnamed_10c` | `BLL_TRN1_QC_MEDIUM_TO_SLOW_DURATION` | `V90_BLL_TRN1_QC_MEDIUM` -> `_SLOW` (`:1502`) |

`v90demprog.json`'s "the ladder's comparison is strict" anchor follows the
`unnamed_10c` rename; its label and fault are unchanged.

LEFT RETAINED, with the reasons on record, not renamed:

- `unnamed_0f4` and `BLL_TRN1_QC_SLOW_K2` at +0x0f0: D901 records an alias
  ambiguity, and `V90Parameters.cpp:176-177` reads BOTH `BLL_TRN1_QC_SLOW_K1`
  and `BLL_TRN1_QC_SLOW_K2` into the +0x0f0 field -- a separate wiring
  question, not a naming one. Do not rename these until that is resolved.
- `unnamed_080` (`nofUcodesInTrn2`'s configured value): finding 3527
  deliberately ruled it keeps its offset name.
- `unnamed_1b0/1b4/1b8` (the German-PBX DIL betas): the alias fields
  `GERMAN_PBX_LINEAR_EQU_DIL_*_BETA` at +0x18c/0x190/0x194 already carry
  those names, and the unnamed fields are their source; a non-colliding name
  needs the alias/source relationship resolved first.
- `unnamed_434`, `unnamed_440`: each is used once (a V.34-fallback threshold
  and the `..._ALT_RBS` mean-error ratio) and the object prints neither; a
  name would be inference with no typed anchor.
- `_tagModemParameters::unnamed_0003` (`cfgFlags3`): sits in the host
  `dp_runtime` record, not `V90Parameters`; naming it crosses a namespace
  boundary the inventory warns about.

Batch 20 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical.

## Batch 21: issue #119 Tier D -- census defects and stale banners

No renames; this corrects the inventory apparatus and one stale banner.

- `tools/namingcensus.py` classified the real identifiers `ref`
  (`sgd_det_cfg`, "reference sequence") and `read` as offset names -- both
  match the `^[rf][0-9a-fA-F]{2,}$` `rNN`/`fNNNN` shape (`ref` is r+ef, `read`
  is r+ead). A stoplist now keeps dictionary words on the `named` side, and
  `docs/naming-inventory.md` is regenerated from the corrected tool.
- `ModulusCoder.h`'s banner said `progress` "is another batch's work" and that
  the seven fields' meanings were unstated. Both `progress` members are
  written (`V90ModulusEncoder.cpp`, `V90ModulusDecoder.cpp`) and the
  constructor's `@param` docs give each field a role -- five conversion
  moduli, a bit count and one argument neither reads. The declaration comment
  is corrected; the fields keep offset names because the object spells none.
  Naming them (and thereby `V90Mapper`/`V90Demapper::word_08` as
  `modulusBitCount`) remains on issue #119.

Batch 21 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`.


## Batch 22: ModulusCoder fields and word_08 (issue #119)

`ModulusEncoder` and `ModulusDecoder` (one header, one layout, adjacent in the
blob) each carry seven `unsigned int` members that the constructor's mangling
and seven ascending stores settle as seven, and that `progress` now types:
five mixed-radix conversion moduli and a bit count. The names come from the
caller, not from inference -- `V90Mapper::reset` (`V90Mapper.cpp:253-259`) and
`V90Demapper::reset` (`V90Demapper.cpp:780-786`) store their six
`constellationSize[]` entries into +0x00..+0x14 and their modulus bit count
into +0x18, in that order, and the constructor's `@param` docs already tie the
arguments to those roles.

| Previous | New | Role |
| --- | --- | --- |
| `ModulusEncoder/Decoder::field_00` | `constellationSize0` | modulus for output digit 0 |
| `ModulusEncoder/Decoder::field_04` | `constellationSize1` | modulus for output digit 1 |
| `ModulusEncoder/Decoder::field_08` | `constellationSize2` | modulus for output digit 2 |
| `ModulusEncoder/Decoder::field_0c` | `constellationSize3` | modulus for output digit 3 |
| `ModulusEncoder/Decoder::field_10` | `constellationSize4` | modulus for output digit 4 |
| `ModulusEncoder/Decoder::field_14` | `constellationSize5` | stored, read by neither `progress`; the sixth digit is the remainder |
| `ModulusEncoder/Decoder::field_18` | `bitCount` | bits packed/unpacked by `progress` |
| `V90Mapper::word_08` | `modulusBitCount` | `bitsPerFrame - signBitsPerFrame` |
| `V90Demapper::word_08` | `modulusBitCount` | the same, seventh word handed to `ModulusDecoder` |

The `field_14` disposition is not a rename of convenience: only the first five
members are divided out as moduli, and `out[5]` is the leftover accumulator, so
naming it `constellationSize5` records what the caller stores rather than
claiming a sixth divisor. `progress` is what resolves `word_08` from "the
frame bits that are not sign bits" to the modulus bit count: the demapper's
`resetNoSpectral` computes `bitsPerFrame - signBitsPerFrame` and stores it as
the decoder's seventh word, and `ModulusDecoder::progress` reads that word as
its bit count. `V90Mapper.h`'s "deliberately unnamed spelling" and
`V90Demapper.h`'s "left unnamed on purpose" paragraphs are corrected to say so.

CROSS-OWNER TRAPS, all left untouched: `V92ModulusEncoder::field_18`
(`include/dsplib/V92ModulusEncoder.h`, `src/pump/v90/V92ModulusEncoder.cpp`,
`test/mutations/v92modulusencoder.json`) is a different class's member;
`V90CP::word_08` (`include/dsplib/V90CP.h`, `V90CP.cpp`, `v90cpb2i.json`,
`t_v90cpb2i.cpp`, `t_v90cpinfo.cpp`, `t_v90modprog.cpp`, `t_v90p4mgen.cpp`) is
a V90CP field; `TAG_DiagnosticResults::word_080` is a different identifier
entirely. `t_v90modchain.cpp` mixes both owners in one file -- its V90Mapper
`word_08` (lines 746, 965, 1243) is renamed and its V90CP `word_08`
(line 3834, `plant_cp`) is not. The `MODENC_OFF`/`MODDEC_OFF` assertions take
the new member name but keep their original tags and offsets (rule 4);
`V90MAPPER_OFF`/`DEM_OFF` tags were already offset-spelled and did not move.
Mutation manifests were transformed structurally in their `find`/`replace`
VALUES only; every label, fault and other property is byte-for-byte unchanged,
and all eight changed manifests' `find` strings still match their source
exactly once.

Batch 22 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical.

## Batch 23: V90Parameters pdsnr/threshold names (issue #119)

Batch 20 retained `unnamed_434` and `unnamed_440` on the ground that each was
used once with no typed anchor. Both now have one, so this batch supersedes
that disposition. Neither type changed: `+0x434` stays the declared `int` and
`ce_param_float` union read, and `+0x440` stays `float`.

`unnamed_434` is named from the object's own diagnostic. In
`V90ConnectionEvaluator::evaluatePhase4` the store `fsts 0xac(%ebx)` copies
this parameter word into the evaluator's `phase4ErrorForV34Fallback`
(`V90ConnectionEvaluator.cpp:882`), and the debug string printed beside exactly
that store names the value stored:
`"V90ConnectionEvaluator(phase4): pdsnrCurrentV34DropThreshPhase4 set to = ..."`
(`:887-891`). That is the author's identifier for the slot, and
`include/dsplib/V90ConnectionEvaluator.h:566-597` already derives the whole
path. The F878 int/float question is a separate site and is not touched here.

`unnamed_440` is the ALT-RBS variant of the already-named `+0x438` threshold.
`V90Phase3Demodulator` copies it into
`PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH` only when
`autoDigitalImpDetector->isThereAnyAltRbsPhase()` is true
(`V90Phase3Demodulator.cpp:800`, `:956`, `:1133`) and unconditionally in
`setAltRbsParams` (`:2397`), sitting directly beside the already-named QC
variant at `+0x43c` (`QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH`).

| Previous | New | Anchor |
| --- | --- | --- |
| `V90Parameters::unnamed_434` | `PDSNR_CURRENT_V34_DROP_THRESH_PHASE4` | diagnostic beside the `fsts 0xac(%ebx)` store (`V90ConnectionEvaluator.cpp:887`) |
| `V90Parameters::unnamed_440` | `PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH_ALT_RBS` | copied into `+0x438` only under `isThereAnyAltRbsPhase()` (`V90Phase3Demodulator.cpp:800,956,1133,2397`) |

Mutation manifests were transformed structurally in their `find`/`replace`
VALUES only. `v90p3ddec.json`'s two alt-RBS anchors keep their faults: the
swap still exchanges `..._THRESH` and `..._THRESH_ALT_RBS`, and the
one-slot-along mutation still writes the QC threshold. The `why` prose in both
manifests and the four C string literals in `t_v90conneval.cpp` that spell the
old identifier (`:1970`, `:2070`, `:3586`, `:3592`) are deliberately left as
they were -- prose and literals are not identifiers. Every changed `find`
still matches its source exactly once, and all 260 finds in the two manifests
still resolve.

Batch 23 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical.

## Batch 24: issue #119 final (D901, float_a980, German-PBX disposition)

The last three open #119 items. This batch resolves the D901 field-name
correction and the German-PBX source disposition that Batch 20 left retained,
and names `float_a980`.

### A. D901: the +0x0f0 field is `SLOW_K1`, and +0x0f4 is `SLOW_K2`

`docs/deviations.md` D901 records that `loadParams` reads the
`"BLL_TRN1_QC_SLOW_K1"` string and then the `"BLL_TRN1_QC_SLOW_K2"` string
into the SAME field at +0x0f0, and that +0x0f4 is the real K2 slot. The field
names were therefore on the wrong offsets. They are corrected:

| Offset | Previous | New | Anchor |
| --- | --- | --- | --- |
| +0x0f0 | `V90Parameters::BLL_TRN1_QC_SLOW_K2` | `V90Parameters::BLL_TRN1_QC_SLOW_K1` | `V90Resampler::setBllState`'s TRN1_QC_SLOW arm copies +0x0f0 into `bllK1` (`V90Resampler.cpp:273`); D901 |
| +0x0f4 | `V90Parameters::unnamed_0f4` | `V90Parameters::BLL_TRN1_QC_SLOW_K2` | the same arm copies +0x0f4 into `bllK2` (`:274`); D901, and the 2e-12f `K2` series |

The defect itself is **preserved**: `V90Parameters.cpp:180-181` still reads
both file names into the one field (`&BLL_TRN1_QC_SLOW_K1`), and
`setToDefault` writes 0.0001f / 2e-12f into +0x0f0 / +0x0f4 under the
corrected names. String literals (`"BLL_TRN1_QC_SLOW_K1"`,
`"BLL_TRN1_QC_SLOW_K2"`) are unchanged, as is `t_v90loadparams`, which
compares the two calls entry for entry. Header and file comments were
rewritten to say +0x0f0 is `SLOW_K1` (the K2-name read into it being D901's
defect) and +0x0f4 is `SLOW_K2`; the "alias ... K1 -- D901" wording is gone.

`tools/paramcheck.py` gains a **declared exception** for the defect. Its rule
is that the last read of an offset is the name the header carries; D901 is
the one offset where that is false. `DEFECT_READS` names
`('V90Parameters.h', 0x0f0)` -> first read `SLOW_K1`, with the reproduced
second read `SLOW_K2` still required, so a repair of the object's defect would
also fail the gate and have to update the register. The other three aliases
(`+0x18c/+0x190/+0x194`) keep the last-read rule. Both branches were shown to
fire: a perturbed +0x0f0 header name gives `NAME`, and a perturbed expected
second-read name gives `DEFECT`, each exit 1; restored, exit 0.

### B. `float_a980` -> `varThreshScale`

`V90AutoDigitalImpDetector::float_a980` is set 1.75f / 1.5f by the
connection-type test (`setConnectionType`, `:467,471`; `reset`, `:520,525`)
and read once as `product = sum * varThreshScale * 0.05f; varThresh =
product;` (`:2684`), scaling the reference-phase variance sum into the
maximum-ucode threshold. The name matches the code's own `varThresh`. This is
**usage inference** -- no string or callee types it -- and the header comment
says so. No type changed: it stays `float`.

### C. German-PBX source betas -- explicit disposition, no rename

`V90Parameters::unnamed_1b0/1b4/1b8` (+0x1b0/1b4/1b8, floats, hardcoded
8.5e-11 / 6e-11 / 1.5e-11) are copied into the active DIL-beta fields at
+0x18c/0x190/0x194 by `V90Demodulator.cpp:1314-1319` when the connection type
is 2 (German PBX), exactly as `params->LINEAR_EQU_DATA_BETA =
params->GERMAN_PBX_LINEAR_EQU_DATA_BETA` sits above them (`:1310-1311`). The
+0x18c/0x190/0x194 fields are read by `V90Equalizer.cpp:2292,2301,2319,2325,2331`
under their existing `GERMAN_PBX_LINEAR_EQU_DIL_*` names and are also read
from the parameter file under the generic names (`V90Parameters.cpp:211-213`).

Because the source set would collide with the active field names it
overwrites, it correctly keeps offset names. No German-PBX field was renamed.
The header comments for **both** sets now record this structure and the reason
the source set stays neutral.

### Mutation manifests and verification

`test/mutations/v90resampler.json` and `test/mutations/v90adid.json` were
transformed structurally -- JSON decoded, only `find`/`replace` VALUES
substituted -- because the raw files carry literal `\t`. Labels, `why`,
`description`, `note`, `equivalent` and every other property are byte-for-byte
identical. Every changed `find` still matches its source exactly once
(v90resampler 4 changed leaves, v90adid 12; 0 non-find/replace leaves changed).
The resampler "reads +0x0f0 for both gains" fault is preserved: its `replace`
still writes the +0x0f0 field (`BLL_TRN1_QC_SLOW_K1`) into `bllK2`.

A token-aware forward substitution of `HEAD` reproduces all eight changed
source/test files. Four of them (`V90Parameters.h`, `V90Parameters.cpp`,
`V90Resampler.cpp`, `V90AutoDigitalImpDetector.h`) carry intended prose
rewrites in comments; the other four are pure identifier substitutions with
zero residual diff. Over CODE tokens only (comments blanked, string literals
preserved), every file reports **0 non-`{old,new}` identifier count
differences**, and each pair balances: `V90AutoDigitalImpDetector.cpp`
`float_a980` 6->0 / `varThreshScale` 0->6, `t_v90adid.cpp` 6->0 / 0->6, and
the chained pair `BLL_TRN1_QC_SLOW_K2`/`unnamed_0f4` ->
`BLL_TRN1_QC_SLOW_K1`/`BLL_TRN1_QC_SLOW_K2` conserves tokens in
`V90Parameters.cpp` (4/1 -> 2/4 reads plus the stores). The manifests report
the same: 0 non-`{old,new}` token diffs.

`docs/naming-inventory.md` was regenerated: named 2630 -> 2632, offset-named
391 -> 390, placeholder 74 -> 73.

### Gate

Batch 24 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical
(`/home/philpem/slmodem/tmp/issue119-final-before.sha256` vs
`issue119-final-after.sha256`).

## Batch 25: FAX V.17 receive cluster (issue #130)

Owner-scoped first pass over the V.17 receive cluster: `struct v17rx_state`,
`struct v17rx`, `struct v17rx_priv`, `struct v17rx_cfg` and `struct
v17rx_ctl`. This is an owner-scoped candidate denominator, not a claim that
the FAX inventory is complete. `v17_smc` and `v17tx_fp` are transmit-only
readers and are out of scope.

Six fields are renamed, each by evidence rank 2 (a typed destination or an
already-named sibling field of the same role); the rest are retained neutral
with the reason recorded beside them. A wrong name is worse than an offset,
and the object gives most of this cluster no role to carry across.

### Renamed

| Owner | Previous | New | Evidence | Confidence |
| --- | --- | --- | --- | --- |
| `v17rx_cfg` | `int_0000` | `protocol` | `V17RX_status` stores it into `v17_status::protocol` (`v17.c:1781`); `V17RX_OBJ_PROTOCOL` is +0x00, and `v17tx_cfg` names the same offset `protocol` | high |
| `v17rx_cfg` | `int_0014` | `short_train` | `V17RX_create` copies it into the already-named `v17_dec::short_train` (`v17.c:345`), the decoder field that selects the short-vs-long training path | high |
| `v17rx_ctl` | `int_0010` | `short_train` | `V17RX_control` copies it into `cfg->short_train` on REINIT (`v17.c:1729`), the field above | high |
| `v17rx_priv` | `r10` | `short_train` | `V17RX_create` copies it straight into `v17_dec::short_train` (`v17.c:529`); `v17dec.h` already records that the decoder field comes from the control block at +0x10 | high |
| `v17rx_state` | `r4fb0` | `quality_threshold` | `QualityDetectV17` compares the smoothed error `qavg` against it (`v17.c:3000`); `V17RX_create` seeds it per rate (0xa28/0x514/0x341/0x1c2). Usage inference, single reader; the offset comment block already called it "the quality threshold" | medium-high |
| `v17rx_priv` | `r2e` | `offband_latch` | set on a data-carrier failure and gates the V.21 offband watch (`v17.c:2892`, `:2895`); `V17RXC_SHORT_002E`'s own block calls it "a latch". Usage inference, single role | medium |

The three names form one chain -- `v17rx_ctl::short_train` ->
`v17rx_cfg::short_train` -> `v17rx_priv::short_train` ->
`v17_dec::short_train` -- and are the same value at each hop. No format
string names any of them; the carry-across is from our own already-accepted
decoder field, and it does not claim an author string.

### Retained neutral, with the reason

| Owner | Member | Reason | Confidence in retention |
| --- | --- | --- | --- |
| `v17rx_cfg` | `short_0006` | copied in, never read | high |
| `v17rx_cfg` | `int_0008` | written by `V17RX_control` from its +0x04, read by nothing | high |
| `v17rx_cfg` | `int_000c`, `int_0010` | never read | high |
| `v17rx_ctl` | `int_0004` | destination `cfg->int_0008` is unread; no role | high |
| `v17rx_priv` | `r08` | gate `RxHdxDataV17` must see as zero; three writers, no established meaning (F9442) | high |
| `v17rx_priv` | `r0e`, `r22` | unread/unwritten in the object | high |
| `v17rx_priv` | `r20` | read once to pick a `DataCarrierDetectV17` body, never written by anything traced; role unstated | high |
| `v17rx_state` | `r04`, `r08`, `r10` | ANDed with the AGC `signal` flag to enable the SRE adapt / FSE PLL / FSE LMS loops; only the plumbing is established, not a meaning (F9102) | high |
| `v17rx_state` | `r00` | cleared by `V17RX_control` and reported as a status bit; multi-role | high |
| `v17rx_state` | `r1c` | bit 0 reported as a status bit; meaning unstated (F9474) | high |
| `v17rx_state` | `r0c`, `r14`, `r18`, `r20`, `r28` | constructor writes one constant, nothing reads | high |
| `v17rx_state` | `r26` | read by nothing | high |
| `v17rx_state` | `r4f88[4]` | four-byte gap below the equaliser, untouched | high |
| `v17rx_state` | `r4fb2` | write-only quality verdict; no reader in the object | high |
| `v17rx` | `r42`, `r4e`, `r5a` | never written by the constructor or anything else | high |
| `v17rx` | `r44`, `r48`, `r4c`, `r50`, `r54`, `r58` | constructor writes 0, nothing reads | high |

`struct v17rx_ctl`'s `unmapped_0000`/`unmapped_0008` keep their explicit
placeholder names and `flags_0c`/`flags_0d` their existing names.

### Scoping, macros and manifests

Every occurrence of the six old identifiers was classified by owning object
before editing. The rename touches only the five V.17 receive owners:
`include/dsplib/v17fax.h`, `include/dsplib/faxcfg.h`, `src/fax/v17.c`,
`src/fax/faxcfg.c`, `src/fax/class1rx.c`, `test/unit/t_faxcfg.c` and
`test/unit/t_v17rxcreate.c`. The V.27/V.29 `int_0000`/`int_0014` fields,
`faxvmi_link::int_0014`, `voice`'s and `v21cfg`'s `int_0014`, and
`v17tx_control_req::int_0010` are untouched; the `test/unit/t_v27fax.c` and
`t_faxadapt.c` occurrences are other owners' fields.

The offset constants that name the same storage keep their spelling, because
the tests reach the fields through them: `V17RXC_INT_0010` (+0x10),
`V17RXC_SHORT_002E` (+0x2e) and `V17RXS_SHORT_4FB0`/`_4FB2` (+0x4fb0/2).
Their comments now record the field names. There are no `*_OFF` assertion
macros in this cluster, so no assertion-label identifier moved.

No mutation suite covers these files (`test/mutations/suites.json` maps only
`v17data` to `src/pump/v17/v17data.c`, the transmit data leaves), so no
manifest carries a changed `find`/`replace`.

### Verification

A token-aware forward substitution of `HEAD` reproduces every changed
non-doc file; the header files differ only by intended prose rewrites in
comments. Test edits are identifier-only with string labels and line counts
preserved, because `diff_eq_int` embeds `__FILE__`/`__LINE__` and the period
test objects must stay byte-identical. The `anchorcheck`/`refcheck` tiers
report 0 anchors matching other than exactly once over 10041 mutations and
13966 references.

### Gate

Batch 25 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical
(`/home/philpem/slmodem/tmp/fax-v17rx-before.sha256` vs
`/home/philpem/slmodem/tmp/fax-v17rx-after.sha256`).

`docs/naming-inventory.md` was regenerated: named 2632 -> 2638, offset-named
390 -> 384, on-record 245 -> 273, residual 145 -> 111.

## Batch 26: FAX V.21 cluster (issue #130)

Owner-scoped pass over the V.21 control-channel cluster: `struct v21_rx`,
`struct v21_rx_dsp`, `struct v21rx_cfg`, `struct v21tx_cfg`,
`struct v21_status`, `struct v21_tx_hdx`, `struct v21rx_ctl` and
`struct v21tx_ctl`. This is an owner-scoped candidate denominator, not a
claim that the FAX inventory is complete. The V.17/V.27/V.29 owners are
untouched.

Two fields are renamed, each by evidence rank 2 (a typed destination plus an
already-named sibling at the same offset with the same role); the other
thirty-two members in scope are retained neutral with the reason recorded
beside each field. A wrong name is worse than an offset, and most of this
cluster is written by a constructor and read by nothing.

### Renamed

| Owner | Previous | New | Evidence | Confidence |
| --- | --- | --- | --- | --- |
| `v21tx_cfg` | `short_0000` | `protocol` | `V21TX_status` stores it into `v21_status::protocol` (`v21.c:1272`); `V21TX_OBJ_PROTOCOL` is +0x00; `v27tx_cfg` and `v29tx_cfg` name the same offset `protocol`. `V21TX_create` tests it 16-bit (`movzwl`/`test %ax`, 0x0993c4), forcing the `short` width. | high |
| `v21tx_cfg` | `int_0010` | `flags` | `V21TX_control` ORs bit 2 into the byte at +0x10 (`orb $0x4,0x10(%esi)`, 0x0a2bc9) and `V21TX_status` reads that byte back (`movzbl 0x10(%ecx)`, `and $0x4`) into `v21_status::flags`; `V21TX_OBJ_FLAGS` is +0x10; `v27tx_cfg`/`v29tx_cfg` name +0x10 `flags` for the same bit-2 role. The struct keeps `int` -- the 7-dword copy at 0x099328 is width-blind and widths are held -- and the byte is reached by the existing cast. | high |

### A stale claim corrected

`v21fax.h`'s banner and `v21cfg.h`'s `v21tx_cfg` block both said the transmit
config area was WRITE-ONLY, "read back by nothing reconstructed". That is
false, and it was the stated reason these two fields had stayed offsets:
`V21TX_status` reads +0x00 and +0x10 out of the handle, and `V21TX_control`
writes +0x08 and +0x10. The sentences are corrected in place. This is the
F10139/F10140 shape -- a rationale that went stale when a reader was
reconstructed -- and it is exactly what Batch 25's method note warns about.
The old "CONFIRMED-EXHAUSTED" text also claimed the siblings' `flags` field
was "further along the struct"; it is at +0x10 in both `v27tx_cfg` and
`v29tx_cfg`, inside the range the text said it was outside.

### Retained neutral, with the reason

| Owner | Member | Reason | Confidence in retention |
| --- | --- | --- | --- |
| `v21_rx` | `ptr_001c` | `V21RX_create` stores `dsp->fsd.trace` here; no reconstructed reader, so the field's purpose is not established. `V21RX_OBJ_TRACE` names the source, not the role. | high |
| `v21_rx` | `int_0020` | written 0 by the constructor; read by nothing | high |
| `v21_rx` | `ptr_0024` | constructor stores `&dsp->fsd.last_count`; no reader | high |
| `v21_rx` | `int_0028`, `int_002c`, `int_0034`, `int_0038`, `int_0040`, `int_0044` | written 0; read by nothing | high |
| `v21_rx` | `short_0030`, `short_003c`, `short_0048` | written 0; read by nothing | high |
| `v21rx_cfg` | `short_0002`, `short_0006`, `int_000c`, `int_0010` | zero and untouched; read by nothing; `v17rx_cfg`/`v27rx_cfg`/`v29rx_cfg` carry the same shape unnamed | high |
| `v21rx_cfg` | `int_0008` | 60000; `V21RX_control` writes it from its own +0x04, read by nothing; 60000 in all four sibling tables | high |
| `v21tx_cfg` | `short_0004`, `short_0006` | copied, never read | high |
| `v21tx_cfg` | `int_000c` | 3200; never read; no sibling carries a role at +0x0c (`v17tx_cfg` is unnamed there, V.27's `scale_mul` is PPS-specific) | high |
| `v21tx_cfg` | `int_0014` | 0; never read; NOT the siblings' `fifo_size_factor` -- V.21's transmit FIFO is a fixed 6 elements, not derived from any field here | high |
| `v21tx_cfg` | `int_0018` | 0; never read; matches V.27's/V.29's own still-unnamed `int_0018` | high |
| `v21_status` | `short_0a` | written 0 by both fillers; `v22_status`/`v17_status`/`v29_status_prefix` leave it unnamed | high |
| `v21_status` | `short_0c` | written 0 by `V21TX_status`; siblings leave it unnamed | high |
| `v21_status` | `short_12` | written 0 by TX; `V21RX_status` computes it from `fsd.f22`/`fsd.cfg.bit_samples`, one route to the same 300 the literal `rx_bps` carries; no sibling name | high |
| `v21_tx_hdx` | `int_0004` | arm gate (zero selects the FIFO arm); set from `V21TX_control`'s flags bit 4; meaning unstated; `v27_tx_source::int_0008`/`v29_tx_params::int_0008` are the same gate and unnamed | high |
| `v21_tx_hdx` | `short_000e` | zeroed by the START and IDLE transition arms only; no reader | high |
| `v21rx_ctl` | `int_0004` | copied into `cfg->int_0008`, which nothing reads; no role | high |
| `v21tx_ctl` | `int_0004` | copied into `cfg->int_0008`, which nothing reads; no role | high |
| `v21_rx_dsp` | `int_0000` | read by nothing traced | high |
| `v21_rx_dsp` | `int_0004`, `int_0008` | ANDed by `CarrierDetectV21`; roles known (`agc.signal` / tone-present) but the only B.103 parallel would mis-name one of the pair (F8895) | high |

`v21tx_cfg::int_0008` is already on record (the 60000 literal every sibling
carries) and was not a candidate in this batch; `bit_rate` is already named.

### Scoping, macros and manifests

Every occurrence of `short_0000`/`int_0010` was classified by owning object
before editing. The rename touches only `include/dsplib/v21cfg.h`,
`include/dsplib/v21fax.h`, `src/fax/v21.c`, `src/fax/v21cfg.c` and
`test/unit/t_v21txcreate.c`. `Fdspkrnl`'s `short_0000[2000]`, the retained
`v21rx_cfg::int_0010`, `v17tx_cfg::int_0010`, `v17tx_ctl::int_0010`,
`faxvmi_cfg::int_0010` and the V.27/V.29 `int_0010` are untouched.

The offset constants keep their spelling: `V21TX_OBJ_PROTOCOL` (+0x00) and
`V21TX_OBJ_FLAGS` (+0x10), which the tests reach through; the
`V21TX_PROTOCOL(m)` accessor now reads `cfg.protocol`. There are no `*_OFF`
assertion macros in this cluster.

No mutation suite covers any V.21 file: `test/mutations/suites.json` maps no
`src/fax/v21*.c`, `V21rx.c`, `V21tx.c` or `class1*.c`.
`test/mutations/fdspkrnl.json`'s `short_0000` is the fdspkrnl array, a
different owner, and is unchanged. So no manifest carries a changed
`find`/`replace`.

### Verification

A token-aware, owner-scoped forward substitution of `HEAD` reproduces every
changed non-doc file, code token for code token: `v21cfg.h` 95, `v21fax.h`
772, `v21.c` 2391, `v21cfg.c` 34 and `t_v21txcreate.c` 1586. String literals
(the `t_v21txcreate.c` case labels) are preserved, and the test-local
`cases[].short_0000` is renamed with the field it feeds. Manifest check: every
non-`{old,new}` identifier count is unchanged. The pairs balance --
`short_0000` 13 -> 0 against `protocol` 3 -> 16, and `int_0010` 2 -> 1 against
`flags` 35 -> 36 -- and the one surviving `int_0010` is the retained
`v21rx_cfg` field, the only `int_0010` code occurrence left.

### Gate

Batch 26 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`; 655/655 period objects byte-identical
(`/home/philpem/slmodem/tmp/fax-v21-before.sha256` vs
`/home/philpem/slmodem/tmp/fax-v21-after.sha256`). Structural checks in the
same log: 13973 references resolve, 10041 mutations over 272 suites, 0 anchors
matching other than exactly once, 2202 offset annotations matching.

`docs/naming-inventory.md` was regenerated: named 2638 -> 2640, offset-named
384 -> 382, on-record 273 -> 300, residual 111 -> 82.

## Batch 27: FAX V.27/V.29 clusters (issue #130)

Owner-scoped pass over the V.27 and V.29 FAX clusters. This is an
owner-scoped candidate denominator, not a claim that the FAX inventory is
complete; the V.17 and V.21 owners are untouched.

V.27 owners: `v27rx_cfg`, `v27_rx`, `v27_rx_block`, `v27_rx_shared`,
`v27rx_ctl`, `v27_status_prefix`, `v27_tx_source`, `v27tx_cfg`,
`v27tx_ctl`. V.29 owners: `v29rx_cfg`, `v29_rx`, `v29_rx_block`,
`v29_rx_decoder`, `v29_rx_detector`, `v29_status_prefix`, `v29_tx_params`,
`v29tx_cfg`, and the two runtime control-request types.

The V.29 owner names in the batch brief -- `struct v29rx_ctl`, `struct
v29tx_ctl`, `struct v29_rx_shared` -- do not exist in this tree. V.29's
runtime requests are `struct v29rx_control_req` / `struct
v29tx_control_req` (`v29fax.h`), and V.29 has no shared block: the
detection block (`v29_rx_detector`) plays that role. Those are the types
reviewed here.

Four fields are renamed, each by evidence rank 2 (a typed destination or
an already-named sibling with the identical role); every other member in
scope is retained neutral with the reason recorded beside it in its own
header and below. A wrong name is worse than an offset, and most of the
V.27 cluster is a constructor seed that nothing reads.

### Renamed

| Owner | Previous | New | Evidence | Confidence |
| --- | --- | --- | --- | --- |
| `v27rx_cfg` | `int_0014` | `short_train` | `V27RX_create` derives `v27_rx_shared::train_long` from `(short_train == 0)` (`v27.c:233`); `v17rx_cfg::short_train` is the same field at the same offset with the identical role (Batch 25), and the `v17_dec::short_train` chain is the same value at each hop | high |
| `v29rx_cfg` | `int_0000` | `protocol` | `V29RX_status` stores it into `v29_status_prefix::protocol` (`v29.c:1714`); `v17rx_cfg::protocol` is the same field at the same offset on the identical store-to-status evidence (Batch 25) | high |
| `v29_tx_params` | `short_0016` | `countdown` | `v27fax.h`'s `struct v27_tx_source::countdown` is the same field at the same offset (+0x16): the per-state budget `TxNextStateV29` seeds and `TxHdxQuiet/AB/EQCond/SCR1V29` decrement and test (`v29.c:2100`..`2236`); `TxHdxDataV29` reads it as a one-shot flag | high |
| `v29tx_control_req` | `int_0008` | `scale_mul` | `V29TX_control` stores it into the shaper's `cfg.scale`, multiplied by `V29TX_PPS_SCALE[rate]` (`v29.c:2336-2337`); `v27tx_ctl::scale_mul` and `v17tx_control_req::scale_mul` are the same field of the same five-effect request shape, and `v17fax.h:1928-1932` (F10144) named this copy a candidate for whoever visited V.29 next | high |

`short_train`, `protocol` and `scale_mul` are carried from the siblings
because the store target and the role are identical, not because the
offsets are adjacent. `countdown` is the one name carried from V.27 rather
than from a V.29 reader, and V.29's four handlers' own decrement/test loop
is the second, independent statement of the role.

### Retained neutral, with the reason

| Owner | Member | Reason | Confidence in retention |
| --- | --- | --- | --- |
| `v27rx_cfg` | `int_0000` | no reader; V.27's `V27RX_status` reports only whether a block was supplied, so it has no `protocol` store target to carry the V.17 name to | high |
| `v27rx_cfg` | `short_0006` | never read | high |
| `v27rx_cfg` | `int_0008` | written by `V27RX_control` from its `+0x04`, read by nothing | high |
| `v27rx_cfg` | `int_000c`, `int_0010` | never read | high |
| `v27rx_cfg` | `ptr_0018` | the constructor's 4th argument, handed to three modules' `aux`/`reserved34` slots; no agreed name, and `v17rx_cfg::ptr_0024` is unnamed too | high |
| `v27_rx` | `int_0038`, `int_003c`, `short_0040`, `int_0044`, `int_0048`, `short_004c` | the six `V27RX_create` zeroes; nothing else in the object touches any of them | high |
| `v27_rx_block` | `int_0000` | set 1 by `V27RX_create`, cleared by `V27RX_control`'s mask bit 3; no reader | high |
| `v27_rx_block` | `int_000c` | constructor writes 0; no reader | high |
| `v27_rx_shared` | `int_0004` | `V27RX_control` writes 0/1, `RxHdxDataV27` gates on zero; the gate is measured but the field's meaning beyond it is not. `V29DET_INT_0008` (the same gate one modulation over) is likewise unnamed | high |
| `v27rx_ctl` | `int_0004` | copied into `v27rx_cfg::int_0008`, which nothing reads; `v17rx_ctl::int_0004` is the same | high |
| `v27_status_prefix` | `short_0e` | not written by `V27TX_status`; `v17_status::short_0e` is the same slot, likewise unnamed | high |
| `v27_status_prefix` | `short_16` | not written by `V27TX_status`; `v17_status::short_16` is the same slot, likewise unnamed | high |
| `v27_status_prefix` | `word_18` | written from `v27tx_cfg::int_0018`, which is itself unnamed; `v17_status::int_18` and `v22_status` carry the slot without a semantic name | high |
| `v27_tx_source` | `int_0008` | the underrun gate; nothing reconstructed sets it non-zero except `V27TX_control`'s own ctl1 bit 4, and `V29TXP_INT_0008` is the same gate, also unnamed | high |
| `v27tx_cfg` | `int_0004` | never read | high |
| `v27tx_cfg` | `short_0012` | never read | high |
| `v27tx_ctl` | `int_0004` | copied into `v27tx_cfg::int_0008`, which nothing reads; `v17tx_control_req::int_0004` is the same | high |
| `v27tx_ctl` | `int_0010` | copied into `v27tx_cfg::int_0018`, the `V27TXP_TRAIN_LONG` source; that source is itself unnamed | high |
| `v29rx_cfg` | `short_0006` | never read | high |
| `v29rx_cfg` | `int_0008` | written by `V29RX_control` from its `+0x04`, read by nothing | high |
| `v29rx_cfg` | `int_000c`, `int_0010` | never read | high |
| `v29rx_cfg` | `ptr_0014` | the constructor's 4th argument, handed to the module `aux`/`reserved34` slots; no agreed name | high |
| `v29_rx` | `int_0034`, `int_0038`, `short_003c`, `int_0040`, `int_0044`, `short_0048` | the `V29RX_create` zeroes; nothing else touches any of them | high |
| `v29_rx_block` | `int_000c`, `int_0014`, `int_0024` | seeded 0/1/0 by `V29RX_create`; no reader | high |
| `v29_rx_block` | `short_4f62` | `QualityDetectV29`'s once-only verdict; nothing reconstructed reads it, so the polarity is recorded and not named | high |
| `v29_rx_decoder` | `short_001a` | zeroed by `V29RX_create`; touched by nothing else, not by any slicer | high |
| `v29_rx_detector` | `int_0008` | the demodulator gate: written by `V29RX_control`'s ctl1 bit 4, read by `RxHdxDataV29`; the bit's meaning beyond the gate is not established | high |
| `v29_status_prefix` | `short_0c` | written 0 by `V29TX_status`, untouched by `V29RX_status`; `v22_status` leaves `+0x0c` unmodelled | high |
| `v29_status_prefix` | `short_12` | `V29RX_status` stores the config's bit rate and `V29TX_status` writes 0; `v22_status::short_12` (the same block) carries no semantic name | high |
| `v29_tx_params` | `int_0008` | the no-FIFO gate; same disposition as `v27_tx_source::int_0008` | high |
| `v29tx_cfg` | `short_0004`, `short_0006` | never read | high |
| `v29tx_cfg` | `int_000c` | never read; it is not V.27's `scale_mul` -- V.29's shaper scale is the per-rate table alone | high |
| `v29tx_cfg` | `short_0012` | never read | high |
| `v29tx_cfg` | `int_0018` | `V29TX_create` passes it as `FPM_PPS_CFG::aux`; the aux itself is unnamed | high |
| `v29rx_control_req` | `int_0004` | copied into `v29rx_cfg::int_0008`, which nothing reads; `v27rx_ctl::int_0004`/`v17rx_ctl::int_0004` are the same | high |
| `v29tx_control_req` | `int_0004` | copied into `v29tx_cfg::int_0008`, which nothing reads; `v17tx_control_req::int_0004` is the same | high |

### Stale comments corrected (no code change)

- `V27SH_INT_0004`'s block said the field had "no writer at all". False:
  `V27RX_control` writes it -- cleared to 0 on every request and set to 1
  by the request's FORCE_NOCARRIER flag. The same claim in
  `RxHdxDataV27`'s doc comment is corrected in place.
- `v27tx_cfg::int_0018`'s comment said `faxcfg.h`'s
  `v27rx_cfg::int_0014` was unnamed; it is now `short_train` (Batch 27).

### Scoping, macros and manifests

Every occurrence of the four old identifiers was classified by owning
object before editing. The rename touches `include/dsplib/faxcfg.h`,
`include/dsplib/v29data.h`, `include/dsplib/v29fax.h`, `src/fax/v27.c`,
`src/fax/v29.c`, `src/fax/class1tx.c` (one initializer comment), and the
tests `t_faxcfg.c`, `t_faxadapt.c` (its `v29tx_control` fixture), `t_v27fax.c`
(its `crt_setup` mirror field and the config it builds) and `t_v29txcreate.c`.

NOT renamed, and verified unchanged: `v29.c`'s
`((struct v29_rx_block *)rx)->int_0014` (a different field),
`faxvmi_link::int_0014` and every adapter's `dp->int_0014`,
`v21cfg`/`voice`'s `int_0014`, the retained `v21`/`v27`/`v29` `int_0000`
and `int_0008`, `struct sgd`'s `short_0016`, and `v29tx_cfg::int_0008`.

Offset constants keep their spelling. `V29TXP_SHORT_0016` (+0x16) is the
only one naming a renamed member; its comment now records the new field
name and the rank-2 derivation. `V27RXH_ZERO_*`, `V29RX_SHORT_4F62` and
`V29DET_INT_0008` name retained fields and are unchanged apart from the
stale `V27SH_INT_0004` paragraph above.

No mutation manifest is changed. `test/mutations/suites.json` maps
`v27status` to `src/fax/v27.c`, `faxadaptcreate` to `src/fax/V27rx.c` and
`faxadaptcreate_v29tx` to `src/fax/V29tx.c`, but no `find`/`replace`
string in any of them contains a renamed member: `faxadaptcreate`'s four
`int_0014` leaves are the `faxvmi_link` field `dp->int_0014`, a different
owner. `v27status.json`'s one `int_0018` is `v27tx_cfg`'s own retained
field.

### Verification

A token-aware comparison of `HEAD` against the working tree, with comments
blanked and string/character-literal contents removed, shows every changed
non-doc file differs only by the four rename pairs, in balance, with zero
non-`{old,new}` identifier change:

- `faxcfg.h` `int_0014`-1 / `short_train`+1, `int_0000`-1 / `protocol`+1
- `v29fax.h` `short_0016`-1 / `countdown`+1, `int_0008`-1 / `scale_mul`+1
- `v27.c` `int_0014`-1 / `short_train`+1
- `v29.c` `int_0000`-1 / `protocol`+1, `short_0016`-18 / `countdown`+18,
  `int_0008`-2 / `scale_mul`+2
- `t_faxcfg.c` `int_0014`-4 / `short_train`+4, `int_0000`-5 / `protocol`+5
- `t_faxadapt.c` `int_0008`-1 / `scale_mul`+1
- `t_v27fax.c` `int_0014`-5 / `short_train`+5
- `t_v29txcreate.c` `int_0008`-1 / `scale_mul`+1

`v27fax.h`, `v29data.h`, `faxcfg.c` and `class1tx.c` are comment-only and
show zero code-token change. Every surviving occurrence of an old
identifier is a retained field (`v29_rx_block::int_0014`,
`v29_rx_block::int_0000`, `struct sgd::short_0016`, the V.21 chain, the
`faxvmi_link` adapters); none is a renamed member, and no other owner's
occurrence moved.

`docs/naming-inventory.md` was regenerated: named 2640 -> 2644,
offset-named 382 -> 378, on-record 300 -> 346, residual 82 -> 32;
placeholder 73, padding 151 and member declarations 3246 are unchanged.

### Gate

Batch 27 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`. All 655 `build/period/*.o` hashes are
**byte-identical** to the pre-edit snapshot
(`/home/philpem/slmodem/tmp/fax-v2729-before.sha256` vs
`/home/philpem/slmodem/tmp/fax-v2729-after.sha256`, 655/655). Structural
checks in the same log: 2202 offset annotations matching
`__builtin_offsetof`, 13973 references resolve, 10041 mutations over 272
suites with 0 anchors matching other than exactly once. Log:
`build/structure-fax-v2729/gates.log`.

The first gate run failed to compile `t_faxadapt` on `arg.int_0008` -- a
`v29tx_control_req` consumer the scoping sweep's `req->int_0008` pattern
missed because the variable is named `arg`. It was renamed to
`arg.scale_mul`, the verification's file list gained `t_faxadapt.c`, and
the rerun above is the deciding gate.

One follow-on correction, and it is a coverage trap worth recording: moving
a `+0xNN` annotation from a trailing comment to a comment above a
declaration drops it from `offcheck.py`, which requires the annotation on
the declaration line (`field; /* +0xNN`). The first green run therefore
read 2186 annotations against HEAD's 2202. The sixteen affected
declarations -- `v27rx_cfg`'s seven and `v29rx_cfg`'s six, plus
`v27tx_cfg::int_0004`, `v29tx_cfg::int_0018` and
`v29tx_control_req::scale_mul` -- were restored to single-line trailing
annotations, and the final run reports 2202 again.

## Batch 28: FAX V.17 leftovers and faxvmi_cfg (issue #130)

Owner-scoped pass over the last offset-named FAX members: `struct v17_status`
(`short_0a`, `short_0c`, `short_12`), `struct v17tx_priv` (`r08`, `r0c`,
`r12`, `r1c`), `struct v17tx_fp` (`r00`, `r8e`), `struct v17_smc` (`r04`,
`r0a`) and `struct faxvmi_cfg` (`ptr_0014`).  These are the twelve the census
reported as offset residuals after Batch 27; `struct v17tx`,
`struct v17tx_cfg` and `struct v17tx_control_req` carry no residual, so
nothing in those owners was in scope.

**None of the twelve is renamed, and that is the batch's result.**  Each is
retained neutral with the reason recorded beside it, because the object
establishes only the plumbing and never a single role.  A wrong name is worse
than an offset, and the siblings these fields were compared against carry no
name to carry across.

### Retained neutral, with the reason

| Owner | Member | Reason | Confidence |
| --- | --- | --- | --- |
| `v17_status` | `short_0a` | Written 0 by both fillers and read by nothing; `v22_status` leaves the same offset unnamed and `v32_status` does not model `+0x0a` as a role | high |
| `v17_status` | `short_0c` | Written 0 by `V17TX_status` alone and untouched by `V17RX_status`; `v22_status` leaves `+0x0c` unmodelled and `v32_status`'s `r0c` has a different source | high |
| `v17_status` | `short_12` | `V17TX_status` writes 0 and `V17RX_status` writes the receive bit rate; `v22_status` leaves it unnamed and `v32_status`'s `r12` is unrelated -- two modules disagree | high |
| `v17tx_priv` | `r08` | The `V17TXP_INT_0008` FIFO-bypass/underrun gate in `V17TX_modem` and `TxHdxDataV17`; the bit's meaning beyond the gate is unstated, the same disposition Batch 27 gave `v27_tx_source::int_0008`/`v29_tx_params::int_0008` | high |
| `v17tx_priv` | `r0c` | The value of `cfg.int_0018` (`V17TXP_INT_000C`) copied at construction and read only by the ALT/EQCOND arms, whose budget and BRIDGE bypass read as short-vs-long training but are not typed; the constant itself is deliberately neutral | high |
| `v17tx_priv` | `r12` | Written and read by nothing reconstructed | high |
| `v17tx_priv` | `r1c` | `V17TXP_SHORT_001C`: cleared by the ALT arm alone and read by nothing | high |
| `v17tx_fp` | `r00[8]` | Eight-byte head touched by nothing reconstructed.  `v27_tx_block`/`v32_symout` call an identical head `pad_*`, but this one's content is not established, so the sibling's name is declined | high |
| `v17tx_fp` | `r8e` | The block's trailing short, written and read by nothing reconstructed | high |
| `v17_smc` | `r04` | In no dataflow: absent from the coder's own field map and written/read by nothing | high |
| `v17_smc` | `r0a` | In no dataflow, like `r04` | high |
| `faxvmi_cfg` | `ptr_0014` | The constructor's fourth argument.  Every reconstructed caller passes NULL, and its destination `faxvmi::int_0014` is read by nothing | high |

The three `v17_status` fields are the offsets `v22_status`/`v32_status`
already disagree on or leave unnamed; `v17fax.h`'s banner records that the
neutral offset name is kept there, and this batch carries that disposition
onto the fields themselves.  `r08`/`r0c`/`r1c` are the storage behind
`V17TXP_INT_0008`/`V17TXP_INT_000C`/`V17TXP_SHORT_001C`; those constants are
themselves deliberately neutral, so the fields cannot take a stronger name
than the constants do.

`r00[8]` is the one place a sibling name was declined rather than carried:
`v27_tx_block`'s `pad_0000[8]` and `v32_symout`'s `pad00[8]` are the same
shape, but calling this one padding would be a claim about content the
object does not make.  It matches Batch 25's treatment of
`v17rx_state::r4f88[4]`.

### Scoping, macros and manifests

Every renamed identifier was classified by owning object before editing;
there is none, so no `src/`, test or mutation occurrence moved.  The
`v17_status` `short_0a`/`short_0c`/`short_12`, the `v17tx_priv` `r08`/`r0c`/
`r12`/`r1c`, the `v17tx_fp` `r00`/`r8e`, the `v17_smc` `r04`/`r0a` and
`faxvmi_cfg::ptr_0014` keep their spellings, so no mutation manifest
`find`/`replace` changes; none of these identifiers appears in
`test/mutations/` at all (checked with a tree-wide search).

The offset constants keep their spelling.  Their comments now record the
disposition of the field each names: `V17TXP_INT_0008` (`v17tx_priv::r08`),
`V17TXP_INT_000C` (`::r0c`) and `V17TXP_SHORT_001C` (`::r1c`).  There is no
constant for `v17_smc` `+0x04`/`+0x0a`, for `v17tx_fp` `+0x00`/`+0x8e`, or
for `v17_status` `+0x0a`/`+0x0c`/`+0x12`, so only the field comments carry
those.  `FAXVMI_CFG`'s own initializer comment in `src/fax/faxcfg.c` records
`+0x14` as `ptr_0014`.

### Verification

Every changed non-doc file differs from `HEAD` by comments alone: a token
comparison with comments and string/character-literal contents removed
reports zero code change in `v17fax.h`, `v17data.h`, `faxcfg.h` and
`faxcfg.c`.  No identifier can have moved, so the manifest check (every
non-`{old,new}` identifier count unchanged, pairs balanced) holds vacuously.
One trap was hit and fixed in the census pass: a comment line ending in a
semicolon (`... read by nothing;`) is parsed as a member declaration and
invented a spurious `nothing` field, taking the total from 3246 to 3247.  The
semicolon was removed before the inventory was regenerated; the census now
reads its baseline 3246 members again.

### Gate

Batch 28 gate result: Gentoo `make phase` exited 0, **`period differential:
375 passed, 0 failed`** and `phase boundary: period differential and
structural checks all OK`.  All 655 `build/period/*.o` hashes are
**byte-identical** to the pre-edit snapshot
(`/home/philpem/slmodem/tmp/fax-v17left-before.sha256` vs
`/home/philpem/slmodem/tmp/fax-v17left-after.sha256`, 655/655).  Structural
checks in the same log: 2202 offset annotations matching
`__builtin_offsetof`, 13973 references checked with 0 resolving to nothing,
10041 mutations over 272 suites with 0 anchors matching other than exactly
once.  Log: `build/structure-fax-v17left/gates.log`.

`docs/naming-inventory.md` was regenerated: named 2644 (unchanged),
offset-named 378 (unchanged), on-record 346 -> 358, residual 32 -> 20; **FAX
offset residual 12 -> 0**.  Placeholder 73, padding 151, member declarations
3246 and owners 242 are unchanged.

## Batch 29: F878 +0x434 retype (issue #127)

The last actionable F878 slot. F878 found nine `V90Parameters` fields whose
`setToDefault` store is a float bit pattern but whose header type was `int`
(they are the offsets `loadParams` never names). Six were retyped earlier;
`PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH_ALT_RBS` (+0x440) was
already `float`; this batch settles `PDSNR_CURRENT_V34_DROP_THRESH_PHASE4`
(+0x434). `unnamed_328` (+0x328) **stays `int` deliberately**: finding F7960
measured that the object cannot distinguish the two types for it -- it has no
sharing partner and all 128 cells of F7960's enumeration are pairwise
identical across the field's type. Its comment is unchanged.

`+0x434` is a float on two independent measurements: `evaluatePhase4` loads it
with `flds 0x434(%ecx)` and stores it straight to `+0xac` with `fsts`, and
`setToDefault` plants `250.0f`. `include/dsplib/V90Parameters.h` now declares
it `float`; `src/pump/v90/V90Parameters.cpp` stores `250.0f`; and the
`ce_param_float(int bits)` union in `src/pump/v90/V90ConnectionEvaluator.cpp`,
which existed only to read the word through the frozen `int` declaration, is
deleted. `V90ConnectionEvaluator.cpp` now reads the field directly and
`V90ConnectionEvaluator.h`'s `+0xac` comment records that the slot is owned as
a `float` rather than read through a union.

The mutation manifest `test/mutations/v90conneval.json` loses the mutation
`"ce_param_float converts rather than reinterprets"`, whose `find`
(`\tu.i = bits;\n\treturn u.f;`) no longer exists. **One other anchor was
re-pointed, necessarily**: `"+0xac is replaced by PHASE4_ERROR_FOR_V34_FALLBACK"`
located the retrain by the removed `ce_param_float(...)` call, so its `find`
was moved to the direct read. Its label and `replace` are unchanged. The two
`why` strings that mention `unnamed_434` are historical prose and were left as
they are. `anchorcheck.py` now checks **10040 mutations over 272 suites, 0
anchors matching other than exactly once** (was 10041; one deleted).

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`.  Log:
`build/structure-issue127/gates.log`.

Of the 655 `build/period/*.o` objects, **654 are byte-identical** to the
pre-edit snapshot (`/home/philpem/slmodem/tmp/issue127-before.sha256` vs
`issue127-after.sha256`). The one that moves is
`src_pump_v90_V90Parameters.o`, and the move is confined to register
allocation in `V90Parameters::V90Parameters(tagModemParameters*)` -- the
constructor's two stores at `+0x4f8`/`+0x4fc` swap `%edx`/`%ecx` with no
change of immediate, offset or instruction. `setToDefault` itself, including
the `+0x434` store (`mov $0x437a0000,%esi; ...; mov %esi,0x434(%ebp)`), is
**byte-identical** before and after: the before object was rebuilt from the
HEAD source with the period compiler and its hash reproduced the recorded
pre-edit hash exactly, so the comparison is against the real baseline.

The reader change moved nothing: `src_pump_v90_V90ConnectionEvaluator.o` is
byte-identical before and after, because the union helper was already inlined
to the same `flds 0x434(%ecx); fsts 0xac(%ebx)` pair, which is exactly the
blob's own encoding (`d9 81 34 04 00 00`, `d9 93 ac 00 00 00`).

### Mutation snapshot

`python3 tools/mutsnap.py --update v90conneval` was attempted and **could not
run the suite**: the modern build on this host is GCC 14.2.0, and `src/fax/v17.c`
already fails it at `-Wincompatible-pointer-types` (`rxs = RXS(modem)`, unrelated
to this batch), so `mutate.py` refuses to judge mutations against a test that
does not build. This is the pre-existing GCC 14 portability break, not a result
of the retype. `test/mutations/snapshot.json` was therefore left **unchanged**
(the update run rewrote it byte-identically); it was already globally stale
(74 commits since its last refresh at `cf7af92e`) and `mutsnap --check` already
exited 1 on two pre-existing MISSING suites (`faxadaptcreate_v29tx`,
`fdspkrnl_tone`). The deleted mutation's recorded verdict remains only in that
stale, non-baseline entry; no verdict was hand-edited.

## Batch: issue #140 struct-cast retyping (v27/v29 tranche)

Lever 16 of `docs/method/refinement.md`: a holder declared with a generic
pointer type whose every `(struct T *)` cast targets a single struct can be
retyped to `struct T *`, and the now-redundant casts removed. Pointer type is
codegen-neutral -- the symbols are `extern "C"`, no mangling or DWARF records
the declaration, and the retype removes no load, store or operand -- so the
change is proven by the period object rather than argued. This tranche covers
`src/fax/v29.c` and `src/fax/v27.c` only. Nothing else in either file changed:
no cast carried a computed value, no holder was used as a byte pointer, and no
`sizeof`/`memcpy`/pointer arithmetic touched a retyped holder.

**No header changed, and none needed to.** Every sub-object pointer in
`include/dsplib/v29fax.h` and `include/dsplib/v27fax.h` is already spelled
`struct ... *` (there is no `void *`/`char *` field in either file), so the
struct-field half of the issue was already done by the earlier naming batches;
there were no consumer casts of those fields to drop. `docs/naming-inventory.md`
was therefore **not regenerated** -- it counts names, not types, and
`tools/namingcensus.py` reproduces it byte for byte.

### Holders retyped

| holder | file | functions (declaration sites) | old type -> new type | casts removed | single-target evidence |
|---|---|---|---|---|---|
| `det` | `src/fax/v29.c` | `V29RX_create`, `DataCarrierDetectV29` (2) | `void *` -> `struct v29_rx_detector *` | 27 | every `(struct v29_rx_detector *)det`; no other struct cast on `det` anywhere in the file |
| `rx` | `src/fax/v29.c` | `V29RX_create`, `DemodDataV29`, `CarrierDetectV29`, `DataCarrierDetectV29`, `QualityDetectV29` (5) | `void *` -> `struct v29_rx_block *` | 63 | every `(struct v29_rx_block *)rx`; file-wide cast census has no second target |
| `dec` | `src/fax/v29.c` | `V29RX_create`, `V29RX_epoch_det`, `V29RX_eq_train`, `V29RX_decision` (4) | `void *` -> `struct v29_rx_decoder *` | 67 | every `(struct v29_rx_decoder *)dec` |
| `prm` | `src/fax/v29.c` | `V29TX_create`, `V29TX_modem`, `TxNextStateV29`, `TxHdx{Start,Idle,Quiet,AB,EQCond,SCR1,Data}V29`, `V29TX_control` (10) | `void *` -> `struct v29_tx_params *` | 66 | every `(struct v29_tx_params *)prm` |
| `sh` | `src/fax/v27.c` | `V27RX_create`, `V27RX_delete`, `RxNextStateV27`, `RxHdxPrtcolV27`, `RxHdxEpochDetV27`, `DemodDataV27`, `DataCarrierDetectV27` (7) | `void *` -> `struct v27_rx_shared *` | 96 | every `(struct v27_rx_shared *)sh` |
| `rx` | `src/fax/v27.c` | `V27RX_create`, `V27RX_delete`, `RxNextStateV27`, `DemodDataV27`, `DataCarrierDetectV27`, `QualityDetectV27`, `EpochDetectV27`, `CarrierDetectV27` (8) | `void *` -> `struct v27_rx_block *` | 83 | every local `rx` is cast only to `struct v27_rx_block *`; the only `(struct v27_rx *)rx` in the file is the separate `V27RX_control(void *rx, ...)` **parameter**, a different holder and left untouched |
| `dec` | `src/fax/v27.c` | `V27RX_epoch_det`, `V27RX_eq_train`, `V27RX_decision`, `DataCarrierDetectV27` (4) | `void *` -> `struct v27_rx_decoder *` | 53 | every `(struct v27_rx_decoder *)dec` |
| `prm` | `src/fax/v27.c` | `V27TX_control`, `SetScramblerV27`, `V27TX_create`, `V27TX_modem`, `TxNextStateV27`, `TxHdx{Start,Quiet,Alt,EQCond,SCR1,Data}V27`, `TxNoCarrierV27`, `GenEQTrnSequenceV27` (13) | `void *` -> `struct v27_tx_source *` | 92 | every `(struct v27_tx_source *)prm` |
| `src` | `src/fax/v27.c` | `V27TX_delete` (1) | `void *` -> `struct v27_tx_source *` | 2 | every `(struct v27_tx_source *)src` |
| `tx` | `src/fax/v27.c` | `V27TX_delete`, `ModDataV27`, `V27TX_create`, `TxNoCarrierV27` (7) | `void *` -> `struct v27_tx_block *` | 15 | every `(struct v27_tx_block *)tx`; the `const void *tx` of `V27TX_status` is a separate parameter cast to `struct v27_tx *` and is left untouched |

Totals: **`src/fax/v29.c` 223 casts removed, `src/fax/v27.c` 341 casts removed**
(564 across the batch).

### Retained, and why

- **The generic handle idiom.** `modem` (`struct v29_rx *` and
  `struct v29_tx_root *`; `struct v27_rx *` and `struct v27_tx *`), `fp` in
  `V29TX_control` (`struct v29_tx_root *` / `struct v29tx_cfg *`), and the
  `req` parameters (`v27rx_ctl` / `v27tx_ctl`) are each one `void *` cast to
  different struct types at different sites. This is exactly the idiom `void *`
  exists for; retyping any of them would falsify the other arm.
- **`existing`** (`src/fax/v29.c` and `v27.c`, `V29TX_create` / `V27TX_create`)
  is cast to `struct fax_fifo *` on one path and `struct sgd *` on the other.
  Multi-target; retained.
- **`aux`** (`V29RX_create` / `V27RX_create`) is a byte payload, not a struct
  handle: `memcpy(&scfg.pad34, &aux, sizeof aux)` and
  `fcfg.reserved34 = aux`. It is never cast to a struct, so there is nothing to
  collapse; retyping would assert a struct type the shared `fpm_*_cfg` payload
  does not have.
- **Public/exported parameters.** `V29RX_status`/`V29TX_status`'s `status`, the
  `V29TX_status` `tx`, and `V27RX_control`'s `rx` and `V27TX_status`'s `tx` are
  part of the published `dp`-layer signature declared in `v29fax.h`/`v27fax.h`
  and reached through `src/fax/V29rx.c`, `V29tx.c`, `V27rx.c`, `V27tx.c`. The
  wrappers pass `void *`, so retyping would change the exported interface and
  every wrapper; per the issue they are left, and their casts remain.

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`. Log:
`build/structure-issue140/gates.log`.

All **655 `build/period/*.o` objects are byte-identical** to the pre-edit
snapshot (`/home/philpem/slmodem/tmp/issue140-before.sha256` vs
`issue140-after.sha256`, `diff` empty). The two that could have moved,
`src_fax_v29.o` and `src_fax_v27.o`, were recompiled at 10:26:46 from sources
edited at 10:24:56, so the comparison is against objects built from the changed
source and the identity is real, not a stale-object artefact.

The modern tier compiles both files clean with **GCC 14.2.0** at
`-m32 -O2 -mfpmath=387 -Wall -Wextra`, including
`-Werror=incompatible-pointer-types` (the check that stopped `make coverage` on
this class of holder in passing).

No mutation manifest edits were needed: no `find`/`replace` value in
`test/mutations/*.json` contains a cast to any of the ten retyped types, and
`anchorcheck.py` still reports **10040 mutations over 272 suites, 0 anchors
matching other than exactly once**. `test/mutations/snapshot.json` was not
touched.

### Remaining for the next tranche

The same per-holder audit is unstarted in `src/fax/v17.c`, `src/fax/v21.c`,
`src/fax/V17rx.c`, `V17tx.c`, `V21rx.c`, `V21tx.c`, `V27rx.c`, `V27tx.c`,
`V29rx.c`, `V29tx.c`, `src/pump/v22/*.c`, `src/pump/v32/*.c` and
`src/pump/v34/*.c`. The v22/v32/v34 files were explicitly out of scope here.

## Batch: issue #140 struct-cast retyping (v32/v34 tranche)

Lever 16 of `docs/method/refinement.md`, the second tranche of #140
(`improve/issue140-pump`). This tranche audited `src/pump/v32/` and
`src/pump/v34/` per function scope. Four local holders in two files were
retyped and their casts removed. **Every other candidate on the #140 list is
a function parameter and is retained**, because the `void *` there is
published in a module header and is the deliberate interface type, or because
the holder is a byte pointer. The retained set is listed below with the
evidence, not merely asserted.

**No header changed, and none needed to.** Nothing retyped here is a struct
field or a parameter, so no published signature moved and no caller was
touched. `docs/naming-inventory.md` was not regenerated: it counts names, not
types, and `tools/namingcensus.py` reproduces it byte for byte.

### Holders retyped

| holder | file | functions (declaration sites) | old type -> new type | casts removed | single-target evidence |
|---|---|---|---|---|---|
| `fp` | `src/pump/v32/V32.c` | `V32FP_recreate` (1) | `unsigned char *` -> `struct v32_fp *` | 38 | every use in the function is `(struct v32_fp *)fp` (32) or the initialiser `(unsigned char *)FP(modem)` (6); no `(struct ...)fp` with a second target, no `sizeof`/`memcpy`/pointer arithmetic on `fp` |
| `hdx` | `src/pump/v32/V32.c` | `V32FP_recreate` (1) | `unsigned char *` -> `struct v32_hdx *` | 53 | every use is `(struct v32_hdx *)hdx` (43), the store `...->hdx = (struct v32_hdx *)hdx` (1) or the initialiser `(unsigned char *)HDX(modem)` (9); no second target and no byte use |
| `fp` | `src/pump/v32/v32fpctl.c` | `SetTxModeV32`, `SetRxModeV32`, `SetAdaptEqV32`, `SetAdaptEcV32`, `SetRxLoopsV32`, `SetECRndTripDelayV32` (6) | `unsigned char *` -> `struct v32_fp *` | 60 | all 39 `(struct v32_fp *)fp` casts plus 21 `fp = (unsigned char *)FP(modem)` initialisers target the one struct; each `fp` is a separate function-local, never used as a byte pointer |
| `hdx` | `src/pump/v32/v32fpctl.c` | `RxClampV32`, `SetToneDetect`, `CalcTurnAroundDelay` (3) | `void *` -> `struct v32_hdx *` | 8 | all 8 `(struct v32_hdx *)hdx` casts target the one struct; the declaration was `void *hdx = HDX(modem)` and `HDX()` is already `struct v32_hdx *` |

Totals: **159 casts removed** (38 + 53 + 60 + 8). The counts include the
initialiser casts `(unsigned char *)FP(modem)` / `(unsigned char *)HDX(modem)`
and the one store cast at `V32FP_recreate`, which become redundant with the
declaration and are removed with the `((struct T *)holder)` forms the issue
names.

### Retained, and why

The #140 candidate list is file-scoped and names params as well as locals.
Audited per function scope, none of the parameter candidates is this task's
target, for one of two reasons.

- **The `void *` is the published, deliberate interface type.**
  `include/dsplib/v32fpctl.h` states it for `modem` in so many words -- "The
  parameter is `void *` and the offsets are named constants" -- and
  `include/dsplib/v34rx.h` states it for `obj` -- "Declared `void *` because
  this header must not depend on v34fsk.h, which is where `struct v34_object`
  is declared." Retyping these means changing the declaration and every
  caller for no code that differs; per the issue they are left. They are
  single-target per scope, so they remain candidates for #145's consolidation
  question, not defects.
  - `v32` `modem` -> `struct v32_modem *`: `v32fpctl.c` (8, the control
    entry points `SetTxModeV32`/`SetRxModeV32`/`SetAdaptEqV32`/`SetAdaptEcV32`/
    `SetRxLoopsV32`/`SetECRndTripDelayV32`/`GetRateV32`/`CalcTurnAroundDelay`);
    `v32nsrng.c` (26), `v32nsans.c` (11), `v32nsorg.c` (8), `v32seq.c` (8,
    including the static `v32_common_rate` whose callers pass the public
    `void *` through), `v32nsloop.c` (4), `v32data.c` (3: `ModDataV32`,
    `TxNoCarrierV32`), `v32hdx.c` (3: `V32TxHdxModem`, `V32RxHdxModem`),
    `v32demod.c` (1: `DemodDataV32`). All are `void *modem` state/data
    functions declared in `v32hdx.h`, `v32seq.h`, `v32data.h`, `v32demod.h`
    and reached from the dp layer and each other.
  - `v32` `ctx` -> `struct v32_ans_tone *`: `v32anstone.c:GenerateAnsTone`
    (1), `void *ctx` in `v32anstone.h`.
  - `v34` `objp`/`obj`/`vobj` -> `struct v34_object *`: `V34RX.c` (14:
    `txinit`, `rxtiminginit`, `rxinit`, `txmit`, `rxtiming`, `v34FreezeEcho`,
    `V34SetupDemodulator`, `adaptecho`, `modem_serrint`, `decoderv34`,
    `setInitialPhase`, `setTimingStateParameters`, `TimingV34`, `receiver`);
    `V34hshak.c` (15 `objp` plus 2 `void *obj` params); `v34pcmif.c` (33, the
    whole `VPcmV34*`/`V34XF_*` PCM interface); `v34hstx1.cpp` (17, the
    `v34tx1_*` transmit arms); `v34info.c` (7, `V34SetINFO*`/`V34Give*`/
    `VPcmV34SetMohMessageBits`); `v34scram.c` (4,
    `scrambleGPC`/`scrambleGPA`/`descrambleGPC`/`descrambleGPA`); `v34diag.cpp`
    (2, `VPcmV34GetDiagnostics`/`VPcmV34GetVisualDiagnostics`); `v34filters.c`
    (2, `V34InitializeImplementationSpecific`/`V34EchoHistoryBackwardClean`);
    `v34digital.c` (1, `preinitdigital`); `v34info1a.cpp` (1,
    `V34SetINFO1aBits`); plus `v34handshak(void *vobj)` (1) and the two
    `void *obj` parameters in `V34hshak.c`. Every one is declared in a
    `v34*.h` header and called from another translation unit.
- **A byte pointer, or a `void *` used as one.** `v34shell.c:fields` is the
  shell object materialised at `fields - 0xa00` (`shell_of()` casts it once);
  the four `fields` parameters are an offset base, not a struct handle.
  `v34hstx1.cpp`/`V34hshak.c`'s static `tx1_get`/`tx1_put`/`tx1_get_int`/
  `tx1_put_int` do `*(const short *)((const char *)objp + off)` -- those
  `objp`s are byte cursors and their `(char *)` casts are load-bearing.
- **Multi-target in one scope.** `v34hstx1.cpp`'s `tx1_*` family casts `objp`
  to `char *` for the byte accessors and `struct v34_object *` in the
  dispatch arms; those are separate functions, but the helper's parameter is
  never a single struct.

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`. Log:
`build/structure-issue140-pump/gates.log`.

All **655 `build/period/*.o` objects are byte-identical** to the pre-edit
snapshot (`/home/philpem/slmodem/tmp/issue140-pump-before.sha256` vs
`issue140-pump-after.sha256`, `diff` empty). The gate recompiled all 280
period objects from the changed sources, so the identity is real and not a
stale-object artefact.

The modern tier compiles both files clean with **GCC 14.2.0** at
`-m32 -O2 -mfpmath=387 -Wall -Wextra -Werror=incompatible-pointer-types`,
with the same three pre-existing `-Warray-bounds` warnings on
`V32FP_recreate`'s `hdx->regs` loop that HEAD produces.

No string literal, width, signedness or layout changed; only identifiers and
types did.

`anchorcheck.py` reports **10040 mutations over 272 suites, 0 anchors matching
other than exactly once**. Four mutation anchors in `test/mutations/v32fpctl.json`
(27 values) and `test/mutations/v32fpsub.json` (8 values) referenced the
retired cast text; only their `find`/`replace` values were transformed, and
`test/mutations/snapshot.json` was not touched.

### Remaining for the next tranche

The parameter candidates above are not work stayed; they are the #145
consolidation question, and `src/fax/v17.c`, `src/fax/v21.c`, `src/fax/V17rx.c`,
`V17tx.c`, `V21rx.c`, `V21tx.c`, `V27rx.c`, `V27tx.c`, `V29rx.c`, `V29tx.c`
and `src/pump/v22/*.c` are still unaudited for locals and fields.

## Batch: issue #140 struct-cast retyping (final tranche and disposition)

Lever 16 of `docs/method/refinement.md`, the third and final tranche of #140
(`improve/issue140-tranche3`). Every remaining candidate on the #140 list was
audited per function scope. **One local holder was retyped.** Every other
candidate is a deliberate leave, and the reason is given per holder below.
No header changed and no published signature moved.

### Holder retyped

| holder | file | function | old type -> new type | casts removed | single-target evidence |
|---|---|---|---|---|---|
| `existing_fifo` | `src/fax/v21.c` | `V21TX_create` (1) | `void *` -> `struct fax_fifo *` | 1 | the local is assigned once from `hdx->fifo` (already `struct fax_fifo *`, `v21fax.h:155`) and used once as `FIFO_create((struct fax_fifo *)existing_fifo, &fc)`; `FIFO_create`'s first parameter is `struct fax_fifo *` (`faxfifo.h:192`); no other cast and no byte use |

`Smc.c` was checked first and is **not** this task's target. `SMCv17_encoder_dif`/
`_abs`/`_tcm`/`SMCv17_init`'s `void *smc` is a **published** signature: declared
in `include/dsplib/v17data.h` and called from `src/fax/v17.c`, a different
translation unit. The three encoders are also stored in
`struct v17tx_block::encoders[]`, whose element type is `v17_encoder_fn` (a
`void *` first parameter, `v17data.h:209`); retyping would make
`encoders[0] = SMCv17_encoder_dif` an incompatible function-pointer assignment
and require a **new** cast at `v17.c:1011-1012` (GCC 14
`-Wincompatible-pointer-types`). It is a published-signature consolidation,
i.e. #145's work, not a cast removal.

### Retained: final per-holder disposition

**A. Published `void *` interface/entry-point parameters -- left, tracked in
#145.** Each signature is declared in a module header and reached from another
translation unit; retyping changes the exported interface and every wrapper.

- `src/fax/Smc.c` `smc` -> `struct v17_smc *` (4 cast sites; `v17data.h`
  declarations; `v17.c` caller; `encoders[]` function-pointer contract).
- `src/fax/v29.c` `status` -> `struct v29_status_prefix *` (33; `V29RX_status`,
  `V29TX_status`) and `tx` -> `struct v29_tx_root *` (4; `V29TX_status`);
  `v29fax.h`, reached through `V29rx.c`/`V29tx.c`.
- `src/fax/v27.c` `rx` -> `struct v27_rx *` (4; `V27RX_status`,
  `V27RX_control`) and `status` -> `struct v27_status_prefix *` (1;
  `V27RX_status`, `V27TX_status`); `v27fax.h`, reached through
  `V27rx.c`/`V27tx.c`.
- `src/fax/v17.c` `modem` -> `struct v17rx_cfg *` (1; `V17RX_control`);
  `v17fax.h:1786`, reached through `V17rx.c`.
- `src/pump/v17/v17data.c` `modem` -> `struct v17tx *` (2; `ModDataV17`,
  `TxNoCarrierV17`); `v17data.h`.
- `src/pump/v29/v29data.c` `modem` -> `struct v29_tx_root *` (2;
  `TxNoCarrierV29`, `GenEQTrnSequenceV29`); `v29data.h`.
- `src/pump/v22/v22data.c` `modem` -> `struct v22fp *` (4; `Detect_v22`,
  `ScrambleDataV22`, `DescrambleDataV22`, `ModDataV22`) and
  `src/pump/v22/v22prc.c` `modem` -> `struct v22fp *` (6; `ReadGTimer`,
  `SetAdaptEqV22`, `TxClockSync`, `CarrierDetect`, `SignalDetect`,
  `GetSignalQuality`); `v22fp.h`.
- `src/pump/v32/*` `modem` -> `struct v32_modem *` (`v32fpctl.c` 8,
  `v32data.c` 3, `v32demod.c` 1, `v32hdx.c` 3, `v32nsans.c` 11, `v32nsloop.c`
  4, `v32nsorg.c` 8, `v32nsrng.c` 26, `v32seq.c` 8) and `v32anstone.c` `ctx`
  -> `struct v32_ans_tone *` (1; `GenerateAnsTone`); `v32*.h`.
- `src/pump/v34/*` `objp`/`obj`/`vobj` -> `struct v34_object *` (`V34RX.c` 14,
  `V34hshak.c` 15 + 2 `void *obj`, `v34digital.c` 1, `v34filters.c` 2,
  `v34info.c` 7, `v34pcmif.c` 33, `v34scram.c` 4, `v34diag.cpp` 2,
  `v34hstx1.cpp` 17); `v34*.h`.
- `src/pump/v34/v34shell.c` `shellp` -> `struct v34_shell *` (4;
  `shellDemapper`, `putFrame`, `decodeDepth`, `demapFrame`); `v34shell.h`.
- `src/pump/v34/v34shell.c` `fields` -> `struct v34_shell_fields *` (1 cast
  site, `shell_of()`; used by `setScramble`, `preinitV34`, `initG248`,
  `initV34`); declared `void *fields` in `v34shell.h`. It is the shell fields
  sub-object base -- `preinitV34((char *)tx + V34_SHELL_FIELDS)`,
  `v34digital.c:54` -- a byte-offset base, not a whole struct.
- `src/service/rd.c` `obj` -> `struct rd *` (3; `RD_delete`, `RD_process`,
  `RD_ring_details`); `ringdet.h`.
- `src/service/voice.c` `obj` -> `struct vce *` (3; `VOICE_delete`,
  `VOICE_command`, `VOICE_process`); `vce.h`.
- `src/pump/v90/V92bitsToSymbol.cpp` `p` -> `struct V92ParamsInfo *` (1;
  `V92BitsToSymbol::reset`); the header spells the parameter
  `V92MappingParams *`, and it is passed to `transmitter->reset(p)`.

**B. Callback-contract generic handle -- left.** `dp_arg` -> `struct dp *` in
`src/pump/b103/b103.c`, `src/pump/v22/v22.c`, `src/pump/v23/v23.c` and
`src/pump/v32/v32.c` (1 each; the static `b103_process`/`v22_process`/
`v23_process`/`v32_process`). The functions are `static`, but they are the
implementation of the published `dp_process_fn` typedef (`dp.h:23`, first
parameter `void *`) and are handed to `dp_wrapper_create` directly. Retyping
makes each registration an incompatible function-pointer argument and requires
a **new** cast, so it is a wash and a signature change, not a cast removal.

**C. Generic field / byte pointer -- left.** `src/pump/b103/b103.c` (and the
sibling datapumps) casts `dp->dp_data` to `struct dp_wrapper *` (2). The holder
is the field `struct dp::dp_data`, deliberately `void *` (`dp.h:47`) as the
opaque datapump state consumed through `dp_wrapper_run`; it is not a
single-struct handle.

### Cumulative retyped (tranches 1-3)

564 casts (`src/fax/v29.c`, `src/fax/v27.c`; PR #144) + 159 casts
(`src/pump/v32/V32.c`, `src/pump/v32/v32fpctl.c`; PR #146) + 1 cast
(`src/fax/v21.c`; this tranche) = **724 redundant casts removed across 15
holders**. Nothing in section A/B/C is a retype deferred for difficulty: every
one is either a published signature (#145's consolidation), a callback
contract, or a deliberate generic/byte pointer.

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`. Log:
`build/structure-issue140-t3/gates.log`.

All **655 `build/period/*.o` objects are byte-identical** to the pre-edit
snapshot (`/home/philpem/slmodem/tmp/issue140-t3-before.sha256` vs
`issue140-t3-after.sha256`, `diff` empty). `src_fax_v21.o` was recompiled at
12:32 from the source edited at 12:31, so the identity is real and not a
stale-object artefact.

The modern tier compiles the file clean with **GCC 14.2.0** at
`-m32 -O2 -mfpmath=387 -Wall -Wextra -Wno-unused-parameter
-Werror=incompatible-pointer-types`, exit 0, no warnings.

No mutation manifest edit was needed: no `find`/`replace` value in
`test/mutations/*.json` contains `existing_fifo` or `fax_fifo`, and the gate's
`anchorcheck.py` still reports **272 suites, 10040 mutations, 0 anchors
matching other than exactly once**. `test/mutations/snapshot.json` was not
touched.

**#140 disposition: complete.** Every remaining item on the #140 list is a
deliberate leave -- a published `void *` interface parameter, a
callback-contract handle, or a generic/byte pointer -- so there is no safe
candidate left for a further #140 tranche. Re-typing the published signatures
is the #145 consolidation question. #140 can close.

**#141 disposition: complete.** The cast-in-macro accessors are gone -- 44
`RXS_MRF`/`RXS_AGC`/`RXS_SRE`/`RXS_FSE`/`RXS_DEC` uses and 13 `CTL_PROCESS`
uses in `src/fax/v17.c`, 8 `SET_HANDLER` uses in `src/fax/v29.c`, and the six
unused `SMC_*` macros -- and the root/state chain (`RXROOT`, `TXROOT`, `RXCTL`,
`RXSTATE`, `TXPRIV`, `TXBLOCK`, `CTL`, `RXS`, `TXP`, `TXFP`) is a deliberate
leave because its cast is on the published `void *modem` and is #145's
question. One spelling was non-neutral and reverted: the direct
`RXS(modem)->agc.value.cfg.alpha++`/`.beta++` at `RxNextStateV17` grew `.text`
by 16 bytes (an extra `mov 0x60(%ebx)` state reload), so the address-deref form
`(&RXS(modem)->agc.value)->cfg.alpha++` is used there; every other site's direct
member form is byte-identical.

## Batch: issue #145 per-function re-scan and shared-header disposition

Lever 16 of `docs/method/refinement.md`. #140's remaining generic holders were
classified FILE-scoped, so a name reused across RX and TX entry points read as
multi-target when each function casts it to exactly one struct. This pass
re-scanned every holder in #145 **per function** and split the result into
retyped, deliberate leave, and unproven consolidation. No struct was
restructured and no published signature moved.

### Holders retyped (single-target local, non-published holder)

| holder | file | function (scope) | old -> new | casts removed | evidence |
|---|---|---|---|---|---|
| `existing` | `src/fax/v27.c` | `V27TX_create`, SGD block (block-scoped local) | `void *` -> `struct sgd *` | 1 | assigned once from `prm->sgd` (`struct v27_tx_source::sgd`, `v27fax.h:299`), used once as `SGD_create((struct sgd *)existing, ...)`; `SGD_create`'s first parameter is `struct sgd *` (`sgd.h:192`) |
| `existing` | `src/fax/v27.c` | `V27TX_create`, FIFO block (separate block-scoped local) | `void *` -> `struct fax_fifo *` | 1 | assigned once from `prm->fifo` (`v27fax.h:298`), used once as `FIFO_create((struct fax_fifo *)existing, ...)`; `FIFO_create`'s first parameter is `struct fax_fifo *` (`faxfifo.h:192`) |
| `fp` | `src/fax/v17.c` | `V17TX_create` (function-local, not the published parameter) | already `struct v17tx_fp *`; cast removed only | 1 | `fp = TXFP(modem)` and `TXFP(m)` is `TXROOT(m)->fp`, `struct v17tx_fp *` (`v17fax.h:599`); the store `TXROOT(modem)->fp = (struct v17tx_fp *)fp` was redundant |

The two `v27.c` `existing` declarations are in DIFFERENT block scopes of one
function, each single-target -- the same shape #147 retyped in `V21TX_create`.
The file-scoped scan had merged them.

### Deliberate leaves, per holder

- **Published `void *` entry-point/state parameters -- left.** Every function
  below is declared in a module header with `void *` first (or second)
  parameter and reached cross-TU; retyping changes the exported interface and
  every caller for code that does not differ. Re-scanned per function, each is
  single-target **within** its own function; the file-scoped >1 report was the
  RX/TX name reuse.
  - `src/fax/v27.c`: `modem` -> `struct v27_rx *` in every RX function
    (`V27RX_create`, `V27RX_delete`, `V27RX_modem`, `RxHdx*V27`,
    `DemodDataV27`, `DataCarrierDetectV27`, `QualityDetectV27`, ...) and
    -> `struct v27_tx *` in every TX function (`V27TX_create`,
    `V27TX_delete`, `V27TX_modem`, `ModDataV27`, `TxHdx*V27`, ...); `req` ->
    `v27rx_ctl` in `V27RX_control` and `v27tx_ctl` in `V27TX_control`.
    Declared `void *modem`/`void *req` in `v27fax.h`.
  - `src/fax/v29.c`: `modem` -> `struct v29_rx *` (RX) / `struct v29_tx_root *`
    (TX); `fp` -> `struct v29_tx_root *` + `struct v29tx_cfg *` in
    `V29TX_control`. Declared in `v29fax.h`.
  - `src/fax/v17.c`: `modem` -> `struct v17rx_cfg *` in `V17RX_control`;
    `fp` -> `struct v17tx_cfg *` + `struct v17tx *` + `struct v17tx_priv *` +
    `struct v17tx_fp *` (via `TXROOT`/`TXPRIV`/`TXBLOCK`) in `V17TX_control`.
    Declared in `v17fax.h`.
  - `src/fax/v21.c`: `modem` -> `struct v21_rx *` (RX) / `struct v21_tx *`
    (TX). Declared in `v21fax.h`.
  - `src/pump/v32/v32.c`: `modem` -> `struct modem *` (the vendored core
    handle), a parameter of the file-local `v32_create`.
  - `src/pump/v34/v34shell.c`: `obj` -> `struct v34_object *` in
    `initdigital` (also used as a byte base, `(char *)obj + V34_RATECFG`);
    `objp` -> `struct v34_shell *` in `getFrame`. `initdigital`/`modulatevector`
    are declared `void *obj` in `v34shell.h:394,406`.
  - `src/pump/v34/VPcmV34Main.cpp`: `objp` -> `struct v34_object *` (spelled
    `tagV34Object` at four sites, the object's own C++ name for the same
    struct); published `void *` parameters.
- **Callback-contract handle -- left.** `dp_arg` -> `struct dp *` in
  `src/pump/v22/v22.c`, `src/pump/b103/b103.c`, `src/pump/v23/v23.c` and
  `src/pump/v32/v32.c` (one per static process function). The functions are the
  implementation of the published `dp_process_fn` typedef
  (`dp.h:23`, first parameter `void *`) handed to `dp_wrapper_create`
  (`dp_wrapper.h:70`); retyping makes the registration an incompatible
  function-pointer argument and needs a new cast.
- **Generic field / byte pointers -- left.** `struct dp::dp_data` is `void *`
  (`dp.h:47`), consumed opaquely; the sibling datapumps cast it to
  `struct dp_wrapper *`. `v34shell.c` `fields` is the shell fields sub-object
  base (`shell_of()` casts once; `obj + V34_SHELL_FIELDS`, `v34shell.h:78`).
  `VPcmV34Main.cpp`'s `m`/`sess`/`cfg`/`k56` are `(unsigned char *)` byte
  cursors. `v17.c` `rx` (`V17RX_status`) and `p` (`V17TX_status`) are
  deliberate byte views of an unmodelled block, with the reason in a comment
  already at each site. `aux` is a byte payload, never a struct cast.
- **Multi-target within one function -- left.** `src/fax/v29.c`
  `V29TX_create` and `src/fax/v17.c` `V17TX_create` each declare ONE
  function-scoped `existing` used for BOTH `struct fax_fifo *` and
  `struct sgd *`; `src/pump/v34/v34shell.c` `modulatevector` casts `obj` to
  `struct v34_object *` AND `struct v34_shell *`.

### Shared-header / consolidation evidence, per pair

Struct layouts are from the headers the object's own field accesses fixed;
the object disassembly is quoted where it is the witness.

| pair | shared leading member at +0x00? | offset / evidence |
|---|---|---|
| `v27_rx` / `v27_tx` | **NO** | `v27_rx` +0x00 is `int int_0000` (value 1, no reader, `faxcfg.h:195`); `v27_tx` +0x00 is `short protocol` (`v27fax.h:143`). Different leading types and different cfg structs (`v27rx_cfg` vs `v27tx_cfg`). |
| `v29_rx` / `v29_tx_root` | **NO** (semantic word only, no shared C type) | `v29_rx` +0x00 is `int protocol` (value 1, `faxcfg.h:212`); `v29_tx_root` +0x00 is `v29tx_cfg::protocol`, a `short` (value 0, `v29data.h:176`). Both named `protocol` but different width and different cfg structs; `V29RX_create`/`V29TX_create` bulk-copy the config 32 bits at a time (`mov (%ebx),%e..; mov %e..,0x0(%ebp)` at 0x9ad85 and 0x9ba45), which is width-blind and does not make them one type. |
| `v21_rx` / `v21_tx` | **NO** | `v21_rx` +0x00 is `short chan2` (value 1, `v21cfg.h:108`); `v21_tx` +0x00 is `short protocol` (value 1, `v21cfg.h:178`). Different names and roles. |
| `v32_modem` / `v32fp_params` | **YES** | `struct v32_modem` embeds `struct v32fp_params params;` as its FIRST member (`v32struct.h:110-111`), so both views share +0x00. Witness: `V32FP_recreate` copies 0x30 bytes to the base -- `mov %ebp,%edi; rep movsl` at 0x7e8ab (0xc dwords = `sizeof(struct v32fp_params)`) -- and then reads `struct v32_modem::fp` at +0x68 (`mov 0x68(%ebp),%esi` at 0x7e8cb). |
| `v34_object` / `v34_shell` | **NO** | `v34_object` +0x00 is `int status` (`v34fsk.h:196`); `v34_shell` +0x00 is `unsigned char pad_000[0xa00]` (`v34shell.h:103`) with its fields beginning at +0xa00. The shell is an offset view (`obj + V34_SHELL_FIELDS`, `v34shell.h:78`), not an embedded base. |

### Unproven consolidation, recorded not changed

The only concrete shared leading member is `v32_modem`/`v32fp_params` (above),
and the holder that sees both is the **published** `void *modem` of
`V32FP_recreate` (`v32fpctl.h` states the parameter is deliberately `void *`).
Collapsing it to a base-pointer model would change that published signature or
introduce a new base type across `v32struct.h`/`v32fp.h` -- a design change
larger than this bounded pass. It is recorded here and belongs in a follow-up
issue, not a struct rewrite now. The v29 `protocol` coincidence is a semantic
word, not a shared type, and the v34 shell is offset arithmetic; neither
supports a base struct.

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`. Log:
`build/structure-issue145/gates.log`.

All **655 `build/period/*.o` objects are byte-identical** to the pre-edit
snapshot (`/home/philpem/slmodem/tmp/issue145-before.sha256` vs
`issue145-after.sha256`, `diff` empty). `src_fax_v27.o` and `src_fax_v17.o`
were recompiled at 16:46 from sources edited at 16:41, so the identity is real
and not a stale-object artefact.

The modern tier compiles both files clean with **GCC 14.2.0** at
`-m32 -O2 -mfpmath=387 -Wall -Wextra -Wno-unused-parameter
-Werror=incompatible-pointer-types`, exit 0 (the one pre-existing
`unused variable 'fp'` warning in `SetEncoderV17` is at HEAD and unrelated).

**#145 disposition:** three redundant casts removed across three holders
(`v27.c` `existing` x2, `v17.c` `fp` x1); every other #145 holder is a
deliberate leave or an unproven consolidation as above. No struct was
restructured.

## Batch: issue #151, the v32_modem / v32fp_params shared base

Lever 16 of `docs/method/refinement.md`; experiment design per
`docs/method/experiment-design.md`. The one concrete shared leading member left
by #145 is `struct v32_modem`'s first member `struct v32fp_params params`
(`v32struct.h:110-111`). This pass characterised that model, enumerated four
holder/macro source forms, and applied the neutral one that removes the most
casts.

### The model, and that the reverse holds at every use site

`params` is at +0x00 of `struct v32_modem` (asserted at
`v32struct.h:155-156`, `V32_SO(struct v32_modem, fp) == 0x68` and
`sizeof(struct v32_modem) == 0x6c`). The object agrees: `V32FP_recreate`'s
`rep movsl` writes the 0xc-dword parameter block to the base (0x7e8ab) and then
reaches `fp` at +0x68 (0x7e8cb). So a `struct v32fp_params *` and a
`struct v32_modem *` name the same address, and both directions of cast are a
zero-adjustment pun. **The base is the first member at every use site, so
`(struct v32fp_params *)obj` and `(struct v32_modem *)params` need no negative
adjustment.** The only place the holder is not the object base is the
`param != 0` argument, which is a separate parameter block and is never cast to
`struct v32_modem *`.

### Enumeration (all cells measured, not scored)

Every cell compiled the complete transitive includer set of `v32fpstat.h`
(`V32.c`, `V32mod.c`, `V32stc.c`, `v32.c`, `t_v32dp.c`, `t_v32fpdisp.c`,
`t_v32fprecr.c`) with the exact period flags (Gentoo GCC 3.4.2-r2,
`-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp
-fomit-frame-pointer -maccumulate-outgoing-args -DDSPLIB_REPRODUCE_BUGS
-D__SIZEOF_POINTER__=4 -std=gnu99 -include tools/toolchain/period_compat.h`)
and compared each object byte-for-byte against `build/period`. Byte identity is
the full-text comparison: same instructions, same operands, same everything.

| model | source form | full-text verdict | casts removed | files touched |
|---|---|---|---|---|
| (a) | `void *modem` + `(struct v32_modem *)` casts (baseline) | — (control) | 0 | 0 |
| (b) | `struct v32fp_params *modem` (base pointer); derived casts kept | **neutral**, all 7 objects identical | 3 (`*(struct v32fp_params *)modem` x2, `p = (struct v32fp_params *)modem` x1); 19 derived casts + the two macros remain | `V32.c`, `v32fpstat.h` |
| (c) | `union v32_modem_view { struct v32fp_params params; struct v32_modem modem; }` as the holder | **NOT neutral** — `src_pump_v32_V32.o` differs: 8 bytes larger, 2,465 bytes differ, instruction scheduling and stack offsets move (union members are assumed to overlap, so the write through `.params` and the read through `.modem.fp` no longer optimise as independent) | 0 net (casts become member selectors) | `V32.c`, `v32fpstat.h`, `v32struct.h` |
| (d) | `struct v32_modem *modem` (derived pointer, signature change) | **neutral**, all 7 objects identical | 22 explicit casts (`((struct v32_modem *)modem)->` x19, `*(struct v32fp_params *)modem` x2, `p = ...` x1) **plus** the two local `HDX`/`FP` macro casts; no derived casts remain | `V32.c`, `v32fpstat.h` |

Only (b) and (d) map (compile and are codegen-neutral). (a) is the control;
(c) is measured non-neutral and rejected. Per step 3, the model that removes the
most redundant casts is applied: **(d)**. It removes 22 explicit casts and makes
both local macros cast-free, against (b)'s 3; (b) leaves 19 necessary derived
casts and types the whole object as its first 48 bytes, which is less truthful
than typing it as the object it is. (d) reaches the base through the shared
first member (`modem->params`), which is exactly the +0x00 relationship the
issue is about, so it is the consolidation, not a bypass of it.

### Applied: (d)

`V32FP_recreate`'s holder is now `struct v32_modem *modem`; the base view is
`&modem->params`, and the derived fields are reached directly. The two local
macros are `((m)->hdx)` / `((m)->fp)`. The published prototype in
`v32fpstat.h` carries the new type with a `struct v32_modem;` forward
declaration. **No caller source changed**: `void *` converts implicitly to
`struct v32_modem *`, so `V32FP_control` (`V32stc.c:122,129`) and
`V32FP_create` (`V32.c:805`) are untouched, and the test fixture's
`V32FP_recreate(a, (struct v32fp_params *)a, 0)` still type-checks. The ABI is
unchanged — pointer types are not observable in the emitted code.

Casts disappeared: 22 explicit plus the two macro-internal ones. Derived casts
remaining: **none** in this holder. `V32FP_recreate` now contains zero
`(struct v32_modem *)` casts.

### Boundary, not taken

The same retype was **not** applied to the other V.32 holders. The shared views
`HDX(m)`/`FP(m)`/`PARAMS(m)` in `src/pump/v32/v32fpdisp-common.h:90-97`, the
seven further own copies of `HDX`/`FP` (`v32fpctl.c`, `v32nsans.c`,
`v32nsloop.c`, `v32nsorg.c`, `v32nsrng.c`, `V32rxhdx.c`, `V32TXHDX.c`), and the
`V32mod.c`/`V32stc.c` consumers of the shared header are held by published
`void *modem` parameters across the remaining thirteen source files; retyping
those to `struct v32_modem *` would remove the remaining 144
`(struct v32_modem *)` casts but fans out well past the bounded pass this issue
set. That is a follow-up, not a silent scope increase. It is a holder retype of
the same measured shape, so its neutrality is expected but is **not** claimed
here.

### Verification

Gentoo `make phase` exited 0, **`period differential: 375 passed, 0 failed`**
and `phase boundary: period differential and structural checks all OK`. Log:
`build/structure-issue151/gates.log`.

All **655 `build/period/*.o` objects are byte-identical** to the pre-edit
snapshot (`/home/philpem/slmodem/tmp/issue151-before.sha256` vs
`issue151-after.sha256`, `diff` empty). `src_pump_v32_V32.o` was recompiled at
18:49:55 from `V32.c` edited at 18:44:42 and is identical, so the identity is
real and not a stale-object artefact.

Modern compile clean on every affected TU (`V32.c`, `V32mod.c`, `V32stc.c`,
`v32.c`, `t_v32dp.c`, `t_v32fpdisp.c`, `t_v32fprecr.c`) with **GCC 14.2.0** at
`-m32 -Werror=incompatible-pointer-types -fsyntax-only`, exit 0.

**#151 disposition:** closed. The holder is a single pointer to the object base
with no redundant casts; the base pointer spelling (b) was measured equally
neutral but removes fewer casts and is not the chosen form.

