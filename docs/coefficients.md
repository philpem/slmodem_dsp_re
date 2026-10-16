# Coefficient tables: derivations

This document has moved to [docs/maintainer/coefficients.md](maintainer/coefficients.md).
The original path is retained for references in source comments and tooling.

## Recovered

See the [complete document](maintainer/coefficients.md).

### `FPM_sqrt_table` — Q15 square root, 193 entries ✅

See the [complete document](maintainer/coefficients.md).

### `FPM_cos_table` / `FPM_sin_table` — quarter-wave sine, 257 entries each ✅

See the [complete document](maintainer/coefficients.md).

## In progress

See the [complete document](maintainer/coefficients.md).

### `FixedRC` polyphase resampler banks — 18 tables

See the [complete document](maintainer/coefficients.md).

### Mode 3 design (9600 -> 8000), fully characterised

See the [complete document](maintainer/coefficients.md).

### The design rate is `L * f_in`, not a fixed host rate

See the [complete document](maintainer/coefficients.md).

### The cutoff design rule

See the [complete document](maintainer/coefficients.md).

### Window fit

See the [complete document](maintainer/coefficients.md).

### What the 4 LSB actually costs, and which filter is better

See the [complete document](maintainer/coefficients.md).

### Remaining for `FixedRC`

See the [complete document](maintainer/coefficients.md).

## V.34 `costbl`

See the [complete document](maintainer/coefficients.md).

## V.34 `intcoef1` / `intcoef2` / `intcoef3` — the FSK interpolator

See the [complete document](maintainer/coefficients.md).

## V.34 `fsklpfcoeff600` — the post-detection low-pass

See the [complete document](maintainer/coefficients.md).

## V.34 timing recovery: the half-baud pair

See the [complete document](maintainer/coefficients.md).

## V.34 `V34TimingIIR_Acoef` / `_Bcoef` — the same filter, written once

See the [complete document](maintainer/coefficients.md).

## V.34 `sqrt_table`

See the [complete document](maintainer/coefficients.md).

## DTMF: two detectors, two coefficient families, one design

See the [complete document](maintainer/coefficients.md).

### The float bank: `eur_coef`, `us_coef`, `biascoef`

See the [complete document](maintainer/coefficients.md).

### The fixed-point bank: `MTD{1..8}_COEF_{8000,9600}`

See the [complete document](maintainer/coefficients.md).

### Where the two extra fixed-point sections come from

See the [complete document](maintainer/coefficients.md).
