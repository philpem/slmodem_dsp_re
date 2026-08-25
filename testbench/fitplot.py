#!/usr/bin/env python3
"""fitplot.py -- least squares against Theil-Sen, drawn rather than tabulated.

    fitplot.py                       # -> fitplot.png
    fitplot.py --out compare.png

Finding F1911 says Theil-Sen is best or joint-best in every simulated cell with
dud bins, and that plain least squares can fall BELOW the object's crude
two-point counter there. That is a table of numbers. This is the same claim as
a picture, because the failure mode is geometric: one wrecked bin drags a
least-squares line by its full leverage, and a median of pairwise slopes simply
does not see it.

Five panels, left to right:

  1. CLEAN -- the two fits lie on top of each other, which is the point. Where
     nothing is wrong, nothing is different, so the change costs nothing.
  2. NOISY -- both wobble, still together.
  3. A DUD AT THE BAND EDGE -- the discriminating case, and the position
     matters. A dud in MID-BAND mostly shifts the intercept and barely moves
     the slope; the first version of this figure put one at bin 17 and the
     panel showed least squares doing BETTER, contradicting its own caption.
     Leverage lives at the ends.
  4. THE DISTRIBUTION over 2000 trials with random duds, which is what finding
     F1911 actually claims. A single draw cannot show a statistical property,
     and showing one as if it could is how a figure lies without any number
     in it being wrong.
  5. REAL -- an actual probe from this bench, with both fits over it, so the
     synthetic panels can be checked against something that really happened.

The dashed grey line is the TRUE trend where it is known (panels 1-3), which is
what a ramp pre-emphasis filter can cancel. In panel 4 nothing is known, which
is the whole reason the simulation exists.
"""

import argparse
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt          # noqa: E402
import numpy as np                       # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import linesim as L                      # noqa: E402


def fits(xs, ys, edge):
    """(ols_slope, theil_sen_slope) over the same points."""
    ols = np.polyfit(xs, ys, 1)[0]
    sl = [(ys[j] - ys[i]) / (xs[j] - xs[i])
          for i in range(len(xs)) for j in range(i + 1, len(xs))
          if xs[j] != xs[i]]
    return ols, float(np.median(sl))


def draw(ax, xs, ys, edge, title, true_slope=None, dud_at=None):
    ax.plot(xs, ys, "o", ms=5, color="#1f77b4", label="probe bins", zorder=3)
    if dud_at is not None:
        m = xs == dud_at
        ax.plot(xs[m], ys[m], "o", ms=11, mfc="none", mec="crimson", mew=2,
                label="dud bin", zorder=4)
    g = np.linspace(xs.min(), xs.max(), 50)
    o, t = fits(xs, ys, edge)
    c = ys.mean() - o * xs.mean()
    ct = np.median(ys - t * xs)
    if true_slope is not None:
        ax.plot(g, true_slope * g + np.median(ys - true_slope * xs),
                "--", lw=2, color="grey", label="true trend", zorder=2)
    ax.plot(g, o * g + c, "-", lw=2, color="#d62728",
            label="least squares", zorder=5)
    ax.plot(g, t * g + ct, "-", lw=2, color="#2ca02c",
            label="Theil-Sen", zorder=5)
    # report each fit as the tilt it would claim across ref..edge
    span = edge - L.REF
    sub = "OLS %+.2f dB   T-S %+.2f dB" % (-o * span, -t * span)
    if true_slope is not None:
        sub += "   true %+.2f dB" % (-true_slope * span)
    ax.set_title("%s\n%s" % (title, sub), fontsize=9)
    ax.set_xlabel("probe bin  (n x 150 Hz)")
    ax.grid(alpha=.3)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default="fitplot.png")
    ap.add_argument("--log", default=None,
                    help="a slmodemd log with V34PROBEBINS for panel 4")
    ap.add_argument("--edge", type=int, default=22)
    ap.add_argument("--tilt", type=float, default=4.0)
    args = ap.parse_args()
    rng = np.random.default_rng(4242)
    edge = args.edge

    fig, axes = plt.subplots(1, 5, figsize=(21, 4.6), sharey=False)

    # 1-3: synthetic, where the truth is known
    for ax, (snr, dud, name) in zip(axes, ((30.0, 0.0, "1. clean line"),
                                           (18.0, 0.0, "2. noisy line"),
                                           (18.0, 0.0, "3. dud at the band edge"))):
        en, t_edge, t_trend = L.synth(rng, args.tilt, snr, edge, 0.0)
        dud_at = None
        if name.startswith("3"):
            dud_at = edge - 1               # high leverage: near the edge
            en = en.copy()
            en[dud_at] = max(1, int(en[dud_at] * 0.06))
        xs, ys = L._pts(en, edge)
        draw(ax, xs, ys, edge, name,
             true_slope=-t_trend / (edge - L.REF), dud_at=dud_at)

    # 4: the distribution, which is what the claim actually is
    ax = axes[3]
    eo, et = [], []
    for _ in range(2000):
        en, _t_edge, t_trend = L.synth(rng, args.tilt, 18.0, edge, 0.08)
        xs, ys = L._pts(en, edge)
        if len(xs) < 6:
            continue
        o, t = fits(xs, ys, edge)
        eo.append(-o * (edge - L.REF) - t_trend)
        et.append(-t * (edge - L.REF) - t_trend)
    bins = np.linspace(-6, 6, 49)
    ax.hist(eo, bins=bins, alpha=.55, color="#d62728", label="least squares")
    ax.hist(et, bins=bins, alpha=.55, color="#2ca02c", label="Theil-Sen")
    ax.axvline(0, color="grey", ls="--", lw=2)
    ax.set_title("4. error over 2000 trials, 8%% duds\n"
                 "OLS sd %.2f dB   T-S sd %.2f dB"
                 % (np.std(eo), np.std(et)), fontsize=9)
    ax.set_xlabel("estimate - true trend (dB)")
    ax.legend(fontsize=8)
    ax.grid(alpha=.3)

    # 5: a real probe off the bench
    ax = axes[4]
    drawn = False
    if args.log:
        from probeplot import parse
        got = parse(args.log)
        if got:
            en = np.array(got[0][0])
            xs, ys = L._pts(en, edge)
            if len(xs) >= 6:
                draw(ax, xs, ys, edge,
                     "5. real probe\n%s" % os.path.basename(args.log)[:28])
                drawn = True
    if not drawn:
        ax.text(.5, .5, "no V34PROBEBINS log given\n(--log <slmodemd.log>)",
                ha="center", va="center", fontsize=9, color="grey")
        ax.set_axis_off()

    axes[0].set_ylabel("bin level (dB, 10log10 of published energy)")
    axes[0].legend(fontsize=8, loc="lower left")
    fig.suptitle("Tilt estimators on the same probe bins — least squares is "
                 "dragged by a dud bin, Theil-Sen is not  (finding F1911)",
                 fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(args.out, dpi=130)
    print("  wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
