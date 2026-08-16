#!/usr/bin/env python3
"""batchanalyse.py -- test covariates against a sample that can reject one.

    batchanalyse.py captures/batch.csv

THE DISCIPLINE THIS ENFORCES, and why it is in code rather than in someone's
head.  Four covariates for the receive-rate deficit have been proposed and
refuted on this bench (findings 1206, 1207): the jitter buffer, per-call ERL,
between-modem ERL, and the equaliser error.  Each looked convincing when it was
first computed.  The equerr case is the clearest: r = -0.787 over nine calls,
and r = -0.263 with one call removed.  A single point carried it.

So every correlation printed here comes with:

  * n, and the RANGE of both variables -- a correlation over a variable that
    barely moves is not evidence, and neither is one over four distinct values.
  * the worst-case LEAVE-ONE-OUT r.  If dropping any single call collapses the
    relationship, the relationship is that call.
  * a permutation p-value, which needs no normality assumption and is honest
    about small n in a way that a t-test on r is not.

A covariate is reported as SURVIVING only if it holds with any one call removed.

AND THE EQUERR CASE DID NOT STAY REFUTED, which is the other half of the
lesson.  At n=22, n=29 and n=29 -- three independent batches -- `equerr_pre`
came back r = -0.69, -0.61, -0.61, worst LOO -0.65, -0.59, -0.59, p < 0.003
every time (findings 1208, 1209, and the `final` batch).  The n=9 result was
not wrong about n=9; it was a sample too small to decide either way, and the
LOO rule correctly declined to call it.  A covariate that fails here is not
disposed of, it is undecided, and the honest thing is to say so and take more
calls.

Nor does surviving mean it explains anything.  V.34 derives the rate FROM the
equaliser's own error measurement, so "high equerr_pre predicts a low rate" is
close to definitional -- it says the receiver is working, and moves the
question back a step to what makes the equaliser converge where it does
(finding 1209: the number of Phase 3 training passes).
"""

import argparse
import csv
import itertools
import sys

import numpy as np

CANDIDATES = [
    ("erl_db",        "echo return loss"),
    ("sig_echo_db",   "signal-to-echo"),
    ("equerr_pre",    "equaliser error, pre-CONNECT"),
    ("equerr_post",   "equaliser error, post-CONNECT"),
    ("connect_secs",  "seconds from dial to CONNECT"),
    ("echo_lag_ms",   "echo lag"),
    # Not a column -- synthesised below.  A 30-call batch is ~35 minutes of
    # continuous work for both modems and the ATA, and every covariate tested
    # so far has been a per-call property, so nothing has ever been able to
    # see DRIFT WITHIN A BATCH: a modem warming up, a far end settling into or
    # out of a state.  A trend here means the calls in one batch are not
    # exchangeable with each other, which is worth knowing on its own.
    #
    # WHAT IT DOES NOT ESTABLISH, and the temptation is strong: this says
    # nothing about whether two batches are comparable.  batchcompare.py
    # permutes labels between batches taken an hour or a day apart, and the
    # threat there is a LEVEL SHIFT between them -- line conditions, room
    # temperature, PBX load, far-end state.  A perfectly flat slope inside
    # each batch is entirely consistent with batch A sitting two rate steps
    # above batch B for reasons that have nothing to do with the variable
    # under test, and no within-batch covariate can see that by construction.
    # The check for THAT is a null-vs-null control: two batches at the same
    # configuration, separated in time, run through batchcompare.  If it calls
    # a difference where there is none, its p-values are optimistic and the
    # size of the error is the bench's between-batch floor.
    ("_ordinal",      "position in the batch"),
]


def perm_p(x, y, iters=20000, seed=12345):
    """Two-sided permutation p for |r|.  No distributional assumption."""
    rng = np.random.default_rng(seed)
    r0 = abs(np.corrcoef(x, y)[0, 1])
    y = np.array(y)
    hits = sum(abs(np.corrcoef(x, rng.permutation(y))[0, 1]) >= r0
               for _ in range(iters))
    return (hits + 1) / (iters + 1)


def loo_worst(x, y):
    """The leave-one-out r furthest from the full-sample r, and which index."""
    r_full = np.corrcoef(x, y)[0, 1]
    worst, idx = r_full, None
    for i in range(len(x)):
        xs = np.delete(x, i)
        ys = np.delete(y, i)
        if xs.std() == 0 or ys.std() == 0:
            continue
        r = np.corrcoef(xs, ys)[0, 1]
        if abs(r) < abs(worst):
            worst, idx = r, i
    return worst, idx


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("csvfile")
    args = ap.parse_args()

    rows = list(csv.DictReader(open(args.csvfile)))
    if not rows:
        print("no rows", file=sys.stderr)
        return 1

    # Position in the batch, counted over ATTEMPTED calls -- a failed call
    # still took its slot and its minute, so numbering the connected ones
    # 1..k would compress the gaps and hide exactly the drift being looked for.
    for i, r in enumerate(rows, 1):
        r["_ordinal"] = str(i)

    conn = [r for r in rows if r["connect"] == "1" and r["our_rx"]]
    print("=== the sample ===")
    print("calls attempted   %d" % len(rows))
    print("connected         %d  (%.0f%%)" % (len(conn), 100.0*len(conn)/len(rows)))
    data = sum(1 for r in conn if r["data_both_ways"] == "1")
    print("data both ways    %d of %d connected" % (data, len(conn)))
    if not conn:
        return 0

    rx = np.array([float(r["our_rx"]) for r in conn])
    tx = sorted({r["our_tx"] for r in conn if r["our_tx"]})
    print("our TX            %s" % ", ".join(tx))
    print("our RX            median %d, range %d-%d" % (np.median(rx), rx.min(), rx.max()))
    vals, cnts = np.unique(rx, return_counts=True)
    print("                  " + "  ".join("%d:%d" % (v, c) for v, c in zip(vals, cnts)))
    bauds = {(r["tx_baud"], r["rx_baud"]) for r in conn}
    print("symbol rates      %s" % ", ".join("%s/%s" % b for b in sorted(bauds)))

    print()
    print("=== covariates against our RX rate ===")
    print("A covariate SURVIVES only if no single call carries it.\n")
    for key, label in CANDIDATES:
        xs, ys = [], []
        for r, y in zip(conn, rx):
            if r[key]:
                xs.append(float(r[key])); ys.append(y)
        if len(xs) < 5:
            print("  %-30s only %d values -- skipped" % (label, len(xs)))
            continue
        x = np.array(xs); y = np.array(ys)
        if x.std() == 0:
            print("  %-30s constant (%.2f) -- no variance to test" % (label, x[0]))
            continue
        r_full = np.corrcoef(x, y)[0, 1]
        r_loo, idx = loo_worst(x, y)
        p = perm_p(x, y)
        survives = abs(r_loo) >= 0.5 and p < 0.05
        print("  %-30s n=%-3d r=%+.3f  worst LOO r=%+.3f  p=%.4f  %s"
              % (label, len(x), r_full, r_loo, p,
                 "SURVIVES" if survives else "does not survive"))
        print("  %-30s   x range %.2f-%.2f, y range %d-%d%s"
              % ("", x.min(), x.max(), y.min(), y.max(),
                 "" if idx is None else "   (dropping %s collapses it)"
                 % conn[idx]["call"]))
    print()
    print("=== what distinguishes the fastest calls ===")
    top = np.percentile(rx, 75)
    hi = [r for r, v in zip(conn, rx) if v >= max(top, rx.min() + 1)]
    lo = [r for r, v in zip(conn, rx) if v < top]
    if hi and lo:
        print("  fast = RX >= %d (n=%d), slow = rest (n=%d)" % (top, len(hi), len(lo)))
        for key, label in CANDIDATES:
            a = [float(r[key]) for r in hi if r[key]]
            b = [float(r[key]) for r in lo if r[key]]
            if len(a) >= 2 and len(b) >= 2:
                print("    %-30s fast median %8.2f   slow median %8.2f"
                      % (label, np.median(a), np.median(b)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
