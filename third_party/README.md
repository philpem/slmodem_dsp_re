# Third-party test peers

## SpanDSP 3

`spandsp/` is <https://github.com/freeswitch/spandsp>, used **only** as an
independent interoperability peer and stimulus generator inside `test/`.

It is **not committed** — 41 MB of third-party source with its own history and
licence. Reproduce it with:

```sh
git clone --depth 1 --branch version-3.1.0 \
    https://github.com/freeswitch/spandsp third_party/spandsp
cd third_party/spandsp && ./bootstrap.sh && \
    ./configure --disable-shared --enable-static && make
```

Pinned revision: **`6a0e9f51fd5a6bd3f8c3a5337522e4b6743a6637`**, branch
`version-3.1.0`. `make interop` links `src/.libs/libspandsp.a` from that tree.

### Do NOT use the distro package

`libspandsp-dev` is 0.0.6, and its `preset_fsk_specs` has the two Bell 103
entries **swapped** relative to 3.x: its `FSK_BELL103CH1` is the answerer's
2025/2225 where 3.x's is the caller's 1070/1270. Building against it
demodulates the wrong band and returns the exact complement of the data —
a symptom that reads as a polarity bug and is not one.

`t_spandsp_b103.c` asserts the two channel frequencies before using them, so
a build against the wrong version fails immediately and says why.

### Licence firewall

SpanDSP is LGPL. This reconstruction is BSD, matching the slmodem tree it
replaces. To keep those separable:

- SpanDSP is linked into test binaries under `test/` and nowhere else.
- It is never a link dependency of the reconstructed library.
- **No SpanDSP source, tables, coefficients or algorithms are read into or
  copied into `src/`.** It is a black box: audio in, audio out, and a verdict.

The check is mechanical and **`make test` runs it** — `make firewall` fails the
build if anything under `src/` or `include/` includes a SpanDSP header:

    grep -rnE '^[ \t]*#[ \t]*include.*spandsp' src/ include/

It matches an `#include`, not the bare word, so a comment may name an interop
test: `src/pump/v23/bwchdem.c` cites `t_spandsp_v23` for what settled the
question about its resonators. A check that fires on prose is a check people
learn to ignore.

### Why it is here

Tier-1 differential testing proves the reconstruction matches the blob. It
cannot tell whether the blob itself is *correct*, only that we copied it
faithfully. SpanDSP is a tested implementation known to interoperate with real
modems, so it is the only evidence available that distinguishes correct from
bug-compatible.

Coverage is partial: SpanDSP implements Bell 103, V.21, V.23, V.22bis and the
fax modems V.17/V.27ter/V.29, but **not** V.32/V.32bis and **not**
V.34/V.90/V.92. Those modules rest on Tiers 1-2 alone. What has actually been
confirmed, direction by direction, is in `docs/interop.md`.
