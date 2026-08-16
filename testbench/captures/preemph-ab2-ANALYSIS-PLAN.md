# Pre-registered analysis plan — pre-emphasis A/B, run 2

Written and committed BEFORE any call of run 2 is placed. Run 1's plan and
result are `preemph-ab-ANALYSIS-PLAN.md` and finding 1476.

## What this is actually testing, which is NOT what run 1 thought

Run 1 was framed as "does fixing D53 help". Finding 1477 established that
framing was wrong, and the correction matters more than the result did:

V.34 specifies pre-emphasis in **two families** (5.4.1). Table 3 covers
indices 0–5 with one parameter α (0, 2, 4, 6, 8, 10 dB); Table 4 covers 6–10
with two, β and γ (0.5–2.5 / 1.0–5.0 dB). They are different SHAPES, not one
ladder of eleven strengths. `probe_preemph`'s counter, preset to 5 and
advanced before the test, yields exactly **Table 4's range** — five tilt
buckets onto five filters, complete and exact. That is deliberate.

So the "fix" arm is **not a corrected version**. Making index 0 reachable by
moving the increment shifts every other bucket down one: two steps of tilt
returns 6 instead of 7, three returns 7 instead of 8. On this bench's channel
both arms land in the middle of the range — run 1 measured the bug arm
requesting **7** and the fix arm **6** — so what is under test is:

> **does one step LESS pre-emphasis suit a codec-limited path better?**

Not "does the bug fix help". The real defect D53 leaves — a flat channel
cannot be told apart from a lightly tilted one, because index 0 is
unreachable — is not what this experiment exercises, because this channel is
not flat: the ATA's codec rolls off approaching 3.4 kHz and bin 22 sits in it.

**Do not report a positive result as vindicating the fix.** It would mean the
bucketing is one step too aggressive here, which is a different claim with
different consequences.

## Statistics — unchanged from run 1

  PRIMARY    `equerr` at the decision that produced the carried rate,
             extracted by `abextract.py` (matches the `finally … rxbitrate N`
             whose N is the rate the DTE was told, then takes the immediately
             preceding decision). Neither the first nor the last block is
             right; both were tried and both were wrong.
  SECONDARY  receive rate.
  TERTIARY   connect rate, over ALL attempted calls.

Rank-sum by permutation, not the median: the rate is a discrete ladder and
finding 1351 caught the median missing a real shift the rank-sum found at
p = 0.0127.

## Sample size

**12 per arm, 24 calls.** Run 1's 8 per arm cleared the plan's floor of six
only in the all-calls panel. Twelve leaves room for the ~12% non-connect rate
and for load exclusions.

## The exclusion rule, CORRECTED — and why this is not rule-shopping

Run 1 excluded on the absolute `load_after` reading. That reading includes
**the call's own cost**: slmodemd, d-modem and the harness together lift the
one-minute average, so a call starting at a quiet 4.8 finished at 6.7 and
failed a test the machine never failed. It threw out 8 of 16.

That defect was recorded mid-run, at call 9 of 16, BEFORE the result was
known, in `preemph-ab-ANALYSIS-PLAN.md`'s addendum — together with the fix to
use next time. Adopting it now is executing a decision already written down,
not choosing a rule that flatters an outcome.

**Exclude a call if `load_after − load_before` > 3.0.**

Calibrated from run 1's sixteen calls, whose rises were:

    -0.48 +0.03 +0.08 +0.12 +0.20 +0.39 +0.67 +0.75
    +0.87 +0.90 +1.24 +1.90 +2.03 +2.24 +2.84 +5.06

median +0.81, mean +1.18. The bulk of that is the call measuring itself. 3.0
sits well above the self-load and below the two excursions that coincided with
real external work.

**Both panels are still reported, always** — all calls, and rise-clean only.
If they disagree, that disagreement is the result and neither may be quoted
alone.

No call is excluded for sounding rough, retraining, or being an outlier.
Those are covariates, not grounds for removal.

## Pre-specified secondaries from ATI11 — declared now so they cannot be trawled

`ATI11` is read on the Courier after every call, before anything resets it.
These fields, and only these, are compared between arms:

| field | why it is here |
|---|---|
| `Preemphasis (-dB)` | the direct readout of the manipulated variable, from the far end's own instrument. Run 1 saw `0/2` on a fix-arm call against `0/4` on stock. |
| `Speed` recv/xmit | the far end's own view of both directions, independent of our CONNECT string |
| `Recv/Xmit Level (-dB)` | whether the arms differ in delivered level rather than in shaping |
| `Nonlinear Encoding` | ALSO chosen by the receiver. One fix-arm call reported `ON/OFF` where stock reports `ON/ON`, and nobody has ever checked whether we choose it correctly |
| `Roundtrip Delay (msec)` | channel covariate, expected identical; a difference invalidates the pairing |
| `Symbol Rate`, `Trellis Code` | confirmation the two arms are comparable links at all |

Anything else `ATI11` prints is recorded but is NOT a test. Finding something
interesting there is a hypothesis for a later run, not a result of this one.

## What would make this inconclusive

- Fewer than 6 usable calls in either arm after exclusions.
- A difference smaller than the within-arm spread.
- `Roundtrip Delay` or `Symbol Rate` differing between arms — that would mean
  the two arms are not the same link and nothing else can be read.
- The primary null and the secondary significant, as in run 1. That is a
  suggestive result and it is not a finding; it would want a third run before
  anything is claimed.

## Addendum, written mid-run at call 8 of 24 — the CONNECT string is not the rate

`pab3-fix-3` reported `CONNECT 14400` while the Courier's own `ATI11` says it
**transmitted 26400**. `pab3-fix-2` never reported CONNECT at all and the
Courier says it transmitted **19200**. Both logged a second, higher
`V34DATARATE ... finally` block after the first.

So post-CONNECT upward renegotiation **happens, and takes effect**, and our
`pty CONNECT nnn` string is emitted once and never revised. Every rate this
bench has measured from that string understates the rate actually carried
whenever a renegotiation succeeds.

**THE PLAN ALREADY COVERS THIS AND THAT IS LUCK, NOT FORESIGHT.** `Speed
recv/xmit` is listed above as a pre-specified secondary — "the far end's own
view of both directions, independent of our CONNECT string" — so analysing it
is sanctioned. It was listed as a cross-check, not because anyone knew the
CONNECT string was wrong.

**WHAT DOES NOT CHANGE.** The pre-registered PRIMARY stays `equerr` at the
decision, and the pre-registered SECONDARY stays `our_rx` from CONNECT. Those
are what was fixed before the data existed and they are reported as such.

**WHAT IS ADDED, as a pre-specified secondary and clearly labelled:** the same
rank-sum on the Courier's `Speed` xmit field. If the two rate measures
disagree, the disagreement is reported and neither is quoted alone — the same
rule the two load panels already follow.

**AND IT PUTS A LIMIT ON EARLIER FINDINGS.** 1466's table and 1476's rate
comparison both used the CONNECT string. 1469 is unaffected — it rests on
`ATI11 Speed 28800/12000`, which is ground truth. Any re-reading of 1466 and
1476 must say which measure it used. The recorded logs still hold the
`finally` blocks, so both can be recomputed without new calls.
