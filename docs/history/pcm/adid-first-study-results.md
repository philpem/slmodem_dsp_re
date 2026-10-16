# ADID first-study partial recovery and spill-screen widening

Baseline `9aa118e1`, production1080/1852 EXACT,119108 original exact bytes.
Fixed complete Gentoo profile and bug define. No fuzzing/mutation execution,
forced registers or profile changes.

## Same-graph spill screen

Refresh `residual_stage_inventory.py` from the current census:95 REGALLOC/BYTES
worst defining copies (65/30), with source/config/object identity and all defining
copies retained. Stack-erased register-consistent screening finds two SDM init
bodies. Their differing accesses only save/restore callee-saved registers and
do not form a consistent displacement bijection. No reload-slot nomination in
this scope. Four controls: actual ADID eight-slot residual fires; exact identity,
nonstack opcode change and inconsistent slot mapping are refused. SIZE and
different operation graphs are excluded, so this is not a source-exhaustion claim.
No SDM source domain is reopened.

## First-study source recovery

First-study repeats the already exact `updateLinMappMeanAndVar` helper but uses
`0.5f` rather than its `0.5`. Six complete-TU cells cross explicit float/double
half or helper reuse with retained/late counter declarations. Counter age is
raw-inert. Helper reuse and explicit double-half produce identical complete
objects, so machine code does not distinguish their original factoring.

The double-half form recovers the **24-instruction inner mean/variance update
block**, from original second count load through final control-word restoration.
The audit detects the exact instruction/operand block in the candidate and its
absence in retained source. The conversion, variance store and mapping store
now match; reciprocal/half setup is retained across the loop. Frame size also
returns from0x4c to original0x5c. This is specific operand/operation recovery,
not adoption because of a better aggregate size.

First-study remains different: baseline779B/BYTES445 becomes795B/SIZE16. The
opening diagnostic evaluations and clamps remain a separate residual. Candidate
is longer despite recovering the inner block and frame. Original literal spelling
is not uniquely proven from a load width; the retained improvement is the tested
double expression mode at the signed-short mapping consumer.

Five opening controls cross threshold2.5f/2.5 with ternary/explicit upper guard,
mean-double fixed. Threshold mode is raw-inert, but upper guard restores the
original jae/pop branch. Five further controls cross threshold and diagnostic
multiplication literal modes. The diagnostic DF multiplication restores the
opening evaluation sequence; initial RTL has SF/SF products for the retained
form and SF/DF when the diagnostic uses2.5. Both candidate families remain795B/
SIZE16, hiding different bodies. The audit's earlier raw-equality assertion
failed and was corrected before retention; equal scores never established erasure.

Two cells cross the independently recovered diagnostic multiplication and clamp
guard. Their combination reproduces the **first66 instruction/operand rows** of
the original, including diagnostic argument evaluations, clamps and loop setup.
Four selection controls then compare combined baseline, n inside the admitted
scan and nested publication arms. Late n declaration restores its original
initialization placement. Nested arms emit the same complete object as late n;
GCC still merges byte-zero/byte-one publication. No separate nested-source
factoring is inferred or adopted. Original later count/sum clearing order and
debug scratch colors remain different. These are explicit remaining boundaries.

Total **22 cells /748 common body verdicts**. Each TU stays17/34 exact, zero gains
or losses. Only first-study differs after relocation-aware body normalization;
eight other raw body differences are changed constant-pool addends. Symbol metadata
and named objects/relocations unchanged. Anonymous constants legitimately change:
`.rodata.cst4`168→164 bytes, `.rodata.cst8`24→32 bytes. Report these changes rather
than claiming all data unchanged. Helper/control-flow source reuse not retained;
retain the mean-half/diagnostic multiplication modes, explicit upper guard and
counter declaration at its first-use boundary. Each has original operation or
placement evidence. Their combined explicit form is the production candidate;
helper and duplicated-selection source factoring are not retained.

## Replay and integration

```sh
python3 tools/residual_stage_inventory.py --census build/byteident-adid-group-state.json --output build/residual-spill-inventory.json
python3 tools/residual_spill_screen.py --adid-baseline build/gcc3-adid-first-study/V90AutoDigitalImpDetector/baseline/candidate.o
python3 tools/gcc3_adid_first_study.py --domain docs/history/pcm/adid-first-study-domain.md
python3 tools/gcc3_adid_first_study_opening.py --domain docs/history/pcm/adid-first-study-domain.md --baseline-dir build/adid-first-study-baseline
python3 tools/gcc3_adid_first_study_literals.py --domain docs/history/pcm/adid-first-study-domain.md --baseline-dir build/adid-first-study-baseline
python3 tools/gcc3_adid_first_study_combined.py --domain docs/history/pcm/adid-first-study-domain.md --baseline-dir build/adid-first-study-baseline
python3 tools/gcc3_adid_first_study_selection.py --domain docs/history/pcm/adid-first-study-domain.md --baseline-dir build/adid-first-study-baseline
python3 tools/gcc3_adid_first_study_audit.py
```

Opening/literal matrices use isolated baseline-dir populated with the raw9aa
baseline object from the first matrix and production `.build-config`, while
integrated production is testing the partial improvement. All raw baselines
reproduce. Artifacts retain commands/hashes/initial/combine/reload dumps, complete
body verdicts and disassembly. Six static first-study locators retargeted for the
recovered expression/guard/declaration boundaries; same intended operations and
method, no mutation run
or snapshot rerecording.

The inventory command above is run on the pinned9aa baseline before integration;
its saved ledger is retained afterward. Screen replay explicitly overlays that
raw ADID baseline once production advances and checks every object's saved hash.
Do not regenerate the historical inventory from newer objects and an old census.

Final production object raw-identical to combined-count-inside control.
`make tc J=8`:300/300 sources/objects, zero failures. Final `make phase J=8`
exits0:388 period passed/0 failed, all structural checks green,285 suites/
10038 static anchors. Six retargeted locators verified inside the first-study
method; no mutation execution or snapshot rerecording. Initial mean-only phase
also passed388/0; final gate covers the combined retained source.

Whole-tree1080/1852 EXACT,119108 original exact bytes, zero gains/losses against
9aa. Remainder673 SIZE/64 BYTES/30 REGALLOC/5 UNRESOLVED: first-study moves from
BYTES to SIZE, a classification change rather than a scalar regression claim.
The canonical exact-set ratchet passes; refcheck14426 references/3022 headings
clean. Screen controls,22-cell/748-verdict audit, Python compilation and whitespace
checks pass. No modern portability claim.

Period strict partial-link before/after uses the same300-input manifest, replacing
only ADID with pinned9aa baseline for the before object. `.text`701436→701452,
consistent with the16-byte first-study increase. Both strict comparisons remain
DIFFERENT/exit1; no whole-object completion claim. Artifacts:
`build/adid-first-study-partial-before.json` and `build/adid-first-study-partial-after.json`.

Next discriminating evidence: trace where separate byte publication merges and
where clearing-store order changes in this complete TU, with an exact same-TU
control. Original compiler intermediates remain unavailable. No additional
selection/literal permutation is justified merely by the16-byte gap.
