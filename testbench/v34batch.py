#!/usr/bin/env python3
"""v34batch.py -- one V.34 batch reduced to handshakes, retrains and rates.

    v34batch.py --label 600r captures/ts0-off-*  --label complex2 captures/cx2-*

WHY ANOTHER WRAPPER.  `callstats.py` already turns one call into a CSV row and
is the tool of record for the covariate work, but the three quantities findings
1917 and 1921 are ABOUT -- how many handshakes, how many of them were retrains,
and what rate survived -- are spread across three files per call and were being
counted by hand.  Counting by hand is how the two arms of a comparison end up
having been counted differently.  This runs `callstats.py` for the rates and
adds the two counts from the same log, for both arms, in one invocation.

HOW A HANDSHAKE IS COUNTED, and it is not a judgement call.  `snrblocks.py`
starts a new handshake block at `Agc gain estimate at the end of phase 3`, and
the object emits exactly one `V34PROBEBINS` per phase 2, so the two agree call
for call on the archive.  This uses the probe count, because the same line is
what `bandshape.py` reads and the two analyses then cannot disagree about how
many handshakes a call had.

    handshakes  probes emitted in the call: 1 = trained once and stayed
    retrains    handshakes - 1, i.e. every handshake after the first

A RETRAIN IS NOT A FAILURE.  Finding 1917's "RETRAIN FAILURE" is the far end's
own word, from its diagnostic; `Retrains Granted` in the Courier's ATI6 is the
far end's count of the ones it agreed to.  Both are reported when the capture
has a `.lastlink.log`, and left blank when it does not, rather than being
inferred from our side.

DENOMINATORS.  Calls named, calls whose logs were found, calls that connected,
calls that carried a probe.  A call that never connected is kept in the table
and excluded from the rate medians, and both counts are printed -- an arm whose
connect rate moved is a different result from an arm whose rates moved.

n IS SMALL AND THIS TOOL SAYS SO on every summary line it prints.  This bench
has retracted four claims made at n=5-9 (findings 1206, 1207, 1970, 1972).
"""

import os
import re
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def read(path):
    try:
        return open(path, "rb").read().decode("latin-1")
    except OSError:
        return ""


def one(prefix):
    log = read(prefix + ".slmodemd.log")
    call = read(prefix + ".call.log")
    link = read(prefix + ".lastlink.log")
    if not log:
        return None

    hs = log.count("V34PROBEBINS")
    row = {"call": os.path.basename(prefix), "handshakes": hs,
           "retrains": max(hs - 1, 0)}

    # our receive rate, as the pty reported it at CONNECT
    m = re.search(r"pty << CONNECT (\d+)", call)
    row["connect"] = int(m.group(1)) if m else None
    row["our_rx"] = row["connect"]
    m = re.search(r"TxRate: (\d+)", call)
    row["our_tx"] = int(m.group(1)) if m else None

    # the far end's own view, which is the only witness to ITS direction
    m = re.search(r"Speed\s+(\d+)/(\d+)", link)
    row["far_tx"], row["far_rx"] = (int(m.group(1)), int(m.group(2))) if m else (None, None)
    m = re.search(r"Retrains Requested\s+(\d+)\s+Retrains Granted\s+(\d+)", link)
    row["far_req"], row["far_granted"] = (int(m.group(1)), int(m.group(2))) if m else (None, None)
    m = re.search(r"Recv/Xmit Level \(-dB\)\s+(\d+)/(\d+)", link)
    row["far_recv_lvl"], row["far_xmit_lvl"] = (int(m.group(1)), int(m.group(2))) if m else (None, None)
    m = re.search(r"Symbol Rate\s+(\d+)/(\d+)", link)
    row["baud"] = int(m.group(1)) if m else None
    m = re.search(r"Preemphasis \(-dB\)\s+(\d+)/(\d+)", link)
    row["preemph"] = "%s/%s" % (m.group(1), m.group(2)) if m else None
    row["far_present"] = bool(link)

    # callstats.py for the quantities it already owns, so the two tools cannot
    # disagree about the same call
    try:
        out = subprocess.run([sys.executable, os.path.join(HERE, "callstats.py"),
                              prefix], capture_output=True, text=True,
                             timeout=120).stdout.strip().splitlines()
        if out:
            f = out[-1].split(",")
            row["equerr_pre"] = f[9] or None
            row["erl_db"] = f[7] or None
            row["connect_secs"] = f[15] or None
    except Exception:
        pass
    return row


def summarise(label, rows):
    n = len(rows)
    conn = [r for r in rows if r["connect"]]
    probed = [r for r in rows if r["handshakes"] > 0]
    print("-" * 76)
    print("ARM %s" % label)
    print("  DENOMINATORS: %d calls named, %d logs found, %d connected, "
          "%d carried a probe" % (n, n, len(conn), len(probed)))
    if not rows:
        print("  nothing to summarise")
        return None
    print("  %-22s %5s %5s %8s %8s %8s %8s %5s %6s"
          % ("call", "hs", "retr", "our_rx", "our_tx", "far_tx", "far_rx",
             "grant", "baud"))
    for r in rows:
        print("  %-22s %5d %5d %8s %8s %8s %8s %5s %6s"
              % (r["call"], r["handshakes"], r["retrains"],
                 r["connect"] or "-", r["our_tx"] or "-",
                 r["far_tx"] or "-", r["far_rx"] or "-",
                 "-" if r["far_granted"] is None else r["far_granted"],
                 r["baud"] or "-"))

    def med(key, src):
        v = [r[key] for r in src if r.get(key)]
        return (statistics.median(v), len(v)) if v else (None, 0)

    hs = [r["handshakes"] for r in probed]
    rt = [r["retrains"] for r in probed]
    rx, nrx = med("connect", conn)
    ftx, nftx = med("far_tx", conn)
    print()
    print("  connected            %d of %d" % (len(conn), n))
    if hs:
        print("  handshakes per call  median %.1f  mean %.2f  range %d-%d  (n=%d)"
              % (statistics.median(hs), statistics.mean(hs), min(hs), max(hs),
                 len(hs)))
        print("  retrains per call    median %.1f  mean %.2f  range %d-%d  (n=%d)"
              % (statistics.median(rt), statistics.mean(rt), min(rt), max(rt),
                 len(rt)))
        print("  calls with >=1 retrain  %d of %d = %.0f%%"
              % (sum(1 for x in rt if x), len(rt),
                 100.0 * sum(1 for x in rt if x) / len(rt)))
    print("  our RX rate          median %s  (n=%d)   values %s"
          % (rx, nrx, sorted(r["connect"] for r in conn)))
    print("  far end's TX rate    median %s  (n=%d)" % (ftx, nftx))
    print("  n IS SMALL: %d calls.  Do not read this as settled." % n)
    return {"n": n, "conn": len(conn), "hs": hs, "rt": rt,
            "rx": [r["connect"] for r in conn],
            "ftx": [r["far_tx"] for r in conn if r["far_tx"]]}


def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        return 2
    arms, label, cur = [], None, []
    it = iter(argv)
    for tok in it:
        if tok == "--label":
            if label is not None:
                arms.append((label, cur))
            label, cur = next(it), []
        else:
            cur.append(tok)
    arms.append((label or "unlabelled", cur))

    out = []
    for lab, pres in arms:
        pres = [p[:-len(".slmodemd.log")] if p.endswith(".slmodemd.log") else p
                for p in pres]
        rows = [r for r in (one(p) for p in sorted(set(pres))) if r]
        out.append((lab, summarise(lab, rows)))

    if len(out) == 2 and out[0][1] and out[1][1]:
        a, b = out[0][1], out[1][1]
        print("=" * 76)
        print("ARM-TO-ARM  %s (n=%d) against %s (n=%d)"
              % (out[1][0], b["n"], out[0][0], a["n"]))
        for k, name in (("hs", "handshakes/call"), ("rt", "retrains/call"),
                        ("rx", "our RX rate"), ("ftx", "far end TX rate")):
            va, vb = a[k], b[k]
            if not va or not vb:
                print("  %-18s not comparable (n=%d, %d)" % (name, len(va), len(vb)))
                continue
            print("  %-18s %s median %s (n=%d)   %s median %s (n=%d)   delta %+.1f"
                  % (name, out[0][0], statistics.median(va), len(va),
                     out[1][0], statistics.median(vb), len(vb),
                     statistics.median(vb) - statistics.median(va)))
        print("  connected          %s %d/%d    %s %d/%d"
              % (out[0][0], a["conn"], a["n"], out[1][0], b["conn"], b["n"]))
        print("  BOTH ARMS ARE SMALL.  This is a first look, not a result.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
