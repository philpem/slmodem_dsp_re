# Retained parallel-decoder fidelity correction

Baseline: `184cb4ab`, the encoder milestone in PR #98. This follow-up restores
the decoder's independently observed memory-access structure; it does not
claim an exact decoder or an exact translation unit.

## Source and evidence

`ParallelDifferentialDecoder<T>::process` now caches `state_` before the loop
and advances input, output and state pointers. It saves the input byte before
writing decoded output, then stores that saved byte to state. The loop still
reloads the member `size_`, preserving the reference's loop bound. No compiler
flags, aliasing assertions, barriers or helper attributes were added.

The three retained-profile controls were unchanged source, cached state with
indexing, and three advancing cursors. They emit 62, 90 and 58 bytes against
the reference's 57. The retained cursor form matches the reference's observed
access structure and changes only this helper, not its caller or other TU
bodies. Size proximity alone was not the retention criterion.

The remaining instruction difference is explicit:

```asm
# reference
mov %dl,%al
xor (%ecx),%al
# retained, one additional byte
movzbl (%ecx),%eax
xor %dl,%al
```

Branch displacements consequently differ too. The helper remains `SIZE(1)`;
this is a one-byte length gap, not one differing byte. No further nearby
load/XOR spelling search is proposed without a new discriminator.

## Experiment validity and scope

The initial worker run used the wrong workspace and a custom instruction
scorer; it is explicitly INVALID, preserved in `parallel-decoder-experiment.md`
and its build artifacts, and excluded from all retained measurements.
The corrected run uses this worktree, shared experiment-toolchain helpers,
`byteident.py` canonical bodies/relocations, and `partialcmp.py` actual contents
and symbol records. See `parallel-decoder-valid.md` and
`parallel-decoder-scope.md`. The initial valid-run prose incorrectly called
symbol records unchanged; its table was correct, and the prose is corrected.

Fresh Gentoo dependency discovery identified 18 header consumers. All 18
unchanged controls reproduced their baseline objects byte for byte. Seventeen
candidate objects remain byte-identical. Only the decoder helper in
`V90SignBitsExtractor.cpp` changes. Its WEAK/default symbol retains name,
type, binding, visibility, section and value; only its size changes, 62 to 58.
All allocated non-text contents remain identical across all 18 consumers.

The published Gentoo GCC 3.4.2-r2 image is used explicitly, with compiler path
`/usr/i386-pc-linux-gnu/gcc-bin/3.4`, its native image user, and the selected
assembler version recorded. Retained flags match `.build-config`, overlay
includes precede the tree, and `DSPLIB_REPRODUCE_BUGS` is appended last.
The full link uses all 275 inputs in `tc_link_manifest.txt` recovered order.

## Applied-source validation

```sh
make partial-link phase J=6 \
  TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/parallel-decoder-retained/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o \
  --json build/parallel-decoder-retained/partial.json
```

Period differential: **375 passed, 0 failed**. Structural phase checks passed.
The independently compared applied-source object reproduces all 12 shared
function bodies and canonical relocations of the validated cursor candidate.
No modern portability run is claimed.

| Metric | Encoder milestone | Decoder follow-up |
| --- | ---: | ---: |
| Exact functions / 1,852 | 830 | 830 |
| Decoder helper size, reference 57 | 62 | 58 |
| Decoder helper verdict | SIZE(5) | SIZE(1) |
| Positioned reference bytes / 943,398 | 68,488 | 68,520 |
| Exact section descriptors | 68 | 68 |
| Ordered section descriptors | 60 | 60 |
| Exact symbol records / 2,907 | 300 | 300 |
| Ordered symbol records | 299 | 299 |
| Exact relocation records / 18,317 | 974 | 974 |
| Ordered relocation records | 966 | 966 |
| Aggregate content-size deficit | 45,770 | 45,774 |

All 830 baseline exact names are preserved, with zero gains and zero losses.
The positional gain is 32 bytes. The aggregate deficit grows by four because
the oversized helper was shortened; this is not a loss of matched content.
The full partial-link verdict remains **DIFFERENT**. Applied-source gate logs
and machine-readable results are in `build/parallel-decoder-retained/`.

## Remaining work and stopping decision

The SignBitsExtractor TU remains 8/12 exact. Besides the decoder residual,
its constructor variant, decoder reset helper and processing body remain
non-exact. Preserve the defined fallback for invalid states; deleting it to
imitate an undefined reference path is not an accuracy improvement.

The SpectralShaper review found no justified new local source matrix.
`docs/findings.md` records the already-tested switch-local control near
line 75046 and the mixed-precision trellis controls in F5854/F5855. The latter
include codegen-matching candidates that fail differential behavior; they stay
rejected. Reopening that line requires new declaration/return-expression
evidence from `V90SpectralShapingFilter::getMetric`, not repeated narrowing.

The method guide now records the decoder as an independent example of the
cached-pointer discriminator. Continue the TU queue with a fresh forced-
instruction or helper-boundary discriminator, rather than repeat exhausted
pointer-lifetime or commutative-expression families.
