"""capture_io.py -- open a capture, whatever it is on disk.

    from capture_io import load
    x, rate = load("captures/base-cx2-1.modem_rx")     # no extension

WHY THIS EXISTS.  The bench used to write every direction TWICE, once as
headerless `.raw` and once as `.wav` carrying the same samples behind a
44-byte header.  That doubled the size of every capture for nothing: 2003
files and 1.9 GB of pure duplication, on a disk that later reached 100% full
and broke other sessions' builds outright with ENOSPC.

Only the wav is kept now, and it is the better half of the pair: it carries
its own sample rate and channel count. The raw never did, so every reader had
to be told 9600-versus-8000 out of band and `audiostats.py` hard-coded 8000
while the datapump's native rate is 9600 -- a discrepancy nothing would have
caught.

Accepts a name with or without an extension, and still reads a `.raw` if one
turns up from an external source, in which case the rate has to be supplied
because the file does not know it.
"""

import os
import wave

import numpy as np


def load(path, raw_rate=None):
    """Return (samples, rate).  Stereo comes back as an (n, 2) array."""
    base = path
    for ext in (".wav", ".raw"):
        if base.endswith(ext):
            base = base[: -len(ext)]
            break

    if os.path.exists(base + ".wav"):
        with wave.open(base + ".wav", "rb") as w:
            ch, rate, n = w.getnchannels(), w.getframerate(), w.getnframes()
            x = np.frombuffer(w.readframes(n), dtype="<i2")
        return (x.reshape(-1, ch) if ch > 1 else x), rate

    if os.path.exists(base + ".raw"):
        if raw_rate is None:
            # Refuse rather than guess.  A headerless file read at the wrong
            # rate produces plausible numbers that are silently wrong, which is
            # exactly the failure this module exists to retire.
            raise ValueError("%s.raw has no rate; pass raw_rate=" % base)
        return np.fromfile(base + ".raw", dtype="<i2"), raw_rate

    raise IOError("no capture at %s(.wav|.raw)" % base)
