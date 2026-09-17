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
