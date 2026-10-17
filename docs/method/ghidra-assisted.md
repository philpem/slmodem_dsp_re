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

The bridge successfully connected to project `slmodem-gh` and registered 200
tools on 17 October 2026. Discovery showed no open programs. This verifies
connection and advertised capabilities only: no import, version calibration,
dataflow result or reconstruction improvement has yet been established.

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

## First bounded investigations

1. EchoCanceler: trace the invariant `e * mu` producer across the empty-tap
   guard. The original product follows the guard; retained code precedes it.
   Preserve the recovered separate predicates and use GCC dumps to locate
   the transformation before trying another source form.
2. GenericIIR: extend mixed scalar/block, non-unit-denominator and compaction
   histories. Compare both histories and all non-pointer state, then inspect
   the remaining loop/publication boundaries.
3. Silence: measure actual Gentoo prologue cost/cache decisions and frontend
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
