#!/usr/bin/env python3
"""chirpdelay.py -- round-trip delay, measured rather than inferred.

    chirpdelay.py captures/probe        # reads <prefix>.rtp{tx,rx}.raw + .chirp.raw

WHY THIS EXISTS.  The delay budget for the 205.6 ms echo was ASSUMED: 40 ms of
our jitter buffer, 80 ms of the VG204's, 40 ms of packetisation, 46 ms of
unexplained remainder.  Cutting our 40 to 20 moved the measured echo not at all
(205.62 ms before, 205.62 after), which means at least one term of that budget
is fiction.  Guessing at the others is how the last four hypotheses died.

So: send a known signal and time it.

WHY A CHIRP.  A steady tone autocorrelates into a ridge one period wide -- every
cycle looks like every other -- so the peak is ambiguous by ±1/f.  A linear
sweep pulse-compresses to a single sharp peak, giving the delay to a sample.
It is also broadband, so no narrowband detector at either end can mistake it
for signalling: 2100 Hz ANSam, 1100 CNG, the V.21 pairs, 1800 V.34 carrier,
DTMF and call progress are all steady sinusoids.  Nothing in V-series sweeps
except V.34's line probe, which only exists inside a handshake.

WHERE IT IS MEASURED.  d-modem's get_frame/put_frame, i.e. the RTP boundary.
That EXCLUDES slmodemd's own pipeline, so what comes out is the network and ATA
loop alone -- which is the thing we are trying to decide is reducible.

The two peaks to read:
  DIRECT   the chirp arriving back having gone out and come off the far
           hybrid: the round trip.
  There is no "our own transmit" peak to subtract, because tx and rx here are
  different physical directions, not a sidetone.
"""

import argparse
import os
import sys

import numpy as np

RATE = 8000.0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prefix")
    ap.add_argument("--max-ms", type=float, default=500.0)
    ap.add_argument("--wav", metavar="PATH",
                    help="write a stereo WAV: LEFT = what we transmitted "
                         "(chirp included), RIGHT = what came back.  The "
                         "stereo file row.sh writes is built from the DATAPUMP "
                         "dumps, and the chirp is injected downstream of those "
                         "-- so it appears only on the returning side there. "
                         "This one is built from the RTP-boundary dumps, where "
                         "both directions carry it.")
    args = ap.parse_args()

    need = [args.prefix + s for s in (".rtptx.raw", ".rtprx.raw", ".chirp.raw")]
    for p in need:
        if not os.path.exists(p):
            print("missing %s" % p, file=sys.stderr)
            return 1

    tx = np.fromfile(need[0], dtype="<i2").astype(np.float64)
    rx = np.fromfile(need[1], dtype="<i2").astype(np.float64)
    ch = np.fromfile(need[2], dtype="<i2").astype(np.float64)

    if args.wav:
        import wave
        n = min(len(tx), len(rx))
        a = np.clip(tx[:n], -32768, 32767).astype("<i2")
        b = np.clip(rx[:n], -32768, 32767).astype("<i2")
        inter = np.empty(n * 2, dtype="<i2")
        inter[0::2] = a          # LEFT  = transmitted, chirp present
        inter[1::2] = b          # RIGHT = received, chirp returns delayed
        w = wave.open(args.wav, "wb")
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(int(RATE))
        w.writeframes(inter.tobytes()); w.close()
        print("wrote %s  (L = transmitted, R = received, %d Hz)"
              % (args.wav, RATE))

    print("%s: tx %.2f s, rx %.2f s, chirp %.1f ms at %d Hz"
          % (args.prefix, len(tx)/RATE, len(rx)/RATE, len(ch)*1000/RATE, RATE))

    # Where did each burst actually leave?  Matched-filter the TX dump against
    # the chirp: this is ground truth for departure, not the frame counter,
    # because a dropped or late frame would shift it.
    def matched(sig):
        n = len(sig) + len(ch)
        size = 1 << int(np.ceil(np.log2(n)))
        c = np.fft.irfft(np.fft.rfft(sig, size) *
                         np.conj(np.fft.rfft(ch, size)), size)
        return c[:len(sig)]

    ctx = matched(tx)
    crx = matched(rx)

    # Bursts are 2 s apart; find each departure, then look for its return
    # inside a window after it.
    thresh = 0.5 * ctx.max()
    departures = []
    i = 0
    while i < len(ctx):
        if ctx[i] > thresh:
            j = min(i + int(0.5 * RATE), len(ctx))
            departures.append(i + int(np.argmax(ctx[i:j])))
            i = j
        else:
            i += 1

    if not departures:
        print("no chirp found in the TX dump -- was DMODEM_CHIRP set?",
              file=sys.stderr)
        return 1

    win = int(args.max_ms * RATE / 1000)
    print("\n%-7s %-12s %-12s %-10s %s"
          % ("burst", "left at", "returned at", "round trip", "peak/noise"))
    trips = []
    for k, d in enumerate(departures):
        seg = crx[d:d + win]
        if len(seg) < 100:
            continue
        # ignore the first few ms: nothing can come back that fast, and it
        # keeps any residual of the outgoing frame out of the search
        guard = int(0.005 * RATE)
        rel = int(np.argmax(np.abs(seg[guard:]))) + guard
        peak = abs(seg[rel])
        noise = np.median(np.abs(seg))
        ms = rel * 1000.0 / RATE
        trips.append(ms)
        print("%-7d %-12.1f %-12.1f %-10.2f %.1fx"
              % (k + 1, d * 1000.0 / RATE, (d + rel) * 1000.0 / RATE, ms,
                 peak / noise if noise else float("nan")))

    if trips:
        t = np.array(trips)
        print("\nround trip: median %.2f ms, mean %.2f, sd %.2f, n=%d"
              % (np.median(t), t.mean(), t.std(ddof=1) if len(t) > 1 else 0,
                 len(t)))
        print("the V.34 canceller's taps reach 157.5 ms; its delay line holds "
              "172.5 ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
