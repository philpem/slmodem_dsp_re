#!/usr/bin/env python3
"""echoratio.py -- how much of what we RECEIVE is our own TRANSMIT coming back?

    python3 echoratio.py captures/ata12-1902-5 [more prefixes ...]

TASK #167.  Arm 2 of the ATA attenuation test cut our transmit by 9 dB at the
gateway and OUR OWN RECEIVE RATE WENT UP (12000/9600/14400 -> 14400 x4, 19200).
Under a pure level model that is impossible: `output attenuation` changes only
what the far end hears.  The candidate is echo -- every FXS port on this VG204
carries `no echo-cancel enable`, so our signal returns through the 2-to-4-wire
hybrid into our own receiver, and 9 dB less transmit is 9 dB less interference.

THE METRIC IS A RATIO, AND THAT IS THE WHOLE POINT.  Our transmit level ALSO
fell in arm 2 (-25.75 dBFS against -24.29) because the Courier asked for more
reduction, so absolute echo power drops for two independent reasons and an
absolute figure cannot separate them.  What matters to the receiver is echo
power as a fraction of what it is trying to demodulate:

    echo fraction = integral of coherence^2(f) . Syy(f)  /  integral of Syy(f)

Syy is the received spectrum; coherence^2 against our transmit is the part of
it linearly predictable from what we sent.  The far end's signal and ours are
uncorrelated, so the coherent part IS the echo.  Reported in dB: -20 dB means
one per cent of received power is our own signal returning.

WHAT THIS CANNOT SEE, and the bound matters: a NON-LINEAR echo.  The G.711
companding in the path returns energy that is not a scaled copy of what we
sent, and no coherence will find it.  A null here bounds the LINEAR echo only.
echoscan.py's docstring makes the same caveat and it is still true.

WINDOWING.  1024-sample Hann at 8 kHz is 128 ms, comfortably longer than the
~30 ms hybrid delay, so the echo stays inside a window and shows up as
coherence rather than being split across two.  Both directions come from the
same loop on the same timebase (row.sh), so no alignment step is needed.

*** AND THE ~30 ms PREMISE IS FALSE, WHICH MAKES THIS TOOL BLIND ON THIS PATH.

The echo on the bench path is not at 30 ms.  `echoscan.py`, repaired under
task #168, finds it at **171.5 ms in 19 of 20 calls to within 0.25 ms**, in
both ATA impedance configurations -- which is the same quantity findings 1204,
1215 and 1216 measured at 205.62 and 165.62/175.62 ms.  Two things here are
shorter than that delay:

  * `peak_lag_ms(..., max_ms=120.0)` -- the lag search stops at 120 ms, so a
    peak at 171.5 ms is outside the array before anything is compared.
  * the 128 ms coherence window itself -- an echo delayed further than the
    window is split across two segments and its coherence is destroyed.

MEASURED, not reasoned.  A -20 dB echo planted at a 30 ms lag reads -20.68 dB
here and is found at 30.0 ms.  The SAME -20 dB echo planted at 171.5 ms reads
**-24.56 dB -- the null floor -- and the lag comes back 47.8 ms**, i.e. this
tool reports "no echo" for an echo twenty times the power of its own floor.
`echoscan.py` returns -20.15 dB at 171.50 ms for the same file.

SO FINDING 1971's "-25 dB, no linear echo on this path" IS THIS BLIND SPOT and
not a property of the channel; 1971's "scattered lags, no consistent path
delay" is the 120 ms cap truncating a 171.5 ms peak.  What survives is
narrower: there is no linear echo WITHIN 120 ms above about -25 dB.

NOTHING IN THE ARITHMETIC HAS BEEN CHANGED, deliberately -- every number 1971
and the ATA-attenuation work quote came out of this code and must keep
reproducing.  Use `echoscan.py` for the lag and for any echo beyond 120 ms;
use this one for a coherence-integrated ratio inside that window.
"""

import math
import sys

import numpy as np

from g711level import active, dbfs, read_wav

NFFT = 1024
HOP = NFFT // 2


def spectra(x, y):
    """Averaged auto- and cross-spectra over Hann-windowed segments."""
    w = np.hanning(NFFT)
    n = (min(len(x), len(y)) - NFFT) // HOP
    if n < 8:
        return None
    Sxx = np.zeros(NFFT // 2 + 1)
    Syy = np.zeros(NFFT // 2 + 1)
    Sxy = np.zeros(NFFT // 2 + 1, dtype=complex)
    for i in range(n):
        a, b = i * HOP, i * HOP + NFFT
        X = np.fft.rfft(x[a:b] * w)
        Y = np.fft.rfft(y[a:b] * w)
        Sxx += (X * X.conj()).real
        Syy += (Y * Y.conj()).real
        Sxy += Y * X.conj()
    return Sxx / n, Syy / n, Sxy / n


def echo_fraction(tx, rx):
    s = spectra(tx, rx)
    if s is None:
        return None
    Sxx, Syy, Sxy = s
    good = (Sxx > 0) & (Syy > 0)
    coh = np.zeros_like(Syy)
    coh[good] = (np.abs(Sxy[good]) ** 2) / (Sxx[good] * Syy[good])
    coh = np.clip(coh, 0.0, 1.0)
    return float((coh * Syy).sum() / Syy.sum())


def peak_lag_ms(tx, rx, max_ms=120.0):
    """Where the echo sits, as a sanity check on the coherence figure."""
    n = 1 << int(math.ceil(math.log2(len(tx) + len(rx))))
    X = np.fft.rfft(tx - tx.mean(), n)
    Y = np.fft.rfft(rx - rx.mean(), n)
    c = np.fft.irfft(Y * X.conj(), n)
    lim = int(max_ms * 8.0)
    c = c[:lim]
    k = int(np.argmax(np.abs(c)))
    denom = math.sqrt(float((tx * tx).sum() * (rx * rx).sum()))
    return k / 8.0, (abs(c[k]) / denom if denom else 0.0)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    # THE BLIND SPOT, ON EVERY RUN.  stdout keeps its exact old format so
    # every archived parse of this tool still works; the caveat goes to
    # stderr, because a tool that reports its floor for a real -20 dB echo
    # must not be able to do so silently.  See the docstring for the
    # measurement that established it.
    print("this tool sees %.0f ms of lag and uses a %.0f ms window; the bench's "
          "echo is at 171.5 ms, OUTSIDE BOTH.  A null here is a null within "
          "120 ms only -- use echoscan.py for the real lag."
          % (120.0, NFFT / 8.0), file=sys.stderr)
    print("%-18s %9s %9s %11s %8s %7s"
          % ("capture", "txdBFS", "rxdBFS", "echo/rx dB", "lag ms", "peak r"))
    for pre in argv[1:]:
        try:
            tx = active(read_wav(pre + ".modem_tx_8k.wav"))
            rx = active(read_wav(pre + ".modem_rx_8k.wav"))
        except OSError as e:
            print("%-18s  cannot read: %s" % (pre.split("/")[-1], e))
            continue
        n = min(len(tx), len(rx))
        if n < 80000:
            print("%-18s  too short (%d samples)" % (pre.split("/")[-1], n))
            continue
        # The LAST 20 s of the shorter stream: data mode, after the handshakes.
        tx, rx = tx[n - 160000:n], rx[n - 160000:n]
        frac = echo_fraction(tx, rx)
        lag, r = peak_lag_ms(tx, rx)
        print("%-18s %9.2f %9.2f %11.2f %8.1f %7.3f"
              % (pre.split("/")[-1], dbfs(tx), dbfs(rx),
                 10 * math.log10(max(frac, 1e-12)), lag, r))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
