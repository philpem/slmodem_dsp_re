#!/usr/bin/env python3
"""g711level.py -- what the 8-bit codec on the SIP path actually costs us.

    python3 g711level.py sweep   [capture-prefix ...]   # SNR against level
    python3 g711level.py levels  [glob]                 # tx/rx level census
    python3 g711level.py sat     [glob]                 # A-law saturation
    python3 g711level.py request [glob]                 # requested dB vs level

THE QUESTION THIS ANSWERS.  The modem's audio crosses a G.711 codec in the
Cisco VG204 in each direction, so every sample is eight bits.  The natural
worry is that we are throwing away signal-to-noise by not driving that codec
hard enough, and that the ATA's gain wants trimming to fix it.

THE ANSWER IS NO, AND THE REASON IS COMPANDING.  G.711 is not a linear 8-bit
quantiser.  Its step size grows with amplitude, so the quantisation error
scales WITH the signal and the SNR is flat -- about 37.5 dB -- across roughly
a 24 dB span of input level.  Driving it harder gains nothing; the only two
things that cost are clipping at the top and running off the bottom of the
companding range.  `sweep` measures that plateau on a real V.34 waveform
rather than on a tone, because the peak-to-average ratio of a precoded,
non-linearly-encoded V.34 signal is 11-14 dB and a sine's is 3.

WHAT `sweep` MEASURES.  The error is taken against the UNCLIPPED input, so
clipping counts as error.  Measuring it against the clipped input instead
makes the curve rise for ever and reports the loudest setting as the best --
it was written that way first, and it said "best SNR at +24 dB with 23% of
samples clipped".

THE CAPTURES.  `*.modem_tx_8k.wav` is what d-modem hands pjsua, immediately
before the A-law encoder; `*.modem_rx_8k.wav` is what comes back out of the
decoder.  Those are the two points either side of the codec, which is why the
census uses them and not the 9600 Hz pair.  Nothing in d-modem scales the
network path between the dump and the encoder -- the `conf_adjust_tx_level`
calls there are all on the splitcomb's loudspeaker ports (finding 1940).  The
receive side proves itself: decoded peaks land on exactly 32256, which is
A-law's top codeword, so nothing has scaled them.

The codec tables are the ITU reference algorithm, checked by their endpoints:
u-law decodes to a maximum of 32124 and A-law to 32256.
"""

import glob
import math
import os
import re
import statistics
import sys
import wave

import numpy as np

CAPROOT = "/mnt/zfs/projects/softmodems/slmodem-re/captures"
DEFAULT_GLOB = CAPROOT + "/*/*/*.modem_tx_8k.wav"


# --------------------------------------------------------------------------
# G.711, the ITU reference algorithm.

SEG_U = [0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF]
SEG_A = [0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF]


def _seg(val, table):
    for i, t in enumerate(table):
        if val <= t:
            return i
    return len(table)


def lin2ulaw(v):
    v >>= 2
    if v < 0:
        v, mask = -v, 0x7F
    else:
        mask = 0xFF
    v = min(v, 8159) + 33
    s = _seg(v, SEG_U)
    if s >= 8:
        return 0x7F ^ mask
    return ((s << 4) | ((v >> (s + 1)) & 0xF)) ^ mask


def ulaw2lin(u):
    u = (~u) & 0xFF
    t = ((u & 0xF) << 3) + 0x84
    t <<= (u & 0x70) >> 4
    return (0x84 - t) if (u & 0x80) else (t - 0x84)


def lin2alaw(v):
    v >>= 3
    if v >= 0:
        mask = 0xD5
    else:
        mask, v = 0x55, -v - 1
    v = min(v, 4095)
    s = _seg(v, SEG_A)
    if s >= 8:
        return 0x7F ^ mask
    a = s << 4
    a |= ((v >> 1) & 0xF) if s < 2 else ((v >> s) & 0xF)
    return a ^ mask


def alaw2lin(a):
    a ^= 0x55
    t = (a & 0xF) << 4
    s = (a & 0x70) >> 4
    if s == 0:
        t += 8
    elif s == 1:
        t += 0x108
    else:
        t = (t + 0x108) << (s - 1)
    return t if (a & 0x80) else -t


_DOMAIN = range(-32768, 32768)
ENC = {"u": np.array([lin2ulaw(v) for v in _DOMAIN], dtype=np.uint8),
       "a": np.array([lin2alaw(v) for v in _DOMAIN], dtype=np.uint8)}
DEC = {"u": np.array([ulaw2lin(i) for i in range(256)], dtype=np.int32),
       "a": np.array([alaw2lin(i) for i in range(256)], dtype=np.int32)}

assert DEC["u"].max() == 32124, DEC["u"].max()
assert DEC["a"].max() == 32256, DEC["a"].max()

ALAW_FS = 32256          # the top A-law codeword, as decoded


def roundtrip(x, law):
    """Encode and decode, exactly as the ATA's codec would."""
    xi = np.clip(np.round(x), -32768, 32767).astype(np.int32) + 32768
    return DEC[law][ENC[law][xi]].astype(np.float64)


# --------------------------------------------------------------------------
# Captures.

def read_wav(path):
    w = wave.open(path, "rb")
    try:
        return np.frombuffer(w.readframes(w.getnframes()),
                             dtype="<i2").astype(np.float64)
    finally:
        w.close()


def active(x, thr_db=-55.0, blk=800):
    """Drop the silent blocks -- ringing, the gap before answer, the tail."""
    m = len(x) // blk * blk
    if m == 0:
        return np.array([])
    b = x[:m].reshape(-1, blk)
    keep = np.sqrt((b * b).mean(axis=1)) > 32768 * 10 ** (thr_db / 20.0)
    return b[keep].reshape(-1) if keep.any() else np.array([])


def dbfs(x):
    return 20 * math.log10(max(math.sqrt(float((x * x).mean())), 1e-9) / 32768)


def prefix_of(tx_path):
    return tx_path[:-len(".modem_tx_8k.wav")]


def requests_for(tx_path):
    """Every power reduction the far end asked of us, in order, from the logs
    beside the capture."""
    out = []
    for lg in sorted(glob.glob(os.path.dirname(tx_path) + "/*")):
        if lg.endswith(".wav"):
            continue
        try:
            t = open(lg, "rb").read().decode("utf8", "replace")
        except OSError:
            continue
        out += [int(v) for v in
                re.findall(r"requested by remote modem is (-?\d+) dB", t)]
    return out


# --------------------------------------------------------------------------
# The four reports.

def cmd_sweep(args):
    paths = args or [CAPROOT + "/qk/qk-courier-1/qk-courier-1.modem_tx_8k.wav",
                     CAPROOT + "/why/why-olinet-1/why-olinet-1.modem_tx_8k.wav"]
    for p in paths:
        x = active(read_wav(p))[-160000:]        # last 20 s: data mode
        if len(x) < 40000:
            print("%s: too short" % os.path.basename(p))
            continue
        pk = float(np.abs(x).max())
        print("=== %s: RMS %.2f dBFS, peak %.2f dBFS, PAR %.1f dB"
              % (os.path.basename(prefix_of(p)), dbfs(x),
                 20 * math.log10(pk / 32768),
                 20 * math.log10(pk / math.sqrt(float((x * x).mean())))))
        print("%7s %10s %8s %8s %8s"
              % ("gain", "RMS dBFS", "SNR A", "SNR u", "clip%"))
        for g in range(-30, 21, 3):
            y = x * 10 ** (g / 20.0)
            snr = {}
            for law in ("a", "u"):
                err = roundtrip(y, law) - y      # clipping COUNTS as error
                snr[law] = 10 * math.log10(float((y * y).mean())
                                           / max(float((err * err).mean()), 1e-12))
            print("%+6d %10.2f %8.2f %8.2f %8.3f"
                  % (g, dbfs(y), snr["a"], snr["u"],
                     float((np.abs(y) > 32767).mean() * 100)))
        print()


def _census(pattern):
    for p in sorted(glob.glob(pattern)):
        rxp = prefix_of(p) + ".modem_rx_8k.wav"
        if not os.path.exists(rxp):
            continue
        tx, rx = active(read_wav(p)), active(read_wav(rxp))
        if len(tx) < 8000 or len(rx) < 8000:
            continue
        yield os.path.basename(prefix_of(p)), tx, rx, p


def cmd_levels(args):
    pattern = args[0] if args else DEFAULT_GLOB
    rows = [(n, dbfs(tx), dbfs(rx)) for n, tx, rx, _ in _census(pattern)]
    print("%-26s %11s %11s" % ("capture", "txRMS dBFS", "rxRMS dBFS"))
    for r in rows:
        print("%-26s %11.2f %11.2f" % r)
    if rows:
        print("\nn=%d  median tx %.2f dBFS  median rx %.2f dBFS" % (
            len(rows), statistics.median([r[1] for r in rows]),
            statistics.median([r[2] for r in rows])))


def cmd_sat(args):
    pattern = args[0] if args else DEFAULT_GLOB
    rows = []
    for n, _tx, rx, _p in _census(pattern):
        rows.append((n, dbfs(rx), float((np.abs(rx) >= ALAW_FS).mean() * 100)))
    rows.sort(key=lambda r: -r[2])
    print("%-26s %11s %9s" % ("capture", "rxRMS dBFS", "sat%"))
    for r in rows[:20]:
        print("%-26s %11.2f %9.4f" % r)
    if rows:
        print("\nn=%d  any saturated sample: %d  above 0.01%%: %d" % (
            len(rows), sum(1 for r in rows if r[2] > 0),
            sum(1 for r in rows if r[2] > 0.01)))


def cmd_request(args):
    pattern = args[0] if args else DEFAULT_GLOB
    pairs = []
    for n, tx, _rx, p in _census(pattern):
        req = requests_for(p)
        seg = tx[-160000:]
        if req and len(seg) >= 40000:
            pairs.append((n, req[-1], dbfs(seg)))
    by = {}
    for _n, r, lv in pairs:
        by.setdefault(r, []).append(lv)
    print("%-8s %5s %14s" % ("req dB", "n", "median txRMS"))
    for k in sorted(by):
        print("%-8d %5d %14.2f" % (k, len(by[k]), statistics.median(by[k])))
    if len(pairs) > 2:
        xs = np.array([p[1] for p in pairs], float)
        ys = np.array([p[2] for p in pairs], float)
        m = np.linalg.lstsq(np.vstack([xs, np.ones(len(xs))]).T,
                            ys, rcond=None)[0][0]
        print("\nslope %.3f dB of level per dB requested "
              "(-1.0 would be full compliance), r=%.3f, n=%d"
              % (m, float(np.corrcoef(xs, ys)[0, 1]), len(pairs)))


def main(argv):
    cmds = {"sweep": cmd_sweep, "levels": cmd_levels,
            "sat": cmd_sat, "request": cmd_request}
    if len(argv) < 2 or argv[1] not in cmds:
        print(__doc__)
        return 2
    cmds[argv[1]](argv[2:])
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
