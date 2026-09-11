# Issue 51: narrowed allocation sizes

This records the recovered allocation behaviour without changing it. The
default build remains blob-identical; any hardening must be opt-in.

## Findings

All results below are from the recovered Gentoo GCC 3.4.2-r2 source and the
blob-derived declarations.

| Site | Narrowing | Result |
| --- | --- | --- |
| `FPM_SRE_init` | `(short)(2 * cfg.coeffs)` | `cfg.coeffs` is a caller-provided signed short. At 16384, the byte count wraps while the coefficient-copy loop still follows `cfg.coeffs`; this is an externally configurable under-allocation. |
| `FPM_SRE_init` | `(short)(2 * taps)` | `taps` derives from `coeffs / 10`; it is reached by the same configuration path and wraps once the derived value exceeds 16383. |
| `FPM_SRE_init` | `(short)(2 * cfg.rms_len)` | `rms_len` is caller-provided. The active RMS path indexes this buffer through that logical length, so a sufficiently large value under-allocates it. |
| `FPM_FSE_init` | `short coeff_bytes = 2 * cfg.taps` | `cfg.taps` is caller-provided. The three coefficient/history buffers wrap above 16383 taps while init still initializes `taps` elements. |
| `FPM_FSE_init` | `short sym_bytes = 2 * (cfg.block / cfg.interp) + 4` | The existing `t_fpm_fse` boundary case uses `block = 32767`, `interp = 1` and records the wrapped symbol allocation. The output arrays are still addressed from the untruncated logical count. |
| `SGD_create` | `(unsigned short)(2 * (hist_len + hist_extra) - 2)` | Both operands are caller-provided signed shorts. The 16-bit byte count can wrap. Independently, `ref_len > hist_extra` already overruns the allocated history span (D1061). |

`FIFO_create` has `(unsigned)(unsigned short)size * 2`, but this is not the
same under-allocation shape: after the 16-bit conversion it is widened before
the multiply. Positive signed `size` values allocate their full element count;
negative values request a large allocation and remain invalid configuration.

`v22_sre.c` carries an inner short conversion too, but its production tap
count is fixed by the V.22 tables. It is retained as recovered code and is not
evidence of a configurable allocation defect.

## Compatibility decision

The source must retain every narrowing conversion: removing FPM_SRE's three
casts changes its complete recovered-Gentoo translation unit, whereas removing
only its C outer pointer casts did not. The same conversions are therefore
part of the reference behaviour.

The reachable configurations above establish real compatibility bugs rather
than merely cosmetic casts. A future safety API may reject values before the
16-bit arithmetic or allocate from a checked widened size, but it must be
separate from the blob-compatible entry points and must define ownership and
error reporting first.
