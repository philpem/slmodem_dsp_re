# Retained parallel-encoder fidelity correction

Baseline: `efa6524a`, the period-validated host-type correction in PR #97.
This report supersedes the isolated experiments' no-adoption / gates-not-run
status. It does not claim that the whole spectral shaper is now exact.

## Retained source

`ParallelDifferentialEncoder<T>::process` in `include/dsplib/DiffCoder.h`
now caches `state_` before its loop, advances state/input/output pointers,
computes one local XOR result, and stores output before state. The loop still
reads the member `size_` on every iteration, as the reference does.

These are observed memory accesses and induction variables, not an attribute,
barrier, aliasing promise or padding inserted to force a score. No compiler
flags or `V90SpectralShaper.cpp` statements changed. The method guide now
distinguishes this evidence from inventing a pointer just to lengthen a
particular register's lifetime.

## Experiments and scope

The source/pass matrix tested six loop structures with retained flags and
`-fno-strength-reduce`: twelve valid cells after rejecting the initial
include-overlay precedence error. The best source form reproduces the
reference's 54-byte helper, with four differing bytes in load/XOR order.
Disabling strength reduction does not improve it and changes other bodies.

Three subsequent staged-read controls all reproduce that same residual.
The tested read-order family is closed; no flag control or staged-read
variant was retained. See `spectral-encoder-experiment.md` and
`spectral-encoder-read-order.md` for the finite domains and invalid controls.

The original compiler's dependency output identified 18 header consumers.
All 18 unchanged-overlay objects matched their baseline objects byte for
byte. Under the candidate, 17 remained byte-identical; only the spectral
shaper object changed, solely in the encoder helper. Its specialization has
one defining source object, retains WEAK/default binding, and has no
relocations. All names, call/data targets and non-text payload are retained,
including the shaper's 532 bytes of named data tables.

## Applied-source validation

After the actual header edit, the source tree was rebuilt using Gentoo GCC
3.4.2-r2 and the published image explicitly for both compilation and linking:

```sh
make partial-link phase J=6 \
  TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/spectral-encoder-retained/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o \
  --json build/spectral-encoder-retained/partial.json
```

Period differential: **375 passed, zero failed**. All structural phase checks
passed. No modern portability run is claimed. The independently compared
applied-source TU matches the validated experimental candidate in all 15
shared function bodies and their canonical relocations.

| Metric | Baseline | Retained |
| --- | ---: | ---: |
| Exact functions / 1,852 | 830 | 830 |
| Exact function bytes / 720,125 | 80,030 | 80,030 |
| Encoder helper size, reference 54 | 60 | 54 |
| Encoder helper grade | SIZE(6) | BYTES(4) |
| Positioned reference bytes / 943,398 | 68,457 | 68,488 |
| Exact section descriptors | 67 | 68 |
| Ordered section-descriptor matches | 59 | 60 |
| Exact symbol records / 2,907 | 299 | 300 |
| Ordered symbol-record matches | 298 | 299 |
| Exact relocation records / 18,317 | 974 | 974 |
| Aggregate content-size deficit | 45,764 | 45,770 |

The independently rescored exact-name sets have **zero gains and zero losses**
among all 830 baseline members, not just the stored 810-member floor.
The helper moves from SIZE to BYTES: the global counts are 81 BYTES and
889 SIZE, with 48 REGALLOC and 4 UNRESOLVED unchanged. The whole-object
verdict remains **DIFFERENT**.

The aggregate size deficit grows by six bytes because the formerly oversized
helper now has its correct reference size. This is why total size proximity
is not an accuracy score. The 31-byte positional gain, corrected symbol and
section sizes, and remaining four-byte function mismatch are distinct facts.

Local applied-source logs and JSON are in `build/spectral-encoder-retained/`;
the header-scope evidence is in `build/spectral-encoder-scope/`. Correct
partial-link comparisons use all 275 inputs in recovered `tc_link_manifest`
order, not the compile manifest's source order. Invalid apparatus attempts
are excluded from the retained measurements.

## Remaining work

The helper's four-byte load/XOR difference remains open. The shaper TU still
has 10/15 exact shared functions. Its `reset`, `process`, `applyAction` and
`advanceTrellis` mismatches are not claimed solved, and previously exhausted
pointer-lifetime and loop-spelling searches were not repeated. Future work
needs a new instruction-level discriminator or shared source/profile evidence
under issue #22, rather than further nearby read-order spellings.
