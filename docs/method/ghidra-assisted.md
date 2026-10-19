# Ghidra-assisted fidelity investigation

This investigation uses Ghidra and GhidraMCP to classify remaining binary
mismatches and nominate independently verifiable reconstruction changes.
It starts after PR #291. The [analysis policy](../../CLAUDE.md) permits
recorded hypotheses and exploratory analysis; the [experiment method](experiment-design.md)
and [period compiler gates](compilers.md) still decide source acceptance.

## Starting evidence

The validated source at archived commit `041faca2e` compares 1852 original
functions: 1101 EXACT, 653 SIZE, 68 BYTES, 29 REGALLOC and one UNRESOLVED.
The remaining 751 functions are observational categories, not proven causes.
Exact functions cover 125983 of 720125 original function bytes. All 724 paired
initialised objects agree. The period gate passed 392 fixtures, zero failed;
strict partial-object comparison remains DIFFERENT.

The original PR history is retained by `archive/pr-291-before-cleanup` because
replay scripts use immutable intermediate commits. Seven additional historical
snapshots used by replay scripts are retained by
`archive/pr-291-replay-baselines`; their accepted changes are included in the
cleaned branch. PR #291 landed as `67d401f8d` with 33 topic commits and an
unchanged final source tree. Refresh these measurements
from the new branch before interpreting subsequent changes.

## First connected investigation: 17 October 2026

The bridge connected to project `slmodem-gh` and registered 200 tools. The
reference and the canonical `e98762ba1` partial link were imported into
separate folders under `/source-recovery-pr293`. The reference import has
1852 functions, ELF relocatable format, `x86:LE:32:default`, GCC compiler
specification and image base `0x10000`. Program creation metadata records
Ghidra 12.2; the local installation identifies itself as 12.2 DEV,
revision `e44be6f91fd160ac6b469f6a4d2ae066959b6215`.

The input SHA-256 values are:

| Input | SHA-256 |
|---|---|
| Reference | `1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379` |
| Canonical baseline partial link | `e6109bff5033f2c35e56c8e921bbe38906ae9245ae863b37182e8f988ff28a9a` |

Use full project paths as program selectors: both programs are named
`dsplibs.o`. Reference `.text` addresses include the loader base: EchoCanceler is at
`000be8c0` for ELF `.text:0xae8c0`. GenericIIR scalar is instead at
`000c2130` in Ghidra but at offset zero in its own ELF COMDAT section,
`.gnu.linkonce.t._ZN10GenericIIRIfdE7processEf`. Locate COMDAT functions by
their own sections, never by adding this base to every symbol value.

### Calibration results and limits

The fixed [x87 assembly controls](../../test/experiment/ghidra/x87.S) and
[native check](../../test/experiment/ghidra/x87_check.c) preserve four known
instruction sequences. Native execution passes 5/5 checks: the arithmetic
operations are observable, and the narrow/wide subtraction witness returns
float words `c0920eaa` and `c0920eab`, respectively. See the
[calibration instructions](../../test/experiment/ghidra/README.md).

With inferred signatures, the three narrow probes all decompile to the
product alone, omitting the final subtraction, addition or division and the
third argument. The unrounded control retains its subtraction. Explicit
`float name(float a, float b, float c)` signatures with `__cdecl` restore
all three omitted operations. These are known annotations on synthetic
controls, not signatures inferred from the reconstructed modem.

After annotation, narrowed and unrounded subtraction produce the same
pseudocode, `b * a - c`, and the same float-width arithmetic in high P-code.
The arithmetic omission depends on signature inference, but accurate
signatures still do not make high P-code an x87 rounding oracle. This
measured 12.2 result replaces assumptions based on the historical 11.4.2
control; it does not establish a version-to-version regression.

The MCP `get_function_pcode` endpoint describes `granularity=basic` as raw
P-code. Its returned basic-block operations include `MULTIEQUAL`, `PTRADD`
and `CAST`: they are decompiler HighFunction operations. The saved exports
are therefore labelled `basic-high-pcode`, not raw instruction P-code.
Changing to `granularity=high` adds a `high_pcodes` array; it does not repair
that provenance. Actual instruction P-code remains unmeasured through this
endpoint. Use independent disassembly for decisive precision evidence.

The bridge's `disassemble_function` also returned empty responses for all
four nonempty calibration functions. These are invalid observations, not
empty functions; `objdump` independently verifies the expected instructions.
MCP inline execution is disabled in this instance and was left disabled.

### Source leads verified independently

The reference GenericIIR decompilation exposed signed decrement-and-test
compaction guards and descending copies. `dis.py` confirms these at scalar
`+0xaf..+0xb0` and block `+0x11c..+0x127`; the previous reconstruction used
unsigned zero tests and ascending counters with reverse indexing. The
investigation also checks accumulator/input/output publication order against
actual original stores, rather than adopting inferred types from the display.

EchoCanceler's Ghidra graph corroborates an empty-tap guard around adaptation.
Independent original instructions at `.text:0xae9b3/0xae9b5` precede the
product at `0xae9b7/0xae9b9`. Its recovery uses the source loop's ownership
and ordinary GCC invariant motion; the compiler trace decides where motion
first occurs. These are bounded source improvements, not whole-function
equivalence claims from decompiled output.

## Verify the analysis inputs

1. Import `ref/slmodemd/dsplibs.o` and a frozen, freshly built canonical
   `build/partial/dsplibs.o` into separately named programs. Record SHA-256,
   source revision, complete build configuration and original ELF metadata.
   Every reconstruction build must enable `DSPLIB_REPRODUCE_BUGS`.
2. Record the actual Ghidra version, bridge version if exposed, processor,
   compiler specification, loader and analysis options. Do not infer the GUI
   version from the default in `tools/decompile.sh`.
3. Check ELF section mappings, relocations, function boundaries, symbols and
   known calling conventions against independent ELF/disassembly readers.
   Use original mangled symbols and section-relative addresses as identities.
4. Pass the program explicitly in subsequent MCP operations. Distinguish
   reference metadata, analyst hypotheses and annotations copied from the
   reconstruction. A copied structure definition cannot validate itself.
5. Never import or inspect `re/` or analysis derived from it. Preserve existing
   analyst work and avoid automatic cross-program annotation propagation.

## Calibrate before surveying the remainder

Use a fixed control set and retain per-control results. An unsupported or
ambiguous result is a reported outcome, not an empty successful report.

| Control | Required discrimination |
|---|---|
| Historical narrow/wide x87 probe in [equivalence](equivalence.md#f3-x87-measured) | Record whether raw and high P-code retain the subtraction, parameter and explicit narrowing; rerun on the installed version. |
| GenericIIR before/after block normalisation | Identify scalar division versus reciprocal-and-multiply, including the persistent double-state witness despite equal float output. |
| EchoCanceler before/after separate guards | Locate predicate reuse, count normalisation and distinct adaptation guards at original instruction addresses. |
| Resampler comparison-cloning controls | Do not classify an extra static comparison as a missing source predicate without operand/path evidence. |
| Existing exact functions | Retain matches without inventing dataflow differences from relocation or register annotation. |
| Known changed call, constant or field-width control | Demonstrate each enabled detector reports the intended difference. |

Ghidra 11.4.2 historically lost an x87 subtraction and parameter in high
P-code while raw P-code retained them. That observation must not silently
become a claim about another version. Raw and high P-code exports require
separate labels and instruction-address mappings. Equal decompiled output,
normalised hashes or P-code do not certify behavioral or byte equivalence.

## Build a complete discrepancy ledger

Reuse the existing ELF and residual inventories; do not build an unrelated
second symbol census. Include all 751 remaining names, every defining COMDAT
copy, and the comparator's worst-copy result. The existing
`tools/residual_stage_inventory.py` selects REGALLOC/BYTES and therefore does
not cover the SIZE remainder by itself.

For each function record:

- Original name, service, source TU, defining objects and input hashes.
- Original/candidate size, grade, section/address and comparison coverage.
- Changed calls, diagnostics, constant/data references, field offsets and
  access widths, loops, guards, state stores and arithmetic operations.
- Raw observations, inferred hypotheses and independently verified findings
  as distinct fields, with addresses and reproduction commands.
- Supported/unsupported/ambiguous analysis status, possible multiple causes,
  relevant fixtures and persistent-state/history coverage.
- Previous controls, closed domains, next discriminator and stopping condition.
- Original bytes affected and other functions sharing the suspected mechanism.

Reconcile classified plus unclassified counts to the current census. Group
candidates by supported mechanism; size alone is insufficient. Prioritise
real discrepancies and mechanisms applicable to multiple functions, balancing
function counts with original bytes affected. Similarity is a navigation aid.

## Next bounded investigations

The first two batches recover EchoCanceler's guarded product and dot-product
memory operand, GenericIIR's compaction/publication boundaries and byte-exact
`silence_progress`. The
[Playbook follow-up](refinement.md#compiler-stage-follow-up-exact-silence-guards-and-echo-operands)
records the measured compiler decisions and closed domains.

1. Widen the V92 diagnostic investigation to native consumers that publish a
   float and then reread that member for the sign argument. The current named-
   setter comparison found no exact analogue: the equalizer setters compare
   before publication, while exact `setParamEia6` compares a computed local.
   Require a new ownership/helper/macro witness before changing `setEchoBeta`,
   `setDecayFactor` or `setState`; first-CSE forwarding is now measured, and a
   blanket qualifier change on the current shared-helper source contradicts
   native reset's literal signs. Do not repeat local arithmetic spellings.
2. Use that native-consumer inventory to choose a fresh structural family if it
   supplies no discriminator. Prioritise actual load-width, call-boundary or
   publication-order differences over a small byte score. The constructor and
   readonly-copy domains are closed; no anonymous unresolved operand remains
   in the current compared-function census.
3. CID raw/9600 success and checksum-failure/recreate/recovery histories now
   pass: 646 paired calls, 1858 events and 24 controls. Further source changes
   need a different reachable failing path or native operand witness, not
   another spelling of the 368-versus-386-byte process body. Keep the 1433
   source/option cells closed. See [#24](https://github.com/philpem/slmodem_dsp_re/issues/24).
4. CenterAdapt's allocated history home and reload deletion are now measured.
   Reopen only with independent death-note/lifetime or common-profile evidence
   explaining the reference's selective two surviving stores. The eleven-byte
   gap does not justify adding stores or reviving closed scalar spellings.
5. Reopen EchoCanceler only with a new operand/lifetime or original-profile
   witness. Both reload orientations cost eight, and the first wins the tie.
   The retained frame has two spill words and zero padding; the original's
   extra four bytes do not establish another local or an alignment option.
6. GenericIIR: require allocator/liveness provenance for the scalar scratch
   and renaming choices and block destination spill. The original block's
   second extra slot receives an input-length store that is never read.
   The prior source domains are closed until a new discriminator appears.
7. Keep silence's two previously surviving opposite-direction threshold
   mutations visible as fixture limits. Byte-exact recovery does not create
   new lifecycle evidence for those inputs.

Initializer owner allocation is now measured: `local_alloc` selects EDX as
the first eligible equal-cost home, while `25.greg` first displays it.
Peephole scratch selection and reverse register renaming explain the final
memset arguments. BYTES106 remains. Reopen source only with an independently
supported competing lifetime or allocation constraint. CenterAdapt and the
V92 builder likewise need a new operand/lifetime/profile witness; completed
ordered-store checks are a reason to pivot, not to repeat source spellings.

The Resampler static-comparison-count domain remains closed without a new
contradictory operand or path. Its extra comparison was reproduced at GCC's
x87 stack-conversion stage.

Queue's measured cost/cache trace explains the retained push prologue. Its
old spelling domain remains closed without a new source boundary or
return-width witness.

## Acceptance and reporting

Every proposed source change needs an original instruction witness, a bounded
experiment with a falsifier, valid period differential coverage and full-TU
collateral review. Audit bystanders, data, bindings, imports, positioned
relocations, diagnostics and exact-set membership. Update mutation anchors
when source expressions change. Never widen period tolerances.

Finish retained batches with `make phase`, canonical/partial-link comparison
and runtime/canonical object checks. Report exact gains/losses separately from
partial graph recovery and demonstrated functional corrections. Update the
[Playbook](refinement.md), the ledger and next discriminating tests. Whole-file
identity also requires section layout, padding, symbols and relocation tables;
function-level EXACT is not the completion criterion for the whole object.

## First retained batch validation

The integrated source passes `make phase J=4`: 392 period fixtures, zero
failures, with structural gates passing. All 300 runtime source objects
raw-match the canonical objects. Only `FloatIIR.cpp` and `Fdspkrnl.c` change
from the canonical baseline; both raw-match the delegates' independent
selected controls. Parent review confirms the changed-body inventories and
preserves data, bindings and resolved relocation consumers.

The complete census is 1101 EXACT, 652 SIZE, 69 BYTES, 29 REGALLOC and one
UNRESOLVED across 1852 functions. Exact membership and its 125983 original
bytes are unchanged. The IIR scalar moves from SIZE to BYTES; this is partial
recovery, not a new exact function. Strict partial-link comparison remains
DIFFERENT, with 2840 candidate versus 2836 original NOBITS bytes.

Reference checks pass 13064 entries. All 10042 mutation anchors across 285
suites resolve uniquely; this is structural validation, not a full mutation
execution. Strict documentation build/audit passes, with 508 Markdown
link targets, 300 module links and 972 generated HTML targets/fragments;
both audit negative controls fire. No modern-portability result is claimed.

## Compiler-stage follow-up validation

The second retained batch passes `make phase J=4`: 392 period fixtures, zero
failures and all structural checks. `silence_progress` adds one exact name
and 623 original bytes, taking the census to 1102/1852 and 126606/720125
original function bytes, with no exact losses. EchoCanceler remains 359 bytes
with 31 differing bytes. GenericIIR is unchanged from the first retained batch.
Only two of 300 canonical objects change; all 300 runtime objects raw-match
the canonical set. Strict partial-link/raw-file comparison remains DIFFERENT.

Three repaired mutation anchors are exercised as period-compiled subsets:
the zero-level and full-word probes fail 140 and 906 silence checks, and the
reversed-history probe fails 4560 Echo checks. These are runtime failures,
not compilation/link failures. All 10042 anchors across 285 suites resolve
uniquely. No full mutation execution or snapshot refresh is claimed.

The [silence controls](../../tools/recovery_silence_guards.py) use six unique
source variants; the [cost observer](../../tools/gcc3_prologue_cost_observe.py)
passes baseline and exact-source controls with raw-object identity. Its
empty-target negative is rejected after a successful compiler run. Full
commands, compiler hashes and traces are retained under
`build/silence-cost-293/`; global measurements are under
`build/continuation293/`. The IIR stage audit is retained in the isolated
`build/worktrees/iir-stages-293/build/iir-stages-293/` artifact directory.

A reproduction needs the archived `5bfa2f09c` period silence object and its
`.build-config` as `--baseline-dir`; later-source objects are not substitutes.
For the six-cell cross and the read-only observation:

```sh
python3 tools/recovery_silence_guards.py --domain guard-width-and-return-boundary \
  --baseline-dir /path/to/archived/5bfa2f09c/tc_repro
python3 tools/recovery_silence_guards.py --audit
python3 tools/gcc3_prologue_cost_observe.py \
  --reproduction-dir build/silence-guards-293 --family silence \
  --target silence_progress --output-dir build/silence-cost-observation \
  --labels baseline short-split
```

The observer extracts the compiler from the recorded image. It verifies the
hook's instruction bytes and rejects empty or ambiguous observations. Use
an assembler/mangled name for overloaded C++ targets. The original capture
of `use_fast_prologue_epilogue` is made before register-save code emission;
counting final disassembly instructions would not reproduce this decision.

## Queue and Echo observation validation

This apparatus-only batch retains the source and canonical objects from
`06ba1f713`: 1102/1852 exact functions, covering 126606/720125 original function
bytes. No additional exact gain is claimed. The prior 392/0 period differential
result still applies to that unchanged source; the runtime suite was not rerun
for these observer changes.

The new [baseline driver](../../tools/gcc3_reload_baseline_reproduce.py)
reproduces both complete TUs from that revision using the canonical
`.build-config`. The Queue and Echo prologue observations, Echo stack trace
and Echo reload trace independently reproduce the same complete objects.
Queue has nineteen layouts and two cost decisions; Echo has nineteen layouts,
one cost decision, four stack requests, nine reload choices and fourteen
orientation observations. Cost/padding mutations fail the audit. Both absent
target controls reach compiler exit zero and then reject the empty observation.

```sh
python3 tools/gcc3_reload_baseline_reproduce.py \
  --domain queue-echo-baseline --baseline-dir build/tc_repro
python3 tools/gcc3_prologue_cost_observe.py \
  --reproduction-dir build/gcc3-reload-baseline --family V92Modulator \
  --target _ZN5QueueIfE5writeEf --output-dir build/queue-cost-observation
python3 tools/gcc3_prologue_cost_observe.py \
  --reproduction-dir build/gcc3-reload-baseline --family Fdspkrnl \
  --target EchoCanceler --output-dir build/echo-prologue-observation
python3 tools/gcc3_uref_stack_observe.py --compiler-kind cc1 \
  --reproduction-dir build/gcc3-reload-baseline --family Fdspkrnl \
  --target EchoCanceler --generic-trace --output-dir build/echo-stack-observation
python3 tools/gcc3_echo_reload_observe.py \
  --reproduction-dir build/gcc3-reload-baseline \
  --output-dir build/echo-reload-observation \
  --stack-observation build/echo-stack-observation \
  --prologue-observation build/echo-prologue-observation
```

For each absent-target control, repeat the corresponding prologue/reload
command with `--target no_such_target_293` and a separate output directory.
Require nonzero wrapper status, a recorded compiler exit of zero and no target
observations; a compiler failure is not the intended negative control.

The Queue original-callsite and RTL-node annotations are retained in
`build/worktrees/queue-cost-293/build/queue-cost-293/handoff.json`.
The parent independently repeats all four observer commands above. The
[Playbook](refinement.md#count-compiler-nodes-before-inferring-a-queue-source-discrepancy)
records the inference limits and closed domains.

A four-cell `FSE_decision_CD` source/scheduler diagnostic repeated F11572's
closed store-order domain: reversing the two source stores leaves the complete
TU raw-identical. Disabling sched2 changes all nine bodies and supplies no exact
gain. This redundant experiment is stopped; it does not reopen that family.
`V22FP_modem`'s current five-byte mismatch is another argument-setup scheduling
swap, with no new source-fidelity witness, and is not promoted on size alone.

## Initializer and split-span recovery

Two further source recoveries use the same Gentoo compiler and flags:

| Function | Before this batch | Retained | Original |
|---|---:|---:|---:|
| `preinitdigital` | 300 bytes, SIZE233 | 541 bytes, SIZE8 | 533 bytes |
| `V34EqualizerCenterAdapt` | 183 bytes, SIZE164 | 317 bytes, SIZE30 | 347 bytes |

These are length gaps, not counts of differing bytes. Initializer visibility
restores two embedded bodies and removes two extra helper calls. The split-span
source restores the two circular loops and continuous coefficient owners.
Neither function becomes exact in this batch.

The initializer pair retains its other twelve bodies, data, 29 global
definitions and ten external imports. Its three focused period fixtures pass
27 groups and 6186579 checks; both moved callback-pair mutants are caught with
Gentoo-compiled objects. Mutation anchors follow the moved definition; no
standard mutation snapshot refresh is claimed.

The selected split-span candidate retains twenty-four neighbouring bodies,
data, bindings and relocations, but changes four bytes in
`V34EchoPreFilter`. Installed-compiler observation explains this loss: the
same scratch request and eligibility enter with persistent search cursor two
versus one and select CX versus DX. The two whole-TU traces contain 78 versus
81 searches, of which two versus five occur in CenterAdapt. The zeroing
difference starts at peephole2; later register renaming adds the shift-carrier
changes. Both traced complete objects remain raw-identical to their controls.

This is a deliberate, reviewed source-fidelity tradeoff. Current exact
membership becomes **1101/1852**, covering **126467/720125 original function
bytes**: this batch loses only `V34EchoPreFilter` (139 original bytes), gains
no exact names and preserves every other previously exact name. The former
1102-name floor is preserved in Git history. The current ratchet is rebased
explicitly; future unexplained losses still fail it. Since the start of PR293,
`silence_progress` is gained and `V34EchoPreFilter` is lost: net zero names
and 484 additional original bytes exact. The strict partial/raw-file result
remains DIFFERENT.

The nine-cell loop experiment covers 234 body verdicts. A corrupted-data
control is detected; an incorrect wrap bound fails 2372 existing lifecycle
checks. The source candidate independently passes 392 period fixtures before
integration. The [Playbook](refinement.md#review-exact-losses-against-source-fidelity)
records the retention decision and inference limits.

Archive the pre-recovery `06ba1f713` period objects and `.build-config` before
rebuilding, or rebuild that revision separately. Current changed objects are
not substitutes for the frozen baseline:

```sh
python3 tools/recovery_preinit_owner.py --baseline-dir build/preinit-owner-baseline
python3 tools/recovery_preinit_owner.py --late-role-owner
python3 tools/recovery_preinit_owner.py --formatted-production
python3 tools/recovery_v34_center_spans.py --domain center-span-ownership \
  --baseline-dir build/center-span-baseline
python3 tools/recovery_v34_center_spans.py --audit
python3 tools/gcc3_mechanism_scratch_observe.py \
  --reproduction-dir build/recovery-v34-center-spans \
  --target V34EchoPreFilter --out build/center-scratch-observed
python3 tools/gcc3_mechanism_scratch_observe.py --self-test
```

The scratch command observes all recorded cells. Parent review used a
two-cell manifest view containing the baseline and
`split-spans-lane-rereads-coeff-pointers` controls. It independently reproduced
their identical target search inputs and differing cursors; validator controls
accept one valid trace and refuse five invalid traces.

The final integrated `make phase J=4` passes **392 period fixtures, zero
failures**, plus the structural gates. All 300 runtime source objects
raw-match canonical objects; the three changed TUs also raw-match their
independently selected experiment controls. The reviewed current-floor check
passes, and all three membership-ratchet self-test cases pass, including the
masked-loss rejection. Strict documentation build/audit passes. No modern
portability, whole-file identity or full mutation execution is claimed.

## Record-owner continuation and CI review

The continuation from `c11823619` retains two further original ownership
boundaries, with no exact-name gains or losses:

| Function | Before | Retained | Original |
|---|---:|---:|---:|
| `preinitdigital` | 541 bytes, SIZE8 | 533 bytes, BYTES147 | 533 bytes |
| `V34EqualizerCenterAdapt` | 317 bytes, SIZE30 | 336 bytes, SIZE11 | 347 bytes |

Initializer record owners and native group-store order recover 131 decoded
instructions, matching the original count. The independent ascending-order
control has the same size/count but the wrong graph. All twelve neighbours,
data and exports survive; both callback-pair mutants are caught. Remaining
publication interleaving is recorded in the
[Playbook](refinement.md#preserve-initializer-record-owners-and-publication-order).

CenterAdapt's two original 93-byte arithmetic regions each equal a candidate
90-byte region after removing one original three-byte spill. Those six bytes
and five original alignment bytes account for the length gap. Twenty-two
neighbours and all data/bindings survive; three neighbours change through the
measured persistent scratch state. PreFilter now differs in forty-three bytes
and is no longer grade 1. This reviewed collateral does not lose another exact
name. The [owner/spill proof](refinement.md#recover-the-centeradapt-history-owner-before-fitting-its-spill)
bounds the next compiler investigation.

The V92 encoder investigation preserves eight source/profile controls and
corrects a false source comment about the unnamed-mode path. Its diagnostic
240-byte exact result is not a retained exact gain. The comment-only complete
object is raw-identical; the production profile and executable tokens stay
unchanged. See the [case-ownership cross](refinement.md#v92-encoder-case-ownership-and-the-inline-boundary).

The final integrated source passes `make phase J=4`: **392 fixtures passed,
zero failed**, with all structural checks. Only two of 300 canonical objects
change; both match independent controls, and all 300 runtime objects raw-match
canonical. The census is **1101 EXACT, 29 REGALLOC, 71 BYTES, 650 SIZE and one
UNRESOLVED**, covering 1852 functions and 126467/720125 exact original bytes.
Exact membership is unchanged. Strict partial/raw-file identity remains
DIFFERENT. No modern portability or full mutation snapshot refresh is claimed.

The prior remote smoke/codegen job passed its smoke tests but failed its
base-versus-head metric check on the explicitly reviewed PreFilter loss. The
local floor rebase alone could not authorize that comparison. The
[CI review mechanism](../../tools/ci/README.md) now requires the exact base SHA,
symbol and current function-body/relocation digest, plus source and evidence
revisions. Its report still lists every lost and gained name and preserves red
numerical regression rows. Additional losses, stale bases, changed bodies and
missing/ambiguous definitions remain failures; period differential gates are
untouched.

Seventeen review tests and eighteen existing metric checks pass. Actual Gentoo
object integration accepts the reviewed body and rejects both the same loss
without a review and a corrupted actual body. The current pin follows the
newly reviewed history-owner source; the earlier four-byte body's pin cannot
authorize this changed body. The current exact-floor update refreshes only
informational category counts, with identical exact names and exact bytes.

The new replay drivers require the archived `c11823619` objects/config under
`build/next293-baseline`. Older production-format controls apply to their named
historical source snapshots; use the current publication control for the
new initializer source rather than interpreting intentional later refinement
as a historical-control failure.

## Publication and compiler-provenance continuation

From frozen `4a6519cba`, one original state publication move improves
`preinitdigital` from BYTES147 to **BYTES106**, retaining **533 bytes and 131
decoded instructions**. A four-cell state/count cross produces two complete
object emissions: count-only is inert, and both-native reproduces state-only.
The minimal state move is retained. All twelve neighbours, named data,
bindings, imports and relocation consumers survive; the three pair exact
names are unchanged. The [Playbook stage trace](refinement.md#separate-witnessed-state-publication-from-scheduled-count-publication)
explains the remaining role comparison through global allocation, peephole
zero materialisation and sched2 flag dependencies.

The [CenterAdapt observer](../../tools/gcc3_center_spill_observe.py) explains
the unused frame word without another source hypothesis: four history pseudos
share one allocated spill home; regmove makes their multiplies destructive;
reload emits and deletes eight output copies before `25.greg`. The reference
retains two write-only history stores, whose selective survival remains open.
Both observed complete objects are raw-identical to the baseline. Four
negative controls fire, including a successful compilation with an absent
target that the observer rejects. No source or exact-floor change is made.

The [V92 provenance cross](../../tools/recovery_v92_inline_provenance.py)
measures the installed auto-inline default of 100 and scalar/array helper
costs of 98/106. The array stops automatic inlining but also loses the exact
440-byte helper, introducing a 28-byte frame and sixteen more code bytes.
It is not retained. A no-unit control leaves all eight function bodies and
positions unchanged while placing the two named tables in the reference's
relative order; contents, bindings and raw relocation addends survive. This
is data-layout evidence, not unique proof of a compiler option. Six cells,
forty-eight body verdicts, five callgraph dumps and two detector controls are
audited. No source/profile alternative is adopted.

The parent independently replays all three investigations and production
initializer compilation. The integrated `make phase J=4` exits zero with
**392 period fixtures passed, zero failed**, plus structural checks including
285 mutation suites and 10042 unique anchors. No fixture or mutation anchor
changes. The initializer's existing focused coverage remains three binaries,
twenty-seven groups and 6186579 checks; both callback sensitivity mutants fail
as expected. The current replay baseline is archived under
`build/continuation293-baseline`; prior drivers retain their historical inputs.

The final census remains **1101/1852 EXACT**, covering **126467/720125
original function bytes**, with no gained or lost exact name in this batch.
Only the shell object changes among 300 canonical objects; all 300 runtime
objects raw-match canonical builds. The exact-membership ratchet passes
without a floor update, and the existing reviewed PreFilter body is unchanged.
Strict partial/raw comparison still reports DIFFERENT: 92 reference versus
99 candidate sections and 2836 versus 2840 NOBITS bytes. This is function-level
partial refinement, not complete-object closure. Strict documentation build
and audit pass, including 536 Markdown targets, all 300 module links, 972 HTML
targets/fragments and both detector controls. No modern portability or full
mutation snapshot refresh is claimed.

## Encoder boundaries and initializer allocation

From `8e216110a`, retained compiler options stay unchanged while two encoder
source refinements restore reference structure:

| Boundary | Before | Retained | Reference |
| --- | --- | --- | --- |
| Encoder table block | subset then coset | coset then subset, 320 exact bytes | coset then subset |
| Encoder process | 503 bytes, SIZE263 | 458 bytes, SIZE218 | 240 bytes |

The definition-order/unit cross produces two raw-identical compensation pairs.
The selected definition swap restores the exact contiguous original data block,
without uniquely recovering declaration order or a compiler option. The old
declaration-order source/mutation note is corrected. A second four-cell cross
combines that layout with the previously tested case-result/common-tail form.
Original and selected unnamed-mode paths perform no trellis reads; baseline
paths perform two. Six original and twenty-four candidate selector paths plus
an altered native branch control validate that bounded detector. No junk-value
runtime comparison or public reachability claim is made.

The combined process still inlines `inverseMap` and differs in valid-case
publication bytes. The duplicate state assignment is idempotent on valid
reset-built histories with distinct nonvolatile arrays. These compiler gaps
do not justify retaining the independently wrong default table loads. All
seven neighbours, six exact names, imports and exports survive. The parent
production control raw-matches the combined selected object. Both reanchored
process mutants fail under Gentoo; the equivalent index full object remains
raw-identical. All forty suite mutation anchors and 10042 global anchors remain.

The new ordered-store observer pins builder bodies and symbol-derived linked
addresses in an existing static Gentoo fixture. Fourteen calls and 5376 store
events match original order, indices and values, including four unnamed
selectors with no writes. Three corrupted traces fail. The 1456-versus-1558
builder remains inexact, but no source discrepancy was established. Fixture
comments now report 64/256/1024 written entries per array, separately from
their 244/504/1024 extents.

The initializer observer corrects a previous inference from dump position:
local allocation, not the global pass, chooses owner 83's EDX home. It
overlaps EAX owner 82, has equal register costs and takes the first eligible
home. Memset size/zero stay memory constants until peephole2, then reverse
renaming selects their final registers. Nine local choices, twenty-six rename
decisions, 100 TU scratch searches and twenty-three target searches are
audited. Actual absent-target and wrong-owner controls fire; complete objects
stay raw-identical. No source change is inferred from that compiler trace.

The integrated source passes `make phase J=4`: 392 period fixtures, zero
failures and all structural checks. The focused fixture passes again after
its comment correction. The complete census remains 1101/1852 EXACT and
126467/720125 exact original function bytes, with no exact name gained or
lost. Only the encoder object changes among 300 canonical objects; all 300
runtime objects raw-match canonical builds. The ratchet floor and reviewed
PreFilter body are unchanged. Current controls use archived
`build/allocation293-baseline`; older production-format controls apply to
their frozen sources. The [Playbook](refinement.md#retain-native-table-layout-and-a-witnessed-case-boundary)
records the adoption boundary and closed domains.

Final strict partial/raw comparison still reports DIFFERENT (exit 1), including
92 reference versus 99 candidate sections and 2836 versus 2840 NOBITS bytes.
No whole-object completion, modern-portability or mutation-snapshot refresh
is claimed. Strict documentation build/audit passes: 541 Markdown targets,
300 module links, 972 HTML targets/fragments and both detector controls;
eleven doc-audit unit tests and all 13067 finding references pass. The parent
independently replays both source crosses, combined production/mutation
controls, builder store observation and initializer allocation observation.

## Selector recovery and native boundary follow-up

The next continuation restores `designWindow<float>`'s explicit BOXCAR arm.
Its zero selector tail-jumps to boxcar while default calls and returns; the
previous source collapsed those equivalent paths. Both defining copies are
now EXACT116, one unique original name gained and no exact losses. Five
complete header consumers contribute 103 function copies; only the two selector
copies change. Canonical and runtime builds agree on all 300 source objects,
with only two changed from `6fe0b1e42`.

At the BOXCAR checkpoint: **1102/1852 EXACT**, **126583/720125 original function bytes
exact**, 29 REGALLOC, 71 BYTES, 649 SIZE and one UNRESOLVED. The exact-set floor
adds only the selector. `make phase J=4` passes 392 period fixtures and all
structural checks, including 285 suites/10042 unique mutation anchors.
Partial/raw comparison remains DIFFERENT: exact sections rise from 67 to 68,
positioned matching bytes from 72069 to 72161, and exact symbols from 456 to
457. These are separate partial-link metrics, not function-body denominators.

CID adds a debug-enabled checksum-failure lifecycle, raising its fixture from
162 to 193 checks. Native ordered calls, results, strings, guards, diagnostics
and TTY delivery agree; seven changed-trace controls fail. The CID source object
and both exact companions remain unchanged. The V92 ownership audit verifies
all relocations and complete per-function decoding of 1852 original/1916
current functions, with five copied-ELF controls. It finds no independent
helper caller, visibility/type mismatch or LOCAL defining-owner witness.

The cross-family compiler experiment is diagnostic: automatic inline off
recovers encoder process and several V90 wrappers, but loses the original
inlined echo-delay setup and emits extra local helpers. Unit-off restores the
echo reset tail but also leaves explicit echo-delay calls and changes data
layout/exact neighbours. No production profile changes. The
[Playbook selector recovery](refinement.md#preserve-equivalent-switch-arms-when-their-call-boundaries-differ),
[CID trace](refinement.md#check-native-delivery-boundaries-before-reopening-cid-source-variants)
and [ownership audit](refinement.md#separate-helper-reference-evidence-from-original-tu-ownership)
record controls, replay commands, bounded conclusions and the next discriminators.


## Reset factoring and CID fixture audit

From frozen `7efb67f39`, ordinary calls to four existing V92EchoCanceller
helpers replace reset's duplicated expansions. The retained period profile
now emits both constructors at **313 bytes, BYTES112**, down from 447 bytes,
with the original ordered reset-tail boundary and inlined delay operations.
Reset stays **EXACT195**, all fifteen strong exports survive, and only the
two constructor code bodies change after relocation normalization. Pooled
string order/addends also move; resolved non-constructor consumers agree.
The complete canonical object raw-matches the independently audited candidate;
the other 299 of 300 canonical objects are raw-identical to the frozen baseline.

The actual Gentoo callgraph explains the change: helper expansions grow
factored reset from an initial inline estimate of 58 through 65/83/101.
GCC then declines constructor inlining; the manually expanded baseline reset
was estimated at 69 and inlined. This is source-factoring evidence at unchanged
flags, not a new threshold setting or unique proof of original source spelling.
The four-cell delay-definition-order cross is complete and remains diagnostic:
retained before/current objects are raw-identical; before-plus-unit-off recovers
the boundary mixture but still differs in bytes and method order. No production
source-order or profile change is adopted.

Focused Gentoo validation passes four binaries, 28 groups and 161199 checks.
Sixteen reset mutation anchors move to their shared helper/reset owners; all
sixteen faults are caught (fourteen differential failures, two runtime signals),
and the restored baseline passes. The two-cell audit checks thirty body
verdicts plus commands, compiler/assembler provenance, data and exports; its
three negatives include a WEAK export whose body remains EXACT. The parent
independently replays both source audits and canonical production compilation.

CID's malformed fixture sine table initially corrupts the new MDMF packets in
both receivers. Replacing it with the intended rounded sine adds four valid
MDMF audio histories and raises the existing fixture from **193 to 476 checks**.
Four nonempty fields are delivered; a rendered fifth field is withheld. The
integrated native trace agrees on **214 paired calls and 830 events**, rejecting
eleven controls. Actual serialized bits independently anchor exact received
wire bytes, including a checksum-preserving equal-corruption negative. Production
CID bytes and both exact companions stay unchanged; process remains 368 versus
386 bytes. The [Playbook](refinement.md#check-native-delivery-boundaries-before-reopening-cid-source-variants)
records the fixture correction and its evidence limits.

The census remains **1102/1852 EXACT**, covering **126583/720125 original
function bytes**, with no exact names gained or lost. Both constructors move
from SIZE to BYTES: current categories are 29 REGALLOC, 73 BYTES, 647 SIZE and
one UNRESOLVED. The exact-membership ratchet passes without a floor update.
Strict partial/raw identity remains DIFFERENT: 68 exact sections, 72093/943398
positioned matching bytes and 457/2931 exact symbols. Positioned bytes fall
from 72161 as code/relocations shift; that aggregate does not decide whether a
native source/call boundary has been recovered.

The remaining constructor reassociation is already present in the earliest
saved tree and initial RTL. Trace installed-compiler expression construction
next; later allocation/scheduling cannot explain its origin. CID raw output
and 9600-Hz histories remain coverage tasks. Keep previously closed spelling
and option domains closed without new evidence.


The integrated `make phase J=4` passes **392 period fixtures, zero failed** and
all structural checks, including 285 mutation suites and 10042 unique anchors.
All 300 runtime source objects raw-match canonical builds. The final comment
clarification also preserves the complete audited object. No fixture count,
tolerance or exact floor changes; no modern-portability or full mutation
snapshot refresh is claimed. Strict documentation build and audit pass,
including 549 Markdown targets, 300 module links, 972 HTML targets/fragments,
both detector controls and eleven doc-audit unit tests. All 13067 finding
references resolve.


## Parser-fold recovery and raw/9600 CID coverage

From frozen `c7cf4f8cd`, the installed Gentoo parser observer identifies the
constructor's multiplication factoring inside `fold`, called from C++ additive
expression construction. Actual tree pointers connect that unsigned 32-bit
fold to the `historyAlloc` assignment. Target and absent-target observations
preserve the complete period object; missing target, binding, fold and changed
type controls reject. Independent fresh-directory replay passes.

Three bounded source cells retain the baseline, separate the quotient product
into `roundedSpan`, and restore the native order of the two span operands.
Both constructors progress **BYTES112 → BYTES2 → EXACT313**. The selected
source gains two exact names and **626 original function bytes**, with no exact
losses. All thirteen other TU bodies, fifteen strong exports, data, positions
and relocation consumers remain unchanged. The canonical object raw-matches
the independently audited candidate; the other **299/300** source objects are
raw-identical to the frozen baseline. Four focused period binaries pass
161199 checks; four moved mutation faults are caught (three differential
failures, one runtime signal), and the restored baseline passes.

CID adds six positive raw/9600 histories at debug 0/2, raising the fixture from
476 to **836 checks**. Independent native replay agrees on **430 paired calls
and 1420 ordered events**, rejecting **19/19 controls**. Rate-dependent FSK
retuning, resampling, raw formatting and complete rendered waveforms are
checked. No production discrepancy is found; the CID object is raw-identical.
These are component success histories, not failure/recovery or modem end-to-end
coverage.

The anonymous literal-copy audit leaves `getSegmentPointer` UNRESOLVED. Its
64 copied bytes agree, but a future identity proof must establish bounded
copy state, pointer nonescape and safe entries. Thirteen hazardous ELF controls
also expose an independent overlapping-symbol extraction defect in the existing
comparator. Keep the detector fix separate from any identity-proof extension;
no literal-derived exact gain is claimed in this batch.

The [Playbook parser-fold entry](refinement.md#trace-parser-folding-before-explaining-a-spill)
records the causal trace and replay commands. The
[CID entry](refinement.md#cross-cid-output-mode-and-sample-rate-at-the-component-boundary)
records mode/rate boundaries and negative controls. Next priorities are the
bounded literal-copy proof, `setDecayFactor`'s x87 member-readback lifetime,
and CID failure/recovery coverage at the new mode/rate boundaries.


The canonical census is **1104/1852 EXACT**, covering **127209/720125 original
function bytes**; 29 REGALLOC, 71 BYTES, 647 SIZE and one UNRESOLVED remain.
There are 748 inexact compared symbols. No exact names are lost. Strict
partial/raw identity remains **DIFFERENT**: 68 exact sections,
72088/943398 positioned matching bytes and 457/2931 exact symbols. These
position-dependent whole-object measures remain separate from the two exact
function-body gains; no whole-blob completion claim is made.


The comparator now validates ELF function ownership, explicit address coverage,
raw bytes and relocation-offset inventory before grading a body. Overlapping
owners fail closed as NODATA; exact-extent aliases remain supported. An
immutable old/new audit of 300 objects, 1852 names and 1886 candidate copies
finds no ordinary body or verdict changes. Four real ELF regression cases,
two constructor alias entries and five decoder-output corruptions cover the
fix. The literal-copy identity extension remains deferred.

Integrated `make phase J=4` passes **392 period fixtures, zero failed**, plus
all structural checks, including 285 mutation suites and 10042 unique anchors.
All 300 runtime source objects raw-match canonical builds. The strict manual
build, documentation audit and finding-reference check pass. No tolerance,
compiler profile, fixture denominator or modern-portability claim changes.


## Readonly-copy identity and first-CSE diagnostic tracing

From frozen `51f62d213`, the decoded readonly-copy proof resolves the existing
115-byte `getSegmentPointer` body. It proves direction, count, private frame,
saved-register disjointness, pointer nonescape, immediate source-register death,
readonly contents and safe entry boundaries. Only the consumed anonymous
relocation receives the proved content identity. No reconstruction source or
emitted bytes change. The completed before/after audit covers 1852 names,
1886 candidate body copies and 300 objects: this is the sole body-identity and
grade change, with no losses. Twenty-seven focused ELF cases pass, including
25 hazardous/unsupported same-pair refusals and two safe variants that differ
from the original. The shared extractor additionally rejects relocations
crossing function boundaries. CI runs the focused controls and keys its metric
cache on the new proof module.

The setter trace identifies first RTL CSE as the missing member read's origin,
not parser folding, reload or final stack conversion. Both setters and all six
`setState` diagnostic copies lose their reads there. Native `reset`, however,
uses literal signs, matching current EXACT195 reset. The result does not
license a blanket qualifier, profile or source change. Two complete baseline/
comment objects are raw-identical; thirty body verdicts, 64 RTL streams, eight
trees and six corrupted-trace controls pass. Four focused period binaries pass
161199 checks. The source comment now distinguishes native readback from the
current emitted register comparison. A read-only 72-copy named-setter screen
finds no exact store-then-member-sign analogue; current local spelling domains
remain closed pending a wider independent source witness.

CID follows the real host's terminal-result contract: failure, deletion,
fresh construction and successful delivery at the same configuration. Six
mode/rate/debug pairs pass per implementation, raising the fixture to **1106
checks**. Independent native replay agrees on **646 paired calls and 1858
events**, rejecting **24/24 controls**. All 44 accepted constructions across
both sides have fresh packet state. Receiver addresses are reused in all twelve
recovery pairs, so event generations—not pointer inequality—establish freshness.
The failed attempt emits no TTY or host delivery. No production CID discrepancy
is found, and its complete object remains raw-identical.

The body census becomes **1105/1852 EXACT**, covering **127324/720125 original
function bytes**. The remaining **747** comprise 29 REGALLOC, 71 BYTES and
647 SIZE; none is UNRESOLVED. This batch adds one identity proof for existing
code, not a newly reconstructed executable body. All 300 canonical objects
remain raw-identical to the frozen baseline. Whole-object raw identity remains
DIFFERENT.


The final integrated `make phase J=4` passes **392 period fixtures, zero
failed**, plus all structural checks (285 mutation suites, 10042 unique
anchors). All 300 runtime source objects raw-match canonical builds. The
partial object is raw-identical to the prior checkpoint and still fails strict
reference identity: 68 exact sections, 72088/943398 positioned matching bytes,
457/2931 exact symbols. The exact floor adds only the proved 115-byte function.
Strict documentation build/audit and comparator self-tests pass. No new
executable source recovery, field-type correction, modern-portability result or
full mutation snapshot refresh is claimed.

## Native-operation and field-owner checkpoint (18 October 2026)

The next batch preserves **1105/1852 EXACT** and **127324/720125 original
function bytes**, with no exact-name gains or losses. The 747 remaining names
are now 29 REGALLOC, 74 BYTES and 644 SIZE, with no unresolved identities.
Six of 300 canonical objects change; all 300 runtime objects raw-match their
canonical period builds.

The principal corrections are five wrong V32 field selections inherited from
a raw-to-typed refactor, native unset locals in three reproduction paths,
SGD short quotient/countdown and sequence-expression recovery, and floating
absolute value plus early publication in the Parameters diagnostic. Some
native source recoveries increase a local size or byte distance. The Playbook
records those tradeoffs, the compiler stages that explain them, and the
bounded source domains already tried. No global flag change is adopted.

The four status operand fixes are tested through constructor/component
histories: old source fails 80/560 new checks and corrected source passes all
560. This fixture uses the established aggregate link mode because a reached
logarithm lookup reads neighboring data. Holding the objects and input fixed,
individual linking fails 79/560; the physical table-word audit explains the
result. The existing math fixtures independently pass 1217541 checks.

The integrated `make phase J=4` passes **392 period fixtures, zero failed**, and
all structural checks, including **285 mutation suites / 10042 anchors**.
This is anchor validation, not a refreshed whole mutation snapshot. The
Parameters mutation was explicitly reanchored and executed; it retains its
existing equivalent classification for the nonnegative input domain.

The integrated partial link has **72216/943398 positioned matching bytes**,
up 128 from the frozen 72088 checkpoint. Its 68 exact sections and 457/2931
exact symbols remain unchanged. Both strict function-independent and raw-file
identity still fail as expected. Strict documentation build and audit pass.
No modern portability claim is made.

Continue with the raw-to-typed substitution audit, checking statement roles
before comparing offsets; matching pairs by equal offsets would conceal the
very defect being sought. Two further bounded leads are phasor intermediate
widths and V34 SNR helper visibility/TU ownership. These remain investigations,
not accepted changes or evidence of exhausted leads.

## Helper graphs, widths and retained partial recoveries (18 October 2026)

At source checkpoint `95c40ead0`, the fresh canonical census is
**1110/1852 EXACT**, covering **127983/720125 original function bytes**.
Five Bell 103 functions become exact: `RxHdxStartB103`, `TxHdxDataB103`,
`TxHdxMarksB103`, `TxHdxSilenceB103` and `RxHdxDataB103`. No exact name is
lost. The remaining 742 functions are **31 REGALLOC, 77 BYTES and 634 SIZE**;
there are no unresolved identities or relocation/data-only rejections.
Thirteen of 300 objects differ from the frozen opening baseline; all 300
runtime objects raw-match the canonical period objects.

The batch also retains supported partial recoveries: phasor short fractions,
V34 SNR helper visibility, the native two-switch timing helper, Bell 103
receiver owner reloads and modem index widths, and Jd CRC promotion/wrapper
structure. The Playbook and linked family audits distinguish source evidence
from compiler-state hypotheses and record full-TU collateral. The Jd endpoint
had previously been withheld without period/mutation validation because it
produced no exact-name gain; completing those gates supports retaining it.

The stable-source `make phase J=4` passes **392 period fixtures, zero failed**
and all structural checks, including **285 mutation suites / 10044 anchors**.
Ten affected Jd mutants are caught; the timing default-arm mutant is also
executed and caught. These targeted executions do not refresh the whole
mutation snapshot. No modern portability result is claimed.

The final partial link measures **72502/943398 positioned matching bytes**,
414 above the frozen opening baseline. It has 68 exact sections,
457/2931 exact symbols and 1002/18317 exact relocations. Intermediate partial
links did regress while function identity improved; these metrics describe
different properties and must not be treated as interchangeable. Strict
whole-object and raw-file identity remain DIFFERENT.

The historical field-access audit remains a bounded review of a known
raw-to-typed conversion, not proof that every reconstructed field is correct.
Its complete per-file ledgers and selected-row coverage are recorded in the
[typed-owner audit](../typed-owner-conversion-audit.md). Continue following
unverified changed access roles and native-supported partial candidates whose
validation is incomplete; do not restart exhausted source palettes without
new evidence.


The next concrete validation lead is `V22_FSE_init`'s configuration capture
(F11741, prior report `7b436a0ef`). The reference reads both configuration
pointers before publishing either destination pointer. The earlier whole-config
copy endpoint reaches the native 372-byte size with BYTES5 remaining and no
reported bystander/data/binding changes, but was withheld without executing
period or mutation validation. Replay only the baseline and that endpoint,
then run `t_v22_fse` and `t_v22rate` at their existing init/reinit boundaries.
Reanchor and execute the existing I/Q-swap mutant without changing its fault.
The observed self-configuration reset path supports the boundary; arbitrary
partially overlapping configurations are not established reachable inputs.
Do not reopen the previous source palette solely to eliminate five bytes.


## Configuration capture and argument fidelity (18 October 2026)

The next refinements retain V22 FSE's two-pointer configuration capture and
FPM tone's native unchecked allocation path in bug-reproduction mode. The V22
initializer reaches the native 372-byte length, leaving five register bytes;
the tone constructor's size gap grows from 84 to 90 bytes while removing an
unsupported branch. Each complete-TU comparison preserves its exact bystanders.
Both changes pass the integrated period/structural gate: 392 fixtures, zero
failures, with no full mutation campaign.

The V32 MRF call now zero-extends the unsigned-word count into its integer
argument slot, matching the reference. Twenty of twenty-four TU bodies stay
fixed; three unchanged-source methods change first at period `28.peephole2`.
The sole exact-name loss is `SetAdaptEqV32`, whose seven differing bytes are
scratch-register choices. Three focused period fixtures pass 27045 checks
per endpoint; no high-count reachability claim or unsafe fixture is added.
The exact-floor change records this intentional source-fidelity tradeoff.
The final integrated V32 checkpoint also passes all 392 period fixtures and
structural checks, with zero failures.

The fresh canonical census is **1109/1852 EXACT**, covering
**127904/720125 original bytes**. The remaining 743 names are **32 REGALLOC,
78 BYTES and 633 SIZE**, with no unresolved identities. All 300 runtime objects
raw-match their canonical counterparts; sixteen objects differ from the
opening frozen checkpoint. Partial-link comparison is **72456/943398 positioned
bytes**, 68 exact sections, 457/2931 exact symbols and 1001/18317 exact
relocations. Strict whole-object/raw identity remains DIFFERENT.

The receiver field audit now covers all 123 changed historical accesses and
seven mappings, with no additional offset/width mismatch. Its forty-row sample
is superseded. The durable period probe checks 83 distinct ordinary member paths
across the complete ledgers and detects the same five known mismatch controls.
The completed constructor ledger counts 116 accesses in 75 groups plus thirteen
mappings and a direct padding store. Its period probe extends the shared checks
to 125 ordinary paths and five size expectations. A path inventory alone was
insufficient; occurrence pairing exposed a separate bounds issue: seven words
are cleared through a five-element register array into two adjacent fields.
The seven native stores are confirmed. Native `LoadReg` and `StoreReg` also
limit indices to 0–4, but that API bound does not prove the original array
extent: seven registers with two private entries or an incorrect API bound
remain possible. Constructor changes are held while accesses to the final
two words are reviewed against those competing models.

Mutation-gate policy and its historical results are archived; the mutation
tooling has since been removed. This work keeps period correctness and source
fidelity primary. The following checkpoint validates the previously measured
wrapper arithmetic and reload candidate.


## Wrapper arithmetic and V34 snapshot index checkpoint

The wrapper now follows the native unsigned minima, division and fragment
comparison, and reloads fragment size after both external copies. All nine
header consumers were audited; only the wrapper object changes. Its existing
component fixture passes 33686 checks. The V34 snapshot loop now uses a signed
short index, recovering native word increments and comparisons. Both changes
retain their independently measured native evidence even though their emitted
bodies grow.

The canonical period build covers all 300 objects. Its census remains
**1109/1852 EXACT**, with **32 REGALLOC, 78 BYTES and 633 SIZE**; exact
membership is unchanged from the V32 count checkpoint. All 300 runtime objects
raw-match canonical, and seventeen differ from the opening frozen checkpoint.
Partial-link identity is **71996/943398 positioned bytes**, with 68 exact
sections, 457/2931 exact symbols and 1002/18317 exact relocations. The lower
positioned-byte score includes placement changes; whole-object identity is
still DIFFERENT. The complete `make phase J=4` gate passes **392 period
fixtures, zero failures**, plus structural checks. The source refinements are
committed as `58e65e409` (wrapper) and `13c1e3ded` (snapshot).

The next larger discriminator is one ordinary V34 case-body reintegration.
Before the snapshot change, the canonical handshake was 12867 bytes against
the native 61541, with 113 versus 394 direct calls. Twenty-four additional
local functions remain emitted, including five large table-1 arms which a
source comment incorrectly claimed were all inlined. Period cgraph output
raw-linked to that baseline reports the inline limit for `v34tx1_trnseg4a`.
The bounded experiment crosses outlined/direct case placement with the old
and recovered snapshot-index types, using retained flags and one physical
body home. Historical claims of distinct missing exit classes were refuted
by F11523; do not reintroduce those speculative exits.


## V32 initialization after the storage-model cross

The four-cell layout/initialization experiment separates the unresolved array
extent from the native seven-store operation. The original fourteen-TU runs
use the intended alternate headers: seven-element loop objects match the
five-plus-two loop objects, and both explicit-store models match each other.
The first promoted replay accidentally omitted `/include` from its overlay
search path; its apparent pass is invalid and retained as such. The corrected
replay must prove header selection with period layout witnesses and a wrong-
header control before its results are used.

Production now uses seven explicit stores through the current five-plus-two
layout. This avoids exceeding the declared array and restores native addresses
+0x3c through +0x48 without changing the API or claiming original field extent.
Its object raw-matches the previously period-validated explicit candidate.
The constructor grows 3729 to 3745 bytes; the other five function bodies and
nontext data remain unchanged. The two constructor-backed fixtures pass
689769 checks in total.

The canonical census remains 1109/1852 EXACT, 32 REGALLOC, 78 BYTES and 633
SIZE. All 300 runtime objects raw-match canonical; eighteen differ from the
opening checkpoint. Partial-link comparison is 72146/943398 positioned bytes,
68 exact sections, 457/2931 exact symbols and 1000/18317 exact relocations.
Strict whole-object identity remains DIFFERENT. Full `make phase J=4`
validation passes 392 period fixtures with zero failures and all structural
checks.


The corrected register replay independently passes all 56 TU/cell builds,
four layout witnesses, both absent-field controls and the wrong-header
control. Its A-cell objects reproduce all fourteen frozen baseline hashes.
The constructor recovery and durable replay are committed as `3f81aab29`.
The invalid first promoted run remains excluded.

## V8 DFT cursor and sample-read ordering

A four-cell cross independently restores the sample cursor and moves the
sample read after bin-phase publication, while keeping it inside the bin
loop. Only the combined endpoint reproduces both native boundaries; their
ordering is already visible in initial RTL. The selected body is 171 bytes
against native 164, improving the previous 149-byte body's length gap from
fifteen to seven. Its two exact siblings and all nontext data stay fixed.
The existing component fixture passes 1081 DFT checks, and production
raw-matches the selected replay object. No alias or unsafe-count fixture was
invented. [The family audit](../v8-dft-read-age-recovery.md) records actual
compiler header dependencies and all twelve body verdicts across four cells.

The canonical census remains 1109/1852 EXACT (32 REGALLOC, 78 BYTES, 633 SIZE),
with no exact membership change. All 300 runtime objects raw-match canonical;
nineteen differ from the opening checkpoint. Partial-link comparison is
72202/943398 positioned bytes, 68 exact sections, 457/2931 exact symbols and
1002/18317 exact relocations. Full `make phase J=4` passes all 392 period
fixtures with zero failures and all structural checks.


## Rebase onto the mutation-policy update

The PR was rebased onto `origin/master` at `c13fb7902` on 2026-10-18. Local
`master` was fast-forwarded to the same commit; its unrelated unfinished edits
were restored with an unchanged patch ID. The 61 PR commits replayed without
conflicts. Range comparison found only changed surrounding workflow context;
`src/`, `include/`, the period toolchain and shared experiment helper are
unchanged between the old and rebased committed trees.

The annotated tag `archive/pr293-before-rebase-20261018` preserves the old
`24ceb2d08` history, including immutable source/evidence commits referenced by
replay tools. Historical commit IDs in the audits remain valid. The reviewed
`V34EchoPreFilter` loss now names base `c13fb7902`: the intervening master
commits change no reconstruction source, headers or period toolchain inputs,
and the reviewed head body/relocation fingerprint remains unchanged.

[PR #297](https://github.com/philpem/slmodem_dsp_re/pull/297) has landed the
[then-current mutation policy](https://github.com/philpem/slmodem_dsp_re/blob/28ccee066/docs/method/mutation-policy.md), closing the separate policy follow-up.
Whole-tree mutation execution and snapshot refresh are optional; period
differential correctness and structural anchor checks remain required.

Repository cleanup removed 33 unchecked-out branches whose tips are ancestors
of master or the pre-rebase PR tip, archived four additional fully patch-equivalent branch histories
as local tags, and removed their redundant branch names. Five registrations
for absent worktrees were pruned. Existing
worktrees with changes or ignored build evidence were preserved. No remote
branches or unique local commits were deleted. The pending tone-filter
refinement was preserved across the rebase; its validation is recorded separately.


## Tone filter ring/count refinement after rebase

The [tone filter audit](../fpm-tone-ring-count-recovery.md) recovers the native
word countdown and conditional ring-index clearing together. The function
moves from 238 to 236 bytes against native 235; it remains nonexact. Seven
exact TU bodies and all ten bystander bodies/relocations are retained. The
four-cell Gentoo replay distinguishes both operation witnesses independently,
and its selected object raw-matches both canonical and runtime objects.
Full `make phase J=4` passes 392 period fixtures with zero failures and all
structural checks on the integrated master base. Two existing mutation
locators were mechanically retargeted, with all 41 tone locators passing;
no full mutation run or snapshot refresh was needed.


## V34 shared-continuation diagnostic checkpoint

The [V34 placement audit](../v34-handshake-body-placement-audit.md) records
an unadopted experiment: expanding the native-absent helper closure and
sharing the terminal transmit continuation recovers three dispatch tables,
with a 66889-byte root against native 61541. All nine exact common bodies
remain exact. Debug format-slot analysis resolves 262/262 native and 286/286
candidate calls, with no added or missing unique texts; differing static
multiplicities remain to be explained by shared blocks and inline boundaries.
Seven apparatus controls include removed arguments and register/stack
clobbers. This checkpoint adds replay tools and evidence, not production V34
source. The next implementation should express the shared continuation with
ordinary cases and one body home, then remeasure complete-TU collateral and
period fixtures.


## Consolidated history and PR295 integration

The 66-commit checkpoint at `a5a661a06` is preserved by the published tag
`archive/pr293-before-consolidation-20261018`. Related adjacent implementation,
evidence and follow-up commits were consolidated into 48 commits without
reordering. Every group endpoint and the complete final tree matched its old
counterpart exactly before integration. Unrelated source recoveries remain
separate; immutable replay and reviewed-loss references remain available
through the archive tags.

The resulting branch rebased cleanly onto master `2a41138f4`, which includes
PR295's INFO0 carrier fix. Local master was fast-forwarded to that same commit
and the owner's unfinished edits restored with the same patch ID. PR295's
normal-build fix is retained, including its fixture and mutation locators.
Both accepted CI loss reviews now name the integrated base; neither reviewed
function's implementation changes. The next V34 continuation candidate must
carry the new normal-build guard forward as well as preserving the period
reproduction path.

The canonical period rebuild retains all 300 object hashes from the prior
checkpoint, including the complete V34 TU. The reconstruction census therefore
remains 1109/1852 exact, with unchanged body/relocation review fingerprints.


Final integration also includes master `a87ae5c35`, which lands the separate
PR298 reference-checker fix and PR299 fixture correction. PR295's documented `AT&D0` command had been
misread as a deviation citation; the standalone parser correction preserves
real citations. A fifth control ensures the tracked regression fixture itself
contains no literal citation examples for the repository scan to misread.
Both CI loss reviews name
this final base. The first integrated period run passed 393 fixtures with
zero failures but stopped on that parser error; the final gate includes its
fix. New tone tracing and ordinary V34 continuation experiments are held for
the next PR rather than extending this source checkpoint.

## Typed owner comparisons

Generate analysis declarations from the canonical headers, preserving the
ILP32 data organisation. Compare imported structure sizes and field offsets
and widths with constants emitted by the period compiler. Keep the untouched
reference separate from the copy carrying reconstructed types: those types
are hypotheses, and their presence in the decompilation is not independent
confirmation of the source layout.

Check imported function ownership before using CFG statistics. The current
ELF importer can leave one-byte function placeholders even when the decompiler
follows enough flow to return a large function. Verify body extents against
ELF symbol sizes and instruction coverage against independent disassembly.
Record unsupported regions rather than interpreting an incomplete CFG as
missing source logic.

Inspect private helper argument storage before applying prototypes. The
ordinary-C candidate's `t72_ladder` receives its two pointers in EAX and EDX;
Ghidra's `__regparm2` model describes that measured storage. This analysis
annotation neither changes the reconstruction's compiler flags nor establishes
that the original author declared a register calling convention. Incorrect
stack-argument annotations can hide the very dataflow being compared.

Export the complete decompilation once, then review bounded address-backed
regions with their owner types, signatures, original instructions and fixture
domain. Distinguish arithmetic and memory-access recovery, helper-boundary
recovery, REGALLOC gains and exact-function gains in the result.

## Current V34 baseline refresh

After PR305 and PR308 merged, the typed candidate was refreshed from
`5a09b5ca7` in a separate Ghidra program:
`/source-recovery-size-regalloc/current-5a09b5ca7-verified/typed`.
The original verified reference and frozen PR303+305 experiment remain
untouched. The latter is not the merged delivery. The baseline ELF SHA-256 is
`f582fdbd9ecfa2071ab3fc81f9439377f09541b7921725576d17f974517e3be8`.

The source checkpoint `7c98085fc` is also imported separately as
`/source-recovery-size-regalloc/current-7c98085fc-verified/typed`, from ELF
SHA-256 `c55f4542a75ca67b7d90abddee5ec6bcf457638760cc76768176d79ffba5cdb8`.
It repeats all checks below. Every selected body is unchanged from the merged
baseline, while its ELF entry moves by -64 bytes after the V.32 and phase-3
changes. Its ledger carries the revised addresses; the baseline export remains
preserved. Final evidence is in the sibling directory
`source-recovery-handover/2026-10-19-next-batch/v34-current-final-c55f4542/`.

The refresh checks 492 layouts: 17 structure sizes and 467 fields, plus the
candidate frame size and seven fields. A deliberately wrong field offset is
rejected. Eleven selected function bodies contain 4,651 instructions;
independent objdump and Capstone decoding agree on all instruction starts and
bytes. All 635 selected relocation sites agree on offsets, types, targets,
addends and applied status. Copied types validate annotation consistency, not
the inferred meaning of fields. Static helper register conventions are
analysis annotations, not recovered source compiler flags.

The current root contains 12,924 bytes, 2,662 instructions and 117 calls;
the reference root contains 61,541 bytes, 12,234 instructions and 394 calls.
Their helper boundaries differ. Compare bounded closures, including shared
callees and joins, before interpreting these root-size differences.

Six analysis partitions cover micro55, three micro58 regions, micro59 and
the micro62/63 common successor. A proposed micro59 setup-call discrepancy
was rejected: the emitted setup is already inline on both sides. The next
confirmed target is the micro58 reset family: native loops at
`0x660e5..0x660fc` and `0x6d78b..0x6d7a2` increment and sign-extend a short
counter, then compare its low word with nine. Current code uses int induction.
Both write ten short elements at root `+0xabae + 2*k`, followed by the separate
`+0xabc2` store. The bounded counter cannot overflow; this is a reconstruction
width discrepancy, not an established output defect. Trace all shared-helper
callers and preserve owner/read timing before experimenting with source.

The source/input manifests, type checks, native excerpts, typed output,
state-transition ledger and verification controls are preserved outside Git
under `source-recovery-handover/2026-10-19-next-batch/v34-current/` beside the
repository. No V.34 source change or new differential claim accompanies this
analysis refresh.
