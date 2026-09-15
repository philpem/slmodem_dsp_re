# V92BitsToSymbol: inline expansion and scheduling controls

Baseline: merged `11d23fd1`, Gentoo GCC 3.4.2-r2; see
`byte-fidelity-baseline.md`. No reconstruction source or flags were changed.
Artifacts and the finite experiment runner are in `build/v92-bits-experiment/`.

## Question

Nine of ten shared functions in `src/pump/v90/V92bitsToSymbol.cpp` are exact.
The remaining function, `setSymbolsBlockSize(unsigned)`, is 108 bytes in both
objects but differs in 48 bytes. Its initial argument/member register choices
differ, and its internal branch displacement differs. The reference has the
arithmetic in this function without an out-of-line helper call. That is
consistent with inlining, but does not distinguish inlining from explicit
source expansion.

The source axis compares the retained call to `nofBitsForNextTime()` with
an explicit copy of its arithmetic in the setter. Both preserve unsigned
32-bit arithmetic and the two distinct multiply/divide orderings; no rounding
formula was simplified. This is a mechanism control, not a recommendation to
duplicate source.

Each form was crossed with retained flags, `-fno-rename-registers`, and
`-fno-schedule-insns2`. These option controls distinguish register-renaming
and final scheduling effects; they are not candidate global profiles.

## Validity and results

The unchanged experimental object is byte-identical to the baseline object.
Commands derive the C and C++ flags from `build/tc_repro/.build-config` and
use the published Gentoo image explicitly. The shared experiment helper
appends `DSPLIB_REPRODUCE_BUGS` after all configurable flags. Executed compiler
and selected-assembler version output is saved in `identity.log`; each cell
retains its complete command, generated source, compiler log and object.

Six of six cells compiled successfully. Every cell retained the baseline's
defined symbol names, types, bindings, visibility and sizes. All ten shared
function bodies and canonical relocations were compared against the reference
and against the baseline.

| Source | Profile | Exact / shared | Setter differing bytes / 108 |
| --- | --- | ---: | ---: |
| retained helper call | retained | 9 / 10 | 48 |
| explicit expansion | retained | 9 / 10 | 48 |
| retained helper call | no register renaming | 2 / 10 | 50 |
| explicit expansion | no register renaming | 2 / 10 | 50 |
| retained helper call | no final scheduling | 0 / 10 | 78 |
| explicit expansion | no final scheduling | 0 / 10 | 78 |

Explicit expansion under retained flags reproduces the complete baseline
object byte for byte. Under each option control, its function results also
agree with the corresponding helper-call control. Disabling register renaming
changes eight bodies, losing seven previously exact members; disabling final
scheduling changes all ten, losing all nine exact members. Neither option
fixes the setter. These observed changes are positive controls proving that
the experiment detects changes rather than silently reusing the baseline.

## Decision

Adopt nothing. Explicit expansion gives no independent source discriminator,
and neither isolated option explains the target mismatch. The results do not
establish that the retained global profile is uniquely original; an untested
source/profile interaction remains possible.

No candidate reached partial-link or differential adoption gates. The
retained-source result is the identical object, while losing option controls
were stopped at full-TU comparison. The prior 375-pass period baseline remains
the latest gate; no new differential pass is claimed for these controls.

Move to other near-exact TUs rather than enumerate synonymous setter bodies.
Before returning here, identify a specific declaration/emission-order carrier
from the setter's initial pseudo-register lifetimes and test it with the
unchanged helper as a control. Do not reorder methods solely to obtain a hit:
reference method order and all nine exact members remain independent evidence.
