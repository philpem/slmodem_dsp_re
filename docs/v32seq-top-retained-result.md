# Retained V.32 sequence-generator intermediate

Baseline: `a09a9251`, the scrambler-constructor milestone in PR #98.
This result retains only the V.32 change. The separately measured
V90Parameters floating-absolute-value candidate is not applied.

## Recovered source property

`InitGenSequence` previously narrowed `total / width - 1` into an
`unsigned short` local before writing two unsigned-short fields. The reference
keeps that computed value full-width and narrows only at the stores. The extra
`movzwl %ax,%eax` in the reconstruction was exactly its three-byte size gap.

Both full-width controls recover the reference body. Plain `int` is retained:
the unsigned-short operands undergo integer promotion, so this is already the
expression's type. The unsigned-int control gives the same object but does
not establish an unsigned source declaration. Neither destination field is
retyped. When `total < width`, the promoted result is `-1` and both stores
still write `0xffff`. D401 division-by-zero and D402 shift behavior remain
unchanged.

See `v32seq-top-experiment.md` for the complete three-cell domain. It uses
the published Gentoo GCC 3.4.2-r2 image, its selected assembler, unchanged
retained flags, shared experiment helpers and `DSPLIB_REPRODUCE_BUGS` last.
The unchanged source bind-overlay reproduced the baseline object byte for
byte. Both wide controls produce identical objects and improve 9/13 shared
functions to 10/13 exact. Only `InitGenSequence` changes; data, bindings,
visibility and all owner-object relocation records remain unchanged. The
only symbol-record delta is its size, 71 to 68.

## Applied-source validation

Two mutation anchors were updated to the new declaration while preserving
their faults: omitted decrement and reversed division. No mutation labels,
equivalence status, coverage denominators or tests were removed.

```sh
make partial-link phase J=6 \
  TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/v32seq-top-retained/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o \
  --json build/v32seq-top-retained/partial.json
```

Period differential: **375 passed, 0 failed**. All structural phase checks
passed. Mutation-anchor validation covered **272 suites, 10,041 mutations,
zero skipped**, with zero non-unique anchors. This does not claim a fresh
runtime mutation sweep or modern portability run.

An independent comparison of the applied object with the validated `int`
candidate found all 13 function bodies and canonical relocation maps equal.
Final artifacts are in `build/v32seq-top-retained/`.

## Whole-object result

| Metric | Before | Retained |
| --- | ---: | ---: |
| Exact functions / 1,852 | 831 | 832 |
| Exact function bytes / 720,125 | 80,137 | 80,205 |
| InitGenSequence size, reference 68 | 71 | 68 |
| InitGenSequence verdict | SIZE(3) | EXACT |
| Positioned reference bytes / 943,398 | 68,594 | 68,596 |
| Exact section descriptors | 69 | 69 |
| Exact relocation records / 18,317 | 976 | 976 |
| Exact symbol records / 2,907 | 301 | 301 |
| Aggregate content-size deficit | 45,777 | 45,777 |

The sole gained name is `InitGenSequence`; all 831 previous exact names are
preserved. The full 275-input recovered-order partial link gains two positioned
bytes without changing total content size or downstream layout. The whole
object remains **DIFFERENT**.

## Parameters experiment: measured, not retained

`V90Parameters::loadModemParamsData` has a distinct instruction discriminator:
the reference takes floating absolute value before converting its printed
whole part to integer, while the retained source converts first and then takes
integer absolute value. The unsigned input, divided by five and multiplied by
0.5, is finite, nonnegative and within signed-int range, so these operations
agree over the full reachable domain. Existing tests cover upper-half and
maximum unsigned inputs and compare both objects and diagnostic transcripts.

The initial matrix failed to remove the old integer-absolute-value step.
Its member-input variant also read the member before assigning the current
value. Those results are explicitly superseded, with invalid candidates
excluded from any source-retention claim. Corrected two-cell bind-overlay
controls are in `params-abs-replacement.md`; their unchanged object raw-matches
the baseline.

The corrected replacement recovers the reference's 344-byte size from 332,
but remains `BYTES(159)`. It changes no other shared body and preserves the
TU's 7/9 exact names, data and relocation targets. Its full-object effect is
not hidden: alignment grows `.text` by 16 bytes, positioned matches against
the blob fall from 68,594 to 68,241, and exact relocation records fall from
976 to 967. These are layout changes, not 500 KB of changed function bodies.

`v32-params-scope.md` records V.32-only, Parameters-only and combined controls,
each linked from all 275 inputs. The combined candidate preserves the exact
function set but does not resolve Parameters' residual. This milestone keeps
that candidate unapplied rather than bundling its unresolved instruction and
layout effects with the exact V.32 recovery. Non-monotonic results remain
useful evidence, not a reason to choose unsupported store permutations.

## Next discriminator

The Parameters reference materializes the fractional argument from the signed
value, then stores the floating-absolute whole value directly into its outgoing
argument slot, and finally reloads the member for the sign test. The valid
candidate's precomputed locals allow different x87 lifetimes and forwarding.

The next bounded hypothesis is direct `edprintf` argument expressions versus
precomputed locals, both using the true floating-absolute replacement and the
original member assignment position. Keep a separate unchanged-path control,
preserve the fractional integer absolute, and compare the full TU again.
Right-to-left materialization is a compiler/source hypothesis, not a C++
language guarantee. No more member-fabs or store-position permutations are
justified by the current evidence.

The remaining V.32 misses are `CodeESeq`, `GenSequence` and `DetSequence`.
Their larger body/register differences were not rewritten in this batch.
The method guide now records the exact late-narrowing example.
