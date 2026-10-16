# Sample-rate assumptions

This document has moved to [docs/maintainer/sample-rates.md](maintainer/sample-rates.md).
The original path is retained for references in source comments and tooling.

## 🔴 R-1 — Host rate is 9600, pumps are 8000; the wrapper bridges

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-2 — `d-modem.c` resamples 8000 → 9600 for a pump that wants 8000

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-3 — `FixedRC` coefficient banks are designed per rate pair

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-4 — Host-rate service layer carries `_8000`/`_9600` coefficient pairs

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-5 — `dcr_create` initialises three phase intervals at 9600

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-8 — Bell 103's FSK core is clocked at 7200 Hz

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-9 — `FPM_TONE_create` bakes in 8 kHz

See the [complete document](maintainer/sample-rates.md).

## 🟢 R-6 — `FPM_FSM_init` scales tone frequencies by 1.11108 — **explained**

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-10 — `FPM_TONE`'s phase-reversal period is counted in 8-sample units

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-11 — V.34's handshake counts in FOUR-SAMPLE TICKS, which are 2400-baud symbol periods only at 9600 Hz

See the [complete document](maintainer/sample-rates.md).

## 🟡 R-7 — `dp_wrapper`'s rate table is a fixed list of six pairs

See the [complete document](maintainer/sample-rates.md).

## 🔴 R-12 — `v32_create` rescales `MDMPRM_IODELAY` from 9600 to 8000 by ×5/6

See the [complete document](maintainer/sample-rates.md).
