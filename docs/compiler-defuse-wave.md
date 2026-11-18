# Bounded compiler value-origin diagnostics

The six-step recovery wave adds explicit pseudo-register def/use evidence to
`tools/gcc3_stage_divergence.py`. It changes analysis apparatus only. No source,
compiler option, function grade or equivalence rule changes in this task.

## Supported evidence and refusals

Each compared RTL stream retains instruction kind, source-location-bearing
headers, printed basic blocks and omitted label/barrier boundaries from the
same captured bytes used for parsing and SHA-256. The analyzer reports direct
register definitions and their subsequent explicit uses inside a straight-line
segment. A single `SET` inside a `PARALLEL` with explicit register clobbers is
supported; simultaneous multiple assignments remain unclassified.

Calls, jumps, labels, barriers, changing basic blocks and opaque effects end
propagation. Auto-update addresses and partial-register writes are refused.
Only an identical pseudo-register operand, including its mode and retained
annotations, can select a local definition. Hardware-register aliasing,
implicit operands, memory values and CFG joins remain outside this analysis.

Around the first uniquely anchored changed instruction, the JSON contains a
three-instruction window on either side and operand-path correspondences.
Different register numbers are reported explicitly; they are never renamed.
Equal definition patterns describe equal printed expressions, not equal runtime
values: their input definitions may differ. Every report sets `semantic_proof`
to false and every operand correspondence reports `input_values_proved_equal`
as false. Unknown origins and unclassified register occurrences retain separate
denominators.

## Controls and measured CPt evidence

The new suite has 22 control groups. It exercises successful definition/use
pairing, redefinition age, read-before-write, real header formats, clobbers,
subregister offsets, opaque effects, labels/loops/joins, calls, memory values,
auto-increments, absent metadata, mismatching registers and input values, and
unanchored instruction differences. All 22 pass normally and with `python3 -O`.
The existing 27 stage-analysis controls also pass in both modes, including the
read-once stream/cgraph race control.

The frozen pilot CPt baseline and adopted candidate were analyzed over 29
stage pairs and 58 RTL streams per function. For the candidate caller
`generateSymbol`, the `19.life` stream contains 1,139 instructions and 1,099
classified register uses: 194 have a supported local definition and 905 have
unknown origins. Another 327 register occurrences belong to unsupported
patterns. At `20.combine`, 1,017 instructions contain 977 classified uses,
128 supported local origins and 849 unknown origins; 327 occurrences remain
unclassified. These are diagnostic coverage counts, not recovered source.

A concrete conversion window demonstrates the use of the new evidence.
Before combine, UID3172 reads pseudo 670 defined by UID3164, retaining both
printed signed-word conversion boundaries. Combine removes UID3164 and changes
UID3172 to read a frame-relative halfword directly. The resulting memory value
is explicitly unknown to this analyzer. This locates an observed compiler
rewrite without claiming the original blob had these RTL UIDs or that the
frame home is itself recovered source.

The candidate standalone `generateCPt` retains two signed-word conversion
instructions in both `19.life` and `20.combine`; its 37/31 instruction streams
have 8/3 supported local origins. The caller has 47/35 signed-word conversion
instructions. Twelve removed caller conversion UIDs and sixteen changed ones
are recorded with captured dump hashes. This inventory is broader than CPt's
own inline copies and does not attribute every removed conversion to CPt.

## Allocation observation boundary

The existing pinned read-only observer was replayed without modification on
`beepgen_start_beep`. It records 10 target local choices out of 141 TU choices
and two target pass entries out of 46 TU pass entries. Observed/plain assembly
and object files remain raw-identical. An absent-target replay fails with
`target has no observed local choices`; all 10 allocation-validator control
groups pass normally and with `python3 -O`.

This replay records no global choices, evictions or reload decisions. The new
RTL analysis deliberately stops propagating at hard-register assignment:
`generateSymbol` has zero supported pseudo origins at `25.greg`, despite
1,013 classified explicit uses. A global/reload extension must pin the actual
selected compiler hook and distinguish assignment decisions, displacement,
spill creation and reload cleanup. It must observe a family whose residual
involves those decisions, retain explicit unsupported cases, and demonstrate
unchanged assembly/object output and a failing absent-target control. A late
register difference alone does not establish that extension's usefulness.

## Reproduction

Run from the checkout containing these tools:

```sh
python3 tools/gcc3_stage_defuse_test.py
python3 -O tools/gcc3_stage_defuse_test.py
python3 tools/gcc3_stage_divergence_validate.py --output build/stage-controls
python3 -O tools/gcc3_stage_divergence_validate.py --output build/stage-controls-O
python3 tools/gcc3_alloc_trace_test.py
python3 -O tools/gcc3_alloc_trace_test.py
```

For existing frozen dump directories, use the unchanged stage-comparison CLI:

```sh
python3 tools/gcc3_stage_divergence.py LEFT RIGHT \
  --function 'int V92Phase4Modulator::generateSymbol()' \
  --output build/cpt-symbol-defuse.json
```

Local wave artifacts are under `build/six-step-wave/` at the retained
`wave-defuse` evidence path: `cpt-{symbol,cpt}-defuse-reviewed.json`,
`cpt-conversion-stage-evidence.json`, `cpt-one-window.json`, stage control
reports and `allocator-{replay,absent}/`. Each parsed RTL dump is identified by
its captured-byte SHA-256. These files are local measurements, not distribution
dependencies or a fresh whole-project gate.
The worker checkout was removed after integration; the consolidated wave's
`worker-cleanup.json` maps this compatibility path to its retained evidence.
