# AGC quiet/gain arm ownership

Pin45f2a5d6, retained Gentoo GCC3.4.2-r2 profile and mandatory
DSPLIB_REPRODUCE_BUGS. [Domain](agc-arm-ownership-domain.md) distinguishes this
shared-control-boundary witness from F11962–F11964's closed local-type family.

## Original witness and selected source

Original quiet clear and gain saturation exits meet at0xa6852: sample advance,
signed outer decrement and repeat test. Both gate comparisons precede quiet
clear; the gain arm follows the sole epilogue. This is observable original CFG
and operand evidence, not a demand for a smaller function.

Cross explicit quiet-if/gain-else with one common sample advance, against the
previously observed unsigned-word countdown/separate-pointer loops. Four
complete-TU cells preserve all four bodies, data, exports and call boundaries.
Only AGC changes in each alternative; the three exact bystanders remain exact.

| Cell | SIZE gap | Initial CFG blocks/edges | bbro blocks/edges | STC duplicated blocks |
| --- | ---: | --- | --- | ---: |
| baseline | 3 | 25/39 | 25/38 | 1 |
| explicit arms | 42 | 24/38 | 27/41 | 3 |
| pointer countdowns | 44 | 25/39 | 25/38 | 1 |
| both, selected | 22 | 24/38 | 25/38 | 1 |

Initial RTL has two sample-advance assignments in baseline/pointer, one in
arms/both. The observed source difference precedes allocation. Indexed arms
later acquire three STC block copies (21,4,22); pointer arms acquire only the
loop-header copy4. Dumps establish these saved transformations, not unavailable
original RTL, a complete reconstruction of probability heuristics, or identical
block-number meanings across cells.

Only the combined cell restores all selected categories: two word gates before
quiet clear, quiet loop before return before gain loop, independent sample
pointer and unsigned-word countdown, and the gain exit targeting the quiet
loop's common advance block. Arm-only lays quiet code after the epilogue;
pointer-only leaves the second gate after that epilogue. These known negative
controls prevent accepting a detector that simply reports the expected shape
for every cell.

Adopt the combined source on this original graph/operand evidence, even though
its size gap grows3→22. The whole function still differs: incoming sample
pointer spills, cached mult/level zero-test widths, smoothing arithmetic
scheduling, explicit shift-mask setup, saturation-loop entry/exit details,
prologue and epilogue allocation. Original178 instructions versus selected184;
no exact or register-only claim. The restored categories do not establish a
unique authorial spelling. The bitmask remains, full-width alpha intermediate
remains, shared types/return and fourth argument remain unchanged.

## Replay

```
python3 tools/gcc3_agc_arm_ownership.py --domain docs/agc-arm-ownership-domain.md --historical-headers --baseline-dir build/agc-arm-before-objects
python3 tools/gcc3_agc_arm_audit.py
```

Archive the current AGC object under src_dsp_fpm_agc.c.o and .build-config before
edits. For integration archive all300 canonical objects, .build-config and the
faithful partial object too. Drivers pin sources/headers, preserve commands,
compiler/selected assembler identity, all RTL stages, assembly and hashes.
Raw baseline and selected production object match. Audit4 complete-TU cells/
16 shared-body verdicts; metadata, named/allocated data, bystanders and calls
are unchanged. Graph detector fires on original/both and refuses baseline,
pointer-only and indexed-arm layouts. This is apparatus, not a second byte
identity comparator.

Existing initialized AGC tests cover quiet/acquire/squelch/freeze/release/gain/
saturation/sample partition and explicit return, without changing the fixture.
All comparisons stay hard; no new synthetic-history, fuzz, mutation, tolerance
or portability claim. Integration measurements are recorded in F11966.

The finite common-advance cross is closed. A future gain-loop control needs a
new discriminator, such as an observed value-use/narrowing boundary tied to a
specific graph segment. Do not repeat old type, return or declaration-order
palettes because a combined score is promising. The current AND31 setup versus
original byte-load shift is a distinct unresolved witness; valid shipped shifts
and any original undefined shift semantics must be separated before a source
change, rather than silently deleting the mask to fit code.

## Integration result

make phase J=8 exits0:388 passed/0 failed; structural285 suites/10038 static
anchors green. Unchanged initialized AGC fixture10 groups/51643 hard checks,
476 returned-value trials34 zero/442 one. V23 valid debug lifecycle22 groups/
4656926 checks pass. One/300 objects changes,299 and configuration raw-identical;
all300 faithful inputs raw-match canonical. Census1082/1852 EXACT/119759 original
exact bytes unchanged,669 SIZE/65 BYTES/31 REGALLOC/5 UNRESOLVED; ratchet green.
Partial .text702396→702412, allocated918404→918420, positioned equal
69557→69524/943398; both strict comparisons remain DIFFERENT/exit1. Record this
layout cost explicitly: recovery of supported structure did not improve byte
counts. No all-input behavior proof, whole-object or full gain-graph identity.
