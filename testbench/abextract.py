#!/usr/bin/env python3
"""abextract.py -- pull the rate decision out of a run's logs, correctly.

    abextract.py captures/pab-*-*.run.log > captures/preemph-ab.csv

WHY THIS EXISTS RATHER THAN A GREP IN THE RUNNER.  The obvious
`grep 'equerr = [0-9]*'` is wrong and wrong in a way that looks plausible: a
V.34 call logs `V34EQU, equerr = N` about 57 times as the equaliser trains,
and `V34DATARATE, equerr = N,preerr=M` exactly once, when the rate is chosen.
Taking the first match gets an early training sample -- 30431 on the first
call of the pre-emphasis A/B, against a decision value in the low thousands --
and 30431 next to a 28800 connection reads as a contradiction of the object's
own threshold table rather than as a measurement error.

So the anchor here is the full `V34DATARATE` line, which appears once and
carries both figures together.

THE PRE-EMPHASIS INDEX IS PER BAUD RATE.  `V34PREEMPHASIS, - index is N,
baudrate= B` is emitted once for each candidate rate, ten times a call. Only
the one at the baud rate actually used means anything, and this bench runs
3429 throughout, so that is what is taken -- and the symbol rate is read from
the log rather than assumed, so a call that fell back is not silently
mislabelled.
"""

import csv
import os
import re
import sys


def read(path):
    try:
        return open(path, "rb").read().decode("latin-1")
    except OSError:
        return ""


def main():
    w = csv.writer(sys.stdout)
    w.writerow(["call", "arm", "connect", "our_tx", "our_rx",
                "equerr", "preerr", "preemph", "tx_baud", "rx_baud",
                "decisions", "retrains", "far_recv", "far_xmit"])
    for run in sorted(sys.argv[1:]):
        base = run[:-len(".run.log")] if run.endswith(".run.log") else run
        name = os.path.basename(base)
        arm = name.split("-")[1] if "-" in name else ""
        t = read(run)
        sl = read(base + ".slmodemd.log")

        m = re.search(r"pty +CONNECT (\d+)", t)
        rx = m.group(1) if m else ""
        m = re.search(r"TxRate: *(\d+)", t)
        tx = m.group(1) if m else ""

        # PAIR THE DECISION WITH THE RATE THE CALL ACTUALLY CARRIED.
        #
        # A call can hold several decision blocks -- 84 of 247 stock calls do,
        # up to five -- because a retrain or a renegotiation evaluates the rate
        # again.  Neither "the first" nor "the last" is right, and both were
        # tried and were wrong: `base-cx2-10` logs 7200 then 14400 and CONNECTs
        # at 7200, while `base-cx2-2` logs 9600 then 4800 and CONNECTs at 4800.
        # Some evaluations do not take effect.
        #
        # So match on the OUTCOME: find the `finally ... rxbitrate N` whose N
        # equals the rate the DTE was actually told, and take the equerr from
        # the decision immediately preceding it.  If nothing matches -- the
        # call never connected -- fall back to the last block and say so with
        # an empty rate rather than pairing something arbitrary.
        blocks, pend = [], None
        for m in re.finditer(r"V34DATARATE, (?:equerr = (\d+),preerr=(\d+)"
                             r"|finally txbitrate \d+,rxbitrate (\d+))", sl):
            if m.group(1):
                pend = (m.group(1), m.group(2))
            elif pend:
                blocks.append((m.group(3), pend[0], pend[1]))
        eq = pe = ""
        if blocks:
            hit = next((b for b in blocks if rx and b[0] == rx), blocks[-1])
            eq, pe = hit[1], hit[2]
        ndec = str(len(blocks))

        m = re.search(r"setfinalrate, txbaudrate = (\d+),\s*rxbaudrate = (\d+)", sl)
        txb, rxb = (m.group(1), m.group(2)) if m else ("", "")

        pp = ""
        if txb:
            m = re.search(r"V34PREEMPHASIS, - index is (\d+), baudrate= %s" % txb, sl)
            if m:
                pp = m.group(1)

        nret = str(len(re.findall(r"retrain request detected", sl)))

        # THE FAR END'S OWN ACCOUNT OF WHAT IT TRANSMITTED.  `pty CONNECT nnn`
        # is emitted once and never revised, so a post-CONNECT upward
        # renegotiation -- which does happen and does take effect -- leaves it
        # stale: pab3-fix-3 said CONNECT 14400 while the Courier reported
        # transmitting 26400.  ATI11's `Speed recv/xmit` is the other end's
        # measurement and is not subject to that.
        far_rx = far_tx = ""
        m = re.search(r"Speed\s+(\d+)/(\d+)", read(base + ".lastlink.log"))
        if m:
            far_rx, far_tx = m.group(1), m.group(2)
        w.writerow([name, arm, "1" if rx else "0", tx, rx, eq, pe, pp, txb, rxb,
                    ndec, nret, far_rx, far_tx])
    return 0


if __name__ == "__main__":
    sys.exit(main())
