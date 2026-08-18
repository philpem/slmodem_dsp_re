#!/usr/bin/env python3
"""bandshape.py -- the channel's magnitude response and in-band tilt, from the
V.34 line probe, scored against the curve the emulator hardcodes.

    bandshape.py captures/*.slmodemd.log --label 600r
    bandshape.py --label 600r  a/*.log  --label complex2  b/*.log

WHY NOT THE CHIRP, which is what the brief asked for.  The chirp probe
(`chirpdelay.py`, finding 1215) is injected by d-modem at the RTP boundary
under `DMODEM_CHIRP`.  That code is GONE: it was an uncommitted working-tree
change to `d-modem.c`, and commit df93682d ("the vendored fork is cryan209's
again") restored the fork to its committed state and took it with it.  It is
not in any of the three built binaries either.  Rebuilding it means editing
`d-modem/`.

AND IT WOULD NOT HAVE ANSWERED THE QUESTION ANYWAY, which is the better
reason.  Every archived `.chirp.wav` is a 100 ms sweep over roughly
600-3000 Hz.  Every point of the emulator's fit that is in dispute is at
3400, 3700 and 3900 Hz.  The chirp has no energy where the answer is.

THE LINE PROBE DOES.  V.34 11.2 / Table 17: tones 150 Hz apart from 150 Hz to
3750 Hz, with 900/1200/1800/2400 omitted so the receiver can read the noise
floor.  `dftfreqinit` gives bin n = n * 150 Hz at the datapump's 9600 Hz rate,
so the 25 bins span the whole band and FOUR of them -- 3300, 3450, 3600,
3750 -- sit in the roll-off the fit describes.  It is emitted at the probe,
before the far end applies any pre-emphasis, which is the window finding 1907
says is the only safe one for a channel measurement.  1956 measured the
band-edge cliff this way and probeplot.py already reads it.

WHAT IS REPORTED, and why each column is here:

  * **Per bin: median dB relative to the 750 Hz reference, and n.**  Median,
    not mean, because a retrain-heavy call contributes several probes and a
    dud bin should not drag the estimate.
  * **`floored`, as a fraction.**  `dftenergy` publishes
    `energy = (short)(e >> 16)`, so a bin below 65536 reads ZERO and the value
    is reconstructed from `shift` alone -- a LOWER BOUND, not a reading.  At a
    rolling-off band edge the top bins are exactly the ones that go floored.
    A response quoted from floored bins is a response that is at least that
    steep and may be steeper, and no fit may be taken from them.
  * **The fit's own value at each bin, and the residual.**  The fit is
    interpolated ONTO THE BIN GRID, never the other way round: the bins are
    the measurement and the fit is the thing being scored.
  * **In-band tilt over TWO bands, and they disagree on purpose.**  450-3150
    is finding 1956's band and is the honest "is there tilt" number, because
    it stops short of the codec corner.  300-3400 is what the brief asks for
    and it includes the corner, so most of its slope is band limit wearing
    tilt's clothes.  Both are printed; read the verdict off the first.

DENOMINATORS ON EVERY LINE.  Logs read, logs carrying a probe, probes, and
per-bin n.  A glob that matches nothing must not print like a clean channel.
"""

import argparse
import os
import sys

import numpy as np

# probeplot owns the parse and the dB reconstruction; importing them is what
# makes this the SAME tool as the plot rather than a second opinion.
from probeplot import HZ_PER_BIN, NOISE_BINS, REF_BIN, parse, to_db

#
# THE CURVE EVERY EMULATOR RESULT RIDES ON.  chanshim.py:73-74, attributed to
# finding 1907.  Reproduced here verbatim so the comparison is against the
# literal the emulator uses, not against a paraphrase of it.
#
FIT_F = np.array([0, 300, 3000, 3300, 3400, 3700, 3900, 4000], float)
FIT_D = np.array([0, 0, 0, 0, -6.2, -15.5, -33.6, -40.0], float)


def fit_db(f):
    """1907's fit, referenced to 750 Hz as the probe bins are."""
    return np.interp(f, FIT_F, FIT_D) - np.interp(750.0, FIT_F, FIT_D)


def collect(logs):
    """Every probe in every log, as an (n_probe, 25) dB array + floored mask."""
    dbs, flrs = [], []
    n_logs = n_with = 0
    for path in logs:
        n_logs += 1
        probes = parse(path)
        if probes:
            n_with += 1
        for energy, shift in probes:
            db, floored = to_db(energy, shift)
            dbs.append(db - db[REF_BIN])
            flrs.append(floored)
    if not dbs:
        return None, None, n_logs, n_with
    return np.array(dbs), np.array(flrs), n_logs, n_with


def tilt(f, med, floored_frac, lo, hi, max_floored=0.5):
    """Least-squares slope in dB across [lo, hi], on bins that are readings.

    A floored bin is a lower bound, so a bin floored on most probes is
    excluded rather than fitted; the count that survived is returned with the
    number, because a slope over three bins is not a slope over twenty.
    """
    band = (f >= lo) & (f <= hi)
    noise = np.array([i in NOISE_BINS for i in range(len(f))])
    use = band & ~noise & (floored_frac <= max_floored)
    if use.sum() < 4:
        return None, int(use.sum())
    sl, _ = np.polyfit(f[use], med[use], 1)
    return sl * (hi - lo), int(use.sum())


def report(label, logs):
    dbs, flrs, n_logs, n_with = collect(logs)
    print("=" * 72)
    print("ARM %s" % label)
    if dbs is None:
        print("  NO PROBES: %d logs read, 0 carried a V34PROBEBINS line."
              "  Nothing is reported rather than a clean-looking zero."
              % n_logs)
        return None
    n_probe, n_bin = dbs.shape
    f = (np.arange(n_bin) + 1) * HZ_PER_BIN
    med = np.median(dbs, axis=0)
    q1 = np.percentile(dbs, 25, axis=0)
    q3 = np.percentile(dbs, 75, axis=0)
    ffrac = flrs.mean(axis=0)
    fitv = fit_db(f)

    print("  DENOMINATORS: %d logs read, %d carried a probe, %d probes, "
          "%d bins each" % (n_logs, n_with, n_probe, n_bin))
    print()
    print("  %-4s %6s %8s %8s %8s %8s %9s %9s %s"
          % ("bin", "Hz", "median", "q1", "q3", "floored", "1907 fit",
             "residual", ""))
    for i in range(n_bin):
        note = ""
        if i in NOISE_BINS:
            note = "noise bin (no tone by design)"
        elif ffrac[i] > 0.5:
            note = "FLOORED on %.0f%% of probes -- a LOWER BOUND" % (100 * ffrac[i])
        elif ffrac[i] > 0.0:
            note = "floored on %.0f%%" % (100 * ffrac[i])
        resid = med[i] - fitv[i]
        print("  %-4d %6.0f %8.2f %8.2f %8.2f %7.0f%% %9.2f %9.2f  %s"
              % (i + 1, f[i], med[i], q1[i], q3[i], 100 * ffrac[i],
                 fitv[i], resid if i not in NOISE_BINS else float("nan"),
                 note))
    print()
    for lo, hi, why in ((450, 3150, "finding 1956's band -- stops short of the "
                                    "codec corner, so this IS the tilt"),
                        (300, 3400, "the brief's band -- includes the corner, "
                                    "so it is tilt PLUS band limit")):
        t, n = tilt(f, med, ffrac, lo, hi)
        if t is None:
            print("  tilt %4d-%4d Hz : not evaluable, only %d usable bins"
                  % (lo, hi, n))
        else:
            print("  tilt %4d-%4d Hz : %+6.2f dB across the band, over %d bins"
                  "   (%s)" % (lo, hi, t, n, why))
    print()
    # The four bins the fit is actually about.
    print("  THE FOUR BINS THE FIT IS ABOUT:")
    for i in range(n_bin):
        if f[i] < 3200:
            continue
        print("    %6.0f Hz  measured %7.2f dB   fit %7.2f dB   "
              "measured is %+.2f dB %s the fit%s"
              % (f[i], med[i], fitv[i], med[i] - fitv[i],
                 "above" if med[i] > fitv[i] else "below",
                 "   [FLOORED, lower bound]" if ffrac[i] > 0.5 else ""))
    return {"f": f, "med": med, "ffrac": ffrac, "n_probe": n_probe,
            "n_logs": n_logs, "n_with": n_with, "dbs": dbs}


def main():
    # Hand-parsed, because `--label` repeats and argparse cannot express
    # "this flag partitions the positional list".  Two arms is the whole use.
    argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0 if argv else 2

    arms, label, cur = [], None, []
    it = iter(argv)
    for tok in it:
        if tok == "--label":
            if label is not None:
                arms.append((label, cur))
            label, cur = next(it), []
        else:
            cur.append(tok)
    if label is None:
        label = "unlabelled"
    arms.append((label, cur))

    out = []
    for lab, logs in arms:
        out.append((lab, report(lab, logs)))

    if len(out) == 2 and out[0][1] and out[1][1]:
        a0, a1 = out[0][1], out[1][1]
        print("=" * 72)
        print("ARM-TO-ARM: %s minus %s   (n = %d and %d probes)"
              % (out[1][0], out[0][0], a0["n_probe"], a1["n_probe"]))
        print("  %-4s %6s %10s %10s %10s %s"
              % ("bin", "Hz", out[0][0], out[1][0], "delta", ""))
        for i in range(len(a0["f"])):
            note = ""
            if i in NOISE_BINS:
                note = "noise bin"
            elif a0["ffrac"][i] > 0.5 or a1["ffrac"][i] > 0.5:
                note = "floored in at least one arm -- delta is not a reading"
            print("  %-4d %6.0f %10.2f %10.2f %10.2f  %s"
                  % (i + 1, a0["f"][i], a0["med"][i], a1["med"][i],
                     a1["med"][i] - a0["med"][i], note))
    return 0


if __name__ == "__main__":
    sys.exit(main())
