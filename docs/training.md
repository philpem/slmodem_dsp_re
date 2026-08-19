# V.34 training, pre-emphasis and equalisation

How the datapump gets from off-hook to a carried bit rate: what it measures,
what it decides, what it tells the far end, and what it keeps. Written to be
the source the eventual code comments are drawn from, so **every claim here
carries its evidence** and anything unverified says so.

Companion documents: `callprog_states.md` (dial and answer), `rate_assumptions.md`
(the rate ladder's constants), `deviations.md` (where we differ from the blob),
`findings.md` (the measurements).

## Evidence key

Each section is marked:

* **[CODE]** — read out of the reconstruction, with `file:line`. The
  reconstruction itself is differential-tested against the blob, so this is as
  strong as the tests are.
* **[MEASURED]** — observed on the bench or in a simulation, with a finding
  number.
* **[SPEC]** — ITU-T V.34 (02/98), cited by clause.
* **[INFERRED]** — reasoning not yet nailed to either. Treated as a hypothesis,
  never as a fact, and flagged in place.

---

## 1. The shape of the whole thing

Four phases. The microstate names are the object's own, recovered from its
debug strings (`v34hshak.c`) **[CODE]**:

| phase | microstates | what happens |
|---|---|---|
| 1 | `TX_PHASE1_CALL` 55, `RX_PHASE1_CALL` 58, `RX_PHASE1_ANS` 49 | V.8 selects the modulation; round-trip delay is measured |
| 2 | `RX_PHASE2_CALL` 59, `RX_PHASE2_ANS` 50, `TX_L1` 51 | **line probe** — the channel is measured and the transmit parameters chosen |
| 3 | `TX_PHASE3_ANS` 48 | training sequences; the equaliser, echo canceller, AGC and timing converge |
| 4 | `TRNSEG4A` | final training, precoder coefficients, **rate selection** |

The two subsystems this document is about work at different points and on
different sides of the link:

* **pre-emphasis** is chosen in phase 2, from 25 numbers, and applies to the
  **far end's transmitter**. It is a request we send, not a setting we hold.
* **the equaliser** runs in our **receiver**, adapts every symbol from phase 3
  onward, and never stops.

They are a coarse/fine pair. What the pre-emphasis fails to cancel — or
over-corrects — is left for the equaliser to undo with finite taps.

---

## 2. The line probe and how it is measured

**[SPEC]** V.34 §11.2 and Table 17: the probing signal is tones 150 Hz apart
from 150 Hz to 3750 Hz, and the tones at **900, 1200, 1800 and 2400 Hz are
omitted**. The receiver measures the noise floor in those four slots.

**[CODE]** `v34hshak.c:956` — `#define DFT_BIN(n) ((short)((n) << 8))`, and
`dftfreqinit` (968) writes bins 1..25 with increments `n<<8`, a phase step of
n*256 in a 14-bit accumulator. At the 9600 Hz rate the datapump runs at, bin
`n` is **n × 150 Hz**. `V34_PROBE_BINS` is 25 (`v34fsk.h:47`), stored at
`+0xa320`.

So `bins[i]` is (i+1) × 150 Hz, and in particular:

    bins[4]  = bin 5  =  750 Hz   the reference the search compares against
    bins[18] = bin 19 = 2850 Hz   band edge for 2800 baud
    bins[19] = bin 20 = 3000 Hz   3000 baud
    bins[20] = bin 21 = 3150 Hz   3200 baud
    bins[22] = bin 23 = 3450 Hz   3429 baud
    indices 5, 7, 11, 15          the four omitted tones — NOISE, not signal

**THE PUBLISHED SCALE HAS A FLOOR, and it loses bins.** `dftenergy` writes
`energy = (short)(e >> 16)` where `e = re*re + im*im`, plus `shift`, the count
of redundant sign bits of `e`. A bin needs `e >= 65536` to register 1 at all;
anything quieter reads **zero** and is indistinguishable from any other quiet
bin. `shift` still carries the magnitude there — so the information is not
lost, but any consumer reading `energy` alone throws it away **[CODE]**.

`energy` is a **power** quantity. Converting it to dB is `10*log10`, not
`20*log10`. Getting that wrong doubles every tilt figure, is invisible on a
fixed channel, and cost this project a full experiment to catch — finding 1909
**[MEASURED]**.

---

## 3. Choosing the pre-emphasis filter

### 3.1 What the filters are

**[SPEC]** V.34 §5.4.1 defines eleven pre-emphasis filters in **two families**,
not one ladder of eleven strengths:

* **Table 3**, indices 0–5: one parameter α = 0, 2, 4, 6, 8, 10 dB.
* **Table 4**, indices 6–10: two parameters β (0.5–2.5) and γ (1.0–5.0 dB).

Figures 1 and 2 plot magnitude against **normalised frequency f/S from 0 to
1.2**, and conformance is required over `(d/e − 0.45)` to `(d/e + 0.45)` —
415 Hz to 3502 Hz at 3429 baud with a 1959 Hz carrier. They are **ramps across
the band**: tilt correctors for a subscriber loop's high-frequency roll-off,
not notch correctors for one frequency.

### 3.2 What the object does

**[CODE]** `probe_preemp` in `v34hshak.c`. It is a **two-point tilt meter**:

    ref = bins[4].energy            /* 750 Hz  */
    x   = bins[edge].energy         /* the band edge for this baud rate */
    i   = 5
    loop:  x = (x * k) >> 14        /* k = 0x6626 = 1.5961, one step = 4.06 dB */
           i++                      /* D53: advanced BEFORE the test */
           if x > ref: return i
           if i > 9:   return 10

It multiplies the band-edge bin by 1.5961 until it exceeds bin 4, and counts
the steps. The counter is preset to 5 and advanced *before* the test, so
`i == 5` cannot hold: **index 0 is unreachable** and the author's own `return 0`
arm is dead. That is deviation **D53**.

The counter's range — 6 to 10 — is exactly Table 4, which is deliberate rather
than accidental: five tilt buckets onto five filters, complete and exact
(finding 1477).

### 3.3 What is wrong with it

**[MEASURED]**, findings 1904–1912:

| weakness | evidence |
|---|---|
| two samples out of 25 — one dud bin moves the answer a whole bucket | 1911 |
| quantised to 4.06 dB — cannot express 1.5 dB of tilt at all | 1911 |
| samples the band-edge bin, which on a codec-limited path is inside the roll-off corner | 1907 |
| reads ~5× high: 8.17 dB where the band trend is 1.58 dB | 1911 |
| index 0 unreachable, so "flat" cannot be requested | D53 |
| reads `energy` only, discarding `shift`, so sub-floor bins are lost | §2 above |
| chosen once and never revisited within a connection | 1912 |

### 3.4 The fitted alternative (branch `improve/v34-training`)

**[CODE]** `probe_preemp_fit`, reachable only through
`dsplib_v34_fit_preemp` (default 0, set from the environment by
`tools/benchflags.c`, which is linked only into the bench hybrid — never into
the library or any test tier).

It fits a line through every bin that carries a tone, from bin 2 up to the
band edge, excluding the four omitted-tone slots and any zero-energy bin, and
maps the slope to a Table 3 index. Two design choices worth keeping:

* **Theil–Sen, not least squares.** The slope is the median of pairwise
  slopes. Plain OLS gives a wrecked bin its full leverage and, simulated at 8%
  duds, leaves *more* tilt uncorrected than the object's crude counter does
  (1.36 dB against 0.85) — it would have been a regression on exactly the
  channels it was meant to help. Finding 1911.
* **Integer log2 with linear mantissa interpolation**, not libm and not
  `shift` alone. `shift` alone quantises to 6.02 dB, which is *coarser* than
  the counter it replaces.

---

## 4. The equaliser

### 4.1 Geometry **[CODE]**

`struct v34_equalizer`, `v34filt.h:282`, at object offset `+0x3cc`:

    80 complex taps            V34_EQ_TAPS = 80
    dly_re/dly_im[80]          the delay line
    re/im[80]                  tap high halves
    re_frac/im_frac[80]        tap low halves — taps are 32-bit
    cursor                     circular index
    centre taps: 8, starting at index 36   (V34_EQ_CENTRE_FIRST/TAPS)

`V34EqualizerCleanUp` zeroes everything and sets tap 40 to unity — a flat
initial response, a centre spike.

### 4.2 The update rule **[CODE]**

Full complex LMS, at 32-bit width. `eq_adapt_tap` in `v34filters.c`:

    dc = -e * conj(d)

spelled out as: the real part loses `dre*ere + dim*eim`; the imaginary part
loses `dre*eim` and gains `dim*ere`. The tap is reassembled from its hi/lo
pair each time, so the fractional half genuinely accumulates.

The error fed in is scaled first, in `v34rx.c:2635`:

    er = (dr * f218) >> 16
    ei = (di * f218) >> 16

where `dr`, `di` are the difference between the received point and the target.

### 4.3 The step size is scheduled, not adaptive **[CODE]**

`f218` takes four values and the schedule is driven by **symbol counts**, not
by how converged the equaliser actually is:

| value | ≈ fraction of error | where set |
|---|---|---|
| 0x400 | 0.016 | `v34hshak.c:1532` |
| 0x2000 | 0.125 | `v34rx.c:1603`, `1641` — end of frame |
| 0x4000 | 0.25 | `v34rx.c:551` (init), `1639` — half way through the frame |
| 0x7000 | 0.44 | `v34rx.c:2453` — acquisition |

`decoderv34` steps it down at `f124 == baud_rate/2` and again at `f124 == baud_rate`
(`v34rx.c:1639-1641`) — a progress signal for whoever is counting symbols, not
a convergence measure.

### 4.4 The centre taps are updated twice, by two different arithmetics **[CODE]**

`V34EqualizerCenterAdapt` applies the same gradient to the 8 centre taps, at
**twice the error** (`v34rx.c:2675`), but:

* it assembles the tap **from its high half alone**, so whatever
  `V34EqualizerAdapt` accumulated in the fractional half is discarded on the
  way in and left stale on the way out; and
* it rounds with 0x8000 rather than truncating.

Read as a design, that is plausible — a fast coarse pull on the centre during
acquisition, a fine 32-bit one everywhere afterwards. But **if both run, they
fight over the same eight taps and nothing arbitrates**, and those eight carry
most of the energy (tap 40 is the unity spike). **[INFERRED]** that this puts
a floor under misadjustment; not yet measured.

### 4.5 There is no tap leakage **[CODE]**

`eq_adapt_tap` is `tap = tap − gradient`, with no term pulling an
un-excited tap toward zero. **[INFERRED]**: 80 taps driven by a signal with
nothing above ~3.5 kHz is a near-singular correlation matrix, the textbook
case where unleaked LMS lets un-excited taps wander. Candidate mechanism for
drift on a long hold. Not yet measured.

---

## 5. The error metric, and what the rate is decided from

**[CODE]** `v34rx.c:2630-2660`. `equerr` is **not** an instantaneous reading:

    mag = dr*dr + di*di + previous          accumulates into rx->f220
    n   = (f21c + 1) & 0x3ff                counter wraps at 1024
    when n == 0:
        rx->f21a = mag >> 16                equerr, published
        rx->f224 = f228 >> 16               preerr, the precoder equivalent
        rx->f248 = rx->f24c >> 8            SIGNAL POWER, same block
        accumulators cleared

So it is a **block mean over 1024 symbols** — 0.30 s at 3429 baud, which is
exactly the spacing of `V34EQU` lines in every log **[MEASURED]**, finding 1904.

The rate ladder, `v34hstx1.cpp:2624` **[CODE]**:

    while rate > ratemin:
        term = tx1_ts_scale(cfg, rate, -1)
        if tx1_get(o, TX1_RX250) < term: break
        rate--

and `TX1_RX250` is fed `rx->f21a` or `rx->f224` verbatim (2526, 2539, 2858).

**`equerr` IS NEVER NORMALISED BY SIGNAL POWER**, and the thresholds it is
compared against are absolute. The phase 3→4 boundary applies `V34TXSCALE`
power reduction, so the metric's *scale* moves between operating points: the
same equaliser reads ~50 in phase 3 and ~2200 in phase 4 **[MEASURED]**, and
mistaking those for the same quantity produced — and then withdrew — a whole
false finding (1904). The receiver already computes the power in the same
block (`f248`), so normalising is available and nearly free. **[INFERRED]**
this is the strongest candidate for the rate decision returning wrong answers.

---

## 6. Retrain: what is kept and what is thrown away

Three different things get called "retraining" and they are not the same
**[CODE]**:

1. **Full retrain** — back through phase 1/2. `rxinit` runs, and it calls
   `V34EqualizerCleanUp` (`v34rx.c:525`): **taps cleared, back to the centre
   spike**. The probe is re-measured and the pre-emphasis re-chosen. Nothing
   is carried forward. This is what a `V34PROBEBINS` block counts, one per
   handshake attempt.
2. **Partial re-acquire** — `V34EqualizerClearCenterTaps` (`v34rx.c:2493`)
   clears only the centre taps and drops the `TRAINED` flag.
3. **Rate renegotiation** — touches no taps at all; it re-runs the rate
   decision. Finding 1900's ~+11 s rate climb, which the DTE is never told
   about because `pty CONNECT nnn` is emitted once and never revised.

There is also a shortcut: `is_short` (a retry requesting "short phase 2") skips
the round-trip measurement and reuses `prev_bulk_delay` from the previous
session (`v34hshak.c:4116`).

**[MEASURED]** Successive probes within one call read slightly lower than the
first: first probe 0.72–0.84 dB, later probes ~0.63 mean, across 11 calls.
Small, systematic, and unexplained. Too small to change the index.

---

## 7. Outputs

**Sent to the far end** — these change *its* transmitter:

* pre-emphasis index
* precoder coefficients (six values, bit-reversed into the MP message —
  Tomlinson–Harashima precoding, so the far end pre-distorts and our receiver
  needs less feedback equalisation)
* agreed symbol rate and carrier
* transmit power reduction

**Kept locally, never transmitted**: equaliser taps, echo-canceller state, AGC
gain, timing and carrier phase.

**The negotiated link parameters**: data rate each way (`cfg->txbits`,
`cfg->rxbits`), trellis code, constellation size, nonlinear encoding on/off.
This last group is what the far end's `ATI11` reports, which is why it has
been the only trustworthy rate measurement on this bench (finding 1900).

---

## 8. THE FIRST-ORDER PROBLEM: the link spends most of its time retraining

Everything in sections 3-5 is about choosing well. Measured on this bench,
choosing well is a second-order concern:

    12 calls, both far ends, both estimator arms
    **66% of connected time is spent re-handshaking** -- worst calls 88-90%

A negotiated 26400 that is in data mode a third of the time is an effective
~8000, and that is the speed the bench sees. Two things compound (finding
1921):

* **Every retrain restarts the rate ladder near the bottom.** Within one call:
  14400 -> 24000 -> 26400 -> 26400 -> 4800 -> 28800 -> 4800. The climb begins
  again at 12000-16800 each time and ratchets up through further retrains, so
  a high rate is never held.
* **Every retrain is a full ~10 s handshake.** The object has a short path --
  `local_short`, "requesting short phase2", `prev_bulk_delay` reused instead of
  re-measuring the round trip (§6). It is never taken: across every call
  recorded, `requesting short phase2` appears **0** times and `SILENCERETRAIN`
  **264** times. `local_short` comes from `quick` at
  `v34pcmcreate.cpp:343`, inside the `sessionType == 2` arm -- a **V.92**
  session. A call forced to V.34 with `AT+MS=34,1` cannot reach it.

**FIVE RECEIVER-SIDE MECHANISMS WERE TESTED AND KILLED BEFORE THIS** (findings
1918, 1920): tap drift, rate overshoot, equaliser freeze -> retrain, buffer
underflow, renegotiation mishandling. All assumed a defect in the receiver's
signal processing. The receiver reaches 33600 -- our transmit reaches it to
every far end on every call (1917) -- it simply never gets to keep it.

So read section 9 in that light: the pre-emphasis and equaliser items are real
but small, and the retrain duty cycle is the thing that decides the speed.

### 8.1 The cost, measured independently and quantified (1946, 1947)

The 66% above came from this project's own instrumentation on twelve calls.
It has since been reproduced from a string the **blob** prints -- `"V34 bulk
delay estimation %d (FAR=%d)"`, emitted once per handshake by `ApplyBulkDelay`
-- across **470 captures and 614 handshake intervals**, of which **612 contain
an explicit `SILENCERETRAIN` transition**. That instrument cannot share a bug
with ours, and it agrees: **72% of handshakes are followed by another within
15 seconds.**

The same counter gives the price, over 90 calls, counting handshakes in a
FIXED 45 s window so call length cannot manufacture the trend:

    handshakes   n     median FINAL rx   median BEST rx   best - final
         1        7        24000             24000              0
         2       30        24000             24000              0
         3       10        24000             26400           2400
         4       24        18000             26400           8400
         5       19        14400             21600           7200

    span-matched (55-95 s):  <=2 handshakes 24000     >=4 handshakes 12000

**Monotonic in the final rate and flat-to-RISING in the best rate.** Calls that
retrain more reach just as high -- higher, in fact -- and then get knocked back.
That is the ladder-restart mechanism above, visible in the object's own
numbers. **Cutting retrains is worth up to a doubling of throughput here**, and
that is now a measured figure rather than an inference.

Neither the far end nor the call length carries it: handshake counts by
destination are 3.11 / 3.37 / 2.94, indistinguishable, and every band contains
a mix of all three modems.

### 8.2 A real receive-path impairment exists, and it is ours (1941-1943)

With zero packet loss on the wire (`rtp: loss=0 discard=0 jitter=0.3ms`),
d-modem's RTP jitter buffer underruns **0.3-0.5 times a second**, each time
handing the datapump a `PJMEDIA_JB_MISSING_FRAME` or `ZERO_EMPTY_FRAME` -- 10 ms
of fabricated or silent audio spliced into the carrier, upstream of the
equaliser. It is not tunable away: the buffer settles at ~35 ms whatever
prefetch it is given, across a sixfold range of settings.

**Why an insertion is worse than a dropout** (1949): a lost RTP packet
substitutes in phase and the signal resumes where the receiver expects it. An
underrun INSERTS, so everything after arrives 10 ms late -- about 34 symbols of
step at 3429 baud for the timing recovery to chase. In the emulator, insertion
at the bench's own measured rate takes a link that connects 6/6 down to 1/6,
while substitution at 1% does nothing comparable.

**THE KEYSTONE IS STILL MISSING.** That this impairment exists, and that
retrains are expensive, are both established. That the first CAUSES the second
is NOT. The two have never been correlated in time, because slmodemd stamps
relative seconds and pjmedia stamps wall clock and nothing converted between
them. `row.sh` now writes a `BENCHANCHOR` line, so every future capture can be
aligned; historical ones cannot.

## 9. Known weaknesses, and what would test each

Ranked by expected value. Items 1–2 can be evaluated on data already recorded,
without placing a call.

| # | weakness | where | test |
|---|---|---|---|
| 1 | `equerr` not power-normalised, thresholds absolute | §5 | recompute the rate decision offline using `f248`; compare against what was chosen |
| 2 | rate decided from ONE 1024-symbol block, no averaging or confidence | §5 | same offline recomputation, averaging N blocks |
| 3 | pre-emphasis is fire-and-forget; no path from a struggling equaliser back to the shaping request | §1, §3.3 | two-stage: connect at the object's conservative index, renegotiate to the fitted one once trained |
| 4 | `CenterAdapt` discards the fractional half of the 8 dominant taps | §4.4 | fix and run the differential tier; it must not change anything else |
| 5 | no tap leakage over 80 taps on a band-limited signal | §4.5 | long-hold call, watch for drift (#143) |
| 6 | step size scheduled by symbol count, cannot gear back up | §4.3 | error-driven gearing; simply retaining 0x4000 through the Phase-4 decision window was tested on the seeded 18 dB model and increased error at both ends, so it is not retained as a knob |
| 7 | two-point estimator: 5× biased, 4 dB quantised, samples the codec corner | §3.3 | done — findings 1910, 1912 |

**THE TRADE THAT THE ARCHITECTURE CANNOT EXPRESS.** Finding 1912 measured
index 0 giving a higher carried rate (12000 → 21600 median) and **twice the
handshakes** (1.5 → 3.0). Flat transmit means the top of the band arrives
weakest, and the handshake must survive there *before* the equaliser has
converged. More pre-emphasis errs toward connecting at all; less errs toward a
better-conditioned channel once up. The design has no way to say "connect
conservative, then reduce shaping once trained" — which is item 3, and is why
the object's over-request is probably a deliberate trade rather than the bug
finding 1908's arithmetic made it look like.
