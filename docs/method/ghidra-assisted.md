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

1. Trace CenterAdapt's real-history pseudo lifetime and spill allocation with
   the period compiler. Its two arithmetic loop regions now match after
   deleting the original's two write-only stores; do not force those stores
   into source. The earlier spelling domains remain closed.
2. Discriminate `preinitdigital`'s remaining state-index/count interleaving
   with group clears. Its length now matches, but BYTES147 remains. Keep
   publication ownership separate from memset argument setup and scheduling.
3. Resolve the V92 encoder source/profile interaction using #22 and common
   profile evidence. The case-result/common-tail control is EXACT240 only with
   automatic inlining disabled. Retained O3 still differs; no per-file flag or
   functional alternative is adopted. Definition-order and case-form domains
   are closed under their measured controls.
4. Reopen EchoCanceler only with a new operand/lifetime or original-profile
   witness. Both reload orientations cost eight, and the first wins the tie.
   The retained frame has two spill words and zero padding; the original's
   extra four bytes do not establish another local or an alignment option.
5. GenericIIR: require allocator/liveness provenance for the scalar scratch
   and renaming choices and block destination spill. The original block's
   second extra slot receives an input-length store that is never read.
   The prior source domains are closed until a new discriminator appears.
6. Keep silence's two previously surviving opposite-direction threshold
   mutations visible as fixture limits. Byte-exact recovery does not create
   new lifecycle evidence for those inputs.

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
