#!/usr/bin/env python3
"""batchcompare.py -- did the rate distribution actually move between two batches?

    batchcompare.py captures/base-cx2.csv captures/newmodem.csv
    batchcompare.py --field our_rx --threshold 14400 a.csv b.csv

WHY THIS EXISTS SEPARATELY FROM batchanalyse.py.

`batchanalyse.py` asks "does a covariate explain the rate WITHIN one batch".
This asks "is batch B different from batch A" -- a two-sample question, and
the one every bench change actually poses: a new modem, a different country
profile, another impedance, a jitter setting.

THE STATISTIC MATTERS MORE THAN THE SAMPLE SIZE, and this bench learned it the
hard way (finding 1351).  A permutation test on the MEDIAN of the ATA playout
comparison gave p = 0.28 and the change was nearly reported as "not
significant".  The median is the wrong statistic for adjacent categories in a
discrete distribution: V.34 rates come in 2400 bit/s steps, so a real shift of
one step can leave the median on the same value.  The same data gave

    rank-sum            p = 0.0127
    threshold           p = 0.0008

So three tests are run here and all three are printed.  Disagreement between
them is information, not noise -- it usually means the shift is in the tail or
in one part of the range.

  * MEDIAN, by permutation.  Reported for continuity with older records and
    because when it does fire it is easy to explain.  Weakest of the three.
  * RANK-SUM (Mann-Whitney U), by permutation.  Sensitive to a shift anywhere
    in the ordering.  The default reading.
  * THRESHOLD PROPORTION.  "What fraction of calls reached at least X", by
    permutation on the proportion.  The most sensitive when the interesting
    change is at one end -- which for a modem is usually "how often does it
    manage a good rate", not "what is the middle".

Permutation throughout: no normality assumption, exact under the null that the
labels are exchangeable, and honest about small n in a way a t-test is not.

WHAT IT WILL NOT DO.  Rescue an underpowered comparison.  n = 3 per arm cannot
distinguish a configuration effect from this bench's own variance -- the
receive rate ranges over 4800..33600 on a channel whose level and echo
statistics are identical call to call (finding 1460d).  Twenty per arm is the
working minimum and the tool says so when it has less.
"""

import argparse
import csv
import sys

import numpy as np


def load(path, field):
    out = []
    for r in csv.DictReader(open(path)):
        if r.get("connect") == "1" and r.get(field):
            out.append(float(r[field]))
    return np.array(out)


def load_flag(path, field):
    """Every attempted call as 1/0 -- NOT filtered on connect.

    The rate comparison can only see calls that connected, so a modem that
    connects less often looks identical to one that connects always.  That is
    the wrong way round: reliability is the first thing a replacement unit has
    to beat, and #124 is a reliability failure with a perfectly good rate on
    the calls that survive.
    """
    out = []
    for r in csv.DictReader(open(path)):
        v = r.get(field)
        out.append(1.0 if v == "1" else 0.0)
    return np.array(out)


def perm_p(a, b, stat, iters=20000, seed=12345):
    rng = np.random.default_rng(seed)
    obs = stat(a, b)
    pool = np.concatenate([a, b])
    n = len(a)
    hits = 0
    for _ in range(iters):
        rng.shuffle(pool)
        if abs(stat(pool[:n], pool[n:])) >= abs(obs) - 1e-12:
            hits += 1
    return obs, (hits + 1) / (iters + 1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("csv_a")
    ap.add_argument("csv_b")
    ap.add_argument("--field", default="our_rx")
    ap.add_argument("--threshold", type=float, default=14400.0)
    args = ap.parse_args()

    a = load(args.csv_a, args.field)
    b = load(args.csv_b, args.field)
    if len(a) == 0 or len(b) == 0:
        sys.exit("one batch has no connected calls with a %s" % args.field)

    print("A  %-34s n=%-3d  median %8.0f   mean %8.0f"
          % (args.csv_a, len(a), np.median(a), a.mean()))
    print("B  %-34s n=%-3d  median %8.0f   mean %8.0f"
          % (args.csv_b, len(b), np.median(b), b.mean()))
    for name, arr in (("A", a), ("B", b)):
        v, c = np.unique(arr, return_counts=True)
        print("   %s spread: %s" % (name,
              "  ".join("%d:%d" % (x, y) for x, y in zip(v.astype(int), c))))

    print()
    d, p = perm_p(a, b, lambda x, y: np.median(y) - np.median(x))
    print("  median shift      %+9.0f   p = %.4f   %s"
          % (d, p, "significant" if p < 0.05 else "not significant"))

    def ranksum(x, y):
        pool = np.concatenate([x, y])
        r = np.argsort(np.argsort(pool)).astype(float)
        return r[len(x):].mean() - r[:len(x)].mean()
    d, p = perm_p(a, b, ranksum)
    print("  rank-sum          %+9.2f   p = %.4f   %s"
          % (d, p, "significant" if p < 0.05 else "not significant"))

    t = args.threshold
    d, p = perm_p(a, b, lambda x, y: (y >= t).mean() - (x >= t).mean())
    print("  share >= %-8.0f %+8.1f%%   p = %.4f   %s"
          % (t, 100 * d, p, "significant" if p < 0.05 else "not significant"))
    print("     A %.0f%%, B %.0f%%" % (100 * (a >= t).mean(), 100 * (b >= t).mean()))

    # Reliability, over ALL attempted calls.  Separate from everything above
    # because the rate tests are conditioned on connecting, so they are blind
    # to a modem that simply fails more often.
    print()
    for field, label in (("connect", "connected"), ("data_both_ways", "data both ways")):
        fa, fb = load_flag(args.csv_a, field), load_flag(args.csv_b, field)
        if len(fa) == 0 or len(fb) == 0:
            continue
        d, p = perm_p(fa, fb, lambda x, y: y.mean() - x.mean())
        print("  %-16s %+8.1f%%   p = %.4f   %s"
              % (label, 100 * d, p, "significant" if p < 0.05 else "not significant"))
        print("     A %d/%d, B %d/%d"
              % (fa.sum(), len(fa), fb.sum(), len(fb)))

    if min(len(a), len(b)) < 20:
        print("\n  UNDERPOWERED: %d and %d connected calls.  This bench's receive"
              % (len(a), len(b)))
        print("  rate ranges 4800..33600 on a channel whose level and echo are")
        print("  identical call to call (finding 1460d).  Twenty per arm is the")
        print("  working minimum; below that a null result means nothing and a")
        print("  positive one is probably one call.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
