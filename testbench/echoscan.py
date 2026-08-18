#!/usr/bin/env python3
"""echoscan.py -- is our own transmit present in our own receive, and at what lag?

    echoscan.py captures/asym-1 [--rate 9600] [--max-ms 400]
    echoscan.py --selftest                     # prove the detector fires

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

--------------------------------------------------------------------------
IT WAS BROKEN FOR THE WHOLE OF ITS RECORDED LIFE, AND FINDING 1971 CAUGHT IT.

    from capture_io import load
    def load(path):
        a = load(path)[0].astype(np.float64)     # <-- calls ITSELF

The wrapper shadowed the import it was wrapping, so every invocation recursed
until the stack went.  It cannot have run since that edit, and nothing noticed,
because a tool nobody could run looks exactly like a tool with nothing to
report -- findings 134, 2400, 2401.  Task #168.

So two things changed with the repair, and neither is cosmetic:

  * **IT REPORTS ITS DENOMINATORS.**  Samples correlated, seconds of audio,
    lags searched, and the sample rate READ FROM THE FILE rather than assumed.
    A correlation over 300 ms of a 90-second call is not the same measurement
    as one over the whole call and must not print the same way.
  * **`--selftest` PLANTS A KNOWN ECHO AND WATCHES IT APPEAR.**  The ladder is
    finding 1971's -- null, then -10, -20 and -30 dB at a 30 ms lag -- so this
    tool and `echoratio.py` are calibrated against the same reference and their
    numbers can be read side by side.  Run it before trusting a clean scan.

WHICH TOOL FOR WHICH QUESTION.  `echoratio.py` is the instrument of record for
HOW MUCH echo there is: it integrates coherence against the received spectrum
and its floor is calibrated, which is where 1971's "below about -25 dB" bound
comes from.  This one is the instrument for WHERE the echo is -- the lag, which
is what has to be compared against the canceller's reach.  The dB column here
is 20*log10(|rho|) at the single strongest lag; it agrees with echoratio on a
clean single-path echo and is the weaker number when the return is spread over
several paths, because a correlation peak reads one lag and a coherence
integral reads all of them.
"""

import argparse
import os
import sys

import numpy as np

from capture_io import load


def load_mono(path):
    """One channel of a capture, as float64, with the file's OWN rate.

    Named so it cannot shadow `capture_io.load` again.  Returns (samples,
    rate) -- the rate is a denominator like any other and the caller has to
    carry it, rather than assuming 8000 or 9600 and converting lags with the
    wrong divisor.
    """
    x, rate = load(path)
    if x.ndim > 1:                       # a stereo capture: take the left
        x = x[:, 0]
    return x.astype(np.float64), rate


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


def db(rho):
    return 20.0 * np.log10(max(abs(rho), 1e-12))


def report(label, tx, rx, rate, max_ms, echo_ref_rate=9600.0):
    """The whole verdict for one tx/rx pair.  Returns (lag_ms, rho, floor)."""
    n = min(len(tx), len(rx))
    max_lag = int(max_ms * rate / 1000)
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

    print("%s" % label)
    print("  DENOMINATORS    %d samples correlated = %.1f s at %d Hz"
          " (rate read from the file), %d lags searched (0..%.0f ms)"
          % (n, n / rate, rate, max_lag, max_ms))
    print("  strongest lag   %5d samples = %6.2f ms   rho = %+.5f"
          "  = %6.2f dB" % (k, k * 1000.0 / rate, peak, db(peak)))
    print("  noise floor     median |rho| over %d lags = %.5f = %.2f dB"
          "  (ratio %.1fx)"
          % (max_lag, floor, db(floor), abs(peak) / floor if floor else float("nan")))

    # The canceller's window, so the number can be read against it directly.
    for iod in (88, 240):
        print("  IODELAY %3d -> echo_delay %3d samples = %5.2f ms"
              % (iod, iod + 60, (iod + 60) * 1000.0 / echo_ref_rate))

    print("  top 5 lags:")
    order = np.argsort(np.abs(c[1:]))[::-1][:5] + 1
    for j in sorted(order):
        print("    %5d samples = %6.2f ms   rho = %+.5f"
              % (j, j * 1000.0 / rate, c[j]))
    return k * 1000.0 / rate, peak, floor


def selftest(rate=8000, seconds=20.0, lag_ms=30.0, max_ms=400.0):
    """Plant an echo of known size at a known lag and watch it appear.

    A tool that prints nothing is indistinguishable from a tool that is broken
    (findings 134, 2400, 2401), and this one printed nothing at all for months
    because it recursed on its own name.  The ladder is finding 1971's, so the
    two echo tools are calibrated against the same reference:

        null (two uncorrelated signals)
        planted -10 dB at 30 ms
        planted -20 dB at 30 ms
        planted -30 dB at 30 ms

    PASS is: the null sits at the estimator's floor, and every planted echo is
    found at the RIGHT LAG with a rho within a decibel or so of what was
    planted, until it disappears under the floor.  The point of the -30 dB rung
    is to show where the instrument stops seeing, which is the number a null
    result has to be read against.
    """
    rng = np.random.default_rng(20260818)
    n = int(rate * seconds)
    lag = int(lag_ms * rate / 1000)

    tx = rng.standard_normal(n)          # our transmit
    far = rng.standard_normal(n)         # the far end: uncorrelated with us

    print("=== echoscan selftest: %d samples = %.1f s at %d Hz, lag planted at"
          " %.1f ms (%d samples)" % (n, seconds, rate, lag_ms, lag))
    print()

    rows = []
    cases = [("null, two uncorrelated signals", None)]
    cases += [("planted %d dB at %.0f ms" % (a, lag_ms), a)
              for a in (-10, -20, -30)]

    for label, amp_db in cases:
        rx = far.copy()
        if amp_db is not None:
            a = 10.0 ** (amp_db / 20.0)
            rx[lag:] += a * tx[:n - lag]
        lag_found, rho, floor = report("--- " + label, tx, rx, rate, max_ms)
        rows.append((label, amp_db, lag_found, rho, floor))
        print()

    print("=== summary, %d cases" % len(rows))
    print("%-34s %10s %10s %10s %10s"
          % ("case", "planted", "found lag", "rho dB", "floor dB"))
    ok = True
    for label, amp_db, lag_found, rho, floor in rows:
        print("%-34s %10s %9.2f ms %10.2f %10.2f"
              % (label, "-" if amp_db is None else "%d dB" % amp_db,
                 lag_found, db(rho), db(floor)))
        if amp_db is None:
            if db(rho) > -30.0:
                ok = False
                print("    FAIL: the null should sit near the floor")
        elif amp_db >= -20:
            if abs(lag_found - lag_ms) > 0.5:
                ok = False
                print("    FAIL: planted at %.1f ms, found at %.2f ms"
                      % (lag_ms, lag_found))
            if abs(db(rho) - amp_db) > 2.0:
                ok = False
                print("    FAIL: planted %d dB, measured %.2f dB"
                      % (amp_db, db(rho)))
    print()
    print("SELFTEST %s -- the detector %s"
          % ("PASS" if ok else "FAIL",
             "fires on a known input and is quiet on a null"
             if ok else "did NOT behave as the ladder requires"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prefix", nargs="?", help="log prefix, e.g. captures/asym-1")
    ap.add_argument("--rate", type=int, default=9600,
                    help="9600 for the .modem_rx/.modem_tx pair (default), "
                         "8000 for the _8k pair.  This selects WHICH PAIR to "
                         "read; the rate used for the arithmetic is the one "
                         "the wav itself carries.")
    ap.add_argument("--max-ms", type=float, default=400.0)
    ap.add_argument("--from-s", type=float, default=0.0,
                    help="skip this many seconds -- use it to look at the data "
                         "phase alone rather than the handshake")
    ap.add_argument("--selftest", action="store_true",
                    help="plant a known echo and prove the detector fires")
    args = ap.parse_args()

    if args.selftest:
        return selftest(max_ms=args.max_ms)
    if not args.prefix:
        ap.error("a capture prefix is required (or --selftest)")

    suffix = "" if args.rate == 9600 else "_8k"
    prx = "%s.modem_rx%s" % (args.prefix, suffix)
    ptx = "%s.modem_tx%s" % (args.prefix, suffix)
    for p in (prx, ptx):
        if not (os.path.exists(p + ".wav") or os.path.exists(p + ".raw")):
            print("missing %s(.wav|.raw)" % p, file=sys.stderr)
            return 1

    rx, rrate = load_mono(prx)
    tx, trate = load_mono(ptx)
    if rrate != trate:
        print("rx is %d Hz and tx is %d Hz -- these are not the same timebase"
              % (rrate, trate), file=sys.stderr)
        return 1
    if rrate != args.rate:
        print("note: --rate %d selected the %r pair, whose files declare %d Hz;"
              " using %d Hz for the arithmetic"
              % (args.rate, suffix or "9600", rrate, rrate), file=sys.stderr)
    rate = rrate

    skip = int(args.from_s * rate)
    rx, tx = rx[skip:], tx[skip:]
    n = min(len(tx), len(rx))
    if n < rate:
        print("less than a second of audio after --from-s", file=sys.stderr)
        return 1

    report("%s  (skipped %.1f s)" % (args.prefix, args.from_s),
           tx, rx, rate, args.max_ms)
    return 0


if __name__ == "__main__":
    sys.exit(main())
