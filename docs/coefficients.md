# Coefficient tables: derivations

The project's method is that a coefficient table is not reconstructed by
copying its bytes. Each table gets its **design recovered** — family, length,
scaling, cutoff — and a **generator** that reproduces the original bytes
exactly, checked in `make test`. The generator is the maintainable artefact;
the extracted bytes are only the reference it is checked against.

This matters most for the eventual 8 kHz retarget: a regenerated filter is a
re-run of the design, whereas a copied table can only be resampled, which is
not the same thing and loses the design margin.

---

## Recovered

### `FPM_sqrt_table` — Q15 square root, 193 entries ✅

```
table[i] = floor(32768 * sqrt((i + 64) / 256.0))      i in [0, 193)
```

Derived from the index expression `((mantissa + 64) >> 7) - 64`: entry `i`
corresponds to a mantissa of `(i + 64) * 128`, i.e. a Q15 fraction of
`(i + 64) / 256`.

`FPM_sqrt_table_generate()` in `src/dsp/fpm_sqrt.c` implements it, and
`t_fpm_sqrt` checks it against all 193 extracted entries.

The original's table stops at 192 entries, one short of what its own index
expression can produce — see deviation D1 in `docs/deviations.md`.

---

## In progress

### `FixedRC` polyphase resampler banks — 18 tables

**Established.**

Each mode's initialiser stub (reached through the 20-way jump table at
`.rodata 0x10f14`) does exactly two things: store a coefficient pointer at
`state[0]` and a tap count at `state[0x19a]`.

The storage rule is confirmed by layout: **`bytes = taps * up * 2`**, i.e.
`up` polyphase branches of `taps` `int16` coefficients each. Every table
either abuts the next exactly or leaves only 4–24 bytes of alignment padding;
no overlaps and no shortfalls across all 18 tables, which would be a remarkable
coincidence if the rule were wrong.

| mode | down:up | coeffs | taps | phases | bytes |
|--:|--:|---|--:|--:|--:|
| 2 | 5:6 | `0x10c80` | 36 | 6 | 432 |
| 3 | 6:5 | `0x10b40` | 32 | 5 | 320 |
| 4 | 1:6 | `0x10c80` | 36 | 6 | 432 |
| 5 | 6:1 | `0x10aa0` | 68 | 1 | 136 |
| 6 | 1:5 | `0x10b40` | 32 | 5 | 320 |
| 7 | 5:1 | `0x10a00` | 68 | 1 | 136 |
| 8 | 1:4 | `0x10880` | 46 | 4 | 368 |
| 9 | 4:1 | `0x107e0` | 68 | 1 | 136 |
| 10 | 5:24 | `0x10060` | 40 | 24 | 1920 |
| 11 | 24:5 | `0x0fda0` | 68 | 5 | 680 |
| 12 | 4:5 | `0x0fc80` | 28 | 5 | 280 |
| 13 | 5:4 | `0x0faa0` | 58 | 4 | 464 |
| 14 | 2:3 | `0x0f9a0` | 42 | 3 | 252 |
| 15 | 3:2 | `0x0f8e0` | 48 | 2 | 192 |
| 16 | 3:10 | `0x0f5e0` | 38 | 10 | 760 |
| 17 | 10:3 | `0x0f440` | 68 | 3 | 408 |
| 18 | 9:10 | `0x0f140` | 38 | 10 | 760 |
| 19 | 10:9 | `0x0ef00` | 32 | 9 | 576 |

Modes 0 and 1 use a different state layout (40 bytes, three sub-allocations)
and three separate tables at `0x10f0e`, `0x10e60` and `0x10e30`.

**Sharing.** Modes 2 and 4 share a table, as do 3 and 6. Both pairs share an
*up* factor (6 and 5 respectively). The pattern is consistent with designing
one filter per interpolation factor at the more conservative of the two
required cutoffs: mode 3 (up 5, down 6) needs `pi/6` while mode 6 (up 5,
down 1) needs only `pi/5`, so a single `pi/6` design serves both — slightly
narrow for mode 6, but correct. The decimation-only modes (up = 1: modes 5, 7,
9) each get their own table, since there is no interpolation factor to share.

**Scaling is Q14, not Q15.** Every phase of mode 3 sums to 16384 or 16386 —
that is 1.0 in Q14. Phase DC gains are equal by construction, as they must be
for a polyphase bank.

**Unresolved: the coefficient ordering within a table.**

The obvious next step is to recover the underlying prototype FIR and fit its
design (windowed sinc, window type, cutoff). That requires knowing how the
`taps * up` values map onto prototype taps, and **this has not been settled**.

All four plausible interleavings — `(phase, tap)`, `(phase, reversed tap)`,
`(reversed phase, tap)` and `(reversed phase, reversed tap)` — give the same
symmetry error of 2768 against a peak of 12959, i.e. none produces the linear-
phase symmetric prototype a windowed-sinc design would.

The evidence is genuinely mixed, which is why it is being left open rather than
guessed:

- *For* a single symmetric prototype: the main lobe **is** symmetric about
  index 80 under ordering `(phase, tap)` — `3143` at the centre with `5911`,
  `8624`, `10905`, `12425` mirrored either side. Phase peaks progress 15, 15,
  15, 16, 16, exactly the fractional-delay ramp of a polyphase decomposition
  whose centre falls between taps.
- *Against*: the symmetry breaks immediately outside that lobe, and the
  reconstructed prototype's magnitude response, while a good lowpass overall
  (−85 dB at 4800 Hz, −94 dB at 9600 Hz), has implausible bumps at −3 dB at
  6000 Hz and −12 dB at 12000 Hz — precisely `fs/8` and `fs/4`. Artefacts at
  exact binary fractions of the rate are the signature of a wrong
  de-interleave, not of a real filter.

**Settle it by reading `RcFixed_Resample`'s addressing**, not by inference.
That function (`.text 0x0b12a0`, 2640 bytes) indexes the coefficient array
directly, so its address arithmetic is definitive where symmetry heuristics are
not. Once the ordering is known, the prototype can be extracted and fitted, and
only then can a generator be written.

Until that is done these tables must not be copied into the reconstruction as
opaque bytes — doing so would carry the 8 kHz retarget's whole purpose away
with it.
