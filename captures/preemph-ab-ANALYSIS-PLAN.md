# Pre-registered analysis plan for the pre-emphasis A/B

Written and committed BEFORE the run finished, with 3 of 16 calls in, because
the third call was both the best result so far (19200) and the one whose load
rose above threshold during it. Choosing an exclusion rule after seeing that
is how a null becomes a positive.

## The hypothesis

D53's off-by-one makes pre-emphasis index 0 unreachable, so the modem cannot
ask for a flat line (finding 1471). Finding 1473 proposes a mechanism by which
that could cost rate: the equaliser converges, the far end begins applying the
pre-emphasis we asked for, the equaliser is knocked off convergence, and the
rate is decided before it recovers.

**Prediction if the mechanism is real:** the fix arm shows LOWER `equerr` at
the decision, and a higher receive rate.

**Prediction if it is not:** the arms are indistinguishable. That is the prior
-- 1.5 dB of tilt against a gap from equerr ~2800 to a threshold of 205.

## The primary statistic

`equerr` at the decision -- the `V34DATARATE, equerr = N` line, NOT the ~57
per-call `V34EQU` samples -- compared between arms by permutation on the
rank-sum. Rank-sum rather than the median, because the receive rate is a
discrete ladder and finding 1351 showed the median missing a real shift that
the rank-sum caught at p = 0.0127.

Receive rate is the secondary statistic and connect rate the third.

## The exclusion rule, fixed now

**Report BOTH of these, always, and do not choose between them after the
fact:**

1. **All calls**, whatever the load.
2. **Load-clean calls only** -- both the before and after readings under 6,
   which is half the twelve cores.

If the two disagree, that disagreement IS the result and gets reported as
such. A run where the answer depends on which subset is used has not answered
the question, and saying so is worth more than picking the flattering half.

Calls are NOT excluded for any other reason. Not for sounding rough, not for
retraining, not for being outliers. Retrain count and load are recorded as
covariates and may be tested as such, but they do not remove a call.

## What would make the run inconclusive

- Fewer than 6 usable calls in either arm.
- Any difference smaller than the between-call spread within an arm.
- A load covariate that survives, since that would mean the machine and not
  the branch is doing the work.
