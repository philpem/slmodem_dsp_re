#!/usr/bin/env python3
"""freezescan.py -- did a frozen equaliser manufacture the retrain?

    freezescan.py captures/frz-*.slmodemd.log

THE CHAIN THIS TESTS.  `v34rx.c` disables equaliser adaptation for every
`f798 < -64`; the object's own message prints only in the six-wide band
-70 < f798 < -64, so the log undercounts by an unknown factor and
`V34EQFREEZE` was added to count them all.  Separately, the retrain detector
watches the EQUALISED point and reads 140 consecutive near-still symbols as a
far-end retrain request.  If a frozen equaliser stops tracking, its output can
go still, and the receiver then detects a retrain request nobody sent.

WHAT WOULD FALSIFY IT.  Freezes rare, or no closer to retrains than chance.
That second one is the test that matters, and it needs a null: a freeze rate
of ten per second makes "a freeze preceded the retrain" true by accident.  So
the window rate is compared against the call's own overall rate, not against
zero.

TWO EARLIER HYPOTHESES DIED FOR WANT OF EXACTLY THAT (finding F1918) -- tap
drift and rate overshoot both looked plausible and neither beat chance.  This
one gets the same treatment.
"""

import argparse
import os
import re
import sys

import numpy as np


def events(path):
    txt = open(path, "rb").read().decode("latin-1")
    ev = []
    pat = (r"<\s*([\d.]+)>[^\n]*?(V34EQFREEZE, f798 = (-?\d+)"
           r"|V34RTNCOUNT, triggered at (-?\d+), equerr = (\d+)"
           r"|V34PROBEBINS"
           r"|V34DATARATE, finally txbitrate \d+,rxbitrate (\d+))")
    for m in re.finditer(pat, txt):
        t = float(m.group(1))
        if m.group(3) is not None:
            ev.append((t, "freeze", int(m.group(3))))
        elif m.group(4) is not None:
            ev.append((t, "retrain", int(m.group(4)), int(m.group(5))))
        elif m.group(6) is not None:
            ev.append((t, "rate", int(m.group(6))))
        else:
            ev.append((t, "probe", 0))
    return ev


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--window", type=float, default=2.0,
                    help="seconds before a retrain to look for a freeze")
    args = ap.parse_args()

    tot_f = tot_r = 0
    per_call = []
    obs_rates, null_rates = [], []
    trig, trig_equerr = [], []

    for p in sorted(args.logs):
        ev = events(p)
        if not ev:
            continue
        fz = [e[0] for e in ev if e[1] == "freeze"]
        rt = [e for e in ev if e[1] == "retrain"]
        span = ev[-1][0] - ev[0][0]
        tot_f += len(fz)
        tot_r += len(rt)
        trig += [e[2] for e in rt]
        trig_equerr += [e[3] for e in rt]

        # observed: freezes per second inside the window before each retrain
        # null:     freezes per second over the whole call
        if rt and span > 0:
            inwin = sum(1 for f in fz
                        for r in rt if 0 <= r[0] - f <= args.window)
            obs = inwin / (len(rt) * args.window)
            null = len(fz) / span
            obs_rates.append(obs)
            null_rates.append(null)
        per_call.append((os.path.basename(p)[:18], len(fz), len(rt),
                         span, len(fz) / span if span > 0 else 0))

    print("  EQUALISER FREEZES AND RETRAINS, %d calls\n" % len(per_call))
    print("  %-18s %8s %8s %8s %10s" % ("call", "freezes", "retrains",
                                        "span s", "freeze/s"))
    for r in per_call:
        print("  %-18s %8d %8d %8.1f %10.2f" % r)

    print("\n  totals: %d freezes, %d retrains" % (tot_f, tot_r))
    if not tot_f:
        print("\n  NO FREEZES AT ALL -- the chain is broken at step 1 and the")
        print("  110 messages in earlier captures came from calls this batch")
        print("  did not reproduce.  Say so; do not go looking for step 3.")
        return 0

    if obs_rates:
        o, n = np.array(obs_rates), np.array(null_rates)
        print("\n  freeze rate in the %.1f s before a retrain : %.2f /s"
              % (args.window, o.mean()))
        print("  freeze rate over the whole call            : %.2f /s"
              % n.mean())
        ratio = o.mean() / n.mean() if n.mean() > 0 else float("nan")
        print("  ratio (>1 means freezes CLUSTER before retrains): %.2fx"
              % ratio)
        print("\n  A ratio near 1 means freezes are simply common and the")
        print("  clustering is an artefact of that -- which is how the tap")
        print("  drift and overshoot hypotheses died (finding F1918).")

    if trig:
        t = np.array(trig)
        e = np.array(trig_equerr)
        print("\n  the stillness counter when a retrain fired (object prints"
              " 0 here -- see 1918):")
        print("    median %d   range %d..%d" % (np.median(t), t.min(), t.max()))
        print("  equerr at that moment: median %d   range %d..%d"
              % (np.median(e), e.min(), e.max()))
        print("\n  A HIGH equerr means the receiver was already struggling and")
        print("  the stillness is a symptom.  A LOW equerr means the signal")
        print("  was fine and the detector fired on a genuinely still far end")
        print("  -- a real request, not a manufactured one.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
