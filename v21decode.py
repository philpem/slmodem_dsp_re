#!/usr/bin/env python3
"""
Decode the V.21 FSK traffic out of a recorded call.

V.8 negotiation rides on V.21 at 300 bit/s, and the two directions use
different channel pairs:

    channel 1 (the ORIGINATING modem)   mark 980 Hz   space 1180 Hz
    channel 2 (the ANSWERING modem)     mark 1650 Hz  space 1850 Hz

so a recording of the SIP audio contains both V.8 messages in clear, and the
octets can be read back without instrumenting either modem.  That matters here
because one of the two ends is a sealed hardware modem: this is the only way to
see what it actually said, as opposed to what it should have said.

Mark is binary 1 and is also the idle state.  Each character is 10 bits: a
start bit (0), eight data bits LEAST SIGNIFICANT FIRST, and a stop bit (1).

Usage:  v21decode.py <file.raw> <rate> ch1|ch2 [start_s] [end_s]
"""
import sys

import numpy as np

CHANNELS = {"ch1": (980.0, 1180.0, "originating"),
            "ch2": (1650.0, 1850.0, "answering")}
BAUD = 300.0


def goertzel_energy(x, freq, rate, win):
    """Sliding-window energy at `freq`, one output per input sample."""
    n = np.arange(len(x))
    ref = np.exp(-2j * np.pi * freq * n / rate)
    mixed = x * ref
    # boxcar integrator of length `win`, as a cumulative-sum difference
    c = np.concatenate(([0], np.cumsum(mixed)))
    return np.abs(c[win:] - c[:-win])


def decode(path, rate, chan, t0=0.0, t1=None):
    mark, space, who = CHANNELS[chan]
    x = np.fromfile(path, dtype="<i2").astype(float)
    a = int(t0 * rate)
    b = int(t1 * rate) if t1 else len(x)
    x = x[a:b]
    if len(x) < rate // 10:
        print("  (too short)")
        return

    spb = rate / BAUD                       # samples per bit
    win = int(round(spb))
    m = goertzel_energy(x, mark, rate, win)
    s = goertzel_energy(x, space, rate, win)

    level = np.sqrt((x ** 2).mean())
    both = m + s
    # A bit is only meaningful where one of the two tones actually dominates;
    # elsewhere the channel is carrying something that is not this V.21 pair
    # (the other direction's tones, a V.34 probe, silence) and must not be
    # decoded into plausible-looking octets.
    valid = both > (0.25 * np.median(both[both > 0]) + 1e-9)
    bits = (m > s)

    print("  %s channel (%s), %.1f s, rms %.0f, %.0f%% of samples carry this pair"
          % (chan, who, len(x) / rate, level, 100.0 * valid.mean()))

    # Find characters: hunt for a start bit (a run of space while idle), then
    # sample the eight data bits and the stop bit at their centres.
    out = []
    i = int(spb)
    n = len(bits)
    while i < n - int(11 * spb):
        if bits[i] or not valid[i]:          # need a 0 (space) to start
            i += 1
            continue
        # confirm the start bit at its centre
        c = i + spb / 2
        if bits[int(c)]:
            i += 1
            continue
        val = 0
        ok = True
        for k in range(8):
            p = int(c + (k + 1) * spb)
            if p >= n or not valid[p]:
                ok = False
                break
            if bits[p]:
                val |= (1 << k)              # LSB first
        stop = int(c + 9 * spb)
        if not ok or stop >= n or not bits[stop]:
            i += 1
            continue
        out.append((a / rate + (i / rate), val))
        i = stop + int(spb / 2)

    if not out:
        print("    no framed characters")
        return

    print("    %d characters" % len(out))
    runs = []
    for t, v in out:
        if runs and v == runs[-1][2] and t - runs[-1][1] < 0.15:
            runs[-1][1] = t
            runs[-1][3] += 1
        else:
            runs.append([t, t, v, 1])
    for t0_, t1_, v, cnt in runs:
        bar = "%3d x " % cnt if cnt > 1 else "      "
        print("    %7.2f-%7.2f  %s0x%02x  %s" % (t0_, t1_, bar, v, describe(v)))


def describe(v):
    """Name the octets V.8 defines, and nothing else -- an unrecognised value
    is printed as a value, never guessed at."""
    known = {
        0x00: "",
        0xff: "all ones (idle/sync)",
    }
    if v in known:
        return known[v]
    return ""


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        raise SystemExit(2)
    path, rate, chan = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    t0 = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
    t1 = float(sys.argv[5]) if len(sys.argv) > 5 else None
    decode(path, rate, chan, t0, t1)


main()
