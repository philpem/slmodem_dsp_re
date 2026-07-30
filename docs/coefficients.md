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

**Coefficient ordering: settled, from `RcFixed_Resample`'s address arithmetic.**

The inner product at `.text 0x0b13df-0x0b1442` is unambiguous:

```
mov    (%edi),%ebp            ebp = state[0]              coefficient base
movswl 0x19a(%esi),%ecx       ecx = taps                  N
movswl 0x194(%esi),%eax       eax = state[0x194]          phase index
imul   %ecx,%eax              eax = phase * N
lea    0x0(%ebp,%eax,2),%esi  esi = base + phase*N        <- coefficient pointer
lea    0x4(%edi,%edx,2),%ebx  ebx = state+4+(pos-N)*2     <- history pointer
...
movswl (%esi),%eax ; add $2,%esi     walk coefficients forward
movswl (%ebx),%edx ; add $2,%ebx     walk history forward (oldest -> newest)
imul   %edx,%eax ; add %eax,%edi     accumulate
...
sar    $0xe,%edi              >> 14                       <- Q14 confirmed
mov    %di,0x0(%ebp)          store int16
```

So the table is **phase-major**: phase `p` occupies `coeff[p*N .. p*N+N-1]`
contiguously. Both pointers walk *forward*, and the history walks oldest to
newest, so `coeff[0]` multiplies the **oldest** sample. Standard convolution
pairs the newest sample with `h[0]`, so the stored order is reversed relative
to convolution order:

```
coeff[p*N + k] = h[(N-1-k)*L + p]        equivalently
h[j*L + p]     = coeff[p*N + (N-1-j)]
```

That reverse-tap ordering is confirmed independently by the frequency response.
It gives a clean lowpass; the forward-tap alternative gives bumps at exactly
`fs/8` and `fs/4`, the signature of a wrong de-interleave:

| | 3000 Hz | 4000 Hz | 4800 Hz | 6000 Hz | 12000 Hz |
|---|--:|--:|--:|--:|--:|
| reverse-tap (correct) | 0.0 dB | -13.4 | -85.0 | **-73.8** | **-81.4** |
| forward-tap | -6.3 | -27.5 | -85.0 | **-3.0** | **-12.1** |

**Why inference failed earlier.** All four candidate interleavings appeared to
give the same symmetry error of 2768, which suggested none was right. That was
a measurement error: symmetry was being tested about `h[i] == h[159-i]`, the
midpoint of a 160-element array. The prototype is in fact exactly symmetric
about `h[i] == h[160-i]` — **max error 0** — i.e. it is a 161-tap Type-I
linear-phase FIR centred at index 80, of which only the first 160 taps are
stored because 161 does not divide into `L * N = 5 * 32`. The dropped final tap
equals `h[0] = 6`, negligible against a peak of 12959.

The lesson worth keeping: a near-miss symmetry test is worse than none, because
it argues actively against the correct answer. The address arithmetic settled in
minutes what heuristics had made look impossible.

### Mode 3 design (9600 -> 8000), fully characterised

| property | value |
|---|---|
| polyphase | L = 5 branches of N = 32 taps, phase-major |
| prototype | 161-tap Type-I linear-phase FIR, last tap dropped |
| scaling | Q14; accumulator `>> 14`; each phase sums to 16384 |
| design rate | `fs = L * 9600 = 48000 Hz` |
| passband | flat to 3400 Hz (-0.49 dB); -6 dB at 3800 Hz |
| transition | 3400 -> 4400 Hz |
| stopband | >= 49.6 dB from 4400 Hz; -85 dB at 4800 Hz |

This is not a generic `pi/6` anti-alias filter. The passband is placed to
preserve the 300-3400 Hz telephony voiceband exactly, and the stopband starts
below 4000 Hz, the Nyquist frequency of the 8 kHz side. That is a deliberate
voiceband design, and it is the specification the 8 kHz retarget must
reproduce — not the coefficient values.

### Remaining for `FixedRC`

- Fit the prototype to a named design (windowed sinc, Remez, or similar) and
  write the generator. The 49.6 dB stopband with a ~1000 Hz transition at
  161 taps is consistent with a windowed-sinc design; identifying the window
  is the next step.
- Repeat for the other 17 banks. The ordering rule above applies to all of
  them, so this is now mechanical.
