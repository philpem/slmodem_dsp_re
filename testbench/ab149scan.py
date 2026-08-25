#!/usr/bin/env python3
"""ab149scan.py -- score the #149 A/B exactly as records/ab149-PREREG.txt says.

    ab149scan.py captures/ab149-*.slmodemd.log

CONNECT SUCCESS IS CHECKED AND PRINTED FIRST, before any rate or handshake
number, because that is where the 1932 regression appeared -- it gave 1 connect
in 4 while its rates looked unremarkable.  Reading the rates first is how that
would be missed again.

THE PRIMARY OUTCOME IS THE ONE THE PRE-REGISTRATION NAMES and nothing else is
substituted: handshakes in a FIXED 45 s window from the first "V34 bulk delay
estimation" -- the BLOB's own string, so the measurement cannot share a bug
with this project's instrumentation -- on calls with at least 45 s of carrier.

THE KEYSTONE test needs the BENCHANCHOR line row.sh now writes.  slmodemd
stamps <t> seconds since its own start; pjmedia stamps wall clock; the anchor
gives the offset between them.  Captures without it are reported as
uncorrelatable rather than aligned by file position, which does not work --
d-modem's stdout is a file and glibc block-buffers it, so a burst of its lines
can land anywhere relative to slmodemd's.
"""

import argparse
import os
import re
import statistics as st
import time
import sys
from collections import defaultdict

HS = re.compile(r"<\s*([\d.]+)>\s*V34 bulk delay estimation ")
DR = re.compile(r"<\s*([\d.]+)>\s*V34DATARATE, finally txbitrate \d+,rxbitrate (\d+)")
JB = re.compile(r"(\d\d):(\d\d):(\d\d)\.(\d+).*JBSTAT tick: discard=\d+ \([^)]*\) "
                r"lost=(\d+) \(\+(\d+)\) empty=(\d+) \(\+(\d+)\)")
ANCHOR = re.compile(r"BENCHANCHOR wallclock=\S+ epoch=([\d.]+)")
WINDOW = 45.0


def read(path):
    txt = open(path, "rb").read().decode("latin-1")
    hs = [float(m.group(1)) for m in HS.finditer(txt)]
    dr = [(float(m.group(1)), int(m.group(2))) for m in DR.finditer(txt)]
    a = ANCHOR.search(txt)
    jb = []
    for m in JB.finditer(txt):
        wall = (int(m.group(1)) * 3600 + int(m.group(2)) * 60 +
                int(m.group(3)) + int(m.group(4)) / 1000.0)
        jb.append((wall, int(m.group(6)) + int(m.group(8))))   # deltas
    return hs, dr, (float(a.group(1)) if a else None), jb, txt


def rate_from_dte(base):
    """The modem's rate is the SECOND CONNECT in row.sh's own output.

    row.sh drives two ends and prints both: the first is the local serial
    port to the hardware modem at 115200, which is a DTE speed and not a
    line rate.  Taking the first is how a batch of real connects once scored
    as 0/12.  Anything >= 115200 is a DTE speed and is skipped.
    """
    p = base + ".run.txt"
    if not os.path.exists(p):
        return None
    vals = [int(x) for x in re.findall(r"CONNECT[ /]*(\d+)",
                                       open(p, errors="replace").read())]
    vals = [v for v in vals if v < 115200]
    return vals[0] if vals else None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--window", type=float, default=2.0,
                    help="seconds before a retrain to look for underruns")
    args = ap.parse_args()

    arms = defaultdict(list)
    for p in sorted(args.logs):
        m = re.search(r"ab149-(\d+)-a(\d)-(\d+)", os.path.basename(p))
        if not m:
            continue
        dest, arm = m.group(1), int(m.group(2))
        hs, dr, anchor, jb, txt = read(p)
        base = p[:-len(".slmodemd.log")]
        rate = rate_from_dte(base)
        span = (max(t for t, _ in dr) - hs[0]) if (hs and dr) else 0.0
        win = sum(1 for t in hs if t - hs[0] < WINDOW) if hs else 0
        arms[arm].append(dict(dest=dest, rate=rate, span=span, win=win,
                              hs=hs, jb=jb, anchor=anchor, path=p))

    if not arms:
        sys.exit("ab149scan: no ab149-* logs matched")

    # ---- 1. CONNECT SUCCESS, FIRST AND ALONE -----------------------------
    print("=" * 68)
    print("1. CONNECT SUCCESS  (checked first -- this is where 1932 failed)")
    print("=" * 68)
    for arm in sorted(arms):
        v = arms[arm]
        ok = sum(1 for c in v if c["rate"])
        print("   arm %d (%s): %d/%d connected"
              % (arm, "control" if arm == 0 else "treatment", ok, len(v)))
    if len(arms) == 2:
        c = sum(1 for x in arms[0] if x["rate"]), len(arms[0])
        t = sum(1 for x in arms[1] if x["rate"]), len(arms[1])
        if t[1] and c[1] and (t[0] / t[1]) < (c[0] / c[1]) - 0.2:
            print("\n   *** TREATMENT CONNECTS MATERIALLY LESS OFTEN ***")
            print("   The pre-registration says abandon the change on this.")

    # ---- 2. PRIMARY OUTCOME ----------------------------------------------
    print("\n" + "=" * 68)
    print("2. PRIMARY: handshakes in a fixed %.0f s window (>=%.0f s carrier)"
          % (WINDOW, WINDOW))
    print("=" * 68)
    for arm in sorted(arms):
        v = [c for c in arms[arm] if c["span"] >= WINDOW]
        if not v:
            print("   arm %d: no call reached %.0f s of carrier" % (arm, WINDOW))
            continue
        w = [c["win"] for c in v]
        print("   arm %d (%-9s) n=%-2d  median %.1f  mean %.2f   %s"
              % (arm, "control" if arm == 0 else "treatment", len(v),
                 st.median(w), st.mean(w), sorted(w)))

    # ---- 3. SECONDARY ----------------------------------------------------
    print("\n" + "=" * 68)
    print("3. SECONDARY (reported, not decisive)")
    print("=" * 68)
    for arm in sorted(arms):
        v = arms[arm]
        r = [c["rate"] for c in v if c["rate"]]
        sp = [c for c in v if 55 <= c["span"] <= 95 and c["rate"]]
        jb = [sum(d for _, d in c["jb"]) / c["span"]
              for c in v if c["span"] > 10 and c["jb"]]
        print("   arm %d  median rate %-7s  span-matched %-7s  jb events/s %s"
              % (arm, st.median(r) if r else "-",
                 st.median([c["rate"] for c in sp]) if sp else "-",
                 ("%.3f" % st.median(jb)) if jb else "-"))
        for d in sorted({c["dest"] for c in v}):
            rr = [c["rate"] for c in v if c["dest"] == d and c["rate"]]
            print("        dest %s: %s" % (d, sorted(rr)))

    # ---- 4. KEYSTONE ------------------------------------------------------
    print("\n" + "=" * 68)
    print("4. KEYSTONE: do jitter-buffer underruns cluster before retrains?")
    print("=" * 68)
    obs, null = [], []
    noanchor = 0
    for arm in sorted(arms):
        for c in arms[arm]:
            if c["anchor"] is None:
                noanchor += 1
                continue
            if len(c["hs"]) < 2 or not c["jb"] or c["span"] <= 0:
                continue
            # BOTH CLOCKS ARE THE SAME CLOCK.  slmodemd's "<t>" is not
            # seconds since its own start -- it is **Unix epoch seconds mod
            # 1000**, verified on all 24 logs of this batch: the anchor's
            # `epoch % 1000` equals the log's first <t> to within 4 ms, every
            # time.  So the two stamp formats were always convertible and the
            # earlier claim that they were not was simply wrong.
            #
            # Work in epoch space.  The mod-1000 wrap is resolved by the
            # anchor, which is why it is still worth writing.
            A = c["anchor"]
            base = (A // 1000) * 1000
            def sl_epoch(t, base=base, A=A):
                e = base + t
                if e < A - 500:
                    e += 1000
                return e
            lt = time.localtime(A)
            midnight = A - (lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec) \
                         - (A - int(A))
            ev = [(midnight + w, d) for w, d in c["jb"]]
            rt_e = [sl_epoch(t) for t in c["hs"][1:]]
            ev = [(t, d) for t, d in ev if d > 0]
            if not ev:
                continue
            if not rt_e:
                continue
            inwin = sum(d for t, d in ev
                        for r in rt_e if 0 <= r - t <= args.window)
            obs.append(inwin / (len(rt_e) * args.window))
            null.append(sum(d for _, d in ev) / c["span"])
    if noanchor:
        print("   %d call(s) have NO BENCHANCHOR and are UNCORRELATABLE." % noanchor)
        print("   (Do not align by file position: d-modem's stdout is block")
        print("    buffered, so its lines can land anywhere in the file.)")
    if obs:
        o, n = st.mean(obs), st.mean(null)
        print("   underruns/s in the %.1f s before a retrain : %.3f" % (args.window, o))
        print("   underruns/s over the whole call            : %.3f" % n)
        print("   ratio (>1 means they CLUSTER before retrains): %.2fx"
              % (o / n if n else float("nan")))
        print("\n   A ratio near 1 means underruns are simply common and any")
        print("   clustering is an artefact of that -- which is exactly how the")
        print("   tap-drift and rate-overshoot hypotheses died (finding F1918).")
    else:
        print("   not enough anchored calls with both retrains and underruns.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
