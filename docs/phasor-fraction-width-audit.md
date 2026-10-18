# Phasor fraction width

The reference narrows the interpolation fraction to a signed word after both
`phase - (idx << 5)` and its odd-quadrant reflection, `32 - frac`. The
reconstruction now uses a `short` fraction in the shared split helper. This
recovers the reflection's sign extension in all three phasor functions. It
does not make any of them byte exact.

## Native evidence and scope

In `FPM_phasor_demod`, the reference first loads the phase with `movzwl`,
keeps a signed view for the index and phase advance, and subtracts the shifted
index from the original value. At 0xa9513 it sign-extends the low word of that
difference. The odd-quadrant arm subtracts the fraction from 32 and repeats
the sign extension at 0xa9521. The resulting value feeds a full-width
interpolation multiply. `FPM_phasor` and `FPM_phasor_dp` contain the same two
narrowing boundaries.

This establishes the narrow operations. It does not uniquely distinguish a
`short` local from explicit casts on both assignments. The local represents
both boundaries coherently. The fraction's ordinary range makes the
narrowing behaviorally redundant, so differential tests cannot establish its
source type.

## Crossed controls

The baseline source is pinned at `a0c079f61`. The replay uses the frozen
object tree's complete `.build-config`, Gentoo GCC 3.4.2-r2, and the shared
experiment-toolchain helper's final `DSPLIB_REPRODUCE_BUGS` define. It requires
literal reproduction of the unchanged full translation-unit object before
interpreting any candidate. It saves commands, source and object hashes,
compiler dumps, all three symbol verdicts, and nontext/export comparisons.

The declared cross varies the split helper's phase carrier (`int` or
`unsigned short`, retaining a signed index view) and fraction (`int` or
`short`). A fifth cell tests a short phase formal, index and fraction as an
RTL mechanism control. These are diagnostic controls; the unsigned-phase,
int-fraction cell can change negative-phase behavior and is not a candidate
for retention.

| Cell | `FPM_phasor` | `FPM_phasor_dp` | `FPM_phasor_demod` |
| --- | ---: | ---: | ---: |
| Reference | 211 | 246 | 161 |
| Baseline | 199 | 233 | 142 |
| Short fraction, retained | 200 | 234 | 143 |
| Unsigned phase | 204 | 236 | 147 |
| Both changes | 200 | 232 | 143 |
| HI-mode mechanism control | 202 | 235 | 143 |

All entries are function sizes in bytes. Every function remains `SIZE`;
these are length gaps, not counts of differing bytes. The retained gaps are
11, 12 and 18 bytes respectively. There are zero exact gains or losses, no
new or missing symbols, and no changed exported bindings. All three bodies
change; there are no other function bodies in this translation unit.
Nontext section contents, types, flags and sizes remain identical.

The retained source adds both narrow operations in the initial RTL. The
first is folded into `phase & 31` at `20.combine`; the reflected subtraction
keeps its sign extension and recovers the reference's instruction node.
The baseline's initial subtraction is also folded at `20.combine`. Thus the
remaining initial-subtraction difference is a compiler-context question,
not evidence that another nearby spelling will recover it. The bounded
line is closed until new evidence explains that transformation.

## Differential validation and replay

`make period T='t_fpm_phasor t_fpm_phasordp' J=8` passed both binaries with
zero failures. The existing fixtures report 3,342,336 exhaustive phasor
checks and 4,456,448 exhaustive demodulator checks. The double-precision
fixture reports 1,005,941 differential trials, including fractional carry,
accumulator wrap and fed-back trajectories. Their existing component-table
write and out-of-domain labels remain applicable. No mutation anchors or
test inputs changed.

Replay the native comparison against an unchanged frozen baseline:

```sh
python3 tools/recovery_phasor_fraction_width.py \
  --objects build/copy293-baseline --out /tmp/phasor-fraction-width
```

The replay does not run differential tests or prove whole-object identity.
Integration still requires `make phase` and the partial-link comparison.
