#!/usr/bin/env python3
"""handshakeorder.py -- the rate decision, broken out by WHICH handshake it is.

    python3 snrblocks.py captures/*.slmodemd.log > snr.csv
    python3 handshakeorder.py snr.csv

WHY THIS EXISTS.  3201 quotes a single median deficit -- phase 3 SNR minus the
SNR the ladder read -- of 5.80 dB, and calls it a systematic duplex-only
degradation.  The attenuation-arm captures give 17 dB for what should be the
same quantity, and 3201 flags that discrepancy as the first thing to resolve.

It resolves to a MIXTURE.  `snrblocks.py`'s `block` column is the handshake
index within a call: it starts a new one at `Agc gain estimate at the end of
phase 3`, and each carries its own `ethreh`, its own thresholds and its own
`finally rxbitrate`.  Split on it and the deficit is not systematic at all --
it is ~16 dB on the FIRST handshake of a call and ~0 by the fourth.  A median
over the pooled set measures the mix of first-to-later handshakes in whatever
captures went in, which is why two capture sets disagree by 11 dB.

THE DISTRIBUTION IS BIMODAL, so `--mode` reports the split rather than a mean.
Decision SNR clusters at ~18 dB and ~29 dB with a trough between; TROUGH_DB is
where that gap sits, measured off the histogram and not tuned.  93% of first
handshakes are below it and 33% of second ones are.

AND THE LOW MODE IS NOT 3202.  On a 3202 row phase 4 converged and the ladder
read a one-block spike, so `p4_best_db` is high and `spike_db` is large.  In
the low mode `p4_best_db` is ~18 too: phase 4 never got there at all.  These
rows are excluded as spikes here (spike_db < SPIKE_DB) precisely so the two
cannot be confused.

Never averages a `sat` row: equerr rails at 32767 while sigpow does not, so a
saturated block reads as a ~7 dB floor whatever the channel was.
"""

import csv
import re
import statistics as st
import sys
from collections import Counter, defaultdict

# Measured off the decision-SNR histogram, not chosen: 2 dB bins over the
# 1901/1903 clean rows put 70 rows at 18, 9 at 20, 1 at 22, 2 at 26 and 10 at
# 28.  The gap is 20-26, so anything in it lands on the correct side by a
# margin of several bins.
TROUGH_DB = 23.0

# 3202's discriminator, its own value: converged-minus-decision above 6 dB is
# the spiked population, and its distribution is bimodal with a gap at 6-9.
SPIKE_DB = 6.0


def num(row, col):
    v = row.get(col, "")
    return float(v) if v not in ("", "None") else None


def far_end(call):
    """The extension, from the call name.  Some batches name the modem."""
    m = re.search(r"19(0[123])", call)
    if m:
        return "19" + m.group(1)
    c = call.lower()
    for key, ext in (("olinet", "1903"), ("supra", "1901"),
                     ("cx", "1901"), ("courier", "1902")):
        if key in c:
            return ext
    return "?"


def clean(row):
    """A row whose decision is a fair sample: not railed, not a 3202 spike."""
    if row["flag"] == "sat":
        return False
    spike, deficit = num(row, "spike_db"), num(row, "p3_minus_dec")
    return spike is not None and deficit is not None and spike < SPIKE_DB


def by_handshake(rows, cap=4):
    """Rows grouped by handshake index, everything at or past `cap` together."""
    out = defaultdict(list)
    for r in rows:
        out[min(int(r["block"]), cap)].append(r)
    return out


def report(rows, label):
    print(f"\n=== {label}: n={len(rows)} clean decisions ===")
    print(f"  {'hs':>3s} {'n':>5s} {'decision':>9s} {'p4 best':>9s} "
          f"{'phase 3':>9s} {'deficit':>9s} {'% low':>6s}  {'median rate':>11s}")
    groups = by_handshake(rows)
    for hs in sorted(groups):
        g = groups[hs]
        best = [num(r, "p4_best_db") for r in g if num(r, "p4_best_db") is not None]
        low = sum(1 for r in g if num(r, "snr_db") < TROUGH_DB)
        rate = sorted(int(r["choice"]) for r in g if r["choice"] not in ("", "None"))
        label_hs = f"{hs}+" if hs == max(groups) and hs == 4 else str(hs)
        print(f"  {label_hs:>3s} {len(g):5d} "
              f"{st.median([num(r,'snr_db') for r in g]):9.2f} "
              f"{(st.median(best) if best else float('nan')):9.2f} "
              f"{st.median([num(r,'p3_db') for r in g]):9.2f} "
              f"{st.median([num(r,'p3_minus_dec') for r in g]):9.2f} "
              f"{100.0*low/len(g):5.0f}% "
              f"{(st.median(rate) if rate else float('nan')):11.1f}")


def modes(rows, label):
    lo = [r for r in rows if num(r, "snr_db") < TROUGH_DB]
    hi = [r for r in rows if num(r, "snr_db") >= TROUGH_DB]
    print(f"\n=== {label}: the two modes ===")
    for name, g in (("low ", lo), ("high", hi)):
        if not g:
            continue
        print(f"  {name} n={len(g):4d}  decision {st.median([num(r,'snr_db') for r in g]):6.2f}"
              f"  p4 best {st.median([num(r,'p4_best_db') for r in g]):6.2f}"
              f"  spike {st.median([num(r,'spike_db') for r in g]):5.2f}"
              f"  deficit {st.median([num(r,'p3_minus_dec') for r in g]):6.2f}")
        print(f"        handshake index: {sorted(Counter(min(int(r['block']),4) for r in g).items())}")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    rows = [r for r in csv.DictReader(open(sys.argv[1])) if clean(r)]
    report(rows, "ALL far ends")
    modes([r for r in rows if far_end(r["call"]) in ("1901", "1903")],
          "1901 + 1903")
    for ext in ("1901", "1902", "1903"):
        sub = [r for r in rows if far_end(r["call"]) == ext]
        if sub:
            report(sub, f"ext {ext}")


if __name__ == "__main__":
    main()
