# V.90 phase 4 demodulator type consolidation

`V90Phase4Demodulator.h` now owns the complete phase 4 demodulator class.
`V90SessionFlag.h` includes that definition instead of declaring a second,
shorter class. This resolves maintainer-guide finding MG-01 and removes the
last exception from `tools/onedef.py`.

## Layout and inclusion evidence

Finding F7584 records the earlier barrier: including both headers in one
translation unit caused a class redefinition. The partial definition was kept
because that investigation covered one function; consolidation was deferred
for a separate change with a wider validation boundary.

The complete definition has a 0x351c-byte allocation witness in
`V90Demodulator::V90Demodulator` and assertions in
`V90Phase4Demodulator.cpp`. The former prefix ended at 0x2ffc, immediately
after the embedded `V90Phase4Modulator` at +0x50. The complete model preserves
that prefix, including `sessionFlag` at +0, `int_0038` at +0x38 and `int_003c`
at +0x3c, then describes the two R detectors and the decision-state tail.

No source that previously saw only the partial definition allocates the class
using `sizeof`. The allocating demodulator and the phase 4 implementation
already include the complete definition. The compatibility header introduces
no inclusion cycle. Removing the partial definition allows callers to include
both headers safely and gives every translation unit the same class layout.

The session-flag differential fixture now reserves the complete class size,
plus its existing 64-byte slack. Previously its phase 4 slot covered only the
prefix. Its synthetic seeded object graph tests component-method stores and
propagation; it does not establish public modem-call reachability.

## Validation boundary

The baseline is `ac7f28e6`. Baseline and candidate use the unmodified
`tools/toolchain/period.mk` profile, including `DSPLIB_REPRODUCE_BUGS`:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h -DDSPLIB_REPRODUCE_BUGS
C++: -fno-exceptions -fno-rtti -fno-math-errno -ffast-math
DCR: -O2 -fno-rerun-cse-after-loop
```

The compiler is Gentoo GCC 3.4.2-r2 from
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`. Executing the assembler
selected by `gcc -print-prog-name=as` reports GNU assembler 2.15.92.0.2
20040927, target `i386-pc-linux-gnu`.

Validation completed on 16 October 2026:

- `make phase J=12`: 388 period differential binaries passed, zero failed;
  all structural checks passed. No new test binary was added.
- `tools/onedef.py`: 335 types in 168 files, zero known duplicates.
- Full period build: 300/300 source ELF objects are byte-for-byte identical
  between baseline and candidate, with no missing or added objects. This
  includes all function bodies, relocations, data, exports and binding.
- `byteident.py --ratchet`: 1086/1852 exact functions, 29 register-renaming
  matches and five unresolved section relocations; the candidate and baseline
  censuses agree. The instruction-sequence ratchet also passed.
- Both 300-input partial links are byte-for-byte identical, SHA-256
  `21eda0aa7556e2db555a8deb6d697e33bcbd26e006ed63558a8757b85687dd91`.
  Against the original, `partialcmp.py --require-exact` still reports
  `DIFFERENT` (397/2907 symbol records exact). Consolidation introduces no
  additional difference; original whole-object fidelity remains unfinished.
- Reference and anchor checks passed: 14,454 references and 10,038 mutation
  anchors across 285 suites. These are static anchor checks; no mutation
  execution or modern portability run is claimed.

The baseline and candidate object sets, build configurations and partial-link
manifests were retained under `build/phase4-type-baseline` and `build/tc_out`
in the validation worktree. The full phase log is
`/tmp/slmodem-phase4-phase.log`.
