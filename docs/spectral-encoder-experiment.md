# Parallel differential encoder process experiment

## Question

`ParallelDifferentialEncoder<unsigned char>::process(unsigned char *, unsigned char *)` is a 54-byte WEAK function in the reference and a 60-byte WEAK function in the retained `V90SpectralShaper.cpp` object. The retained 275-object build has one defining copy, in `build/tc_repro/src_pump_v90_V90SpectralShaper.cpp.o`; there is no alternate exact COMDAT copy to borrow.

The complete body difference predicts source structure rather than a six-byte epilogue tweak. The reference loads `state_` once before the loop, advances state/input/output pointers, loads the prior state byte once, computes the result, writes output before state, and reloads `size_` for the loop comparison. The retained indexed source reloads `state_` twice per iteration, mutates state first, then reloads it for output.

This experiment does not revisit the exhausted `V90SpectralShaper::reset` pointer-local or `process` cursor domains.

## Reproducible setup and validity

Artifacts are in `build/spectral-encoder-experiment/`; `results.json` records all 12 cells, complete commands, hashes, the 15-name shared denominator, per-symbol verdicts and relocations, binding records, and allocated-section checks. `toolchain.txt` records the selected compiler and assembler.

The run explicitly uses `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, compiler path `/usr/i386-pc-linux-gnu/gcc-bin/3.4`, and `native=True`. Flags match `build/tc_repro/.build-config`, including the C++ flags `-fno-exceptions -fno-rtti -fno-math-errno -ffast-math`; `tools/experiment_toolchain.py` appends `-DDSPLIB_REPRODUCE_BUGS` last.

All changes are isolated header overlays. The versioned header and sources are untouched. The corrected unchanged overlay reproduces the retained SpectralShaper object byte-for-byte.

An initial invocation placed the overlay include after `-Iinclude`; every source form therefore compiled the versioned header and falsely appeared unchanged. The detector fired because all variant objects matched the control. That run is invalid and excluded. The script was corrected to put the overlay first, and the complete declared matrix was rerun. Only the corrected results are in `results.json`.

## Declared domain

Six behavior-preserving source forms were crossed with retained flags and diagnostic `-fno-strength-reduce`, for 12 cells:

- current indexed mutate-then-copy;
- indexed local result, output then state;
- hoisted `state_` pointer but indexed accesses;
- advancing state cursor with indexed input/output;
- advancing state/input/output cursors with a local result and output-before-state;
- advancing cursors with mutate-state-then-copy.

No form caches `size_`: the reference reloads it in the loop comparison. No `restrict`, attributes, volatile objects, padding, or counter permutations were introduced. Input/output aliasing remains permitted. Output-before-state and state-before-output store the same computed byte for ordinary and in-place buffers; store order is a code-generation discriminator, not a claim that the existing in-place test distinguishes the forms.

## Results

All corrected cells compile. No cell is exact.

| Source form | Retained flags | `-fno-strength-reduce` |
| --- | --- | --- |
| current | SIZE 6, 60 bytes | SIZE 6, 60 bytes |
| indexed local | SIZE 5, 59 bytes | SIZE 5, 59 bytes |
| hoisted state, indexed | SIZE 1, 55 bytes | SIZE 1, 55 bytes |
| state cursor | BYTES 15, 54 bytes | BYTES 15, 54 bytes |
| all cursors, local result | BYTES 4, 54 bytes | BYTES 4, 54 bytes |
| all cursors, mutate/copy | BYTES 8, 54 bytes | BYTES 8, 54 bytes |

The primary predicted form, all cursors with a local result and output-before-state, reaches the reference's 54-byte size and is closest at BYTES 4, but it is not exact and is not adopted. The progression is still causal evidence: local-result source removes one byte, hoisting state removes four more, and cursor structure reaches the correct length. Store order distinguishes the three correct-size emissions.

Across the 15 shared function names, the retained-profile variants keep the baseline exact set at 10 names: zero exact gains and zero exact losses. Their only changed body is this helper. All helper forms retain WEAK FUNC binding and the complete symbol surface.

`-fno-strength-reduce` never changes the helper verdict for a given source form. It independently changes `V90SpectralShaper::advanceTrellis` and `V90SpectralShaper::process`; the latter grows from 362 to 364 bytes. It therefore changes three bodies in nonbaseline source cells, provides no helper gain, and is rejected as a profile candidate.

The two named GLOBAL data tables, 20-byte `pow10Table` and 512-byte `V90SpectralShaper::actionLookupTable`, total 532 bytes and remain byte-identical in every cell. The full non-text allocated payload also remains equal; the `.data` section is 544 bytes including layout/padding, plus an unchanged four-byte constant section. Function and object bindings/visibility remain unchanged. The helper itself has no relocations.

## Shared-header scope

Six other public headers directly include `DiffCoder.h`: `Agc.h`, `Scrambler.h`, `V90Mapper.h`, `V90Phase3Demodulator.h`, `V90SignBitsExtractor.h`, and `V90SpectralShaper.h`. In the current retained object set, only the SpectralShaper object defines this exact encoder specialization. A future versioned-header adoption would nevertheless require the complete dependency rebuild and whole-object comparison; the isolated one-TU result cannot certify indirect consumers.

## Conclusion and next discriminator

No exact candidate exists in the declared 12-cell domain, so no source or option change is adopted. The strongest retained-profile near miss is the all-cursor local-result form at BYTES 4. It establishes the likely pointer/local-result family but not the original spelling.

If reopened, the next bounded step is to compare that 54-byte candidate against the reference at the four differing bytes/instructions and define one source property that predicts them. Do not expand into counter permutations, cached loop bounds, alias assertions, attributes, or more optimizer toggles without that evidence. Any eventual shared-header candidate must preserve all defining copies and bindings, pass the existing differential alias coverage plus any targeted overlap case justified by the chosen store order, rebuild every dependency, and be evaluated over the complete 275-object result.

No versioned source, gates, or commits were changed.
