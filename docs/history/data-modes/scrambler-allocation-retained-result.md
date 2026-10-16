# Retained scrambler allocation-count correction

Baseline: `1e440a50`, the decoder follow-up in PR #98. The retained change
recovers one additional exact function name, including both defining copies.

## Source evidence and finite controls

The reference `Scrambler<int, unsigned char>` constructor computes an element
count including the guard element, then scales it to bytes:

```asm
lea 1(%esi,%ebx),%ecx
shl $2,%ecx
```

The former source instead emitted a sum followed by a scaled address with
displacement four. That explains its entire 110-versus-107-byte mismatch.
The three retained-profile source forms were:

| Form | Constructor | Full V90Modulator TU |
| --- | --- | --- |
| `(1 + b + c) * sizeof(T)` | SIZE(3), 110 bytes | 18/25 exact |
| `(b + c + 1) * sizeof(T)` | SIZE(3), 110 bytes | 18/25 exact |
| unsigned count local, then scale | EXACT, 107 bytes | 19/25 exact |

Reassociation alone is byte-identical to baseline. The retained ordinary
`unsigned int count = b + c + 1` preserves the allocation amount and unsigned
arithmetic, while reproducing the reference's complete instruction body and
canonical relocation targets. No flags, attributes, casts or barriers changed.
This supports the source intermediate within the measured domain; it does not
prove that this was the author's unique spelling.

The `Descrambler` sibling remains untouched. The reconstruction guide now
records the distinction between element count and byte-size conversion.

## Experiment validity and shared-header scope

See `scrambler-allocation-experiment.md` and
`scrambler-allocation-scope.md`. Experiments use the published Gentoo
GCC 3.4.2-r2 image, explicit compiler path and native image user, retained
`.build-config` flags, first-priority header overlays, and the shared helper
to append `DSPLIB_REPRODUCE_BUGS` last. The selected assembler was executed
and identified as GNU assembler 2.15.92.0.2. Commands, hashes and invalid
pre-compilation apparatus failures are recorded; invalid runs are excluded.

Fresh original-compiler dependency discovery found 30 header consumers.
All 30 unchanged controls reproduced their baseline objects byte for byte.
Only two candidate objects change, in `V90Modulator.cpp` and
`V92Modulator.cpp`, solely in `_ZN9ScramblerIihEC1Ejjj`. Both copies become
exact. Other Scrambler specializations and every Descrambler definition remain
body-identical, and all 30 consumers preserve allocated non-text contents.

The two changed symbol records retain names, FUNC type, WEAK binding, DEFAULT
visibility, sections and values. Only size changes, 110 to 107. The two
relocation types and targets inside each copy remain unchanged, at offsets
three bytes earlier. Full linking uses all 275 inputs in recovered
`tc_link_manifest.txt` order with the published image selected explicitly.

## Applied-source gates

```sh
make partial-link phase J=6 \
  TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest \
  PERIOD_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
BLOB=ref/slmodemd/dsplibs.o TC_OUT=build/tc_repro \
  tools/toolchain/byteident.py --ratchet \
  --json-out build/scrambler-allocation-retained/byteident.json
tools/toolchain/partialcmp.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o \
  --json build/scrambler-allocation-retained/partial.json
```

The first phase run passed all 375 period tests but correctly rejected eight
mutation anchors containing the old constructor text. Those anchors and
replacement bodies were updated in `test/mutations/scrambler.json` to retain
the same faults, including the extra element and incorrect byte scaling.
No mutation was deleted, relabelled equivalent, or skipped.

The subsequent `make phase` passed: **375 period tests passed, zero failed**,
plus all structural checks. Anchor validation covered **272 suites and 10,041
mutations, zero skipped**, with zero non-unique anchors. This is structural
anchor validation, not a newly executed runtime mutation sweep. No modern
portability result is claimed. Both applied constructor copies were also
independently rescored as EXACT against the reference.

## Full-object result

| Metric | Before | After |
| --- | ---: | ---: |
| Exact function names / 1,852 | 830 | 831 |
| Exact function bytes / 720,125 | 80,030 | 80,137 |
| Positioned reference bytes / 943,398 | 68,520 | 68,594 |
| Exact section descriptors | 68 | 69 |
| Exact section contents | 60 | 61 |
| Exact relocation records / 18,317 | 974 | 976 |
| Exact symbol records / 2,907 | 300 | 301 |
| Aggregate content-size deficit | 45,774 | 45,777 |

All 830 previous exact names survive. The only gained name is
`_ZN9ScramblerIihEC1Ejjj`; both defining copies satisfy the comparison.
The gain is **74 positioned bytes**, with no global layout movement. The
content-size deficit grows by three because the oversized COMDAT constructor
is shortened to its correct size. Raw linked-file size stays unchanged.
The complete partial-link verdict remains **DIFFERENT**, not completion.

Final logs and JSON are in `build/scrambler-allocation-retained/`.

## Other investigated TUs and next discriminator

The remaining SignBitsExtractor constructor variant differs in register choice;
its decoder-reset helper differs only in the dead stack-pop register. Neither
justifies a local source edit without new emission-order evidence.

NoK56Flex remains 20/22 exact. Its Create and Delete functions have ordinary
calls in the reference and sibling jumps in the reconstruction. Array deletion
was considered because the recovered TU is C++, unlike the earlier C-based
investigation. It was rejected before compilation: no replacement unsized
array-delete operator is visible, so the call target would become `_ZdaPv`
instead of `sysdep_free`. Adding an operator and a guessed allocation type
solely to obtain a match would manufacture the desired mechanism. The four-cell
proposal recorded in issue #22 was therefore not executed. Reopen only with
independent paired allocation/deallocation or operator-placement evidence.

The allocation-count domain is now closed with a validated retained result.
Continue the non-exact TU queue with a new forced-instruction discriminator;
do not expand allocation synonyms or repeat the exhausted SpectralShaper
pointer and mixed-precision families.
