# Parallel decoder source-form experiment

> INVALID EXPERIMENT. Parent review found that the runner used the original
> workspace instead of `/tmp/slmodem-byte-fidelity`, did not use the shared
> experiment-toolchain helpers, and substituted a custom instruction scorer
> for `byteident.py`. Its non-text check measured section sizes, not contents.
> The measurements and acceptance conclusion below are preserved as invalid
> artifacts and must not be used. The corrected controls are recorded separately
> in `parallel-decoder-valid.md`. A gain in exact-function count is not required
> to retain an independently evidenced source correction after full validation.

## Scope and controls

Published image: `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`. Compiler path: `/usr/i386-pc-linux-gnu/gcc-bin/3.4`. The full TU was `src/pump/v90/V90SignBitsExtractor.cpp`; overlay include was first and `-DDSPLIB_REPRODUCE_BUGS` was last. No compiler option varied.

The unchanged overlay was raw-object identical to `build/tc_repro/src_pump_v90_V90SignBitsExtractor.cpp.o` (`c4214bb5ac14d74e1cee00133194dc8689594dd7c155d2bf04921a8203d68a31`). All comparisons cover the 12 symbols shared by that TU and the reference object. The current parallel encoder source was copied unchanged into every overlay.

## Toolchain identity

```text
gcc=/usr/i386-pc-linux-gnu/gcc-bin/3.4/gcc
gcc (GCC) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)
Copyright (C) 2004 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

i386-pc-linux-gnu
assembler=/usr/lib/gcc/i386-pc-linux-gnu/3.4.2/../../../../i386-pc-linux-gnu/bin/as
GNU assembler 2.15.92.0.2 20040927
Copyright 2002 Free Software Foundation, Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License.  This program has absolutely no warranty.
This assembler was configured for a target of `i386-pc-linux-gnu'.
```

## Results

| form | object SHA-256 | decoder bytes | decoder exact | exact / 12 | gains | losses | bindings |
|---|---|---:|---|---:|---|---|---|
| `unchanged` | `c4214bb5ac14d74e1cee00133194dc8689594dd7c155d2bf04921a8203d68a31` | 62 | no | 8 | - | - | same |
| `state_pointer_indexed` | `aa62ada60f54010607990fd51a48e70fe2803e4e82762023a037d9c2ee38da1b` | 90 | no | 8 | - | - | same |
| `three_cursors` | `a73d32c9d467a3a88e121783ef241fe2a02df5ab9cb8a092303f589a1607e609` | 58 | no | 8 | - | - | same |

## Interpretation

Best measured form: `unchanged`. It has 8/12 exact canonical instruction bodies; decoder size 62 bytes versus reference 57 bytes. Decoder exactness is not achieved.

`results.json` records per-symbol canonical body and relocation equality, exact gains/losses, bindings, non-text allocated-section summaries, complete commands, hashes, and the baseline configuration.

## Scoped acceptance

Adoption is warranted only if a changed form improves the full 12-symbol exact set without losses, preserves bindings/non-text output, and then passes the independently run consumer and period differential gates. A size-only near miss is evidence, not an adoption criterion.
