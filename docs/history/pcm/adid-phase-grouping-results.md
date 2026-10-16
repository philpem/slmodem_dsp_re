# ADID phase grouping: operation graph recovered, spill homes remain

Baseline `860732a2`, production 1079/1852 EXACT and 118507 original exact
bytes. Complete `V90AutoDigitalImpDetector.cpp` and current headers, retained
Gentoo GCC 3.4.2-r2 profile with `DSPLIB_REPRODUCE_BUGS`. Every raw baseline
reproduces the retained object. Initial RTL uses `-dr`: the established ADID
workaround for the compiler's `-da` ICE. No profile options were varied.

Target `uniteLinMappInfoOfUnsuspectedPhases(unsigned char)` is original
601 bytes at `0x41550`, retained 635 bytes. Original retains the group-clear,
five-leader grouping, inner phase scan, pooling and publication loops. The
earlier nine-versus-eleven aggregate backedge nomination does not identify
missing expansion. Original initialization and accepted/rejected path joins
instead supply a bounded source-graph cross.

## Complete-TU controls

Each cell compares 34 common bodies, with 16 EXACT. All 22 cells compile;
none gains or loses an exact body. Only the target changes. All 33 bystanders,
symbol type/binding/visibility, named data including canonical relocations,
allocated nontext sizes and relocation-cleared allocated data are unchanged.

| Domain | Cells | Target results |
| --- | ---: | --- |
| Late best/count definitions × positive grouping guard × positive pooling guard | 8 | Baseline SIZE34; pool guard alone SIZE4; both guards BYTES147; both guards plus late definitions BYTES61. Other cells SIZE34/38. |
| Double-half expression with baseline/early-guard/late-guard forms, plus raw baseline | 4 | SIZE34 baseline; SIZE18 double baseline; both guards BYTES145 early or BYTES59 late. |
| Unsigned carrier after signed conversion or conversion at short store × float/double half, plus raw baseline | 5 | Unsigned carrier BYTES61/59; direct store BYTES36/10. |
| best/count first-definition order × floating/total zero definition order, plus raw baseline | 5 | BYTES10/8 with count first; BYTES19/17 with best first. |

Total: **22 cells / 748 common body verdicts**, zero exact gains/losses.
BYTES counts are literal relocation-aware byte differences, not instruction
counts. Both positive guards recover the original 601-byte length but are not
enough to recover identity.

The unsigned carrier changes the signed word promotion to a zero-extension:
one opcode byte differs from its corresponding signed-carrier control. Equal
original-relative BYTES counts did not mean those controls were raw-inert.
The first audit deliberately checked raw equality, rejected that hypothesis,
and was corrected to check the measured one-byte difference.

## Closest control and evidence boundary

The closest control uses both positive guards, initializes count/best after
group clearing, converts `mean + 0.5` directly at the short mapping store,
and defines the floating pool zeros before total. It remains **601B/BYTES8**.
The audit verifies equal complete instruction lists except exactly eight
operands where `0x20(%esp)` and `0x24(%esp)` are exchanged.

Original count is initialized at `0x41586`, stored at `0x4159a` to `+0x24`;
best is zeroed at `0x41590`, stored at `0x415a0` to `+0x20`. The closest
control assigns the opposite stack homes. Its rounded-value reload and all
x87 operations, guards, jumps, registers and pool-zero instruction placement
now match. Merely swapping first definitions makes additional differences;
it does not restore those homes. Declarations did not move in these controls.

Initial RTL of the late-definition guard control defines count as pseudo71
(insn54), then best as pseudo61 (insn58). That is a measurement of our compile,
not evidence of the original RTL or original declaration order. The remaining
stack displacements do not uniquely recover an author spelling. No declaration
permutation or forced-register search follows this bounded miss.

All controls preserve threshold-before-distance, ordered-not-below `>=` skip,
cumulative count bug D283, uninitialized bestGroup D284, per-add short count
truncation, empty-pool return and output publication/clearing operations.
Direct conversion at a guarded store can differ at the source floating-exception
boundary; its emitted conversion occurs before the publication loop, as in the
original. This is a compiler control, not an adopted general equivalence claim.

**No production source changed.** Production remains 1079/1852 EXACT. The prior
source gate remains `make phase` 388 passed / 0 failed; no new runtime gate,
fuzzing or mutation execution was required for these unadopted controls.

## Replay and next discriminating evidence

From this branch's worktree, with the baseline production object and toolchain:

```sh
python3 tools/gcc3_adid_phase_grouping.py --domain docs/history/pcm/adid-phase-grouping-domain.md
python3 tools/gcc3_adid_phase_grouping_half.py --domain docs/history/pcm/adid-phase-grouping-domain.md
python3 tools/gcc3_adid_phase_grouping_consumer.py --domain docs/history/pcm/adid-phase-grouping-domain.md
python3 tools/gcc3_adid_phase_definition_age.py --domain docs/history/pcm/adid-phase-grouping-domain.md
python3 tools/gcc3_adid_phase_grouping_audit.py
```

Each runner retains complete commands, source/header/object hashes, raw baseline
proof, initial RTL, verdicts and changed-body disassembly under its named
`build/gcc3-adid-*` directory. Audit writes `build/adid-phase-grouping-audit.json`.
Its positive control detects all eight remaining stack operands.

Reopening this function needs new compiler evidence: observe which stage assigns
the two pseudos' stack homes and whether a measured source lifetime/ownership
difference changes that stage. Compare with an exact same-TU control. That is
a trace of our compiler, not access to the original compiler state. Without
such a witness, move to another operation-graph nomination rather than trying
larger declaration permutations or adopting the closest-size source.
