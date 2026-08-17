#!/usr/bin/env python3
"""echoscan.py -- is our own transmit present in our own receive, and at what lag?

    echoscan.py captures/asym-1 [--rate 9600] [--max-ms 400]

WHY.  Task #101 established that the V.34 link is asymmetric in one direction
only: we transmit 33600 every call and receive 12000.  Symbol rate, level and
receive-path starvation were all excluded by measurement, which leaves a
bits-per-symbol verdict -- our receiver judged an equally loud signal at the
same symbol rate to be much noisier.  The leading candidate is our own transmit
echoing back beyond the canceller's reach:

    echo_delay = IODELAY + 60 = 300 samples at 9600 Hz = 31 ms at IODELAY 240

THIS MEASURES IT DIRECTLY rather than inferring it from a rate.  `row.sh`
records both directions from the same loop on the same timebase, so the two
files line up sample for sample and a cross-correlation of transmit against
receive is meaningful without any alignment step.

The far end's signal and ours are uncorrelated, so a peak at lag k says our
signal is coming back k samples later.  No peak says there is no linear echo to
cancel, and the canceller's reach is not the problem.

THE ONE THING THIS CANNOT SEE is a non-linear echo -- companding distortion in
the G.711 path returns energy that is not a scaled copy of what we sent, and no
correlation will find it.  A null result here bounds the linear echo only.
"""

import argparse
import os
import sys

import numpy as np

from capture_io import load


def load(path):
    a = load(path)[0].astype(np.float64)
    return a


def xcorr_norm(tx, rx, max_lag):
    """Normalised cross-correlation of rx against tx, lags 0..max_lag.

    Both sides are mean-removed and energy-normalised, so the result is a
    correlation coefficient: 1.0 would be rx being exactly a scaled copy of tx
    at that lag, 0.0 no linear relationship at all.
    """
    n = min(len(tx), len(rx))
    tx = tx[:n] - tx[:n].mean()
    rx = rx[:n] - rx[:n].mean()

    size = 1 << int(np.ceil(np.log2(n + max_lag + 1)))
    F = np.fft.rfft(tx, size)
    G = np.fft.rfft(rx, size)
    # correlate(rx, tx)[k] = sum rx[i+k] * tx[i]
    c = np.fft.irfft(G * np.conj(F), size)[:max_lag + 1]

    denom = np.sqrt((tx * tx).sum() * (rx * rx).sum())
    return c / denom if denom else c


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prefix", help="log prefix, e.g. captures/asym-1")
    ap.add_argument("--rate", type=int, default=9600,
                    help="9600 for the .modem_rx/.modem_tx pair (default), "
                         "8000 for the _8k pair")
    ap.add_argument("--max-ms", type=float, default=400.0)
    ap.add_argument("--from-s", type=float, default=0.0,
                    help="skip this many seconds -- use it to look at the data "
                         "phase alone rather than the handshake")
    args = ap.parse_args()

    suffix = "" if args.rate == 9600 else "_8k"
    prx = "%s.modem_rx%s" % (args.prefix, suffix)
    ptx = "%s.modem_tx%s" % (args.prefix, suffix)
    for p in (prx, ptx):
        if not os.path.exists(p):
            print("missing %s" % p, file=sys.stderr)
            return 1

    rx, tx = load(prx), load(ptx)
    skip = int(args.from_s * args.rate)
    rx, tx = rx[skip:], tx[skip:]
    n = min(len(tx), len(rx))
    if n < args.rate:
        print("less than a second of audio after --from-s", file=sys.stderr)
        return 1

    max_lag = int(args.max_ms * args.rate / 1000)
    c = xcorr_norm(tx, rx, max_lag)

    # Lag 0 is not echo -- it is whatever the two directions share right now,
    # and for a full-duplex modem that should be nothing.  Echo is a delayed
    # copy, so search from 1.
    k = int(np.argmax(np.abs(c[1:]))) + 1
    peak = c[k]

    # The noise floor of this estimator: the median magnitude across all lags.
    # A peak has to stand above the correlation you get from two unrelated
    # signals of this length, which is what everything except the echo is.
    floor = float(np.median(np.abs(c[1:])))

    print("%s  %.1f s at %d Hz, lags 0..%.0f ms"
          % (args.prefix, n / args.rate, args.rate, args.max_ms))
    print("  strongest lag   %5d samples = %6.2f ms   rho = %+.5f"
          % (k, k * 1000.0 / args.rate, peak))
    print("  noise floor     median |rho| over all lags = %.5f  (ratio %.1fx)"
          % (floor, abs(peak) / floor if floor else float("nan")))

    # The canceller's window, so the number can be read against it directly.
    for iod in (88, 240):
        print("  IODELAY %3d -> echo_delay %3d samples = %5.2f ms"
              % (iod, iod + 60, (iod + 60) * 1000.0 / 9600))

    print("  top 5 lags:")
    order = np.argsort(np.abs(c[1:]))[::-1][:5] + 1
    for j in sorted(order):
        print("    %5d samples = %6.2f ms   rho = %+.5f"
              % (j, j * 1000.0 / args.rate, c[j]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
