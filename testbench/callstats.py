#!/usr/bin/env python3
"""callstats.py -- reduce one call's logs to a single CSV row.

    callstats.py captures/batch-7 [--header]

WHY A CSV.  Four covariates have been proposed for the receive-rate deficit and
all four were refuted once the sample grew (findings 1206, 1207): the jitter
buffer, per-call ERL, between-modem ERL, and the equaliser error.  Every one
looked strong at n=5-9.  The problem is not that the hypotheses were unusually
bad, it is that nine calls cannot distinguish a covariate from an outlier -- in
the equerr case a single call moved r from -0.787 to -0.263.

So the next measurement is a bigger sample, and it has to record every candidate
AT THE SAME TIME, per call, in a form that does not require re-parsing nine log
formats afterwards.  One row per call, one column per candidate.

Everything here is already produced at `-d9`; nothing new is instrumented.

Reads the logs as latin-1: a run log can contain raw line noise echoed from the
modem, and a strict decode would throw on exactly the failed calls that are most
worth keeping.
"""

import argparse
import os
import re
import subprocess
import sys

import numpy as np

FIELDS = [
    "call", "modem", "connect", "data_both_ways",
    "our_tx", "our_rx",
    "echo_lag_ms", "erl_db", "sig_echo_db",
    "equerr_pre", "equerr_post", "n_pre", "n_post",
    "tx_baud", "rx_baud",
    "connect_secs",
]


def read(path):
    try:
        return open(path, "rb").read().decode("latin-1")
    except OSError:
        return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prefix", nargs="?")
    ap.add_argument("--header", action="store_true")
    args = ap.parse_args()

    if args.header:
        print(",".join(FIELDS))
        if not args.prefix:
            return 0

    pre = args.prefix
    run = read(pre + ".run.log")
    sl = read(pre + ".slmodemd.log")

    row = {k: "" for k in FIELDS}
    row["call"] = os.path.basename(pre)

    m = re.search(r"^MODEM: (\S+)", run, re.M)
    row["modem"] = m.group(1) if m else ""

    m = re.search(r"pty +CONNECT (\d+)", run)
    row["connect"] = "1" if m else "0"
    row["our_rx"] = m.group(1) if m else ""

    m = re.search(r"TxRate: *(\d+)", run)
    row["our_tx"] = m.group(1) if m else ""

    row["data_both_ways"] = "1" if "DATA BOTH WAYS: PASS" in run else "0"

    # Symbol rate, both directions -- confirms per call that the two directions
    # really are running the same rate, rather than trusting one earlier check.
    m = re.search(r"setfinalrate, txbaudrate = (\d+),\s*rxbaudrate = (\d+)", sl)
    if m:
        row["tx_baud"], row["rx_baud"] = m.group(1), m.group(2)

    # Seconds from dial to CONNECT, as a cheap proxy for a laboured handshake.
    d = re.search(r"^\s*([\d.]+) pty +ATDT", run, re.M)
    c = re.search(r"^\s*([\d.]+) pty +CONNECT", run, re.M)
    if d and c:
        row["connect_secs"] = "%.2f" % (float(c.group(1)) - float(d.group(1)))

    # The equaliser error, split at CONNECT: V.34 fixes the rate in Phase 4, so
    # only the pre-CONNECT half can be a cause of the rate.  32767 is the
    # pre-convergence sentinel and is dropped rather than averaged in.
    eq = [(float(t), int(v)) for t, v in
          re.findall(r"<\s*([\d.]+)> V34EQU, equerr = (\d+)", sl)
          if int(v) < 32767]
    conn = re.search(r"<\s*([\d.]+)>.*modem report result: 1 \(CONNECT\)", sl)
    if eq and conn:
        t0 = float(conn.group(1))
        a = [v for t, v in eq if t <= t0]
        b = [v for t, v in eq if t > t0]
        row["n_pre"], row["n_post"] = str(len(a)), str(len(b))
        if a:
            row["equerr_pre"] = "%.0f" % np.median(a)
        if b:
            row["equerr_post"] = "%.0f" % np.median(b)

    # Echo, from the recordings.  Only meaningful if the call carried signal.
    if row["connect"] == "1":
        out = subprocess.run(
            ["python3", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "echofit.py"), pre],
            capture_output=True, text=True).stdout.split()
        if len(out) == 3:
            row["echo_lag_ms"], row["erl_db"], row["sig_echo_db"] = out

    print(",".join(row[k] for k in FIELDS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
