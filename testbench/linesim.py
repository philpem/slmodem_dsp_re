#!/usr/bin/env python3
"""linesim.py -- score tilt estimators against a simulated subscriber loop.

    linesim.py                 # the default sweep
    linesim.py --trials 20000

WHY A SIMULATION.  The bench cannot answer this question. Its "line" is two
metres of desk lead into a VG204, so what the probe measures is a FIXED
digital filter (`bearer-cap 3100Hz`) plus a very quiet path -- finding 1907's
tilt baseline held to 0.07 dB across five calls. A channel that never varies
cannot show which estimator survives variation, and the difference between a
two-point reading and a least-squares fit is precisely a claim about
variation. So the channel is modelled instead.

WHAT IS MODELLED, and each part is here because a real loop has it:

  * **Skin-effect roll-off.** A copper pair's loss grows roughly as sqrt(f),
    which is the tilt pre-emphasis exists to correct. Parameterised by the
    total loss across the band so the sweep can ask "how well is a 2 dB loop
    estimated, a 6 dB one, a 12 dB one".
  * **A codec band limit** above 3400 Hz, which is what the bench's own
    measurement shows the VG204 doing (-6.2 dB at 3400, -15.5 at 3700) and
    which puts the highest probe bins in a corner where the two-point method
    takes its sample.
  * **Noise**, as an SNR against the probe tones. V.34 Table 17 omits the
    tones at 900/1200/1800/2400 Hz precisely so the receiver can measure this,
    so the simulation omits them too and the estimators must skip them.
  * **Dud bins.** An occasional bin corrupted or dropped -- the case the whole
    robustness argument is about. A two-point estimator that samples a dud has
    no way to know.
  * **The object's quantisation.** `dftenergy` publishes
    `energy = (short)(e >> 16)`, so a bin below 65536 reads ZERO and is
    indistinguishable from any other quiet bin. Modelled, because it is what
    makes low-level bins useless to any estimator.

THE ESTIMATORS, all scored on the same synthetic bins:

  * `two_point`  -- Smart Link's. Multiply the band-edge bin by k=1.5961 until
    it exceeds bin 4, count the steps. One bin against one bin, quantised to
    4.06 dB.
  * `ols`        -- least squares over every tone-carrying bin. What
    `probe_preemp_fit` does today.
  * `theil_sen`  -- median of pairwise slopes. Same data, but a single wild
    bin cannot move the median. Proposed because the OLS arm passed a 6.22 dB
    outlier straight through on 1 of 90 real probes.

HOW THEY ARE SCORED, and this is the part that matters.  Not by the dB number
each reports -- those are not in the same units -- but by **the filter each
one ends up asking for**, against the filter the channel actually wants.
V.34 5.4.1 Table 3 is indices 0-5 at alpha = 0,2,4,6,8,10 dB; Table 4 is 6-10
at gamma = 1,2,3,4,5 dB. The residual is |applied lift - true loss| in dB,
which is what the far end's transmit is left tilted by and what our equaliser
then has to undo.
"""

import argparse
import math
import numpy as np

HZ = 150.0
NBINS = 25
REF = 4                      # bins[4], 750 Hz
OMITTED = {5, 7, 11, 15}     # 900/1200/1800/2400 Hz -- V.34 Table 17
K = 0x6626 / 16384.0         # 1.5961; one step = 4.06 dB
STEP_DB = 20 * math.log10(K)

TABLE3 = [0.0, 2.0, 4.0, 6.0, 8.0, 10.0]        # indices 0-5, alpha
TABLE4 = [1.0, 2.0, 3.0, 4.0, 5.0]              # indices 6-10, gamma


def lift_of_index(i):
    return TABLE3[i] if i <= 5 else TABLE4[i - 6]


def synth(rng, tilt_db, snr_db, edge, dud_p, codec_hz=3400.0):
    """One probe measurement, as `dftenergy` would publish it."""
    n = np.arange(1, NBINS + 1)
    f = n * HZ
    # skin effect: loss ~ sqrt(f), normalised to `tilt_db` across ref..edge
    shape = np.sqrt(f)
    lo, hi = shape[REF], shape[edge]
    loss = tilt_db * (shape - lo) / (hi - lo)
    # codec corner above the band limit
    loss += np.where(f > codec_hz, 12.0 * np.log2(f / codec_hz), 0.0)

    amp = 10.0 ** (-loss / 20.0)
    amp[[i for i in OMITTED]] = 0.0                  # no tone sent here

    noise = 10.0 ** (-snr_db / 20.0)
    re = amp + rng.normal(0, noise, NBINS)
    im = rng.normal(0, noise, NBINS)
    e = (re * re + im * im)

    if dud_p > 0:                                    # occasional wrecked bin
        dud = rng.random(NBINS) < dud_p
        e[dud] *= rng.uniform(0.02, 0.2, NBINS)[dud]

    # dftenergy's scale: an arbitrary but fixed gain, then energy = e >> 16
    energy = np.floor(e * (1 << 16) * 400.0 / (1 << 16)).astype(int)
    #
    # TWO GROUND TRUTHS, because the estimators target different quantities
    # and scoring both against one number is how the first version of this
    # simulation produced a misleading answer.
    #
    #   edge  -- loss at the band-edge bin relative to the reference bin.
    #            What `two_point` measures.  Includes the codec corner, which
    #            is a notch at one end and NOT a property of the loop.
    #   trend -- least-squares slope of the true loss over the tone-carrying
    #            bins, expressed over the same ref..edge span.  What a ramp
    #            filter can actually cancel.
    #
    # V.34 5.4.1's templates are ramps across the whole band (Figures 1 and 2,
    # conformance from 415 to 3502 Hz at 3429 baud), so `trend` is what
    # pre-emphasis is shaped to correct.  `edge` is reported beside it because
    # it is what the object aims at, and the gap between them is the argument.
    #
    keep = [i for i in range(1, edge + 1) if i not in OMITTED]
    sl = np.polyfit(np.array(keep, float), -loss[keep], 1)[0]
    return (np.clip(energy, 0, 32767),
            float(loss[edge] - loss[REF]),
            float(-sl * (edge - REF)))


def two_point(energy, edge):
    ref, x = int(energy[REF]), int(energy[edge])
    if ref <= 0 or x <= 0:
        return None
    n = 0
    xf = float(x)
    while xf <= ref and n < 12:
        xf *= K
        n += 1
    return n * STEP_DB


def _pts(energy, edge):
    xs, ys = [], []
    for i in range(1, edge + 1):
        if i in OMITTED or energy[i] <= 0:
            continue
        b = int(energy[i]).bit_length() - 1
        ys.append(3.0102999 * (b + (int(energy[i]) - (1 << b)) / float(1 << b)))
        xs.append(i)
    return np.array(xs, float), np.array(ys, float)


def ols(energy, edge):
    xs, ys = _pts(energy, edge)
    if len(xs) < 6:
        return None
    return max(0.0, -np.polyfit(xs, ys, 1)[0] * (edge - REF))


def theil_sen(energy, edge):
    xs, ys = _pts(energy, edge)
    if len(xs) < 6:
        return None
    sl = [(ys[j] - ys[i]) / (xs[j] - xs[i])
          for i in range(len(xs)) for j in range(i + 1, len(xs))]
    return max(0.0, -float(np.median(sl)) * (edge - REF))


def index_two_point(db):
    # the object's own mapping: steps counted from a counter preset to 5,
    # advanced before the test, saturating at 10 (D53)
    return min(10, 5 + max(1, int(round(db / STEP_DB))))


def index_fit(db):
    return min(5, int((db + 1.0) / 2.0))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--trials", type=int, default=4000)
    ap.add_argument("--edge", type=int, default=22)      # 3429 baud
    ap.add_argument("--seed", type=int, default=20260813)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)

    print("  RESIDUAL TILT AFTER CORRECTION, dB -- |applied lift - true loss|")
    print("  lower is better; this is what the equaliser is left to undo\n")
    print("  scored against the BAND TREND, which is what a ramp filter cancels")
    print("  %-9s %-6s %-6s | %-14s %-14s %-14s"
          % ("trend/edge", "SNR", "duds", "two-point", "least squares",
             "Theil-Sen"))

    for tilt in (1.5, 4.0, 8.0):
        for snr, dud in ((30.0, 0.0), (18.0, 0.0), (18.0, 0.08)):
            res = {"two": [], "ols": [], "ts": []}
            truth = {"edge": [], "trend": []}
            for _ in range(args.trials):
                en, t_edge, t_trend = synth(rng, tilt, snr, args.edge, dud)
                for key, fn, imap in (("two", two_point, index_two_point),
                                      ("ols", ols, index_fit),
                                      ("ts", theil_sen, index_fit)):
                    d = fn(en, args.edge)
                    if d is None:
                        continue
                    res[key].append(abs(lift_of_index(imap(d)) - t_trend))
                    truth["edge"].append(t_edge); truth["trend"].append(t_trend)
            row = []
            for k in ("two", "ols", "ts"):
                a = np.array(res[k]) if res[k] else np.array([float("nan")])
                row.append("%5.2f +-%4.2f" % (a.mean(), a.std()))
            print("  %-4.1f/%-4.1f %-6.0f %-6.2f | %-14s %-14s %-14s"
                  % (np.mean(truth["trend"]), np.mean(truth["edge"]),
                     snr, dud, row[0], row[1], row[2]))

    print("\n  two-point can only answer in %.2f dB steps, and samples ONE bin"
          " at the band edge --" % STEP_DB)
    print("  the bin the codec corner attenuates most.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
