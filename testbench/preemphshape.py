#!/usr/bin/env python3
"""preemphshape.py -- pick a V.34 pre-emphasis index by SHAPE, not by tilt.

    preemphshape.py captures/burst-2.origin.log

WHAT THE DATAPUMP DOES NOW, and why it is questionable on two counts.

`probe_preemp` (v34hshak.c) walks a counter from 5, multiplying the band-edge
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
    2/V.34 holds 0 dB out to f/S = 0.4, transitions freely to beta at 0.8, then
    rises to **beta + gamma** at 1.2 -- a TOP-OF-BAND shelf.  A two-point tilt
    is the natural criterion for the first family and is being used to choose
    from the second.  (Phil read the stacked dimension arrows correctly and I
    did not: the top is beta + gamma, and the 0.4-0.8 segment is unconstrained.
    See template_db().)

WHAT THIS DOES INSTEAD.  Reconstruct the channel from all 25 probe bins,
evaluate every one of the 11 templates against it, and pick the index whose
residual is smallest over the conformance band.  That allows Table 3 when the
line has a broadband tilt and Table 4 when it is flat-then-cliff, and picks the
magnitude on evidence rather than on a 2 dB counter.

THE ENERGY DECODE IS AN ASSUMPTION AND IS VALIDATED, NOT ASSERTED.  Each bin
is reported as `energy/shift`; the true power is taken as energy * 2^-shift.
Run this on an EMULATED capture, where `vbt-chanshim` imposed a known response
(flat to 3300 Hz, -6.2 dB at 3400, -15.5 at 3700, -33.6 at 3900, finding
F1907's measurement of the VG204), and the recovered curve is printed beside
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

    Figure 2 (6-10): 0 dB out to f/S = 0.4, then a free transition to beta at
    f/S = 0.8, then linear to **beta + gamma** at f/S = 1.2.

    THE TOP OF THE TEMPLATE IS beta + gamma, NOT gamma.  At 400 dpi the two
    dimension arrows are plainly stacked: beta runs from the 0 line up to the
    tick where the diagonal begins, and gamma runs from that SAME tick up to
    the top dashed line.  Reading the top as gamma understates every Table 4
    curve by beta -- index 10 tops at 7.5 dB, not 5.0.

    THE 0.4-TO-0.8 SEGMENT IS UNCONSTRAINED and is modelled as a straight line
    for want of anything better.  The grey tolerance bands bound 0 dB only out
    to 0.4 and resume along the diagonal from 0.8; between them the
    Recommendation draws no band, so any reasonable monotonic shape conforms.
    A different interpolation there changes the residuals below, and nothing in
    V.34 says which one a transmitter uses.
    """
    if idx <= 5:
        return ALPHA[idx] * fs
    beta, gamma = BETA_GAMMA[idx - 6]
    top = beta + gamma
    if fs <= 0.4:
        return 0.0
    if fs <= 0.8:
        return beta * (fs - 0.4) / 0.4          # unconstrained; linear default
    if fs >= 1.2:
        return top
    return beta + (top - beta) * (fs - 0.8) / 0.4


def inband_bins(baud):
    """The in-band probe bins for `baud`: [(bin index, normalised freq)]."""
    de = DE.get(baud, 4.0 / 7)
    lo, hi = de - 0.45, de + 0.45
    return [(i, (i + 1) * BIN_HZ / baud) for i in range(25)
            if i not in NOISE_BINS and lo <= (i + 1) * BIN_HZ / baud <= hi]


def median_smooth(pairs):
    """Drop NARROWBAND features; keep the general shape of the channel.

    Pre-emphasis exists to match the channel's broad shape.  Residual
    resonance and per-bin error are the equaliser's job -- it has 80 complex
    taps and adapts every symbol, which is the right tool for a notch a few
    hundred hertz wide.  So the selector must not chase an isolated bin: an
    interferer leaking into one probe slot and a narrow notch are BOTH things
    it should ignore, and an earlier version of this code moved eight indices
    on the first and ten on the second.

    The discriminator is not up-versus-down, it is NARROW versus BROAD.  A
    three-point median over the level-vs-bin sequence does exactly that:
    isolated impulses of either sign vanish, while monotone edges survive
    untouched.  Verified on the cases that matter --

        flat + one +12 dB bin   -> flat
        flat + one -18 dB notch -> flat
        band-edge cliff         -> unchanged
        broad roll-off          -> unchanged

    THE ENDPOINTS ARE DELIBERATELY LEFT RAW, and this was measured rather
    than assumed.  Filtering them looks strictly better at 3429 baud --
    immunity to an isolated tone AND an isolated notch goes from (2, 10) index
    errors to (0, 0) for the cost of one index on the real capture.  It also
    BREAKS the identity property at 2400 baud, because for a monotone ramp
    `median(v0, v1, v2) == v1`: filtering an endpoint pulls a ramp's end inward
    and distorts the very shapes the templates are.  At 2400 there are only ten
    in-band bins, so damaging two of them is 20% of the evidence, and the
    selector stopped being able to name templates 5, 7, 8 and 10 at all.

    Correctness first.  Identity is the property that makes this thing worth
    having; edge-bin immunity is a hardening.  So:

        KNOWN LIMIT -- an isolated bad bin EXACTLY at a band edge can still
        move the answer, by up to ten indices for a deep notch.  The fifteen
        of seventeen interior bins are immune.  Fixing this needs an endpoint
        rule that preserves monotone ramps -- a linear extrapolation guard
        rather than a median -- and that has not been written.

    KNOWN LIMIT: a three-point median removes runs of ONE bin.  Two adjacent
    corrupted bins survive it.  Widening to five would catch those and would
    also start blurring genuinely narrow channel features, which is the trade
    this deliberately does not make -- a real two-bin defect is 300 Hz wide and
    is a channel, not an interferer.
    """
    if len(pairs) < 3:
        return pairs
    idx = [i for i, _ in pairs]
    lv = [d for _, d in pairs]
    out = list(lv)
    for k in range(1, len(lv) - 1):
        out[k] = sorted(lv[k - 1:k + 2])[1]
    return list(zip(idx, out))


def score_templates(level_of_bin, baud, smooth=True):
    """`level_of_bin` maps bin index -> dB.  Returns [(rms, index)] sorted."""
    pairs = [(i, level_of_bin(i)) for i, _ in inband_bins(baud)]
    if smooth:
        pairs = median_smooth(pairs)
    fs = dict(inband_bins(baud))
    out = []
    for idx in range(11):
        c = [d + template_db(idx, fs[i]) for i, d in pairs]
        m = sum(c) / len(c)
        out.append((math.sqrt(sum((x - m) ** 2 for x in c) / len(c)), idx))
    out.sort()
    return out


def best_template(level_of_bin, baud, smooth=True):
    return score_templates(level_of_bin, baud, smooth)[0][1]


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

    inband = [(i, d - ref) for i, d in good
              if lo <= (i + 1) * BIN_HZ / baud <= hi]
    print("\n  residual after applying each template (RMS dB over %d in-band bins):"
          % len(inband))
    lvl = dict(inband)
    scores = score_templates(lambda i: lvl.get(i, 0.0), baud)
    rms_of = {i: r for r, i in scores}
    for idx in range(11):
        fam = "Table 3 alpha=%4.1f" % ALPHA[idx] if idx <= 5 else \
              "Table 4 b=%.1f g=%.1f top=%.1f" % (BETA_GAMMA[idx - 6][0],
                                                 BETA_GAMMA[idx - 6][1],
                                                 sum(BETA_GAMMA[idx - 6]))
        star = "  <-- object chose this" if idx == chose else ""
        print("    index %2d  %-30s  rms %6.2f dB%s"
              % (idx, fam, rms_of[idx], star))
    best = scores[0][1]
    print("\n  best by shape : index %d" % best)
    print("  object chose  : index %s" % chose)
    if chose is not None and best != chose:
        print("  DISAGREE by %d, and across families" % abs(best - chose)
              if (best <= 5) != (chose <= 5) else
              "  DISAGREE by %d, same family" % abs(best - chose))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
