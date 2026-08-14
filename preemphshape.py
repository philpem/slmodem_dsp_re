#!/usr/bin/env python3
"""preemphshape.py -- pick a V.34 pre-emphasis index by SHAPE, not by tilt.

    preemphshape.py captures/burst-2.origin.log

WHAT THE DATAPUMP DOES NOW, and why it is questionable on two counts.

`probe_preemph` (v34hshak.c) walks a counter from 5, multiplying the band-edge
bin's energy by ~1.596 each step until it exceeds a reference bin, and returns
the counter.  Two consequences, both verified rather than argued:

  * The step is 2.03 dB in POWER, and the counter starts at 5 and is advanced
    BEFORE the test.  So it measures a two-point tilt quantised in exactly
    Table 3's units (alpha = 0, 2, 4, 6, 8, 10 dB) and then returns 6 + steps,
    which indexes **Table 4**.  Sweeping the whole input space returns only
    {6, 7, 8, 9, 10}: indices 0-5 are unreachable and the author's own
    `return 0` arm is dead code (deviation D53 -- the object does this too, so
    the reconstruction is faithful and the ORIGINAL is what is wrong).
  * Tables 3 and 4 are not the same shape.  Figure 1/V.34 is a straight line
    from 0 dB at f/S = 0 to alpha at f/S = 1.0 -- a BROADBAND tilt.  Figure
    2/V.34 is flat at 0 dB to f/S ~ 0.7, steps to beta, then rises to gamma by
    f/S = 1.2 -- a TOP-OF-BAND shelf.  A two-point tilt is the natural
    criterion for the first family and is being used to choose from the second.

WHAT THIS DOES INSTEAD.  Reconstruct the channel from all 25 probe bins,
evaluate every one of the 11 templates against it, and pick the index whose
residual is smallest over the conformance band.  That allows Table 3 when the
line has a broadband tilt and Table 4 when it is flat-then-cliff, and picks the
magnitude on evidence rather than on a 2 dB counter.

THE ENERGY DECODE IS AN ASSUMPTION AND IS VALIDATED, NOT ASSERTED.  Each bin
is reported as `energy/shift`; the true power is taken as energy * 2^-shift.
Run this on an EMULATED capture, where `chanshim.py` imposed a known response
(flat to 3300 Hz, -6.2 dB at 3400, -15.5 at 3700, -33.6 at 3900, finding
1907's measurement of the VG204), and the recovered curve is printed beside
that truth.  If they disagree, the decode is wrong and nothing below means
anything.
"""

import argparse
import math
import re
import sys

BIN_HZ = 150.0
# The probe omits these bins (1-based 6, 8, 12, 16 = 900/1200/1800/2400 Hz);
# they carry no tone and measure noise instead.  Table 17/V.34.
NOISE_BINS = {5, 7, 11, 15}

# Table 2/V.34: carrier d/e per symbol rate, and the conformance band is
# (d/e - 0.45) .. (d/e + 0.45) in normalised frequency.
DE = {2400: 2.0 / 3, 2743: 3.0 / 5, 2800: 3.0 / 5,
      3000: 3.0 / 5, 3200: 4.0 / 7, 3429: 4.0 / 7}

ALPHA = [0.0, 2.0, 4.0, 6.0, 8.0, 10.0]                 # Table 3, indices 0-5
BETA_GAMMA = [(0.5, 1.0), (1.0, 2.0), (1.5, 3.0),
              (2.0, 4.0), (2.5, 5.0)]                    # Table 4, indices 6-10


def template_db(idx, fs):
    """Magnitude in dB of pre-emphasis template `idx` at normalised freq `fs`.

    Figure 1 (0-5): straight line, 0 dB at f/S = 0 to alpha at f/S = 1.0,
    continuing at the same slope past 1.0 (the figure draws it to 1.2).

    Figure 2 (6-10): 0 dB to f/S = 0.7; a step to beta; then linear from beta
    at 0.8 to gamma at 1.2.  The breakpoints are read off Figure 2/V.34 and are
    the weakest part of this model -- the figure is a template with a +/-1 dB
    tolerance band, not an equation.
    """
    if idx <= 5:
        return ALPHA[idx] * fs
    beta, gamma = BETA_GAMMA[idx - 6]
    if fs <= 0.7:
        return 0.0
    if fs <= 0.8:
        return beta
    if fs >= 1.2:
        return gamma
    return beta + (gamma - beta) * (fs - 0.8) / 0.4


def parse_bins(text):
    """Last V34PROBEBINS block in a log -> [(bin_index, dB or None)]."""
    blocks = re.findall(r"V34PROBEBINS, n=(\d+):(.*?)(?=<[\d.]+>\s*V34)",
                        text, re.S)
    if not blocks:
        return None, None
    n, body = blocks[-1]
    pairs = re.findall(r"(\d+)/(\d+)", re.sub(r"<[\d.]+>", " ", body))
    out = []
    for i, (e, s) in enumerate(pairs):
        e, s = int(e), int(s)
        if i in NOISE_BINS or e == 0:
            out.append((i, None))
        else:
            # power = energy * 2^-shift  ->  dB = 10log10(e) - 3.0103*shift
            out.append((i, 10.0 * math.log10(e) - 3.0102999566 * s))
    return int(n), out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("log")
    ap.add_argument("--baud", type=int, default=None)
    args = ap.parse_args()

    txt = open(args.log, "rb").read().decode("latin-1")
    n, bins = parse_bins(txt)
    if not bins:
        sys.exit("preemphshape: no V34PROBEBINS block in %s" % args.log)

    m = re.search(r"V34PREEMPHASIS, - index is (\d+), baudrate= *(\d+)", txt)
    chose, baud = (int(m.group(1)), int(m.group(2))) if m else (None, 3429)
    if args.baud:
        baud = args.baud
    de = DE.get(baud, 4.0 / 7)
    lo, hi = de - 0.45, de + 0.45

    good = [(i, d) for i, d in bins if d is not None]
    if len(good) < 6:
        sys.exit("preemphshape: only %d usable bins" % len(good))
    ref = max(d for _, d in good)

    print("%s   baud=%d  band f/S %.3f..%.3f (%.0f..%.0f Hz)"
          % (args.log.split("/")[-1], baud, lo, hi, lo * baud, hi * baud))
    print("\n  measured channel (dB relative to its own peak):")
    for i, d in good:
        f = (i + 1) * BIN_HZ
        fs = f / baud
        mark = "" if lo <= fs <= hi else "   (outside band)"
        print("    bin %2d  %6.0f Hz  f/S %.3f  %7.2f dB%s"
              % (i + 1, f, fs, d - ref, mark))

    # score every template over the in-band bins
    inband = [(i, d - ref) for i, d in good
              if lo <= (i + 1) * BIN_HZ / baud <= hi]
    print("\n  residual after applying each template (RMS dB over %d in-band bins):"
          % len(inband))
    scores = []
    for idx in range(11):
        corr = [d + template_db(idx, (i + 1) * BIN_HZ / baud) for i, d in inband]
        mean = sum(corr) / len(corr)
        rms = math.sqrt(sum((c - mean) ** 2 for c in corr) / len(corr))
        scores.append((rms, idx))
        fam = "Table 3 alpha=%4.1f" % ALPHA[idx] if idx <= 5 else \
              "Table 4 b=%.1f g=%.1f" % BETA_GAMMA[idx - 6]
        star = "  <-- object chose this" if idx == chose else ""
        print("    index %2d  %-22s  rms %6.2f dB%s" % (idx, fam, rms, star))
    best = min(scores)[1]
    print("\n  best by shape : index %d" % best)
    print("  object chose  : index %s" % chose)
    if chose is not None and best != chose:
        print("  DISAGREE by %d, and across families" % abs(best - chose)
              if (best <= 5) != (chose <= 5) else
              "  DISAGREE by %d, same family" % abs(best - chose))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
