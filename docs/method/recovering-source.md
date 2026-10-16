# Recovering source from a compiled object

Use this guide to plan a source reconstruction from a relocatable object whose
source is missing. It describes the sequence used for `dsplibs.o`, a partially
linked, 32-bit x86 ELF object built with GCC 3.4.2, and explains what to adapt
for another project. You will need working knowledge of C, assembly and linking.
The companion guides cover [reading object code](reading-object-code.md) and
[recovering the build environment](recovering-build.md).

Run repository examples from the repository root. They require GNU binutils,
Python 3 and the analysis dependencies in `tools/ci/requirements.txt`. Period
builds additionally require Docker and the compiler image described in
[build recovery](recovering-build.md). For another project, first establish
how to execute the reference and supply its imported services on the target ABI.

The result is evidence-backed replacement source. Optimisation discards names,
types and source structure, so several source programs can produce the same
object. Even byte identity does not recover the author's exact original text.
Keep functional equivalence, recovered structure, function-body identity and
complete linked-object identity as separate objectives.

## How this project developed the method

The [recovery timeline](timeline.md) links dated milestones and identifies
the agent clients and development tools where the records support attribution.

The initial survey found surviving filenames, symbols, template names and a
narrow host interface. Those made it possible to recover modules and compare
replacement functions with the original in one executable. The early findings
record that survey and the first working differential harness (findings F1,
F2, F3 and F12).

Reconstruction then proceeded through callable components and their
dependencies. The large V.34 state machine prompted a fast pass: retain
assembly reading, structural comments and differential tests, while deferring
coefficient derivations and longer investigations. The
[historical fast-pass plan](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fastpass.md)
records that sequencing decision. A direct copy of a table could support
behavioural reconstruction before its design formula was known.

Compiler matching became more important as the work progressed. A modern
compiler had encouraged source changes that compensated for its different
floating-point behaviour. Rebuilding with the period compiler exposed those
compensations (finding F1352). Later work recovered structures and refined
instruction and relocation identity across complete translation units. Final
partial-link comparison added section layout, data and symbol records to the
completion criteria. The procedure below brings those lessons forward so a
new project can establish the compiler and oracle earlier.

## 1. Preserve and survey the reference

Keep an immutable reference and record its hash and provenance. Save tool
versions and raw reports alongside the investigation. These commands use GNU
binutils and a shell variable pointing to this project's reference; replace
the path for your own object:

```sh
reference_obj=ref/slmodemd/dsplibs.o
sha256sum "$reference_obj"
file "$reference_obj"
readelf -hW "$reference_obj"
readelf -SW "$reference_obj"
readelf -sW "$reference_obj"
readelf -rW "$reference_obj"
readelf -p .comment "$reference_obj"
nm -u "$reference_obj"
nm -C --defined-only "$reference_obj"
```

Establish the architecture, byte order, ELF type, section layout, symbol
binding, imports and compiler clues before interpreting a function. Check for
debug information rather than assuming it is absent. A relocatable object
retains relocation records that an executable may no longer have.

Read the available host headers and call sites to establish the public
application binary interface (ABI). Record argument types, callbacks, buffer
ownership, block sizes, configuration and error handling. The dsplibs host
interface is much smaller than its internal symbol set (finding F3); that
boundary made a standalone test harness practical. A different project may
need an emulator or a captured host environment instead.

**Checkpoint:** a reproducible inventory, a known execution environment and
an explicit boundary at which the original can be called.

## 2. Recover compilation units and dependencies

A translation unit is a source file after preprocessing. In this partial
link, surviving `STT_FILE` entries and local symbols provide anchors for the
original compilation units. Global functions appear separately in the symbol
table, so their textual position alone does not identify their source file.

```sh
python3 tools/tumap.py ref/slmodemd/dsplibs.o
python3 tools/tuattrib.py ref/slmodemd/dsplibs.o --verify
```

These are project tools; inspect their assumptions before adapting them. The
mapper distinguishes local-symbol envelopes from shared, uncertain brackets.
Do not extend an anchored unit to the next anchor and assign every intervening
function to it. That rule misattributed functions here (finding F11351).
Validate name-based attribution against units with surviving local evidence;
retain provisional assignments where that control is unavailable.

Build a dependency graph from calls, data references, callback tables and
registration code. Distinguish the graph's static references from runtime
reachability. An indirect call or externally invoked export may have no direct
call relocation. Conversely, a reference does not establish that an execution
path can take it. Use the graph to select a component whose dependencies can
be linked and whose behaviour can be measured.

**Checkpoint:** a module map with confidence labels and a bounded first
component, including the data and helper functions it requires.

## 3. Establish the differential oracle

The original implementation is the test oracle: it supplies the expected
results for the inputs you exercise. Agreement covers those observations;
it does not establish equivalence for untested inputs.

Link the reference and reconstruction into one test executable where possible.
Rename the reference definitions, then provide two independent instances of
every stateful input or service. In this project the reference uses `ref_`
names; [symmap.py](../../tools/symmap.py) generates the rename map and classifies
imports. The build also globalises uniquely named local symbols where safe.
Duplicate local names require another strategy, such as testing through an
observable caller; blindly globalising them creates collisions.

Classify imports individually. An ordinary stateless memory-copy routine may
be shared. Bit-stream callbacks, configuration stores, registries and diagnostic
sinks need separate state. If both implementations consume one input queue,
the second receives different input even when the calls look identical.
Reject unclassified imports rather than silently sharing them (finding F12).

For each fixture:

1. Construct and reset each side through its own implementation. Supply
   equivalent configuration, tables, input and callback sequences.
2. Give each side separate writable buffers, allocations and random-generator
   state. Use a deterministic seed and record it.
3. Compare return values, output lengths, produced data, state transitions and
   diagnostics. Check state after meaningful steps to locate the first drift.
4. Compare pointers by their role, target contents or offset into an owning
   allocation. Separately allocated objects naturally have different addresses.
   Exclude padding or addresses only with an explicit reason.
5. Demonstrate that a deliberate, relevant fault makes the fixture fail. Report
   how many cases and checks ran, including skipped and unsupported cases.

In this harness, `diff_eq_obj` coalesces differing bytes into runs and
`tools/whichfield.py` maps an offset through the reconstruction's debug types.
Those debug types describe your proposed layout; they are not original type
information. Audit comparison exclusions when a test stays green unexpectedly.

State the fixture's scope: a valid component call, a witnessed lifecycle or
a synthetic internal-state probe. Arbitrary field writes do not establish
that the public API can reach the state. Preserve exploratory results while
adding valid lifecycle cases; see [compiler and fixture policy](compilers.md).

**Checkpoint:** a passing reference control and a known fault that the oracle
detects, with input history and denominators visible.

## 4. Reconstruct one callable slice

A callable slice consists of a function and the helpers, data and state needed
to invoke it at a stated boundary.

Read the complete function with its relocations, callers, constructor and
reset path. Write down the observed arguments, accesses, branch conditions,
loop bounds, calls and outputs before assigning algorithm names. Use
[the object-reading techniques](reading-object-code.md) to turn those
observations into types and control flow.

Recover the containing state layout as you go. Give each shared type one
definition and assert sizes and offsets under the target ABI. Leave unknown
regions as padding and keep known-width fields neutrally named until their
meaning is supported. A persuasive but incorrect field name can survive every
behavioural test.

Implement a small dependency-complete slice, then run the focused differential
test. Exercise zero-length and boundary calls where the contract permits
them, repeated processing, resets, error exits and both sides of each observed
decision. Stateful code often needs a sequence of calls before a difference
becomes visible. Start diagnosis at the first divergent state or output rather
than repairing downstream consequences.

For a large dispatch function, enumerate dispatch tables and follow basic-block
edges. Shared tails may belong to several states; the distance to the next
table entry is not a state's size. This project's `rxtiming` investigation
needed control-flow analysis to distinguish separate pull paths (findings
F121k and F121l). Keep partially recovered cases explicit and unclaimed.

**Checkpoint:** source supported by object observations and a passing focused
period differential over a stated domain. Record unsuccessful attempts too.

## 5. Recover data and explain the algorithm

Preserve table bytes, element widths, relocations and ordering before deriving
a generator. For scalar tables, compare extracted values with candidate
formulae using the original rounding and truncation rules. For pointer-bearing
configuration records, reconstruct targets and addends rather than copying
unlinked address words as numbers.

The fixed-point reciprocal table is a worked example: its stored entries use
truncation, and an index boundary reads into adjacent data in the original.
The source records both the formula and the compatibility consequence in
[fpm_div.c](../../src/dsp/fpm_div.c), with the deviation tracked as D4. A
mathematically improved table would change reference behaviour.

Add an algorithm explanation once the loops, state and coefficients support
it. Record units, fixed-point scale, saturation, filter history, table indexing
and update order. Keep exact extraction separate from design recovery: a
byte-exact table copy supports compatibility, while a verified design formula
supports regeneration at a new rate.

**Checkpoint:** reproducible data extraction and an explanation whose claims
can be traced to accesses, arithmetic or independently checked formulae.

## 6. Refine and integrate without overstating completion

Rebuild with the recovered compiler and compare generated code. Classify the
earliest mismatch before trying alternatives: width, signedness, control flow,
inlining, floating-point narrowing, allocation or layout. Use bounded crossed
experiments rather than changing source spellings until a score rises. See
[build recovery](recovering-build.md), [experiment design](experiment-design.md)
and [refinement](refinement.md).

Inspect all affected functions, data, exports and relocations in the complete
translation unit. A local improvement can move neighbouring functions. Then
run the integration gates. In this repository:

```sh
make period T=t_vpcmrun J=4
make phase J=4
```

The focused binary is an example; choose the test for the component being
changed. `make phase` runs the period differential and structural gates;
`make portability` requests the separate modern compiler and interoperability
checks. Run mutation suites for the behaviours being added or revised, keeping
uncaught, unusable and equivalent mutations visible. Diagnostic transcript
tests and independent conformance tests answer further questions; the
[oracle guide](tiers.md) explains their boundaries.

Record the reference and source revisions, compiler commands, tested domain,
results and unresolved questions with each accepted change. Count missing
symbols separately from insufficient validation and non-identical code.
Replacing every named function does not establish full behavioural coverage.
Matching every tested function body also leaves section layout, anonymous
data, symbols and the final partial link to check.

For another project, carry over the sequence and evidence boundaries first.
Port the project-specific tools only after identifying their assumptions about
ELF, instruction set, compiler, symbol naming and build layout. The
[tool guide](tools.md) records examples of those dependencies.
