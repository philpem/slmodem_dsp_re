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

The first batch recovered EchoCanceler's guarded product and GenericIIR's
compaction/publication boundaries, and added mixed-call persistent-state
coverage. The [Playbook](refinement.md#ghidra-assisted-boundary-recovery-17-october-2026)
records the closed source domains and measured improvements.

1. GenericIIR scalar: trace the first feedforward register/base/index choices
   and input-compaction LEA schedule through initial RTL, allocation and
   scheduling before proposing another source variant. Its length now agrees,
   but 63 bytes still differ.
2. GenericIIR block: account for live values and spills behind the original
   `0x34` and retained `0x2c` frames, alongside the twelve-byte length gap.
   Matching the frame alone would not establish the missing source boundary.
3. EchoCanceler: trace the remaining dot-product load/operand schedule and
   adaptation consumer ownership through combine/reload. The original
   guarded producer is recovered; consumer operand reversal was raw-identical.
4. Silence: measure actual Gentoo prologue cost/cache decisions and frontend
   range folding. Ghidra can map the resulting graph but cannot recover the
   unavailable original GCC RTL. Keep surviving threshold mutations visible.

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
