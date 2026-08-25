# V.34 rate collapse over SIP: root cause and remedy

**Status:** root cause identified and quantified. The remedy is implemented and
gated default-off, and **its pre-registered replication came back null** — 48
calls at 24 per arm, handshakes per minute of carrier p = 0.130, connect success
p = 0.416 (§6.2). The DEFECT is established from code and from 470 captures
(§3, §4); that THIS CHANGE fixes it is not. The flag should stay off.

Evidence key, as elsewhere in these docs:
`[CODE]` read from our source or the object · `[MEASURED]` from bench or
emulator data · `[SPEC]` from the Recommendation · `[INFERRED]` reasoning over
the other three.

---

## F1. Summary

A V.34 call between `slmodemd` and a hardware modem across the SIP path
negotiates a reasonable rate and then fails to keep it. The cause is not the
line, the far end, the equaliser or the pre-emphasis choice. It is that **the
datapump's recovery ladder is inverted**: the expensive recovery (a full
~10 s retrain) is triggered on a quarter of the evidence required by the cheap
one (a 2–3 s V.34 §11.6 rate renegotiation), so the cheap path effectively
never runs. The link spends most of its time re-handshaking, and every retrain
restarts the rate ladder near the bottom.

**72% of handshakes are followed by another within 15 seconds** `[MEASURED]`.
Calls that handshake four or more times in their first 45 s end at half the
throughput of calls that handshake twice or fewer — **12000 against 24000,
span-matched** `[MEASURED]`.

Raising the retrain's threshold above the renegotiation's reduced handshakes
from a median of 3.0 to 1.0 on the bench `[MEASURED]`.

---

## F2. The symptom

A negotiated 26400 delivers something closer to 8000. The far end reports a
healthy link throughout; our own transmit reaches 33600 to every far end
tested. The rate is not merely low, it is *unstable* — within a single call:

    14400 -> 24000 -> 26400 -> 26400 -> 4800 -> 28800 -> 4800

The receiver reaches high rates repeatedly and never holds one.

---

## F3. Root cause: the recovery ladder is inverted

### F3.1 The three thresholds

`datapumpv34` maintains three counters and acts on each at a fixed threshold
`[CODE]`:

| Response | Counter | Threshold | Cost |
|---|---|---|---|
| Full retrain | `DP_RX_BAD` | `> baud_rate >> 1` | ~10 s, restarts the rate ladder |
| Renegotiate **down** | `DP_RX_BAD_LONG` | `> 2 * baud_rate` | 2–3 s, keeps the equaliser |
| Renegotiate **up** | `DP_RX_GOOD` | `> 8 * baud_rate` | 2–3 s |

The retrain fires at `v34hshak.c:10017`, on either the far end's explicit
request (flag `0x40`) or our own bad-block run.

### F3.2 Why that ordering is wrong

The counters do **not** tick per symbol. `f128 = 4` samples per `receiver()`
call on a 9600 Hz stream means a fixed **2400 ticks per second** regardless of
the symbol rate `[CODE]`. At 3429 baud the thresholds are therefore:

| | ticks | seconds of bad blocks |
|---|---|---|
| Full retrain | `baud_rate >> 1` = 1714 | **0.71 s** |
| Renegotiate down | `2 * baud_rate` = 6858 | **2.86 s** |
| Renegotiate up | `8 * baud_rate` = 27432 | 11.4 s |

**The expensive response needs a quarter of the evidence the cheap one does.**
Any impairment lasting longer than 0.71 s triggers a full retrain long before
the renegotiation counter reaches its threshold, so the renegotiation path is
starved: it can only run for impairments that persist past 2.86 s *without*
having tripped the retrain first, which the ordering makes nearly impossible.

This matters because §11.6 of the Recommendation exists for precisely this
case — it *"can also be used to resynchronize the receiver without going
through a complete retrain"* `[SPEC]`, at a cost of S 128T, S̄ 16T, TRN ≤ 2000 ms
plus round-trip delay, then MP: two to three seconds against roughly ten.

### F3.3 Why the cheap path cannot be replaced by a shorter retrain

V.34 has **no short Phase 2** `[SPEC]`. Table 14 allocates all INFO0 bits
0:48, leaving nothing to negotiate an abbreviated probe. The object's own
short path (`local_short`, "requesting short phase2", reusing
`prev_bulk_delay` instead of re-measuring the round trip) is gated on
`sessionType == 2` — a **V.92** session `[CODE]`. A call forced to V.34 cannot
reach it. Across every call ever recorded on this bench, `requesting short
phase2` appears **0** times and `SILENCERETRAIN` **264** times `[MEASURED]`.

So the ~10 s retrain is genuinely the expensive option, the 2–3 s
renegotiation is genuinely the cheap one, and the thresholds have them the
wrong way round.

### F3.4 It is not the line, the far end, or the path

* Six hardware-to-hardware pairs cross the same ATA and SIP leg at
  28800–33600 with normal terminations `[MEASURED]`.
* Handshake counts by far end are 3.11 / 3.37 / 2.94 — indistinguishable
  across a Rockwell, a USR and a Conexant `[MEASURED]`.
* Our transmit reaches 33600 to every far end on every call `[MEASURED]`.
* At least 6 of 11 full retrains in an instrumented batch were our own
  bad-block run, not a far-end request `[MEASURED]`.

---

## F4. What it costs

Measured from a string the **blob itself** prints — `"V34 bulk delay
estimation %d (FAR=%d)"`, emitted once per handshake — so the measurement
cannot share a defect with this project's own instrumentation. 470 captures,
614 handshake intervals, of which **612 contain an explicit `*RETRAIN*` state
transition** (the other two are 0.1 s apart, one handshake calling
`ApplyBulkDelay` twice).

**72% of handshakes are followed by another within 15 s.** Median gap 11.6 s.

Over 90 calls, counting handshakes in a **fixed 45 s window** so that call
length cannot manufacture the trend:

| handshakes | n | median FINAL rx | median BEST rx | best − final |
|---|---|---|---|---|
| 1 | 7 | 24000 | 24000 | 0 |
| 2 | 30 | 24000 | 24000 | 0 |
| 3 | 10 | 24000 | 26400 | 2400 |
| 4 | 24 | 18000 | 26400 | 8400 |
| 5 | 19 | 14400 | 21600 | 7200 |

Span-matched to 55–95 s of carrier: **≤2 handshakes → 24000; ≥4 → 12000.**

The second column is the mechanism made visible. Calls that retrain more reach
*just as high* — higher, in fact, 26400 against the 24000 the quiet calls
manage — and are then knocked back down. The retrains do not prevent the ladder
climbing; they restart it near the bottom and the call ends before it can climb
again.

---

## F5. What was changed

### F5.1 The fix

`src/pump/v34/v34hshak.c`, immediately ahead of the retrain arm:

```c
if (dsplib_v34_rrn_on_badblock
    && !(T3C_RX(obj)->flags & 0x40)
    && dp_rxget(obj, DP_RX_BAD) <= (short)((int)obj->baud_rate * 3))
        dp_rxput(obj, DP_RX_BAD, 0);
```

It raises the retrain's effective threshold above the renegotiation's, so the
cheap recovery gets first refusal and the retrain becomes the fallback rather
than the reflex. **The far end's own request (flag `0x40`) is exempt and still
retrains immediately** — this changes only what *we* decide to do about *our*
bad blocks.

**It is a threshold, not a new code path**, and that distinction is the whole
of the design. An earlier attempt added a fifth branch calling
`VPcmV34InitiateRateRenegotiation` directly from this site; it bypassed the
layering instead of correcting it, skipped the `DP_MODE` / `DP_RX_WHY` /
`DP_RX_RATE` bookkeeping the normal path performs, and regressed to **one
connect in four** with rates ratcheting 14400 → 7200. Suppressing a counter
lets every existing mechanism run exactly as it already does.

### F5.2 Supporting changes

| Change | Why |
|---|---|
| `faa96` renamed `baud_rate` (106 uses) | It is the symbol rate in baud. The threshold arithmetic above is unreadable until the field is named, and the "N × baud_rate = N seconds" reading it invites is wrong — see §3.2. |
| `dsplib_v34_rrn_on_badblock` + `tools/benchflags.c` | The change is **default-off**, switched by `DSPLIB_V34_RRN_ON_BADBLOCK` at runtime, so master's behaviour is bit-identical and an A/B differs by one environment variable. |
| `V34HSINIT`, `V34RTNCOUNT`, `V34EQFREEZE`, `V34EQUPOW` | Instrumentation at the *callee* `v34handshakinit` with `__builtin_return_address(0)`. An earlier version instrumented 1 of 14 call sites and reported zero events. |
| `probe_preemp_fit` (Theil–Sen) | Second-order, and gated separately — see §7. |
| `tools/hybrid_link.sh` | The final hybrid link was hand-run and recorded nowhere; the binary on the bench was an hour older than the source it was meant to be testing. |

Every declared instrumentation string is registered in
`docs/invented_strings.txt` so `make strings` reports it as *declared* rather
than *invented*.

---

## F6. Evidence that the change works

24 bench calls, overnight, per `testbench/records/ab149-PREREG.txt` — written
before any call of the batch existed. 6 per arm per far end, 1901
(SupraExpress) and 1902 (USR Courier), arms **interleaved** so drift over the
night hits both equally, both arms on the same d-modem build so they differ by
one environment variable. The harness gated on a quiet machine before every
call and paused nine times.

**Connect success, checked first** — this is where the earlier attempt died:

    control    11/12
    treatment   9/12

**Primary outcome — handshakes in the pre-registered fixed 45 s window:**

    control    [2, 3, 3, 3, 5]   median 3.0
    treatment  [1, 1, 1, 2, 3]   median 1.0
    Mann-Whitney U = 22, exact one-tailed p = 0.036

**Rate, secondary:**

    1901 Supra    control  [4800, 7200, 12000, 14400, 14400]
                  treat    [14400, 14400, 14400, 14400]
    1902 Courier  control  [24000, 28800 x5]
                  treat    [28800 x5]

On the Courier both arms sit at 28800 with no room to improve. On the
SupraExpress the control scatters from 4800 to 14400 while every treatment call
lands on 14400 — the variance collapses, which is what removing a ladder
restart should look like.

---

### F6.1 The significance depends on the exclusion, and that is a problem

The pre-registered outcome excluded 13 of 23 calls for carrying less than 45 s
of carrier. Re-scoring the same batch with outcomes that exclude less — chosen
to maximise RETENTION, not to maximise the effect — gives:

    outcome                 calls kept   control        treatment      p (exact, 1-tailed)
    fixed 45 s window (pre-reg)  10/23   [2,3,3,3,5]    [1,1,1,2,3]    0.036
    fixed 15 s window            19/23   med 2.0        med 1.0        0.090
    handshakes per minute        23/23   med 5.28       med 2.58       0.085

**The smallest p comes from the metric that discards the most data.** That is
the signature of a fragile result, and it must be said plainly: on the two
outcomes that use all or nearly all of the calls, this batch does **not** reach
p < 0.05.

What survives is the effect SIZE, which is consistent and substantial across
all three — the median roughly halves however it is measured (3.0 → 1.0,
2.0 → 1.0, 5.28 → 2.58 per minute). That pattern is what an underpowered
measurement of a real effect looks like, and it is also what a null looks like
when a lenient metric is applied to a small sample. **This batch cannot
distinguish those two, and neither can any re-analysis of it.**

### F6.2 The replication: null

48 calls per `testbench/records/ab149r-PREREG.txt`, 24 per arm, both far ends,
interleaved, with the primary outcome changed to one that excludes nothing.

    connect success                 control 22/24  treatment 19/24   p = 0.416
    handshakes per minute of carrier  4.62 median    3.31 median      p = 0.130

**Doubling the sample moved p the wrong way** — 0.085 at 12 per arm, 0.130 at
24. A real effect of the size the 45 s window implied would have sharpened, not
blurred. §6.1's warning was right: that significance came from the exclusion.

The distributions show the one thing worth keeping. The treatment's best ten
calls run 0.8-2.2 handshakes/min against the control's 1.6-3.3, while its worst
call is the worst in either arm at 16.4. **It appears to help typical calls and
do nothing for bad ones** — consistent with its mechanism, since suppressing our
own bad-block counter cannot help when the far end is the one demanding the
retrain (flag 0x40 is exempt by design).

## F7. What this does not settle

* **n = 5 per arm on the primary outcome.** Only 10 of 24 calls carried 45 s
  of carrier; the rest died early, which is this bench's normal behaviour.
* **The exclusion criterion is outcome-adjacent.** Dropping calls with < 45 s
  of carrier conditions on something the treatment could itself affect — a
  collider. Pre-registering it prevents it being a fishing expedition; it does
  not prevent it being a bias.
* **Connect success was 9/12 against 11/12.** Not significant at n = 12, not
  the collapse that killed the earlier attempt, but the direction is
  unfavourable and must be a primary outcome in any replication.
* **One night, one operator, one batch.** p = 0.036 at n = 5 is a first
  result, and §6.1 shows it does not survive a less exclusionary outcome.
  Treat the effect size as the finding and the significance as unestablished.
* **The flag stays default-off** until replicated at 12+ per arm with a
  shorter scoring window (so fewer calls are excluded) and connect success
  tracked as a primary.
* **Pre-emphasis and equalisation are untouched by this.** The Theil–Sen probe
  fit is a real improvement to a real weakness — the object does a two-point
  fit that gives a single wrecked bin full leverage — but it is second-order
  against the retrain duty cycle and is gated separately.
* **The jitter-buffer defect is real and is NOT this.** See Appendix B.

---

## Appendix A — how this was found

Ordered as it happened, because the order is the argument.

1. **The work started somewhere else entirely** — improving the pre-emphasis
   estimator and the equaliser's convergence. Both are genuine weaknesses.
2. **The duty cycle was measured almost incidentally:** 66% of connected time
   spent re-handshaking, worst calls 88–90%. That reframed everything: choosing
   a better pre-emphasis index is second-order if the link is re-handshaking
   two-thirds of the time.
3. **Localisation, by elimination.** Our transmit reaches 33600 to every far
   end, so the transmitter is fine. Six hardware-to-hardware pairs cross the
   same ATA and SIP at 28800–33600, so the path is fine. Instrumenting the
   far-end request flag at the point it is produced showed at least 6 of 11
   retrains were our own counter. The trigger is ours.
4. **The obvious fix was checked and did not exist.** If retrains are
   expensive, take the short one — but V.34 has no short Phase 2, and the
   object's short path is gated on a V.92 session. Closed with nothing to fix.
5. **§11.6 was found in the Recommendation** — a documented mechanism for
   resynchronising without a complete retrain, at a quarter the cost.
6. **The thresholds were then read carefully**, and the inversion fell out:
   the retrain at `baud_rate >> 1`, the renegotiation at `2 * baud_rate`.
7. **The "N × baud_rate = N seconds" reading was wrong**, and only survived
   because it was challenged. `f128 = 4` fixes the tick rate at 2400/s
   independent of the symbol rate, so the real figures are 0.71 s and 2.86 s,
   not 0.5 s and 2 s. The 4× ratio — and therefore the inversion — is
   unaffected, but the absolute numbers in the first draft were wrong.
8. **The first fix attempt regressed** to one connect in four, by adding a code
   path rather than adjusting a threshold.
9. **An emulator was built** to iterate cheaply. It could not reproduce the
   phenomenon (Appendix C), so the bench remained the only instrument.
10. **A pre-registered bench A/B** settled it as far as one batch can.

### The measurement that made §4 credible

Every number in this investigation was produced by instrumentation this
project wrote, on captures this project chose. That is a single point of
failure for the whole argument. Late on, the object's own
`"V34 bulk delay estimation %d (FAR=%d)"` was noticed — printed once per
handshake, present in 470 existing captures, and never read. It gives an
independent handshake counter that cannot share a bug with our own, and it
agrees: 72% against the 66% our instrumentation reported.

---

## Appendix B — hypotheses tested and withdrawn

Fifteen, of which the last five were withdrawn on evidence gathered
specifically to test them. Recorded because the negative results are most of
the value: each one is a mechanism a future reader will otherwise re-propose.

| Hypothesis | How it died |
|---|---|
| Equaliser tap drift | 42% association — below chance |
| Rate overshoot | p ≈ 0.12 |
| Equaliser freeze → retrain | 0.00× clustering against the call's own base rate |
| Buffer underflow count | r = −0.15 |
| Renegotiation mishandling | 13%, 1.59× |
| Local `f124 > 7*baud_rate` timeout | Never fires |
| The SIP path itself | Six-pair hardware matrix, 28800–33600 |
| "4.5× tilt error" in the probe fit | Artefact of excluding a band-edge bin the object includes |
| "40× decision transient" | Two operating points compared, not one defect |
| Pre-emphasis index choice | Changing it altered nothing across 25 blocks — a *recording* cannot comply with a request |
| Progressive jitter-buffer discard | `discard` stayed **0** for every connected second; the algorithm is self-limiting |
| A 0.6% consumption-rate error | RTP arrival is 49.985 / 50.009 / 49.999 pkt/s — 50.000 to within 0.03% |
| Jitter-buffer depth as a lever | Prefetch 2 → 12 frames changes neither latency (34–38 ms) nor event rate |
| Echo canceller span exceeded | 1084 measurements: median RTD 155.8 ms, max 325.8, never once rejected; far EC enabled in 1082 |
| **Jitter-buffer underruns causing the retrains** | **1.10× clustering — noise. See below.** |

### The one that mattered most

d-modem's RTP jitter buffer underruns **0.3–0.5 times a second with zero
network loss**, each time handing the datapump a `MISSING_FRAME` or
`ZERO_EMPTY_FRAME` — 10 ms of fabricated or silent audio spliced into the
carrier, upstream of the equaliser. That is real, it is a genuine defect, and
a frame *insertion* is categorically worse than a dropout: it shifts everything
after it by 10 ms, about 34 symbols of step at 3429 baud, where a lost packet
merely substitutes in phase.

It is **not what causes the retrains**. Underruns before a retrain occur at
0.659/s against a whole-call base rate of 0.597/s — a ratio of 1.10, which is
noise. It remains worth fixing on its own merits; it is not on the critical
path to the rate problem, and the ladder fix never depended on it.

---

## Appendix C — defects found in the apparatus

Four of these silently corrupted results before they were caught, which is
the argument for writing them down.

* **The emulator ran one call eight times.** `chanshim.py` seeded its noise
  generator with a constant, so every emulated call on a configuration was
  bit-identical. An 8-per-arm A/B was one call reported eight times; the
  giveaway was zero variance — all eight rates exactly 7200. Now `CHAN_SEED`.
  It also means the emulator's earlier calibration is single samples.
* **The emulator modelled the wrong impairment.** `CHAN_LOSS` substituted
  silence *in phase* — a dropped RTP packet. The real defect is an insertion.
  At the bench's own measured rate, insertion takes a link that connects 6/6
  down to 1/6, while 1% substitution does nothing comparable. `CHAN_SLIP` now
  models it, and is deliberately **not** calibrated: it fires on a timer rather
  than when a modelled buffer runs dry, so it accumulates delay a real underrun
  does not.
* **The bench binary was an hour older than the source.** `hybrid.sh` emits
  `dsplibs_hybrid.o` and prints "link it with: …"; the final link lived only in
  a previous session's scrollback. Now `tools/hybrid_link.sh`, including the
  trap that our objects must be on the link line (weakening removes the blob's
  definitions without supplying replacements) and that `VPcmXfCreate.o` must
  come off it (the fork's shim owns that entry point).
* **slmodemd's timestamps were misunderstood for the whole investigation.**
  `<NNN.NNN>` is not seconds since process start; it is **Unix epoch seconds
  modulo 1000**, verified on 24 of 24 logs. The claim that slmodemd's and
  pjmedia's clocks could not be aligned — and therefore that the
  underrun/retrain correlation was impossible — was simply wrong. The
  correlation in Appendix B was computable all along, and on every historical
  capture.
* **`CONNECT` was twice read from the wrong file.** `slmodemd` logs
  `modem report result: 1 (CONNECT)` and drops the speed; only the DTE sees
  `CONNECT 33600`. Reading the log made a working sweep look like total
  failure. Separately, taking the *first* `CONNECT` in a bench run yields
  115200 — the serial DTE speed, not a line rate — and scored 20 real connects
  as zero.
* **A power quantity was converted with 20·log10.** `dftenergy`'s `energy` is
  power, so dB is 10·log10. The error doubled every reported tilt and was
  invisible on a fixed channel; it was found only in simulation.
