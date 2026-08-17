#!/usr/bin/env python3
"""audiostats.py -- signal levels for a call, robust to relay clicks.

    audiostats.py captures/v34-uk-1 [captures/v34-us-1 ...]
    audiostats.py --csv captures/v34-*-[123]

WHY NOT JUST PEAK AND RMS.

A call recording is not stationary and it is not clean.  The line relay
closing, the ATA switching path, and the far end going off-hook all put
impulses into the capture that are 20-30 dB above anything the modem
transmits, and they can echo back.  Peak is then a measurement of the loudest
click, and whole-file RMS is a blend of silence, handshake, tones and data in
whatever proportion the call happened to have.  Neither is comparable between
two calls.

So everything here is per-FRAME (20 ms, the RTP unit) and then summarised by
ORDER STATISTICS:

  * the MEDIAN frame level is the headline.  Half the frames are above it, so
    no cluster of impulses can move it, and it is directly comparable between
    calls of different lengths and different handshake durations.
  * P10 and P90 bracket the working range.
  * P99.9 and PEAK are reported TOGETHER so the gap between them is visible.
    A wide gap is the signature of impulses rather than of a loud signal.
  * CLICKS counts frames more than 12 dB above the median, which is far above
    anything a modem's own signal does frame to frame, and reports what
    fraction of the call they are.  If that number is not small, treat every
    other figure with suspicion.

THE ACTIVE WINDOW.  Levels are computed over frames above a floor derived
from the file itself (median of the non-silent half), so the silence before
the call and after the hangup does not drag the median down.  The window that
was used is printed, because a level over the wrong window is worse than no
level.

WHAT IS NOT HERE.  Echo timing.  `echofit.py` cross-correlates the tx and rx
dumps and reports lag, echo return loss and signal-to-echo, and it is the
right tool for that; this one is about levels.  For an echo measurement that
does not depend on the modem's own signal being correlated with itself, the
chirp probe (`chirpdelay.py`) is the instrument.
"""

import argparse
import os
import sys

import numpy as np

from capture_io import load

FS = 8000.0
FRAME = int(0.020 * FS)          # 20 ms, one RTP packet
FULL = 32768.0


def db(x):
    return 20.0 * np.log10(np.maximum(x, 1e-12) / FULL)


def frames(path):
    """Frame a capture at its OWN sample rate, not an assumed one.

    This used to `np.fromfile` a headerless `.raw` and frame it at a global
    FS = 8000, which was right for the `_8k` files and silently wrong for the
    datapump's native 9600 Hz captures.  The wav carries its rate, so the
    frame length is derived per file and the 20 ms RTP unit stays 20 ms.
    """
    x, rate = load(path)
    x = x.astype(np.float64)
    fr = int(0.020 * rate)
    n = (len(x) // fr) * fr
    if n == 0:
        return None, 0.0
    return x[:n].reshape(-1, fr), len(x) / rate


def stats(path):
    fr, dur = frames(path)
    if fr is None:
        return None
    rms = np.sqrt((fr * fr).mean(axis=1))
    live = rms > 0
    if not live.any():
        return None
    # A floor from the file itself: half the non-silent frames, then 20 dB down.
    floor = np.median(rms[live]) / 10.0
    act = rms > floor
    if act.sum() < 5:
        act = live
    a = rms[act]
    med = np.median(a)
    clicks = int((a > med * 10 ** (12 / 20.0)).sum())
    return {
        "dur": dur,
        "active": act.sum() * FRAME / FS,
        "median": db(med),
        "p10": db(np.percentile(a, 10)),
        "p90": db(np.percentile(a, 90)),
        "p999": db(np.percentile(a, 99.9)),
        "peak": db(np.abs(fr).max()),
        "clicks": clicks,
        "click_pct": 100.0 * clicks / len(a),
        "crest": db(np.abs(fr).max()) - db(med),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prefix", nargs="+")
    ap.add_argument("--csv", action="store_true")
    args = ap.parse_args()

    if args.csv:
        print("call,dir,active_s,median_dbfs,p10,p90,p999,peak,clicks,click_pct,crest_db")
    for pre in args.prefix:
        for d in ("tx", "rx"):
            p = "%s.modem_%s_8k" % (pre, d)
            if not os.path.exists(p + ".wav") and not os.path.exists(p + ".raw"):
                continue
            s = stats(p)
            if s is None:
                print("%s %s: no signal" % (pre, d))
                continue
            name = os.path.basename(pre)
            if args.csv:
                print("%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%.2f,%.1f"
                      % (name, d, s["active"], s["median"], s["p10"], s["p90"],
                         s["p999"], s["peak"], s["clicks"], s["click_pct"],
                         s["crest"]))
            else:
                print("%-16s %s  active %5.1f s of %5.1f   median %6.1f dBFS"
                      "  (P10 %6.1f  P90 %6.1f)" %
                      (name, d, s["active"], s["dur"], s["median"],
                       s["p10"], s["p90"]))
                print("%-16s %s  P99.9 %6.1f   peak %6.1f   crest %5.1f dB"
                      "   clicks %d (%.2f%% of frames)" %
                      ("", " " * len(d), s["p999"], s["peak"], s["crest"],
                       s["clicks"], s["click_pct"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
