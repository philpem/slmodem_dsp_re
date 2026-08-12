#!/usr/bin/env python3
"""abcompare.py -- the pre-emphasis A/B, analysed exactly as pre-registered.

    abcompare.py captures/pab2-*.run.log

The plan is in captures/preemph-ab-ANALYSIS-PLAN.md, committed before the run
finished so that no choice made here could be informed by the data. This
script implements it and nothing else; if you find yourself wanting to add a
subset or swap a statistic, that is the moment to reread the plan.

  PRIMARY    equerr at the decision that produced the carried rate.  The
             hypothesis (1471, 1474) is that a modem which cannot ask for a
             flat line is handed a signal it must equalise around, so the fix
             arm should show LOWER equerr.
  SECONDARY  the receive rate.
  TERTIARY   connect rate, over ALL attempted calls -- a fix that connects
             less often is worse whatever it does to the rate.

  Rank-sum by permutation, not the median: the rate is a discrete ladder and
  finding 1351 caught the median missing a real shift the rank-sum found at
  p = 0.0127.

  BOTH SUBSETS ARE ALWAYS REPORTED -- all calls, and load-clean calls only
  (before and after both under half the cores).  If they disagree, that
  disagreement is the result and neither half may be quoted alone.
"""

import csv
import subprocess
import sys
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
LOAD_LIMIT = 6.0


def perm_ranksum(a, b, iters=20000, seed=12345):
    """Two-sided permutation p on the difference in mean rank."""
    if len(a) < 2 or len(b) < 2:
        return float("nan"), float("nan")
    rng = np.random.default_rng(seed)
    pool = np.concatenate([a, b])
    n = len(a)

    def stat(x, y):
        r = np.argsort(np.argsort(np.concatenate([x, y]))).astype(float)
        return r[len(x):].mean() - r[:len(x)].mean()

    obs = stat(a, b)
    hits = 0
    for _ in range(iters):
        rng.shuffle(pool)
        if abs(stat(pool[:n], pool[n:])) >= abs(obs) - 1e-12:
            hits += 1
    return obs, (hits + 1) / (iters + 1)


def report(rows, label):
    print("\n" + "=" * 66)
    print("  %s  (n=%d)" % (label, len(rows)))
    print("=" * 66)
    arms = {a: [r for r in rows if r["arm"] == a] for a in ("bug", "fix")}
    for a, rs in arms.items():
        conn = [r for r in rs if r["connect"] == "1"]
        print("  %-4s attempted %2d   connected %2d   rates: %s"
              % (a, len(rs), len(conn),
                 ", ".join(r["our_rx"] for r in conn) or "-"))
    if min(len(v) for v in arms.values()) < 2:
        print("\n  too few calls to test.")
        return

    for field, name, better in (("equerr", "equerr at the decision", "lower"),
                                ("our_rx", "receive rate", "higher")):
        vals = {a: np.array([float(r[field]) for r in rs
                             if r["connect"] == "1" and r[field]])
                for a, rs in arms.items()}
        if min(len(v) for v in vals.values()) < 2:
            print("\n  %-24s too few connected calls" % name)
            continue
        d, p = perm_ranksum(vals["bug"], vals["fix"])
        print("\n  %-24s bug median %9.0f   fix median %9.0f"
              % (name, np.median(vals["bug"]), np.median(vals["fix"])))
        print("  %-24s rank-sum %+7.2f   p = %.4f   %s   (fix %s = better)"
              % ("", d, p, "SIGNIFICANT" if p < 0.05 else "not significant",
                 better))

    ca = np.array([1.0 if r["connect"] == "1" else 0.0 for r in arms["bug"]])
    cb = np.array([1.0 if r["connect"] == "1" else 0.0 for r in arms["fix"]])
    d, p = perm_ranksum(ca, cb)
    print("\n  %-24s bug %.0f%%   fix %.0f%%   p = %.4f"
          % ("connect rate", 100 * ca.mean(), 100 * cb.mean(), p))

    if min(len(v) for v in arms.values()) < 6:
        print("\n  UNDERPOWERED: the plan calls 6 usable calls per arm the")
        print("  minimum, and this is below it.  A null here means nothing.")


def main():
    out = subprocess.run([sys.executable, os.path.join(HERE, "abextract.py")]
                         + sys.argv[1:], capture_output=True, text=True).stdout
    rows = list(csv.DictReader(out.splitlines()))
    if not rows:
        sys.exit("no calls found")

    # Load per call comes from the runner's CSV, not the logs.
    loads = {}
    try:
        for r in csv.DictReader(open(os.path.join(HERE, "captures",
                                                  "preemph-ab.csv"))):
            loads[r["call"]] = (float(r["load_before"]), float(r["load_after"]))
    except OSError:
        pass

    report(rows, "ALL CALLS")
    clean = [r for r in rows
             if max(loads.get(r["call"], (99, 99))) < LOAD_LIMIT]
    report(clean, "LOAD-CLEAN ONLY (before and after both < %.0f)" % LOAD_LIMIT)

    dropped = [r["call"] for r in rows if r not in clean]
    if dropped:
        print("\n  excluded from the load-clean set: %s" % ", ".join(dropped))
    print("\n  If the two panels disagree, that disagreement is the result.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
