# ADID grouping-state recovery and spill-context evidence

Baseline `22f4a724`, production1079/1852 EXACT. Full ADID translation unit,
current headers, Gentoo3.4.2-r2 and retained complete flags with mandatory
`DSPLIB_REPRODUCE_BUGS`. No optimization/profile changes.

## Result

`uniteLinMappInfoOfUnsuspectedPhases(unsigned char)` now reproduces all **601
original bytes**. The integrated production object equals the selected complete
experimental object raw. This is the combination of positive grouping/pooling
guards, initialization after clearing groups, floating pool zeros first,
double-half conversion directly at the signed short store, and grouping-state
declarations at the beginning of that phase. Arithmetic and D283/D284 remain
the original's. Identity does not uniquely recover the author's source spelling.

The guard-only candidate was independently supported by original shared joins
and initialization age; it reduced635B to601B/BYTES61. It passed period388/0
but structural validation caught14 detached/ambiguous static locators. Before
retaining that partial change, a new compiler trace enabled the complete match.
The final source retains the exact combined control. No temporary guard-only
source was committed.

## New discriminator, not a larger permutation search

The installed cc1plus observation shows13 target stack requests per compile:
one array allocation during expansion, three x87 word temporaries during
splitting, eight reload spills and a final padding request. `alter_reg` allocates
spilled pseudos in ascending number order. Closest form assigns pseudo61 `best`
to frame offset-28 and pseudo70 `count` to-32. These become stack-pointer homes
+0x24 and+0x20, opposite the original. Assignment order did not change this.

This supports a new finite source family: declare count/best/bestGroup together
after clearing the groups, at their phase boundary, with count initialized first
and bestGroup still uninitialized. The winner assigns pseudo70 `count` to-28,
pseudo71 `best` to-32 and pseudo72 `bestGroup` to-36. Thus the first two homes
become original+0x24/+0x20. They are reload spills, not stack slots reserved for
those source declarations at function entry.

Five complete-TU cells: raw baseline, guard recovery, guard+phase state, closest,
closest+phase state. Only the last gains exactness:16→17/34 exact, no losses,
all33 bystanders and metadata/named data/allocated nontext unchanged. This
reframes F11919 using new allocation evidence, not an unbounded declaration
ordering search. Grouping-state boundary is an idiomatic candidate, not a claim
of unique original declaration order or original GCC RTL.

## Other-function influence control

Four saved/preprocessed cells: raw baseline, guard recovery, closest, closest
with intact exact `updateUref` definition moved past the target. The context
control is diagnostic only. Relative to closest, target body and spill requests
are identical. Relative to baseline, five bodies differ, of which one is the
already-changed target; four are collateral raw changes. Symbol metadata and
named data remain unchanged; relocation-cleared unnamed allocated constants
change. Masking relocation words and comparing their canonical targets makes
every body identical between closest and closest-context: the five raw differences
are relocation addends, not changed instructions or register history. All
four cells remain16/34 exact with no gains/losses. No context reordering was
adopted. One unaffected target does not refute cross-function influence generally.

Nine source controls total:306 common body verdicts. Four plus five target
observations and five exact `updateUref` positive observations require identical
saved/plain/observed assembly and complete objects. GDB only reads state; no
inferior calls, state writes or register constraints. Missing optimized caller
variables are not inferred. Positive audit maps named initial-RTL pseudos to
returned reload slots and verifies the changed best/count order and full match.

## Replay

```sh
python3 tools/gcc3_adid_spill_context.py --domain docs/history/pcm/adid-spill-context-domain.md
python3 tools/gcc3_adid_group_state.py --domain docs/history/pcm/adid-spill-context-domain.md
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-adid-spill-context --target uniteLinMappInfoOfUnsuspectedPhases --output-dir build/gcc3-adid-spill-observation
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-adid-group-state --target uniteLinMappInfoOfUnsuspectedPhases --output-dir build/gcc3-adid-group-state-observation
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-adid-group-state --target updateUref --generic-trace --output-dir build/gcc3-adid-spill-positive
python3 tools/gcc3_adid_spill_audit.py
```

Runners retain source/preprocessor/compiler commands, headers/object hashes,
initial/combine/reload dumps, body verdicts and changed-body disassembly. Observer
retains compiler hash, commands, allocation/caller/frame traces and raw output
proof. The existing Uref observer keeps its historical strict checks by default;
generic tracing is explicitly selected for the new same-TU positive control.

## Integrated validation

`make tc J=8`:300 sources/300 objects/0 failures. Complete production ADID
object raw-identical to the exact control; data/metadata/bystanders unchanged.
Whole-tree canonical census1080/1852 EXACT,119108 original exact bytes:
**+1 function/+601 bytes, zero exact losses** against the1079 baseline.
Remainder672 SIZE/65 BYTES/30 REGALLOC/5 UNRESOLVED. Existing exact-set ratchet
passes. No measurement rules or tolerances changed.

Final `make phase J=8` exits0:388 period differential passed/0 failed,
all structural checks green. Static locators retargeted to the same operations
with original-method ownership verified;285 suites/10038 anchors, zero detached,
nonunique or wrong-arm anchors. No mutation execution or snapshot rerecording.
Refcheck14424 references/3018 finding headings clean. Python compilation,
observational audit14 runs/137 allocations and whitespace checks pass.
No modern portability claim, fuzzing, global profile adoption or forced registers.

Period partial-link before/after controls use the same300-input ordered manifest,
replacing only the ADID object with the pinned baseline for the before control.
The reconstructed `.text` shrinks701470→701436 bytes, consistent with the34-byte
target recovery. Both `partialcmp.py --require-exact` runs remain DIFFERENT
(exit1); whole-object completion is not claimed. Retained JSON:
`build/adid-partial-before.json` and `build/adid-partial-after.json`. Relocation
position changes in this whole-object census are not additional source gains.
The static ownership audit verifies29 grouping locators and three neighboring
Uref locators in their intended method bodies, not just global uniqueness.
