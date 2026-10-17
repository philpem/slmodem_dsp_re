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

1. Screen remaining prologue differences for independently witnessed guard
   boundaries. `Queue<float>::write(float)` remains 71 versus 85 original
   bytes; its old body-spelling domain stays closed without a new witness.
   Use actual frequency/caching observations before proposing source changes.
2. EchoCanceler: inspect the x87 reload alternatives that materialise history
   rather than copy the live product. The choice starts in `25.greg`, not
   stack conversion. Measure the frame arithmetic separately; final operands
   leave the reference's extra four local bytes unused.
3. GenericIIR: observe allocator/liveness provenance for the scalar scratch
   and renaming choices and block destination spill. The original block's
   second extra slot receives an input-length store that is never read.
   The prior source domains are closed until a new discriminator appears.
4. Keep silence's two previously surviving opposite-direction threshold
   mutations visible as fixture limits. Byte-exact recovery does not create
   new lifecycle evidence for those inputs.

The Resampler static-comparison-count domain remains closed without a new
contradictory operand or path. Its extra comparison was reproduced at GCC's
x87 stack-conversion stage.

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
