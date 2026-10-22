# V.92 decision mapping-owner recovery

`V90Phase3Demodulator::getV92Decision(float)` state 16 now captures its mapping
row before companding, reacquires the detector owner after `isAltRbs`, and
retains the incoming decision as the inner branches' sign source. This is a
partial source reconstruction against `ref/slmodemd/dsplibs.o`; the function
remains non-exact. A new component-boundary probe covers both changed inner
branches with valid mapping indices. Reconstruction compiler flags, type
definitions, tolerances and existing fixture assertions remain unchanged.

The baseline is master `cc6377b2c6352923e4ad9f092cb905d0d3c4738a`.
All compilation uses GCC 3.4.2-r2 from
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, whose banner is
`3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)`.

## Original operands and source discrepancy

The reference method occupies `.text:0x21680..0x23828`. Its state-16 jump-table
entry is `0x21a8e`. The three mapping paths have distinct captures:

| Path | Original evidence |
| --- | --- |
| No alternate-RBS flag | `0x229b2` captures the phase before converter calls `0x229cc` / `0x22ec5`; `0x229d6` restores the detector owner saved after `generateSymbol`. |
| `isAltRbs` true | The call is at `0x230f5`; `0x23109` reacquires the detector owner and `0x23114` captures the phase before converter calls `0x2312a` / `0x2372b`. The alternate table is read at `0x2313e`. |
| `isAltRbs` false | `0x2354d` reacquires the detector owner and `0x2354f` captures the phase before converter calls `0x23571` / `0x236ca`. The ordinary table is read at `0x23585`. |

For the inner paths, `0x21ac0` saves the incoming level in the decision
carrier before the detector call. The sign operations use that carrier at
`0x23106` and `0x23553`. The outer path instead uses the incoming level at
`0x229af`.

The previous source retained one `adid` pointer across `isAltRbs`. It assigned
`code = P3D_CODE(...)` in a separate statement and evaluated the mapping macro
afterwards, reading the phase after the converter. Its inner sign expression
also referred to the separate `level` local. These choices erased original
owner and operand-capture boundaries.

The retained source uses a local `short *mapping` for the selected row before
companding. The inner rows come from the reacquired
`autoDigitalImpDetector`; the outer row comes from the previously captured
`adid`. The original widths remain unchanged: a 16-bit unsigned phase,
16-bit signed mapping element and 16-bit returned decision.

This does not uniquely recover the original declarations. The compiler can
implement a captured row using a combined row pointer or separate base and
phase values. The evidence establishes the selected owner, read timing and
sign source. No reachable production failure is claimed: the real
`isAltRbs` implementation reads its detector, and the real companding routines
do not modify the demodulator's phase or owner.

## Bounded source controls

The unchanged complete translation unit reproduces the archived Gentoo object
byte for byte. The following controls use the retained compiler flags and
change only state 16. Instruction counts and LCS exclude alignment padding.
LCS is the ordered common subsequence against the reference's 2,076
instructions; the longest common contiguous run is 64 in every cell.

| Source form | Bytes | Instructions | LCS | State-16 companding pairs |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 8,183 | 1,965 | 1,090 | 2 |
| Converter fused into table-index expression | 8,247 | 1,979 | 1,096 | 2 |
| Fused expression plus owner reacquisition | 8,360 | 2,009 | 1,072 | 3 |
| Separate owner, phase and sign captures | 8,313 | 1,997 | 1,080 | 3 |
| Separate owner/phase captures, sign evaluated later | 8,308 | 1,997 | 1,078 | 3 |
| Separate captures, inner sign from decision | 8,316 | 1,999 | 1,083 | 3 |
| Separate captures, sign retained in decision | 8,329 | 2,004 | 1,083 | 3 |
| Row capture, all signs from level | 8,112 | 1,941 | 1,061 | 3 |
| **Row capture, inner signs from decision; retained** | **8,291** | **1,996** | **1,112** | **3** |
| Reference | 8,616 | 2,076 | 2,076 | 3 |

The source controls test owner representation, capture timing and the sign
carrier witnessed above. Restoring a call count alone does not select a
candidate; several controls restore all three pairs while reducing structural
correspondence. The retained form restores the read boundaries and improves
whole-function LCS from 1,090 to 1,112, or 52.5% to 53.6%. The size gap falls
from 433 to 325 bytes. It remains a SIZE result.

Fresh baseline counting also corrects the earlier handover diagnosis of a
broad companding-call deficit. `getV90Decision` has 148 calls in both objects,
with 22 calls per companding law. `getV92Decision` has 156 reference calls
against 154 baseline calls, with 22 against 21 calls per law. All missing
companding sites belong to state 16. The retained form restores 156 calls and
22 calls per law; call counts in the other DIL states are unchanged.

## Differential boundary and validation

The unchanged and retained-source focused commands are:

```sh
make period T=t_v92dec J=4
```

The original fixture reports 128,584 arm-sweep checks, 84,657 diagnostic checks
and 216 completed-study boundary checks, totalling 213,457 checks. The new
state-16 probe adds 1,250 checks in the same binary, bringing its denominator
to 214,707. Both the unchanged source and the retained source are checked
with the expanded fixture; the unchanged source passes `1 passed, 0 failed`.
The retained source also passes `1 passed, 0 failed` with all 214,707 checks.

The existing fixture constructs or wires collaborators, invokes reset, then
plants dispatch states, counters and mapping tables. Its broad sweep is
**synthetic exploratory component-fidelity coverage**. The completed-study
probe already carries that label. Neither establishes a public modem history
reaching DIL state 16.

Within the state-16 sweep, 12 of the 26 first-pass inputs use reference codes
within 0..127; all phases are within 0..5 and all sample inputs are finite and
short-convertible. These inputs include ordinary mapping, a flagged phase
without a segment level, and a flagged phase with a segment level. Mapping
tables contain finite signed shorts, scrambler/descrambler cursors point into
their wired buffers, and both implementations use the same bounded
collaborator calls. This is a supported storage and input subset at the
explicitly synthetic state boundary, without a claim that the planted tables
result from a complete impairment study. Wider-code exploratory inputs are
not cited as valid per-row indexing evidence.

The new, explicitly synthetic state-16 probe crosses two PCM laws, six
phases, two sample signs and two detector outcomes: 48 trials. It uses
reference code 42, the real reset-produced reference level and TRN1d symbol
producer, `resetLinearMapping`, and `resetStudyUrefHandler(0)` to install the
real distance threshold of 50. `calculateLinearMeanAndVarAlt` followed by
`updateLinMappMeanAndVarAlt` produces an alternate level 200 above the ordinary
level. No threshold value is planted. A sample at the ordinary level selects
the ordinary map; a sample 1,000 above it selects the alternate map. All
values and indices stay within their declared domains.

The reference's real `isAltRbs` confirms 24 ordinary and 24 alternate
outcomes. The probe checks the reference's selected output level, compares
the reconstructed output against it, and compares all collaborator objects
and buffers after every call. State 16, its phase flag and the segment-level
gate remain planted, so this is a supported component-input boundary rather
than evidence of a full impairment-study history. A single wrong-table
negative control selects the ordinary row in the alternate branch: it fails
24 of the 1,250 new checks, plus six existing arm checks and eight diagnostic
checks. That control is removed before the final gate.

The four added producer calls use the reconstruction on both fixture sides
as input-construction apparatus. `resetLinearMapping`,
`calculateLinearMeanAndVarAlt` and `updateLinMappMeanAndVarAlt` are EXACT in
the current census. `resetStudyUrefHandler` remains non-exact, but the full
Gentoo gate's independent `t_v90adid` fixture compares it directly with the
reference in 523 checks, including the plain `qc == 0` path used here.
The accumulator/mapping methods also have direct reference comparisons in
the ADID fixtures. Thus this setup does not claim that every collaborator
was produced by reference code. The additional reference `isAltRbs` observer
at `.text:0x405b0..0x40641` has no calls or object writes, and restores its
temporary floating-point control word before returning; observing these
finite integer samples does not alter one side's object history.

The native-call census fires on the baseline's missing pair and on the
reference's three pairs. It is a structural diagnostic, not a detector of
runtime reachability or proof of whole-function equivalence. No mutation run
or mutation snapshot refresh is used.

## Complete translation-unit collateral

The nine source controls cover 234 body verdicts: 26 functions per cell. All
18 baseline EXACT names survive every control, with no exact gains. The
selected form changes only `getV92Decision`; the other 25 bodies, including
`getV90Decision`, retain their bytes and canonical relocation records.

Symbol interfaces, data/BSS contents, jump-table relocation counts/types and
the resolved diagnostic-literal consumer inventory of all 26 functions are
preserved. The selected cell also preserves the complete string section
byte for byte. Jump-table destinations naturally change with the target's
instruction layout. The retained-source period object matches the selected
source control at all 26 bodies and their canonical relocation records.

Reproduce the retained build and target measurements with:

```sh
make tc J=4
python3 tools/toolchain/structsim.py _ZN20V90Phase3Demodulator14getV92DecisionEf
python3 tools/toolchain/byteident.py --why _ZN20V90Phase3Demodulator14getV92DecisionEf
python3 tools/dis.py ref/slmodemd/dsplibs.o 0x230d4 0x23154
python3 tools/dis.py ref/slmodemd/dsplibs.o 0x23548 0x2358e
```

The complete Gentoo build emits 300 objects from 300 sources with no failures.
Comparing all 300 objects against the clean baseline changes only the
`V90Phase3Demodulator` translation unit. The whole-project census remains
1,143 EXACT, 35 REGALLOC, 84 BYTES and 590 SIZE across 1,852 symbols, with zero
RELOC, UNRESOLVED or NODATA results. The complete ordered `--list-exact`
output is byte-identical to the clean baseline: no exact names are gained or
lost.

The final `make phase J=4` exits 0: `period differential: 394 passed,
0 failed`, followed by `phase boundary: period differential and structural
checks all OK`. The binary count remains 394 because the new checks extend
the existing `t_v92dec` fixture. Its final full-gate run passes all 214,707
checks, including the new 1,250-check boundary.

For this investigation's Gentoo-only verification, scratch `gcc`/`g++`
wrappers dispatch the structural checks to
`/usr/i386-pc-linux-gnu/gcc-bin/3.4/` in the same Gentoo image. The phase
invocation sets the structural C++ flags to
`-Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h
-fno-exceptions -fno-rtti -DDSPLIB_REPRODUCE_BUGS`. The period source and
fixture compiler flags remain those supplied by `period_inner.sh`.
No host GCC or x86-64 portability result is claimed.
