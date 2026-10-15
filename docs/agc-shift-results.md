# AGC shift-count load and source semantics

Pin1ffa3fb9, fixed Gentoo GCC3.4.2-r2 profile/mandatory bug define.
[Domain](agc-shift-domain.md). No reopening closed local-type, return or arm
palettes: original stack MOVZBL before SHL CL is a distinct value-use witness.

## Range and limits

FPM_rms is bounded by its saturated square-root result32703. With initialized
level in0..32703, nonnegative alpha/beta summing to32768, monotonicity bounds
both smoother inputs by the maximum corner.32769 coefficient corner checks
cover that mathematical superset, not all runtime histories. Full-width alpha
and separately floored beta terms still have sum<=32703. Nonzero positive
smoothed shorts give FPM_div norm1..15, hence shift0..14. Exhaustively enumerate
65535 nonzero unsigned denominators; top-bit denominators give norm0/shift-1.
The existing divider differential already covers65536 inputs, including zero.
Zero-level AGC skips gain update; freeze retains a previous count. A legitimate
initialized, retained unity-pair history has bounded counts, but arbitrary
config or planted frozen state does not acquire that proof.

D6's initial config-relocation census cannot prove no caller ever selects the
second coefficient. Live V32 and V17/V27/V29 source does select correct unity
slow pairs. D85's claim of live broken B103/V23 pairs contradicts D6 and is not
supported by those initial relocations; no broken-pair lifecycle is proved here.
Both entries now identify that reachability limit and D85's coefficient typo.
The AGC comment's universal shipped-config claim is replaced by the conditional
bound. No change to coefficient data or to the original defect.

Hardware count proof covers65536 signed-word values: direct low5 masking equals
low8 load followed by low5 masking. A wrong mask15 differs32768 times, so the
control fires. This is a finite count identity, not evaluation of undefined C
shifts, proof of all AGC input/state behavior, or modem reachability. C requires
count0..31; negative and>=32 counts remain original period machine behavior.

## Compiler trace and adoption

| Full-TU cell | SIZE gap | Explicit AND31 | Stack shift MOVZBL |
| --- | ---: | --- | --- |
| baseline | 22 | yes | no |
| raw count, selected | 5 | no | yes |
| explicit byte cast | 20 | no | no |
| byte cast + mask | 22 | yes | no |

All4 cells retain three exact bystanders; no exact gains/losses. Original178
instructions versus baseline184 and selected180; SIZE5 remains substantive
prologue/register/arithmetic differences, not a near-exact or register-only claim. Only raw count
restores the original stack byte-load use. Byte cast creates a new temporary
and the compiler retains/hoists it differently; it is a useful negative control.
The raw initial RTL uses a QI subreg of cached SI shift, not a newly declared
8-bit value. Global reload inserts a memQI-to-CX count load; that remains QI
through postreload, peephole2, renaming and sched2. The final assembly prints
MOVZBL. The width is selected by the consuming instruction/reload/output path,
not proof of an original unsigned-char local.

GCC3.4.2's [i386.md movqi_1](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md)
selects a zero-extending move for the memory-to-Q-register alternative when
TARGET_MOVX is enabled. [i386.h](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.h)
derives that option from the tune mask; [i386.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c)
includes PentiumPro in x86_movx, consistent with retained mtune=i686. This
upstream source explains the mechanism; actual Gentoo dumps/assembly are the
measurement, not a claim that stock GCC is the deciding toolchain.

Adopt unsigned-value shift with raw cached count. Original load/use evidence
supports the spelling independently of its smaller size. Keep arithmetic,
saturation, types, fourth argument and arms unchanged. Initialized valid-count
behavior is defined; nonstandard stored counts retain original period SHL CL
masking, without claiming portable C semantics. This is reconstruction of the
period source/target, not a hardening or modern portability change.

## Replay/audit

```
python3 tools/gcc3_agc_shift.py --domain docs/agc-shift-domain.md --historical-headers --baseline-dir build/agc-shift-before-objects
python3 tools/gcc3_agc_shift_audit.py
```

Archive AGC object/.build-config before edits; archive all300 canonical objects
and faithful partial object for integration. Driver pins source/headers and
stores complete commands, selected compiler/assembler identity, RTL/assembly,
object hashes and results.json. Four full-TU cells/16 shared-body verdicts:
raw baseline/selected production agree, metadata/named/allocated data/calls and
three bystanders unchanged. Known raw/byte and mask/no-mask controls fire.
No extra fixture or fuzz/mutation harness. Existing valid initialized AGC and
V23 lifecycle tests remain hard, including samples, state and explicit return.

Initial phase passed388/0 period but structural refs failed on two findings
not yet written; that apparatus failure is preserved in agc-shift-phase.log,
then repaired with the findings and a final full gate. Integration result is
F11969. No count-family permutations absent another independent witness.

## Integration

Final make phase J=8 exits0:388 passed/0 failed; structural285 suites/10038
static anchors green. Unchanged initialized AGC10 groups/51643 hard checks,
476 return trials34 zero/442 one; V23 lifecycle22 groups/4656926 checks pass.
One/300 objects changes,299/config raw-identical, all300 faithful inputs
raw-match canonical. Census1082/1852 EXACT/119759 original exact bytes unchanged,
669 SIZE/65 BYTES/31 REGALLOC/5 UNRESOLVED; ratchet green. Partial .text
702412→702396, allocated918420→918404, positioned equal69524→69567/943398;
both strict comparisons remain DIFFERENT/exit1. Source load/use recovery and
these measured layout gains do not establish whole-function identity or portable
behavior for invalid C shift counts.
