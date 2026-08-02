# What a third party has confirmed

Tier-1 differential testing proves the reconstruction matches `dsplibs.o`. It
cannot prove the blob is *correct* — a bit-exact copy of a wrong modem passes
every one of those tests. This file records the part that is not
self-referential: what an implementation written from the standard, by someone
else, has agreed with.

That peer is SpanDSP 3.1.0, vendored in `third_party/spandsp`. See
`third_party/README.md` for the licence firewall and for why the distro
package must not be used.

Run it with `make interop`. It is a separate 64-bit binary from the
differential suite, because the blob is i386 and SpanDSP here is amd64 and the
two cannot share a process.

---

## The matrix

A direction is **confirmed** only if a bit stream survived it with a measured
error rate of zero. "Not covered" means SpanDSP has no implementation to test
against, not that something failed.

| modulation | ours → peer | peer → ours | where |
|---|---|---|---|
| Bell 103 originate/answer | confirmed | confirmed | `t_spandsp_b103.c` |
| V.23 channel 1, 1200 bps | confirmed | confirmed | `t_spandsp_v23.c` |
| V.23 channel 2, 75 bps | confirmed | confirmed | `t_spandsp_v23.c` |
| V.8 signalling | confirmed | confirmed | `t_spandsp_v8.c` |
| V.8 full negotiation | confirmed, both ends | | `t_spandsp_v8neg.c` |
| V.8 over a socket, blob included | confirmed | | `t_spandsp_v8sock.c` |
| V.21 | not covered | not covered | shares the Bell 103 code path |
| V.22, V.32 | not covered | not covered | SpanDSP has no V.32; V.22 not yet reconstructed |
| V.34, V.90, V.92 | not covered | not covered | SpanDSP has none of these |

Measured figures, from the last run:

```
ch1 ours -> SpanDSP: 3000 bits sent, 2999 received, lag  -2, 1550 compared, BER 0.00000
ch1 SpanDSP -> ours: 3000 bits sent, 2976 received, lag -19, 1550 compared, BER 0.00000
ch2 ours -> SpanDSP:  892 bits sent,  890 received, lag  -1,  642 compared, BER 0.00000
ch2 SpanDSP -> ours:  900 bits sent,  887 received, lag -12,  650 compared, BER 0.00000
```

---

## What it has actually caught

Twice now, and both times something no differential test could have seen.

**D4 — `FPM_div` reads past its table.** The Bell 103 replay showed the
original losing bit-clock lock on a SpanDSP signal at particular input levels.
Chasing it found a table index that runs one past the end for 0.39% of
denominators and returns a zero reciprocal, which collapses the AGC gain. The
blob does it too, so every differential test agreed with it perfectly.

**Finding 87 — the transmitter's `consumed` is not a cursor increment.**
`v23FP_tx_progress` reports bits *finished*; a bit still in flight has already
been taken from the caller's buffer and is not counted. A caller that advances
a cursor by `consumed` re-supplies the held bit and sends it twice. V.23
channel 1 hides this completely — 160 samples is exactly eight cycles of
`{ 7, 7, 6 }`, so no bit is ever held — and channel 2 came out at a 14.2% bit
error rate with every error at a bit index ≡ 2 (mod 3). The blob has the same
contract and would have been driven by the same wrong caller, so the two would
have agreed about sending the wrong bit.

It also settled an open question rather than finding a fault:
`bwchdem.c`'s mark resonator solves to 364 Hz against a nominal 390 and the
offset is undocumented in the original. It demodulates a standards-conformant
390/450 signal with no errors, so it is neither a reconstruction error nor a
defect in practice.

---

## What this tier cannot do

- **It is not a conformance test.** It shows two implementations agree, which
  is strong evidence and not a certificate. Where they disagree, either could
  be the one at fault, and `t_spandsp_replay.c` exists precisely to ask which.
- **It says nothing about timing or state machines** beyond what a bit stream
  exercises. The V.8 tests are the exception: they negotiate, so they cover
  sequencing as well as modulation.
- **It cannot reach anything SpanDSP does not implement**, which is most of
  what is left: V.32, V.34, V.90, V.92. Those will rest on tiers 1 and 2, and
  the reconstruction should say so wherever it makes a claim they cannot check.
