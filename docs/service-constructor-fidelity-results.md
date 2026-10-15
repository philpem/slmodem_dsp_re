# Service constructor and cadence fidelity

Baseline18f4ad68. [Declared controls](service-constructor-fidelity-domain.md):
nine opening full-TU cells and four diagnostic follow-ups,49 shared-body verdicts.
All raw complete-TU baselines reproduce, including the corrected-switch context
used by the follow-up. Retained Gentoo profile, selected assembler, bug define,
full commands, headers, source/object hashes and RTL are recorded in
build/service-constructor-fidelity and build/cadence-diagnostic-owner.

## Wrapper rejects an unsupported rate pair before reverse dispatch

The original leaves its first converter NULL for unsupported unequal rates,
checks that result unconditionally, and cleans up before attempting the reverse
conversion. The source skipped both checks when its table lookup returned-1,
therefore returning a wrapper with no converters.

Six fixed positive unequal-rate constructor inputs (7200/8000,8000/7200,
16000/9600,9600/16000,44100/48000,48000/44100) all demonstrate the mismatch:
6/12 checks fail before correction. No refused wrapper is run. Existing supported
rate/audio cases remain positive controls. Restore six literal immediate-mode
creation arms through one ordinary helper, sequential result checks, shared
cleanup and original delete-before-create emission order. These boundaries were
already measured separately; this is partial adoption with actual constructor
failure evidence, not a reopened source-order search.

| Wrapper cell | create SIZE gap |
| --- | ---: |
| Retained table/checks |282|
| Sequential failure with retained table |331|
| Literal creation/shared cleanup/delete first |2|

The final2B size gap still has a different prologue, frame/save form and return
carrier. Both opening predicates already emit the original paired SETcc/OR
operations under retained logical-OR source; an eager-OR control has no new
operand witness and is not compiled. No register/declaration/cleanup permutations
follow. Delete remains85B EXACT and the complete run bystander is unchanged.
The invented72B dpw_rate_pairs object and its section disappear; nothing else
changes in metadata, nontext, calls or bindings.

## Cadence banks6/7 accept valid subindices

Original dispatch selects Filter_100_550 for index6 and Filter_350_500 for7
when subindex is1..7. Their out-of-range fallbacks remain CP_450_630 and
CP_100_550 respectively. The source always chose those fallbacks.

Seventy-two fixed DIAL constructor cases cover all indices0..7 and subindices
0..8 through actual parameter getters and constructors. All selected coefficient
and scale contents, setup writes, object words and filter configuration are
compared; no addresses are compared across the two objects. The old source
fails199 checks in14 cases (indices6/7 with valid subindices). Returned filters
are real independently allocated objects. This proves the constructor API
boundary; it does not claim that every country configuration selects each pair.

Adopt the original literal switch/map. The table-only correction is a separate
data-only control, preserving the create body raw; it would fix the values but
retain the invented bank-table representation. The switch removes banks.0,
192B, and emits an eight-entry32B jump table. CadenceNames and all other named
objects/strings are unchanged. The audit retains every new internal destination,
checks instruction boundaries and duplicate-target groups, and records the
original eight native destinations. The conservative full guard/ABI prover
refuses both original and selected code; those refusals remain explicit.
No byte-identity or complete static table proof is claimed from runtime cases.

## Looped matching checks interior tone durations

The original compares both off and on durations in every interior cycle. The
source checked only off there; its separate first/last on comparison cannot
replace the missing middle comparison.

A BUSY detector is constructed with cycles3, looped matching and broad timing
windows. No periods, counters or filter history are planted. Feed550Hz audio
with an initial500ms silence, then three tone/silence pairs. The all-equal500ms
tone sequence is a positive control; the separating sequence uses a1000ms middle
tone and500ms first/last tones, with500ms silences. The source incorrectly
reports CADENCE_DETECTED at sample28159; the original rejects the sequence and
retains its three recorded periods. Pre-fix12/40454 checks fail; the equal case
passes35429 checks. Restore exactly the missing interior on-duration comparison,
retaining the continue-after-failure and asymmetric endpoint behavior.

The first corrected wrapper/cadence targeted run passes2/0, including all72
constructor cases and both constructor/audio sequences. Only cadence_create and
cadence_progress change in the TU; its delete/reset exact bodies remain unchanged.
The original pure countdown loop after the tolerance getter has no independently
identified source preimage and is not recreated as an invented delay.

## TYPE describes the working branch, not returned metadata

The existing fixture skipped all transcript comparisons for tones above RING.
Native default dispatch sets its working type to DIAL at7d501, which TYPE uses
at7d86e. After the report and usable gate, the original reloads descriptor.w6
and descriptor.tone, publishes int_2a4 and clamps/writes the returned name. Thus
TYPE DIAL and returned INVALID are both correct for descriptor tones4/9.

Remove the skip in the existing level1/2/3 constructor cases; assert original
TYPE DIAL at levels2/3 and keep the returned-name comparison. Before source
correction, four transcript checks fail (tones4/9 at levels2/3), while the new
original TYPE assertions pass. The reports stay the same length. No diagnostic
is suppressed or tolerated.

| Corrected-context diagnostic cell | create SIZE gap | Late descriptor reads |
| --- | ---: | --- |
| Existing c->name argument |249|neither|
| Normalised working tone argument |239|neither|
| Working tone, late name only |239|tone only|
| Working tone, original post-report bundle |255|w6 and tone|

Select the complete original bundle despite its larger size gap. The four
sibling-RTL controls independently check that the descriptor loads cross the
last report call at the declared boundary. Working tone is captured at entry
and normalised on the actual default branch. Neither returned metadata nor TYPE
is inferred from the other. All other bodies/data/calls are unchanged; only the
eight internal create jump-table destination offsets move.

## Replay and measurement limits

```
python3 tools/service_constructor_fidelity_reproduce.py --domain docs/service-constructor-fidelity-domain.md --baseline-dir build/service-fidelity-baseline
python3 tools/cadence_diagnostic_owner_reproduce.py --domain docs/service-constructor-fidelity-domain.md --baseline-dir build/service-corrected-baseline
python3 tools/service_constructor_fidelity_audit.py
```

The first baseline archive contains original18f4ad68 objects/config; the second
contains the corrected switch/interior Cadence object. Recreate them with pinned
builds on a fresh checkout. Historical header drift needs explicit
--historical-headers. The initial three diagnostic results remain separate from
the expanded four-cell follow-up. An initial partialcmp invocation ran before
its baseline preparation completed and failed readelf; it is invalid, preserved
and excluded. The subsequent valid measurement uses the archived completed ELF.

No source/profile/register fitting, fuzzing or mutation execution, tolerance
change, or modern-portability/full-phase claim. These service and diagnostic
control families are closed absent another independent original witness. Neither
nonexact size closeness nor the unchanged exact set proves a global ceiling.

## Remaining semantic holds and medium read boundaries

The FIFO capacity and frame-reversal stride corrections are already in current
source and byte-exact; historical domain wording does not make them open defects.
No unsafe wrapping-capacity input is executed. V23 negative-count/no-sample
controls still include an uninitialised original capture and supply no new valid
lifecycle evidence for reopening that closed domain.

Six additional current full-TU cells review two still-present medium-fax
boundaries, making19 complete-TU cells/77 shared verdicts overall:

- _tx_scrambled_ones_state captures *tx_count immediately before FAXVMI_process,
  after intervening input/callback/scratch work. It also compares zero-extended
  FIFO count against signed32-bit tx_bytes_per_block. Native SETGE matches the
  original; baseline SETAE did not. Gap20→3. The observed byte-count writer is
  tx_rate/400; no negative-rate state or public negative-count reachability is
  invented. This is operand/read-age recovery, not a new executed semantic claim.
- faxvmi_hdlc_frame reloads its parent framer across opaque debug/name calls.
  Gap37→99; the larger gap is explicit. No callback-rebinding history is planted.
  Scalar packing/state operations, all call targets and interfaces remain fixed.

The first medium audit correctly rejected an unexpected changed bystander,
faxvmi_hdlc_unframe. Preserve that failed assertion as invalid until classified.
Its source is unchanged; both393-instruction streams agree from the first jump
onward. Only three prefix rows differ: two private spill slots at1c/24 receive
zero from EAX/ECX at exchanged times, with the same EDX-to20 store interposed.
Both registers are independently zeroed before the stores; moves do not change
flags, and the same stack values exist at the first jump. This is5 raw differing
bytes, not exact identity. The constrained audit checks the actual rows rather
than excusing arbitrary compiler collateral; original SIZE58 stays58.

Eight retained compiler stages and six observer positive/refusal controls show
that the unchanged unframer agrees through flow2 under a checked alias-set
bijection. The first instruction differences occur at28.peephole2 (10 UIDs),
then rnreg/sched2 retain four differing UIDs and the final three native prefix
rows. This is an observed retained peephole boundary, not original compiler
state or permission to reorder the unframer's declarations/zero stores.

Metadata, ordinary data, nontext relocations and calls remain unchanged in all
six cells; any changed merge-string pool preserves contents and multiplicities.
No exact function gain/loss. Replay:

```
python3 tools/remaining_medium_boundary_reproduce.py --domain docs/service-constructor-fidelity-domain.md --baseline-dir build/service-fidelity-baseline
python3 tools/remaining_medium_boundary_audit.py
python3 tools/remaining_medium_bystander_trace.py
```

The baseline archive also contains the original cDATAtx/faxvmi_hdlc objects.
No neighbouring source/type/register or profile controls follow. The identified
semantic-hold queue is now classified: corrected service behavior, already-landed
exact FIFO/frame recovery, retained medium read boundaries, and the closed V23
unsupported-capture domain. Further byte work needs a different original witness.


## Observer correction and final validation

The first stage report was invalid: fourteen apparent lreg changes were internal
`string_cst` pointer annotations for unchanged symbols. Preserve
`build/remaining-medium-bystander-stages-invalid-tree-annotations.json`; its early
allocation attribution is retracted. The shared observer now normalizes only
explicit _decl/_type/_cst tree addresses, including an attached closing bracket.
Parsed controls still detect changed symbol identities, flags, stack offsets,
registers and large immediates. Historical positive/equal traces remain green:
19/19 detector controls. The independent alias-bijection audit passes six controls
and its historical six-stage trace; the corrected HDLC trace covers eight stages.
Native byte grading is unchanged.

The final allowed period run passes **387 binaries / 0 failed**, explicitly
excluding t_fuzz. The corrected unequal-tone lifecycle reports40326 checks;
the old false-positive path ran128 extra state checks (40454 total). The equal
positive control reports35429. The smaller unequal-case denominator follows the
removed false verdict, not a weakened assertion. All72 bank cases, six unsupported
wrapper inputs and restored invalid-tone transcripts run in the final suite.
No fuzzing, mutation execution or modern portability/full-phase claim.

The final canonical build changes exactly4/300 objects;296 remain raw-identical.
All four selected controls match production raw, and300/300 faithful objects
match canonical inputs. Compiler profile is unchanged. All19 full-TU cells and
77 shared verdicts pass the declared audits; ordinary metadata/data/nontext/calls
and bystanders remain as constrained above.

Strict census stays **1086/1852 EXACT,120322 original exact bytes** with no exact
losses; the ratchet passes. Other grades:667 SIZE,65 BYTES,29 REGALLOC and5
UNRESOLVED. This batch corrects behavior and partial dataflow without adding an
exact function. Partial text grows702620→703292 bytes, allocated PROGBITS
918628→919012, and positioned equality falls69559→69422/943398. Both strict
whole-object comparisons remain DIFFERENT/exit1. Structural targets pass;
static anchor checks cover285 suites/10038 anchors without executing mutations.
Final reference and provenance checks are recorded with the commit.

The independently supported service/medium queue is closed. Remaining byte
work needs a new native operand, control-flow or compiler-history witness;
these results do not establish a global ceiling or original-source uniqueness.
