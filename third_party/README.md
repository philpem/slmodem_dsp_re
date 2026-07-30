# Third-party test peers

## SpanDSP 3

`spandsp/` is <https://github.com/freeswitch/spandsp>, vendored as a git
submodule and used **only** as an independent interoperability peer and
stimulus generator inside `test/`.

### Licence firewall

SpanDSP is LGPL. This reconstruction is BSD, matching the slmodem tree it
replaces. To keep those separable:

- SpanDSP is linked into test binaries under `test/` and nowhere else.
- It is never a link dependency of the reconstructed library.
- **No SpanDSP source, tables, coefficients or algorithms are read into or
  copied into `src/`.** It is a black box: audio in, audio out, and a verdict.

The check is mechanical — `src/` must contain no `spandsp` include:

    ! grep -rl 'spandsp' src/ include/

### Why it is here

Tier-1 differential testing proves the reconstruction matches the blob. It
cannot tell whether the blob itself is *correct*, only that we copied it
faithfully. SpanDSP is a tested implementation known to interoperate with real
modems, so it is the only evidence available that distinguishes correct from
bug-compatible.

Coverage is partial: SpanDSP implements Bell 103, V.21, V.23, V.22bis and the
fax modems V.17/V.27ter/V.29, but **not** V.32/V.32bis and **not**
V.34/V.90/V.92. Those modules rest on Tiers 1-2 alone. The confirmed matrix
lives in `docs/interop.md`.
