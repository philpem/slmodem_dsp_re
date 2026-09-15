# Scrambler allocation association experiment

This bounded experiment tests the allocation arithmetic in the
`Scrambler<int, unsigned char>` constructor emitted by the full
`V90Modulator.cpp` translation unit. F10229 did not exhaust this three-form
association domain. No versioned source, flags, tests, gates, or git state were
changed.

## Controls and provenance

The retained profile is copied from `build/tc_repro/.build-config`, with the
overlay include first and `-DDSPLIB_REPRODUCE_BUGS` appended last by
`tools/experiment_toolchain.py`. The image is
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`; `gcc` and `g++` both
identify Gentoo GCC 3.4.2-r2. The `g++`-selected assembler is GNU assembler
2.15.92.0.2 at
`/usr/lib/gcc/i386-pc-linux-gnu/3.4.2/../../../../i386-pc-linux-gnu/bin/as`.
Complete commands are retained in each arm's `command.json`, and complete
identity output is in `toolchain.txt`.

The header SHA-256 is
`0ae731ac0f5392fe498a1b3f48f4824c4fb6c214eb068126b5245985678794e1`.
The pinned baseline object SHA-256 is
`69fc97a794e0d5f4aa90958d9f6e62690ff7fa87eecfb343a3db715bb203ed1f`.
The separately compiled unchanged control has that same hash, establishing a
raw 12,104-byte match before interpreting the matrix. The retained and
reassociated arms also have that hash. The count-local arm is
`bf87cfc67846e9702c6f1e4252bbd0bd171dd410c1408364b2edbc384d263617`.

Two pre-compilation apparatus failures are preserved and excluded in
`build/scrambler-allocation-experiment/invalid-runs.txt`.

## Finite result

`byteident.sizes/body/verdict` reports 25 shared function names and 18 baseline
exact names in every arm.

| source form | exact | target verdict and size | changed bodies | gains | losses |
| --- | ---: | --- | ---: | --- | --- |
| `(1 + b + c) * sizeof(T)` | 18/25 | SIZE(3), 110 vs 107 | 0 | none | none |
| `(b + c + 1) * sizeof(T)` | 18/25 | SIZE(3), 110 vs 107 | 0 | none | none |
| `unsigned int count = b + c + 1; count * sizeof(T)` | 19/25 | EXACT, 107 vs 107 | 1 | `_ZN9ScramblerIihEC1Ejjj` | none |

The count-local candidate changes exactly one canonical body,
`_ZN9ScramblerIihEC1Ejjj`; all other 24 shared bodies are identical to the
baseline. Allocated non-text contents are identical in every arm. The
`Descrambler` sibling is unchanged.

`partialcmp` records 26 defined symbol records before and after. Only the
target record changes: its size is 110 to 107. Its full name, `FUNC` type,
`WEAK` binding, `DEFAULT` visibility, linkonce section, and zero value are
unchanged. There are 173 relocations in both objects; the two records inside
the shortened function move by three bytes, while their relocation types and
targets remain unchanged. The candidate's target body, including canonical
relocation targets, is exact against the blob.

## Conclusion

Within this declared retained-profile domain, arithmetic reassociation is
code-generation-equivalent to the current source and does not explain the
residue. Introducing the ordinary `unsigned int count` local is the unique
tested form that reproduces the reference constructor and causes no other TU
body or exact-set loss. This is a source candidate supported by the full-TU
emission, not an adoption or differential-validation claim; no gate was run.

Machine-readable details, including all 25 verdicts per arm, complete symbol
records, partial comparisons, hashes, and commands are in
`build/scrambler-allocation-experiment/results.json`.
