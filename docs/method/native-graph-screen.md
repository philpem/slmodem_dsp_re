# Native/reconstruction graph screening

Use `tools/native_graph/` to nominate differences in supported instruction-effect
relationships before treating a residual as register allocation alone. It reads
little-endian ELF32 i386 relocatable objects. Reports are diagnostics: they do
not establish semantic equivalence, reachable input domains or source defects.
The period differential gate remains authoritative. Python 3, pyelftools and
GNU `as`/`objdump` are required; the repository already uses the ELF helpers.

## Routine workflow

After a recovery wave, screen the changed translation units. Periodically run
the same screen over the full period object directory to find transferable
mechanisms. Use a current `make tc-repro` build with valid header dependencies;
do not mix objects from different source/header baselines.

```sh
python3 tools/native_graph/screen.py \
  --native ref/slmodemd/dsplibs.o \
  --candidate-dir build/tc_repro \
  --output-dir build/native-graph-current
```

For one function or a small set, repeat `--symbol`:

```sh
python3 tools/native_graph/stack_compare.py \
  --native ref/slmodemd/dsplibs.o \
  --candidate build/tc_repro/src_pump_v22_v22mod.c.o \
  --symbol v22_originate --symbol v22_data \
  --output build/v22-stack-graph.json
```

`effect_graph.py` accepts the same function-pair arguments for owner effects.
`screen.py` emits `corpus.json`, `ranked.json` and an input manifest, retaining
snapshots of all parsed objects. Select a new output directory for a new run.
It selects every inexact defining copy of each shared unambiguous symbol,
including REGALLOC and BYTES; duplicate emitted copies remain in the denominator.
The graph screen does not alter byte-identity grading or build gates.

Review timing/predicate nominations first, then unmatched anchors and refusals.
For each admitted investigation, check the actual native instructions, argument
and owner correspondence, width, selector/guard provenance, and operation age
across branches and calls. Compiler scheduling may legally change these ages.
Require a fresh source hypothesis and valid differential fixtures before
retaining source changes. Preserve baseline and candidate reports so a known
positive becoming clean can be checked independently.

## Supported comparisons and limits

The owner layer traces bounded entry-argument owners and selected MOV effects,
direct calls and predicates. Unique anchors are paired by meaning; repeated or
ambiguous occurrences remain unpaired. It compares acyclic reachability and
branch-arm relationships. Reordering independent, non-overlapping stores through
the same known owner is excluded. Calls have no semantic summaries. Later or
nested heap owners, unknown instructions and implicit effects limit coverage.

The stack layer proves fixed entry-frame depth, joins and balanced returns.
It pairs local cells by unique write-width/constant and use fingerprints, never
by their stack offsets. It supports one adjacent CMP/JA guarded table dispatch
and preserves case-region anchors. Matching case indices does not prove selector
semantics; review their value provenance separately. Escaped frame addresses are
reported without inferring callee writes. General dynamic frames, loop iteration
identity, x87 effects and complete def-use/value epochs remain unsupported.

An admitted graph can have zero matched anchors or zero compared relations.
Report those separately from graphs with real comparisons. Owner and stack
coverage overlap; their denominators must not be added. Missing nominations in
a supported subset are never evidence that all effects match.

The apparatus snapshots helper dependencies before import and records hashes of
the bytes it actually executes. `NATIVE_EFFECT_DEPENDENCY_TOOLS` selects an
explicit helper directory when replaying a historical frozen dependency set.
Object bytes are captured once before parsing. Retain source/header/compiler
manifests alongside the report: an object hash alone does not establish its
build provenance.

Run the portable real-ELF controls before trusting a new detector version:

```sh
python3 -m unittest discover -s tools/native_graph -p 'test_*.py'
python3 -O -m unittest discover -s tools/native_graph -p 'test_*.py'
```

The current suite passes 33 controls under normal Python and `-O`.
The controls include real positive call/store timing, branch-arm and signed
predicate differences, width and owner changes; negative controls cover register
renaming, layout inversion, independent stores, ambiguous effects and overlapping
local cells. Unsupported CFG/frame and wrong-architecture inputs are rejected.

## Initial whole-corpus result, 2026-11-05

At revision `401a61d6af928a674d318788d70d3ff5efed4097`, the initial frozen
screen selects **716 unique inexact symbols / 719 defining copies**, from 300
period objects. Grades are 28 REGALLOC / 71 BYTES / 617 SIZE per unique symbol;
there are no ambiguous defining rows. The final packaged-tool run took 76.95 s.

| Layer | Admitted graphs | Refused graphs | Copies with matched anchors | Copies with compared relations |
| --- | ---: | ---: | ---: | ---: |
| Owner | 605 | 114 | 519 | 275 |
| Local stack | 107 | 612 | 87 | 23 |

There are **25 timing/predicate nominations across 13 copies** and **484 unpaired
anchors** (442 owner, 42 stack). These counts are discovery results, not a count
of reconstruction errors. The [compact dated evidence report](../evidence/native-graph-screen-20261105.json)
retains the ranked queue, frozen input identities and apparatus hashes; complete
per-function results are
retained in the recorded build/scratch artifact directories.

The native `v22_originate` gap-clear positive is independently calibrated:
one baseline timing nomination, zero with the retained source repair, and
56 paired event anchors. The exact Detect_v22 control compares three owner
anchors and two relations cleanly; its unsupported stack prologue is refused.
The [V22 report](../v22-originate-recovery-20261105.md) records source controls
and the 399-test period gate.

Parent instruction review also confirms `v22_data`: native V22_status call at
`0x8893e` precedes the local zero store at `0x8894a`; reconstruction store at
`0x189` precedes its call at `0x194`. This is a source-timing lead, with legal
compiler scheduling still to distinguish. FPM_PPS_filter and V34EchoAdapt
nominate entry-owner reads on different sides of count predicates. Audit these
before proposing captures.

The separate `null_process` nomination compares native CMPW/JLE at
`0x9f0ec`/`0x9f0f0` with reconstruction CMPW/JE at `0x40`/`0x46`. The source's
unsigned-short count corroborates a signed-interpretation discrepancy. Input
validity for high-bit counts and lifecycle coverage remain unproven; no source
change or fax reconstruction reopening follows automatically from this screen.
