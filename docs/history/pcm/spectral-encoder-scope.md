# Parallel encoder shared-header scope

## Candidate

This scope measures the reference-evidenced `ParallelDifferentialEncoder<T>::process` candidate established by the bounded matrices: cache `state_`, advance state/input/output cursors, compute a local result, write output before state, and continue to compare the loop counter against the live `size_` field.

The candidate remains a 54-byte BYTES 4 near miss, not an exact recovery. Its retention case is the reference's memory-access structure, not its grade alone.

## Method and validity

Artifacts are in `build/spectral-encoder-scope/`; `results.json` records dependency discovery, every compile command, unchanged-control hashes, changed-symbol inventories, data and symbol records, the 830-name exact-set comparison, and partial-link metrics.

Dependency discovery runs the published Gentoo GCC 3.4.2 compiler's `-MM` over every C++ source in the 275-input manifest. It finds 18 TUs that see `DiffCoder.h`. Each is compiled twice through first-priority isolated overlays: unchanged header and candidate header. All 18 unchanged-overlay objects match their `build/tc_repro` objects byte-for-byte, validating the compile path and complete flags. The helper appends `-DDSPLIB_REPRODUCE_BUGS` last; C++ flags and selected compiler/assembler are recorded.

The candidate full object set contains copied retained objects for unaffected inputs and candidate objects for the 18 dependencies. It is linked in the recovered `tc_link_manifest.txt` order with the explicitly published image.

Two generated-manifest path mistakes caused link refusal before measurement: first the two-field manifest was collapsed to source basenames, then an absolute host path was passed where the wrapper requires a repository-relative container path. Both failed loudly with missing inputs, produced no linked score, and are excluded. The corrected two-field relative manifest links all 275 inputs successfully.

## Affected scope

The original compiler reports 18 dependent TUs. Seventeen candidate objects remain byte-identical to baseline. Only `src_pump_v90_V90SpectralShaper.cpp.o` changes, and within it only `_ZN27ParallelDifferentialEncoderIhE7processEPhS1_` changes.

All non-text data remains identical. All defined names and bindings remain present. The SpectralShaper symbol-record sequence changes only because the helper's recorded size changes from 60 to 54 bytes; its binding remains WEAK with DEFAULT visibility. No other body or relocation changes.

The global byteident exact membership remains 830 to 830: zero gains and zero losses. The helper improves from SIZE 6 to the 54-byte BYTES 4 result but does not enter EXACT.

## Partial-link result

The correct recovered-order candidate partial link succeeds:

- reference allocated bytes: 943,398;
- candidate allocated bytes: 897,628;
- candidate size delta: -45,770;
- equal positioned bytes: 68,488;
- different or missing reference bytes: 874,910.

The supplied post-PR97 baseline is 68,457 equal positioned bytes, so the candidate gains 31 positioned bytes while reducing candidate code by the expected six bytes. This is an intermediate layout measurement, not completion or independent proof of source identity.

## Handoff

The isolated scope finds no collateral object-body, data, binding, relocation, or exact-membership regression. The parent can now apply the exact candidate block to the versioned header and run the required original-compiler differential and structural gates across the real dependency graph. Because `DiffCoder.h` is shared, acceptance still requires that real full rebuild; these overlay results do not replace it.

No versioned source, gates, or commits were changed by this experiment.
