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

### `FPM_cos_table` / `FPM_sin_table` — quarter-wave sine, 257 entries each ✅

```
cos[i] = trunc(32768 * cos(i * pi / 512))
sin[i] = trunc(32768 * sin(i * pi / 512))      i in [0, 257)
```

**Truncated, not rounded.** Rounding differs on 114 of the 257 entries, all by
exactly 1 — the same flat ±1 LSB signature that made the `FixedRC` banks
resist fitting. Here it resolved cleanly: the residual was always 0 or −1,
never +1, which is the fingerprint of truncation rather than of precision
noise. That test was then applied back to `FixedRC`, and it came out negative: the
resampler banks' residual is spread roughly symmetrically from −4 to +4 under
round, truncate *and* floor, whereas a rounding-mode mismatch would be
one-sided like the sine table's strict {0, −1}. So the `FixedRC` fit really is
limited by precision or by a slightly different design, not by rounding, and
the "~9.6 dB worse than a clean regeneration" conclusion stands. Recorded so
task 8 does not chase it again.

Also `sin[i] == cos[256 - i]` exactly, so the two tables are one quarter wave
stored twice. Both are kept because the original has both and `FPM_phasor`
indexes them independently.

257 entries, not 256, so the linear interpolation can read `idx + 1` without a
bounds check — the opposite of `FPM_sqrt_table`, which is one entry *short* of
what its index expression produces (deviation D1). Same library, same era,
opposite outcome.

`t_fpm_phasor` checks the generator against all 514 extracted entries.

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

### The design rate is `L * f_in`, not a fixed host rate

Worth stating explicitly because it is easy to get wrong — an earlier version
of `tools/rcfilter.py` did. `Check_Combination(in, out)` sets `down = in/gcd`
and `up = out/gcd`, so the conversion is `out = in * up / down` and the
polyphase design rate is `up * f_in`, where `f_in` is the **input** rate of
that particular conversion.

For the pair `dp_wrapper` actually uses:

| direction | mode | down:up | design rate |
|---|--:|--:|--:|
| host → pump, 9600 → 8000 | 3 | 6:5 | 5 × 9600 = **48000** |
| pump → host, 8000 → 9600 | 2 | 5:6 | 6 × 8000 = **48000** |

Both land on 48 kHz. Their binding Nyquist is identical too — `min(1/2L, 1/2M)`
is `1/12` either way — and their cutoffs agree closely (0.07925 vs 0.07825 of
`fs`, i.e. 3804 Hz and 3756 Hz). They are near-identical filters at the same
design rate; they exist as separate tables only because the polyphase structure
differs, 5 branches versus 6, so the prototype length must be a multiple of 5 in
one case and 6 in the other.

`tools/rcfilter.py` therefore reports frequencies **normalised to the design
rate**, which holds at any input rate; pass `--fin` for absolute figures.

### The cutoff design rule

Across all 18 banks (`docs/rc_banks.md`), the −6 dB point sits at a consistent
fraction of the binding Nyquist:

```
binding Nyquist / fs = min(1/(2*up), 1/(2*down))
-6 dB cutoff        ~= 0.94 * binding Nyquist          (range 0.92 .. 0.97)
```

Every bank falls in that range except **mode 6 at 0.792**, and that is the
expected exception: mode 6 (1:5) shares mode 3's table, which was designed for
mode 3's stricter `1/12` requirement rather than mode 6's `1/10`. The outlier
is evidence *for* the rule, not against it.

Stopband floors run −55 to −78 dB, tracking the tap budget rather than a single
specification, so each bank was designed individually to its own transition
width and tap count.

**This rule, not the coefficient values, is what an 8 kHz retarget needs.**

### Window fit

Mode 3's prototype fits a Kaiser-windowed sinc closely but not exactly:

| | value |
|---|---|
| window | Kaiser, β ≈ 7.0 |
| cutoff | ≈ 0.0791 × fs (3796 Hz at fs = 48000) |
| best max error | **4 LSB** against a peak of 12959 (0.03%) |
| exact coefficients | 42 of 161 |

Searching β ∈ [6.0, 8.2], cutoff ±3%, both Kaiser normalisation conventions and
free gain does not get below 4 LSB. So the *design* is recovered — enough to
redesign the filter at any rate — but the exact generation recipe is not, and
would depend on tool-specific details of whatever produced the original.

**Consequence for the method.** Unlike `FPM_sqrt_table`, this generator does not
reproduce the bytes exactly, so it cannot serve as the bit-exact source. The
extracted tables stay as the reference for differential testing, and the design
above is recorded as the *regeneration recipe* for the retarget. That
distinction is deliberate and should not be quietly collapsed.

### What the 4 LSB actually costs, and which filter is better

The coefficient difference is small but not negligible where it lands.
Measured against the fitted design:

| | stopband floor, 5.2-24 kHz |
|---|--:|
| ideal Kaiser, unquantised | **-80.6 dB** |
| our Q14 rounding of it | **-77.8 dB** |
| the original | **-68.2 dB** |

So Q14 quantisation costs only about 3 dB, while the original sits a further
**9.6 dB worse**. The worst-case spectral difference between the two filters is
-56.7 dB relative to DC gain — *above* the original's own stopband floor, which
is why the difference shows up there and nowhere else. In the passband the two
are indistinguishable: 0.475 dB peak-to-peak ripple versus 0.495 dB.

**The original's stopband is limited by its own coefficient noise, not by its
window.** Two pieces of evidence:

- The stopband is not equiripple — ripple peaks decay from -67.8 dB near the
  transition to -83 dB at high frequency — so it is a windowed design, not
  Remez. A window that produces -80.6 dB unquantised cannot produce -68 dB
  unless something else is adding error.
- The residual is **flat with respect to coefficient magnitude**: mean 1.65 LSB
  on taps above 2000, 1.82 LSB on taps below 200. A parameter mismatch (wrong
  cutoff, wrong beta, wrong gain) produces error *proportional* to the
  coefficients. Flat error across four orders of magnitude is the signature of
  limited-precision arithmetic or rounding, applied uniformly.

No rounding mode explains it either — round, truncate, floor, and single-
precision variants of each all plateau at 4-5 LSB with only ~17 of 161
coefficients matching.

So the original was very likely generated with a limited-precision tool, and
**a clean regeneration is strictly better** — about 9.6 dB more stopband
attenuation for free.

**Practical impact: real but modest.** Aliased energy at -68 dB is already far
below the SNR that governs V.22 or V.32 slicing decisions, so this will not
change modem performance. It is simply better, and costs nothing.

**Important:** this improvement does *not* apply to the reconstruction as
shipped. We keep the original extracted bytes, so the built filter is
bit-identical to the original and Tier-1 differential testing still holds. The
9.6 dB only materialises if and when the banks are regenerated for 8 kHz —
which is the right trade, and the reason the design and the bytes are tracked
as separate artefacts.

Before trusting regenerated filters, our own design path needs auditing —
rounding mode, series convergence, per-branch normalisation, and fitting
against the response rather than coefficient distance. Tracked as a task.

### Remaining for `FixedRC`

- Reconstruct `RcFixed_Create/_Reset/_Delete/_Resample` themselves and
  differential-test them.
- Extract the remaining banks' coefficients into the reconstruction, keeping
  the extracted bytes as reference and the design rule as the retarget recipe.

## V.34 `costbl`

256 entries, one full period, Q14, at `.data+0x6d60`. Global, and shared
between `DFTC.c` and `V34RX.c`.

**Derivation:** `(short)(16384.0 * cos(2 * pi * i / 256))`, truncated toward
zero, **for 255 of the 256 entries**. Index 128 holds -16383 where the
formula gives -16384.

This is the only table recovered so far that its own generator does not
reproduce, so it is the only one emitted as literal data for a reason other
than convenience. See findings F88 for why the exception is a hand-applied
floor rather than rounding, and why regenerating the table would be wrong at
an index that is reached on every half turn of the phase accumulator.

## V.34 `intcoef1` / `intcoef2` / `intcoef3` — the FSK interpolator

Three tables of 12 shorts, Q14, global in the object, at `.data+0x7088`,
`+0x7070` and `+0x7058`. Used only by `fskdetect`.

**Structure, confirmed:** `intcoef3` is `intcoef1` reversed, exactly, and
`intcoef2` is its own reverse. Interleaving them as `h[3k+p]` -- phase 1 into
`h[0], h[3], h[6]…`, phase 2 into `h[1], h[4]…`, phase 3 into `h[2], h[5]…` --
gives a **36-tap symmetric** prototype. So these are one linear-phase
low-pass decomposed into the three phases of a 3x interpolator, which is what
the reversal relationship between phases 1 and 3 means.

**Gains, measured:**

```
sum(intcoef1) = 21859   = 1.3342 x 16384
sum(intcoef2) = 21872   = 1.3350 x 16384
sum(intcoef3) = 21859   = 1.3342 x 16384
sum(prototype) = 65590  = 4.0033 x 16384
```

Each phase carries a gain of 4/3 rather than the unity an amplitude-preserving
interpolator would use. `t_v34fsk` asserts the reversal and the symmetry, so
a future regeneration has a structural property to fail against and not only
a byte comparison.

## V.34 `fsklpfcoeff600` — the post-detection low-pass

80 taps, symmetric, all positive, at `.data+0x71a0`; a local symbol, so
DPSK.c owns it.

```
sum = 49120 = 2.9980 x 16384          -3 dB at about 280 Hz
                                       (at the 28800 Hz interpolated rate)
```

**The name is a bit rate, not a cutoff** — V.34's INFO messages are sent at
600 bit/s and 280 Hz is a sensible matched cutoff for 600 baud. See
findings F91 before concluding that a table named for a frequency has been
transcribed wrongly.

## V.34 timing recovery: the half-baud pair

Ten tables in `v34filters.c`'s global block, contiguous from `.rodata+0x3590`
to `+0x3624`.

```
negHalfBaud_Acoef_Real   16384  -22805     -33      posHalfBaud_Acoef_Real   same
negHalfBaud_Acoef_Imag       0   22853  -15908      posHalfBaud_Acoef_Imag   negated
negHalfBaud_Bcoef_Real      64       0     -64      posHalfBaud_Bcoef_Real   same
negHalfBaud_Bcoef_Imag       0       0       0      posHalfBaud_Bcoef_Imag   same
```

**`pos` and `neg` differ in exactly one thing: the sign of `Acoef_Imag`.** So
they are one complex second-order section and its mirror image about DC. Both
numerators are `64 * (1 - z^-2)` with a zero imaginary part — a real
band-pass numerator on a complex denominator.

**Derivation.** Normalising by `a0 = 16384` (Q14) and solving
`a0 + a1 z^-1 + a2 z^-2`:

```
   neg:  poles at r = 0.9844, -1179.3 Hz   and   r = 0.9863, -1223.9 Hz
   pos:  the same two, positive
                                            (frequencies quoted at 9600 Hz)
```

Two poles a little either side of **1/8 of the sample rate** — 45 degrees.
An eighth of the sample rate is half the baud rate exactly when the timing
path runs at **four samples per symbol**, which is why one set of coefficients
serves all five V.34 baud rates instead of five sets. The resampler ahead of
them carries the rate, and the object names it: the C++ side has
`ResamplerTiming::adjustHalfBaudBpfGain(float)` and
`ResamplerTiming::SdHalfBaudDft(float)`.

The split pole pair rather than a single one is a deliberately widened
resonance — a band-pass, not a resonator, so it tolerates the timing offset it
is there to measure.

## V.34 `V34TimingIIR_Acoef` / `_Bcoef` — the same filter, written once

```
   A = { 8192, -22805, 31776, -22143, 7723 }      (a0 = 8192, so Q13)
   B = { 1768, 0, -3536, 0, 1768 }
```

`B` is exactly `1768 * (1 - z^-2)^2` — the half-baud numerator squared. And
`A`'s four poles are:

```
   r = 0.9860 at +/-1224.6 Hz        r = 0.9847 at +/-1178.6 Hz
```

which are the half-baud pair's two frequencies, mirrored. So this is a
fourth-order **real** filter that is the cascade of the two complex sections
above, and the object carries the design in both forms. Which one
`V34TimingFilter` uses where is not yet established — it reads the complex
pair; nothing reconstructed so far reads `V34TimingIIR_*`.

`t_v34ec` asserts the structural properties — the mirror relationship, the
real numerators, `B`'s symmetry — as well as the bytes, so a future
regeneration has something to fail against beyond a byte comparison.

## V.34 `sqrt_table`

192 entries at `.rodata+0x2860`, Q15 in and Q15 out, read by
`V34demodulate`'s level estimator.

**Derivation, exact for all 192 entries:**

```
   sqrt_table[i] = floor(sqrt((i + 0x40) * 128 / 32768) * 32768)
```

TRUNCATED, not rounded.  Worth stating plainly because rounding is the
natural first guess and misses 98 of the 192 entries by one -- which reads
as "exact to 1 LSB", i.e. as a table built in floating point and rounded
inconsistently, rather than as the exact output of a deliberate generator.
It is the latter.

The index covers mantissas in `[0.25, 1)`, which is what normalising to
bit 30 and halving on an odd exponent leaves. The lookup is followed by a
right shift of half the exponent, so the whole routine is a normalise-index-
shift square root.

Unlike `costbl`, this one **is** reproducible from its generator — the only
V.34 table so far that is. It is still emitted as data: the generator is a
claim about intent, the bytes are the reference.

## DTMF: two detectors, two coefficient families, one design

The object has **two DTMF receivers** that share no code and no data
(finding F1410).  `Dtmf.c`'s is float and runs at 4 kHz off a bank of eight
notches; `Dtmf_Rx.c`/`Dtmf_Detector.c`'s is Q14 fixed point, runs at the line
rate, and is what Caller ID uses.  Their coefficient tables are laid out
differently and are derived here separately, but they are the same filter
design at two precisions: **a notch at the tone, zeros ON the unit circle,
poles just inside it.**

### The float bank: `eur_coef`, `us_coef`, `biascoef`

Four floats per section, read by `notch()` (see `include/dsplib/notch.h`):

                        1 + c0 z^-1 + z^-2
        H(z) =  c3 * ---------------------------
                      1 - c1 z^-1 - c2 z^-2

        c0 = -2 cos(w0)     c1 = 2 r cos(w0)     c2 = -r^2     c3 = r

| bank | sections | w0 | r |
|---|--:|---|---|
| `us_coef` | 8 | 2*pi*f/4000, f = the eight DTMF tones | 0.98 0.98 0.98 0.98 0.97 0.97 0.97 0.965 |
| `eur_coef` | 8 | the same eight | 0.955 0.95 0.945 0.945 0.945 0.93 0.93 0.925 |
| `biascoef` | 1 | 2*pi*50/4000 | 0.85 |

**Solving `c0` for `f` gives 697.000 770.000 852.000 941.000 1209.000
1336.000 1477.000 1633.000 at fs = 4000, and 49.999 for `biascoef`** -- three
decimal places, no residual worth reporting.  At 8000 the same numbers read
as 1394, 1540, ... 3266 and 100, which is how the sample rate was settled
(finding F1412): `dtmf_detect` is called at 8000 and processes every second
sample.

`c0` is BIT-IDENTICAL between the two plans in all eight sections, so the two
differ only in pole radius -- a narrower notch is a tighter frequency
acceptance, which is the real difference between the two regional DTMF
specifications.

**Regenerating at another rate** is therefore: pick r from the table above,
compute `c0 = -2 cos(2*pi*f/fs)`, and set `c1 = -c0*r`, `c2 = -r*r`, `c3 = r`.
The identities `c2 == -c3*c3` and `c1 == -c0*c3` hold in the shipped bytes to
1e-6 relative, which is what a float carries, so the generator reproduces the
tables to within a float's precision.  They are kept literal all the same,
because the differential test compares them against the object byte for byte.

### The fixed-point bank: `MTD{1..8}_COEF_{8000,9600}`

Five shorts per section, one `FPM_iir_filt` biquad, in that engine's order
`{a2, b2, a1, b1, b0}`, everything Q14:

        a2 = -13271 = -round(0.81 * 2^14)       r = 0.9, all sixteen tables
        b0 = b2 =  16384                        unit numerator ends
        a1 =  round(2 * 0.9 * cos(w0) * 2^14)
        b1 = -round(2 * cos(w0) * 2^14)

with `w0 = 2*pi*f/fs` and `fs` the table's own rate.

**The `_8000` and `_9600` members of a pair are not related to each other.**
Each is generated independently from (tone, rate); neither is a resampling or
a scaling of the other, and there is no closed form that takes one to the
other.  What the derivation above buys is regeneration at a THIRD rate, which
is the point of this document.

**Accuracy of the generator.** Solving `a1` and `b1` back for `f` gives the
eight DTMF frequencies at the table's own rate to within 0.05 Hz for fifteen
of the sixteen tables, and the generator reproduces those fifteen to within
+-1 LSB -- but not bit-exactly, and the residuals are not a consistent
rounding rule.  Solving the pair jointly puts MTD1_8000 at f = 697.00 and
MTD2_8000 at f = 770.10; something carrying less precision than a double
stood between the frequency and the table.  So the bytes stay literal.

**MTD7_COEF_9600 does not fit.**  `a1 = 16751` is 1477.04 Hz like every other
table; `b1 = -21143` is 1328.45 Hz where the design gives -18613.  Its zeros
and its poles are 150 Hz apart, which is not a notch at either frequency.
D250 and finding F1413; reproduced as found, with no mechanism proposed.

### Where the two extra fixed-point sections come from

`DTMF_MTD_detect` runs TEN sections per sample, not eight.  The two extra are
group-splitting pre-notches and they have no tables of their own: the low
group is fed through `MTD5_COEF` (1209 Hz) and the high group through
`MTD4_COEF` (941 Hz).  Anyone regenerating this bank at a new rate gets those
two for free and must not generate a ninth and tenth table.  Finding F1414.
