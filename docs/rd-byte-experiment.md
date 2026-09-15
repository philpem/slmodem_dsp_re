# `rd.c` byte-fidelity experiment

## Question

The post-PR95 baseline has three of `src/service/rd.c`'s four shared functions exact. `RD_create` is BYTES 11: reference and candidate are both 293 bytes, but eleven bytes differ. The difference is confined to physical layout of the three switch result blocks. The reference lays them out as thresholds `1000, 650, 850`; the retained build lays them out `650, 850, 1000`. Instructions, relocations, size, and the other three functions otherwise agree.

This experiment asks whether that order is forced by source case-group order, the compiler's block-reordering pass, or their interaction. It does not adopt a source spelling from a local score.

## Reproducible setup

Artifacts are under `build/rd-byte-experiment/`; `results.json` is the machine record and `toolchain.txt` records the selected compiler and assembler. The image is explicitly:

```text
ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
```

The experiment explicitly selects `/usr/i386-pc-linux-gnu/gcc-bin/3.4` and uses native image identity (`native=True`), because the helper does not recognize the published registry name as its local Gentoo alias. Flags match `build/tc_repro/.build-config`:

```text
-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
-Iinclude -D__SIZEOF_POINTER__=4
-include tools/toolchain/period_compat.h
-DDSPLIB_REPRODUCE_BUGS
```

`tools/experiment_toolchain.py::compile_shell` appends `-DDSPLIB_REPRODUCE_BUGS` last. The recorded complete control command agrees with the baseline configuration.

The unchanged-path control reproduces `build/tc_repro/src_service_rd.c.o` byte-for-byte. This is the required validity control; the matrix is therefore interpretable.

## Declared finite domain

The three semantic case groups are:

- `A`: codec values 4 and 12, threshold 1000
- `B`: codec values 13 and 15, threshold 650
- `C`: codec value 14, threshold 850

The source axis is all six permutations of `A`, `B`, and `C`. The option axis is retained flags versus one isolated `-fno-reorder-blocks` addition. This is a complete 6 x 2 domain, 12 matrix cells, plus the unchanged control. No synonymous statements or byte-score padding were tried.

Prediction: if case source order alone controls physical block order, one retained-flags permutation should make `RD_create` exact without changing its bystanders. If GCC block reordering masks source order, disabling it should expose a source-order-dependent result. A loss in an already-exact bystander falsifies a TU-local improvement.

## Results

All 13 compilations succeeded. Every cell preserves all four names as GLOBAL FUNC symbols with the baseline sizes shown by that cell; no binding or visibility changes occurred.

| Source order | Retained `RD_create` | `-fno-reorder-blocks` `RD_create` | Other three, retained | Other three, no-reorder |
| --- | --- | --- | --- | --- |
| ABC | BYTES 11 | SIZE 3 (290 vs 293) | 3 EXACT | RD_delete SIZE 28; RD_process SIZE 4; RD_ring_details EXACT |
| ACB | BYTES 9 | SIZE 3 (290 vs 293) | 3 EXACT | same |
| BAC | BYTES 9 | SIZE 3 (290 vs 293) | 3 EXACT | same |
| BCA | BYTES 5 | SIZE 3 (290 vs 293) | 3 EXACT | same |
| CAB | BYTES 11 | SIZE 3 (290 vs 293) | 3 EXACT | same |
| CBA | BYTES 9 | SIZE 3 (290 vs 293) | 3 EXACT | same |

There are zero exact `RD_create` hits. Under retained flags, source group order affects emission but only yields three observed distances: 11, 9, and 5 differing bytes. `BCA` is the closest cell, but closeness is not source evidence and it is not adopted. Variant objects do not byte-match the control as whole files because their generated source paths alter file metadata; the reported conclusion is based on all four named function bodies, relocations, and symbol records, not raw variant-file identity.

`-fno-reorder-blocks` is a negative mechanism control. All six source orders have the same full-TU verdict vector: `RD_create` shrinks by 3 bytes, `RD_delete` by 28, and `RD_process` by 4; `RD_ring_details` remains exact. It loses two exact bystanders and does not expose the desired block ordering.

## Conclusion and stopping point

This complete domain excludes the six case-group permutations under retained flags and under `-fno-reorder-blocks`. It also shows that the option is not a viable local candidate: it moves the TU away from the reference and changes functions outside the switch.

The mismatch remains a physical basic-block ordering question, not evidence for different threshold values, case membership, calls, or symbol binding. No source change is retained and no gate is warranted.

The next discriminating test, if this TU is reopened, should inspect GCC's actual block-order constraints and branch probabilities for the three result blocks, then test one bounded mechanism that predicts the reference order while preserving the three exact bystanders. Repeating case permutations, trying nearby synonymous C, or adopting `BCA` because it is closest would add no evidence and should not reopen this domain.
