#!/usr/bin/env python3
"""echofit.py -- one line of echo numbers for a call, for tabulating.

    echofit.py captures/asym-1
    205.62 19.02 19.81
        |     |     |
        |     |     signal-to-echo, dB: everything-else power over echo power
        |     echo return loss, dB = -20log10(a)
        lag of the strongest correlation peak, ms

`echoscan.py` is the one to read when investigating; this is the same
measurement reduced to three numbers so a sweep can put them in a column.

The model is `rx[n] = a*tx[n-k] + v[n]`, fitted by least squares at the lag `k`
that maximises the normalised cross-correlation.  `row.sh` writes both
directions from the same loop on the same timebase, so no alignment is needed
and `k` is a real delay rather than an artefact of two independent clocks.

Prints nothing and exits 1 if the recordings are missing or too short, so a
caller can substitute a placeholder without parsing an error message.
"""

import os
import sys

import numpy as np

RATE = 9600
MAX_LAG_MS = 300.0


def main():
    if len(sys.argv) < 2:
        print("usage: echofit.py <logprefix>", file=sys.stderr)
        return 1
    pre = sys.argv[1]
    prx, ptx = pre + ".modem_rx.raw", pre + ".modem_tx.raw"
    if not (os.path.exists(prx) and os.path.exists(ptx)):
        return 1

    rx = np.fromfile(prx, dtype="<i2").astype(np.float64)
    tx = np.fromfile(ptx, dtype="<i2").astype(np.float64)
    n = min(len(rx), len(tx))
    if n < RATE:
        return 1
    rx, tx = rx[:n] - rx[:n].mean(), tx[:n] - tx[:n].mean()

    max_lag = int(MAX_LAG_MS * RATE / 1000)
    size = 1 << int(np.ceil(np.log2(n + max_lag + 1)))
    c = np.fft.irfft(np.fft.rfft(rx, size) * np.conj(np.fft.rfft(tx, size)),
                     size)[:max_lag + 1]
    denom = np.sqrt((tx * tx).sum() * (rx * rx).sum())
    if not denom:
        return 1
    c /= denom

    # Lag 0 is not echo -- it is whatever the two directions share right now,
    # which for a full-duplex modem should be nothing.  Echo is delayed.
    k = int(np.argmax(np.abs(c[1:]))) + 1

    td, rd = tx[:n - k], rx[k:n]
    a = float(td @ rd / (td @ td))
    if a == 0:
        return 1
    pe = a * a * float(td @ td) / len(td)
    resid = rd - a * td
    pv = float(resid @ resid) / len(resid)

    print("%.2f %.2f %.2f" % (k * 1000.0 / RATE,
                              -20 * np.log10(abs(a)),
                              10 * np.log10(pv / pe) if pe else float("nan")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
