#!/usr/bin/env python3
"""probeplot.py -- the V.34 line probe's measured spectrum, and what the
object's tilt meter made of it.

    probeplot.py captures/probe-1.slmodemd.log            # -> probe-1.png
    probeplot.py captures/*.slmodemd.log --out shape.png  # several, overlaid

WHY THIS EXISTS.  Everything this project has said about the channel's tilt is
inferred from the INDEX `probe_preemp` reports -- a two-point slope, one
band-edge bin against bin 4, quantised to 4.06 dB steps (finding 1475).  The
probe measures TWENTY-FIVE bins and the search reads two of them.  Nobody has
ever looked at the other twenty-three.

THE BIN GRID.  `dftfreqinit` writes bin numbers 1..25 with `DFT_BIN(n) = n<<8`,
a phase step of n*256 in a 14-bit accumulator, so bin n has period 64/n
samples.  At the 9600 Hz rate the datapump runs at that is **n * 150 Hz** --
150 Hz to 3750 Hz, and 150 Hz is the V.34 line probe's own tone spacing.  So
`bins[i]` is (i+1)*150 Hz, and in particular:

    bins[4]  = bin 5  =  750 Hz   the reference the search compares against
    bins[18] = bin 19 = 2850 Hz   band edge for 2800 baud
    bins[19] = bin 20 = 3000 Hz   3000 baud
    bins[20] = bin 21 = 3150 Hz   3200 baud
    bins[22] = bin 23 = 3450 Hz   3429 baud -- the one this bench uses

THE SCALE, AND ITS LIMIT.  `dftenergy` writes `energy = (short)(e >> 16)`
where `e = re*re + im*im`, plus `shift`, the count of redundant sign bits of
`e`.  So `energy` alone has a floor: a bin needs e >= 65536 to register 1, and
anything quieter reads ZERO and is indistinguishable from any other quiet bin.
`shift` still carries the magnitude there, which is why this reconstructs from
both:

    e ~= energy << 16          when energy > 0
    e ~= 2 ** (31 - shift)     otherwise

and says so on the plot rather than hiding a floor in a log.

WHAT IS DRAWN.  Measured dB relative to the 750 Hz reference; the two bins the
search actually reads, marked; and a least-squares line through the passband,
which is the estimator task #144 proposes.  The gap between that line and the
two-point slope IS the argument for changing the estimator, or against it.
"""

import argparse
import math
import os
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt          # noqa: E402
import numpy as np                       # noqa: E402

HZ_PER_BIN = 150.0
REF_BIN = 4                              # bins[4], 750 Hz
EDGE = {2800: 18, 3000: 19, 3200: 20, 3429: 22}

#
# FOUR BINS CARRY NO TONE BY DESIGN, and this is not a measurement failure.
# V.34 11.2 and Table 17: the probing signal is "a set of tones spaced 150 Hz
# apart at frequencies from 150 Hz to 3750 Hz.  Tones at 900 Hz, 1200 Hz,
# 1800 Hz, and 2400 Hz are omitted."  Those are bins 6, 8, 12 and 16, and what
# the receiver measures there is the NOISE FLOOR -- which makes them the most
# informative bins in the set, not the least.
#
NOISE_BINS = {5, 7, 11, 15}              # zero-based indices of 900/1200/1800/2400


def parse(path):
    """Every V34PROBEBINS line in a log, as (energy[], shift[])."""
    out = []
    txt = open(path, "rb").read().decode("latin-1")
    # The daemon stamps EVERY dsplibs_debug_printf with its own `<time>`, so
    # the twenty-five pairs arrive interleaved with timestamps rather than as
    # one contiguous line.  Anchor on the marker, then take the next n pairs
    # wherever they fall -- `495.295411` cannot match `\d+/\d+`, so the
    # timestamps are skipped for free.
    for m in re.finditer(r"V34PROBEBINS, n=(\d+):", txt):
        n = int(m.group(1))
        pairs = re.findall(r"(-?\d+)/(-?\d+)", txt[m.end():m.end() + 40 * n])[:n]
        if len(pairs) == n:
            out.append(([int(a) for a, _ in pairs],
                        [int(b) for _, b in pairs]))
    return out


def to_db(energy, shift):
    """dB per bin, reconstructed from both fields.  See the module docstring."""
    db, floored = [], []
    for e, s in zip(energy, shift):
        if e > 0:
            val = e * 65536.0
            floored.append(False)
        else:
            # energy quantised to zero; shift is all that is left
            val = 2.0 ** max(0, 31 - s)
            floored.append(True)
        db.append(10.0 * math.log10(max(val, 1e-9)))
    return np.array(db), np.array(floored)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", default=None)
    ap.add_argument("--baud", type=int, default=3429)
    args = ap.parse_args()

    fig, ax = plt.subplots(figsize=(10, 6))
    edge = EDGE.get(args.baud, 22)
    n_shown = 0

    for path in args.logs:
        for k, (energy, shift) in enumerate(parse(path)):
            db, floored = to_db(energy, shift)
            noise = np.array([i in NOISE_BINS for i in range(len(db))])
            db = db - db[REF_BIN]                       # relative to 750 Hz
            f = (np.arange(len(db)) + 1) * HZ_PER_BIN
            label = "%s%s" % (os.path.basename(path).replace(".slmodemd.log", ""),
                              "" if k == 0 else " (probe %d)" % (k + 1))
            ax.plot(f, db, marker="o", ms=3, lw=1, label=label, alpha=.85)
            if noise.any():
                ax.plot(f[noise], db[noise], "v", ms=8, color="crimson")
                snr = db[~noise & (f >= 300) & (f <= 3400)].mean() - db[noise].mean()
                print("  %-28s NOISE FLOOR %+.1f dB below the tones"
                      " (measured at 900/1200/1800/2400 Hz)" % (label, snr))
            stray = floored & ~noise
            if stray.any():
                ax.plot(f[stray], db[stray], "x", ms=9, color="orange")
            n_shown += 1

            # least-squares slope over the passband the probe actually spans,
            # which is the estimator #144 proposes
            band = (f >= 300) & (f <= 3400) & ~floored & ~noise
            if band.sum() >= 4:
                sl, ic = np.polyfit(f[band], db[band], 1)
                ax.plot(f[band], sl * f[band] + ic, "--", lw=1, alpha=.6,
                        color=ax.lines[-1].get_color() if not floored.any()
                        else None)
                tilt = -(sl * (f[edge] - f[REF_BIN]))
                print("  %-28s two-point tilt %6.2f dB   least-squares %6.2f dB"
                      " over %d bins" % (label, -(db[edge]), tilt, band.sum()))

    if n_shown == 0:
        sys.exit("no V34PROBEBINS lines found -- was the call run at debug "
                 "level 3 with the instrumented build?")

    ax.axvline(f[REF_BIN], color="grey", ls=":", lw=1)
    ax.annotate("reference\nbin 5, 750 Hz", (f[REF_BIN], ax.get_ylim()[0]),
                textcoords="offset points", xytext=(4, 12), fontsize=8,
                color="grey")
    ax.axvline((edge + 1) * HZ_PER_BIN, color="crimson", ls=":", lw=1)
    ax.annotate("band edge\nbin %d, %.0f Hz" % (edge + 1, (edge + 1) * HZ_PER_BIN),
                ((edge + 1) * HZ_PER_BIN, ax.get_ylim()[0]),
                textcoords="offset points", xytext=(-58, 12), fontsize=8,
                color="crimson")
    ax.axhline(0, color="k", lw=.5)
    ax.set_xlabel("frequency (Hz)   —   probe bin n × 150 Hz")
    ax.set_ylabel("level relative to the 750 Hz reference (dB)")
    ax.set_title("V.34 line probe: measured channel shape\n"
                 "solid = the 25 measured bins   dashed = least-squares fit\n"
                 "▼ = 900/1200/1800/2400 Hz, where V.34 omits the tone: "
                 "these read the NOISE FLOOR (Table 17)")
    ax.grid(alpha=.3)
    ax.legend(fontsize=8)
    out = args.out or (os.path.basename(args.logs[0])
                       .replace(".slmodemd.log", "") + "-probe.png")
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    print("\n  wrote %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
