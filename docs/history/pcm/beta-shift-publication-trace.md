# Beta shift mask and publication: expansion versus destructive reuse

Read-only trace of the three existing beta-x87-mode TUs, two beta setters each, against PR279's retained1070 baseline. No source variants, compilation, flags, runtime or harnesses. The reproducible diagnostic checks24 instruction streams: initial RTL, immediately before and after regmove, and final machine RTL for every setter/cell.

## Measured boundaries

The explicit AND31 is present in **01.rtl**, generated from the reconstructed `one_shifted_by` helper's defined `(unsigned int)n &31u`. It is not a mask introduced by register allocation, late scheduling or x87 conversion.

| Cell, both setters | Unmasked member store | AND31 | ASHIFT |
| --- | ---: | ---: | ---: |
| Baseline XF | UID167 | UID184 | UID186 |
| Float diagnostic | UID166 | UID183 | UID185 |
| Double diagnostic | UID167 | UID184 | UID186 |

In each initial stream the member publication precedes the mask, which precedes the shift. The helper's copied `n` pseudo is subsequently propagated back to the original count.21.ce2 still keeps a distinct mask destination pseudo. **22.regmove coalesces the AND destination with its source**, reusing original count pseudo79 destructively (same phenomenon in all six bodies). After allocation the count is ECX: store original ECX to member, AND ECX with31, then shift using CL. The selected shift destination is later renamed from EAX to EDX, but this does not change the count lifetime.

This creates a real value-preservation dependency. Once ECX is overwritten by its masked value, the original unmasked member value is no longer available. Moving publication after the destructive mask would change behavior for counts outside0..31. The scheduler cannot simply match the original's shift-before-publication order by swapping statements in this graph. Original LE code at `0x365a2..0x365ae` and DFE code at `0x36702..0x3670e` reads the converted count, computes `shl %cl,%edx`, then stores the still-unmodified full ECX to its member. Hardware consumes the low count bits without modifying ECX.

The prior four-cell whole/shift-lifetime domain already tested source capture before publication and found all verdicts unchanged. This trace explains why a statement-order-only hypothesis is insufficient, and does not reopen that source matrix.

## Backend bounds: why the explicit mask is retained

Official stock GCC3.4.2 `i386.md:10800..10832` defines ASHIFT with a QImode register/immediate count, and `ashlsi3_1` constrains the variable count to CL (`cI`). It matches a count operand, not an embedded `(and count,31)` expression. There is no direct AND31-plus-SHL absorption pattern in the inspected scalar shift patterns.

`i386.h:2594..2599` deliberately leaves `SHIFT_COUNT_TRUNCATED` undefined, explaining that shifts truncate counts but bit opcodes do not. `combine.c:94..95` defaults this macro to0. Consequently the generic variable-count truncation simplification at `combine.c:4561..4568` is disabled for this target; it cannot discard the mask merely because SHL hardware ignores high count bits. `simplify-rtx.c:1372..1381` and2158..2185 condition count truncation on the same macro; they do not establish a target-specific absorption path for this variable count.

This is bounded, not a claim that AND31 can never disappear. Constants can fold. More generally `combine.c:7933..7940,7998..8002` drops an AND if `nonzero_bits` proves that its input already has only bits admitted by the mask. Such a proof would make AND31 independently redundant. Here the count is an unconstrained SI conversion of logarithmic field arithmetic; no low-five-bit proof is present in the measured RTL. All six final bodies retain AND31. These are stock-source explanations corroborated by actual Gentoo dump observations; the exact Gentoo patch sources have not been audited.

## Source/domain witness needed before another control

Original setter guards establish only that MMX mode is enabled and beta compares nonzero. The converted count comes from `log10(abs(maxCoef/(beta*2^24)))/log10(2)` (DFE uses2^20), with no clamp or range test before SHL/publication. Beta has a float argument in the mangled signature, and maxCoef is read from a mutable float member. Neither this original graph nor the corresponding reconstructed setter proves a count in0..31.

For a finite positive ideal ratio R, truncating log2(R) gives0..31 if `-1<log2(R)<32`, equivalent to `1/2<R<2^32`; a real proof must account for the actual x87 expression, rounding, conversion and all reaching inputs, not just that algebraic interval. Tiny/large ratios, zero/infinity and exceptional conversion cases are not excluded by the guards. Internal callers include parameter-derived beta values, while the setter is also exported. Observing particular calls with convenient values cannot prove the entire component boundary bounded.

Thus **there is no currently justified mask-removal or store-reordering source control**. Next discriminating work is to establish an original lifecycle/contract bound on converted counts, or an independently supported computation that retains defined masking while making its redundancy provable to the period compiler. Until such evidence exists, preserve `one_shifted_by` and the full unmasked member value. The original unmasked SHL does not uniquely prove the author wrote undefined `1 << shift`, and codegen proximity cannot justify weakening the reconstruction's defined out-of-range behavior.

## Reproduction and pinned sources

```sh
python3 tools/beta_shift_publication_trace.py \
  --dumps ../byteexact-eia6-x87/build/beta-x87-mode/V90Equalizer
```

Artifact `build/beta-shift-publication-trace.json` preserves the selected publication/mask/shift patterns and their UIDs across24 streams, with positive detection of destructive reuse at regmove. Existing complete compiler commands/configuration and source hashes remain in `beta-x87-mode/results.json`.

Sources are the official `releases/gcc-3.4.2` tag at [GCC mirror](https://github.com/gcc-mirror/gcc/tree/releases/gcc-3.4.2/gcc), saved under `build/gcc-x87-mechanism-source`. SHA256 pins:

| File | SHA256 |
| --- | --- |
| `i386.md` | `2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab` |
| `i386.h` | `a88606d1f36e9cc667805bd22b19333608f6aea55beee932d3767079284e95ec` |
| `combine.c` | `8dd7eba5a63e2e72902b890b318aeef576f422e3d56090039b11d74ed9594ab8` |
| `simplify-rtx.c` | `4f5e0d8c90e39d570de5faf7f6150a265ce336c48f0b62a849cf16b87750fce5` |
