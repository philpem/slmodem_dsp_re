# Reload-home recovery in two SIZE bodies

Baseline b6db3cfc, complete current translation units and headers, retained
Gentoo GCC 3.4.2-r2 profile and selected assembler. No production option change.
[Declared domain](size-spill-domain.md): six cells, 72 shared-body verdicts.
Both unchanged preparation objects reproduce the canonical objects raw.

| Function | Original homes | Baseline homes | Retained control |
| --- | --- | --- | --- |
| getConstellationsIndex | count=4, i=8 | count=8, i=4 | declare i before count |
| Psd::process | frames=18, pos=1c, frame=20 | frames=20, pos=18, frame=1c | declare frame/pos before division |

Offsets are hexadecimal from final ESP. Original definitions, comparisons,
increments and consumers establish the values, independently of candidate
scores. In the inlined Mapping caller, the same count/i recovery exchanges
10/14: original count=10, i=14. No extra read, arithmetic, call or branch is
introduced. Initialization remains at its previous executable boundary.

## Compiler observations

All six controls have saved/plain/observed assembly and complete objects equal
raw. Read-only observations record 42 target allocation requests (five per
Mapping cell, nine per PSD cell). The two separate unchanged preparation
observations add fourteen requests; eight observations /56 requests altogether.

Mapping's reload requests are ascending pseudo61,62,113. Baseline names61=count,
62=i; i-first names61=i,62=count. The layout displacement is12. Thus the first
two frame offsets-4,-8 become final homes8,4. Loop-scope leaves both names and
the complete object unchanged.

PSD baseline initially assigns frames65 from udiv result71. Reload spills the
division result, not named frames65. It allocates this58, quotient71, frame73,
pos76; the layout displacement44 gives quotient20/frame1c/pos18. Moving the
uninitialized state declarations before the division creates frame65,pos66,
quotient73, and that ascending reload order recovers frame20/pos1c/frames18.
Making frame/pos adjacent after the division changes pseudo numbering but
produces the baseline object raw. Adjacency alone is insufficient.

## Full-TU and interpretation limits

Live strict grades agree with the stored grades in all72 comparisons. Neither
target becomes exact: Mapping remains282B versus286B (SIZE4), PSD626B versus635B
(SIZE9). All exact bodies stay exact: Mapping4/11, PSD8/13 including weak
template copies. Mapping changes only its target and setV92CPpckFromParamsInfo,
which contains the inlined target; PSD changes only process. A checked stack
offset substitution reproduces each changed instruction stream completely.
All other registers, operations, branches and relocation targets are unchanged.
Symbol metadata, named data, nontext relocations and allocated data agree.

These are supported partial source recoveries, not exact-function gains or a
unique original spelling. Mapping's inner-search entry/block placement and
PSD's output-option graph remain independent differences. Stop declaration
permutations here. Next discriminate original zero-length/search-entry control
flow in Mapping, and original output-option branch ordering in PSD, using the
new home-correct baselines and period RTL before new source controls.

## Reproduction

From the worktree:

```
python3 tools/gcc3_size_spill_prepare.py --domain docs/size-spill-domain.md
python3 tools/gcc3_size_spill_lifetime.py --domain docs/size-spill-domain.md
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-size-spill-lifetime --family V90MappingParamsInt --target getConstellationsIndex --generic-trace --output-dir build/gcc3-size-spill-mapping-lifetime-observation
python3 tools/gcc3_uref_stack_observe.py --reproduction-dir build/gcc3-size-spill-lifetime --family psd --target process --generic-trace --output-dir build/gcc3-size-spill-psd-lifetime-observation
python3 tools/gcc3_size_spill_audit.py --integrated
```

Preparation observations use the same commands with reproduction directory
build/gcc3-size-spill-prepare and output directories ending in-observation
instead of-lifetime-observation. Audit reads the six control observations,
rechecks actual objects and RTL, and validates chosen sources/canonical objects.
Artifacts include commands, toolchain/config/header/source/object hashes,
RTL streams, observer JSON and build/gcc3-size-spill-lifetime/audit.json.

Final integration includes the independently witnessed
[PSD switch](psd-dispatch-domain.md). Run the spill integration audit with
--integrated --psd-dispatch for that combination. Canonical sources/objects
equal their selected experimental controls raw. Census remains1081/1852,
119682 original exact bytes, zero losses. Final make phase J=8 exits0:
388 period passed/0 failed; all structural checks green,285 suites/10038 static
anchors attached/unique/correct-arm. Earlier red gates are preserved: one
declaration locator and four if-chain locators needed static retargeting;
period388/0 passed in both. No mutation/fuzz execution or profile change.
