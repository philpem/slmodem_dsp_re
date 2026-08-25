#!/usr/bin/env python3
"""replay.py -- feed a recorded call into slmodemd in d-modem's place.

    REPLAY_WAV=captures/base-cx2-3.modem_rx_8k.wav slmodemd -d9 -e $PWD/replay.py

WHAT IT IS FOR.  Every blob-versus-ours comparison this project has made is
statistical, taken over calls placed on different days, and the two that exist
disagree about which way round the difference goes: "best equerr before the
rate decision" says the blob is better (44 against 58), while "minimum over the
eight blocks after it" says we are (254 against 1698).  Neither can be trusted,
because #129's between-batch floor has never been measured (finding F1904).

Fidelity is not really a statistical question.  Identical samples in should
give identical `equerr` out.  This puts both datapumps on byte-identical input,
so the answer is exact and needs no calls at all.

HOW slmodemd HANDS OVER THE AUDIO.  `socket_start` in D-Modem-fork's
modem_main.c makes TWO socketpairs, forks, and `execv`s whatever `-e` named:

    argv[1] = dial string   argv[2] = AUDIO socket   argv[3] = call-info socket

so the audio fd is **argv[-2], not argv[-1]**.  (Stock slmodemd passes only
two and would want argv[-1]; the bench does not run stock slmodemd -- it runs
the fork, which is the only one carrying the `-e` option at all.)

THE WIRE FORMAT IS A TYPED FRAME, NOT RAW PCM.  Both sockets carry
`struct socket_frame` from modem.h:

    struct socket_frame {
        enum socket_frame_types type;     /* AUDIO=0, VOLUME=1, SIP_INFO=2 */
        union {
            struct { char buf[SIP_FRAMESIZE * 2]; } audio;
            struct { int value; } volume;
            struct { char info[256]; } sip;
        } data;
    };

With SIP_RATE 8000 and SIP_FRAMESIZE = 8000/(1000/20) = 160 samples, the union
is max(320, 256, 4) = 320 bytes and the whole frame is 4 + 320 = **324 bytes**
on 32-bit x86.  slmodemd primes the audio socket with one AUDIO frame and one
VOLUME frame before anything else happens.

**THE AUDIO IS 8000 Hz**, the SIP rate -- not the 9600 the datapump runs at.
slmodemd resamples internally.  Feed the `*_8k.wav` capture, not the bare one;
`capture_io.load` reads the rate from the file, so a wrong file shows up as a
wrong duration rather than silently as a wrong pitch.

IT IS NOT REAL TIME, AND THAT IS THE POINT.  The sockets are blocking, so
slmodemd consumes at whatever rate it can and this feeds it exactly as fast as
it asks.  A replay therefore CANNOT be perturbed by machine load -- unlike a
bench call, which needs an idle box because slmodemd is a soft modem working to
a clock (waitquiet.sh).  Run these while the world compiles.

NO CALL IS PLACED, structurally.  On the bench it is d-modem, on the far side
of these sockets, that turns a dial string into SIP.  Here this program
occupies that slot and never reads the dial string -- slmodemd passes it as
argv[1] and it is dropped.

THE OPEN-LOOP CAVEAT, STATED PLAINLY.  A modem is a closed loop and a recording
is not.  The far end in the file cannot react to what this run transmits, so
once our transmit diverges from whatever the far end originally heard, the
conversation is fiction.  That does not spoil the comparison it is built for:

  * the receiver's `equerr` is driven by RECEIVED audio, which is identical for
    both binaries by construction, so the trajectories are comparable; and
  * the block at which two datapumps' trajectories diverge is the diagnostic
    being sought, not a failure of the method.

What it CANNOT answer is "what rate would this get on a real call".  For that
there is no substitute for the bench.  Do not quote a replay's CONNECT string.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capture_io                                        # noqa: E402

SIP_FRAMESIZE = 160                     # samples; SIP_RATE 8000, 20 ms
AUDIO_BYTES = SIP_FRAMESIZE * 2         # 320
UNION_BYTES = max(AUDIO_BYTES, 256, 4)  # 320
FRAME_BYTES = 4 + UNION_BYTES           # 324

FRAME_AUDIO, FRAME_VOLUME, FRAME_SIP_INFO = 0, 1, 2


def read_exactly(fd, n):
    """A SOCK_STREAM can split a frame across reads, and one short read that
    is treated as a whole frame desynchronises every frame after it."""
    buf = b""
    while len(buf) < n:
        chunk = os.read(fd, n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def audio_frame(payload):
    payload = payload[:AUDIO_BYTES].ljust(UNION_BYTES, b"\0")
    return struct.pack("<i", FRAME_AUDIO) + payload


def main():
    #
    # CONFIGURED BY ENVIRONMENT, NOT BY ARGUMENTS, and not by preference:
    # slmodemd execs the `-e` program with an argv it builds itself, so there
    # is nowhere to put a flag of our own.  Every other bench knob
    # (SLMODEMD_IODELAY, TTY_EXTRA) already travels this way.
    #
    wav = os.environ.get("REPLAY_WAV")
    if not wav:
        sys.exit("replay: set REPLAY_WAV to the recording to feed in.")
    tx_path = os.environ.get("REPLAY_TX")
    pad_s = float(os.environ.get("REPLAY_PAD", "10"))

    if len(sys.argv) < 4:
        sys.exit("replay: expected <dial-string> <audio-fd> <sip-fd>; got %r.  "
                 "This is not meant to be run by hand." % sys.argv[1:])
    fd = int(sys.argv[-2])

    samples, rate = capture_io.load(wav, None)
    if rate != 8000:
        sys.stderr.write("replay: WARNING %s is %d Hz; the socket carries the "
                         "SIP rate of 8000.  Feed the *_8k.wav.\n"
                         % (os.path.basename(wav), rate))
    body = samples.astype("<i2").tobytes() + b"\0" * (2 * int(pad_s * rate))

    txf = open(tx_path, "wb") if tx_path else None
    sys.stderr.write("replay: %s, %d samples at %d Hz, +%.0fs pad, %d-byte "
                     "frames\n" % (os.path.basename(wav), len(samples), rate,
                                   pad_s, FRAME_BYTES))

    #
    # ONE FRAME IN, ONE FRAME OUT.  slmodemd's loop is symmetrical: it hands
    # over a frame of what it transmitted and expects a frame of what it should
    # receive.  Answering every AUDIO frame with exactly one AUDIO frame is
    # what keeps the socket from filling in either direction -- feeding ahead
    # would eventually block on a full send buffer while slmodemd blocks on a
    # read, and the run would hang rather than fail.
    #
    pos = nframes = 0
    try:
        while True:
            raw = read_exactly(fd, FRAME_BYTES)
            if raw is None:
                break                       # slmodemd closed: run is over
            (ftype,) = struct.unpack_from("<i", raw, 0)
            if ftype != FRAME_AUDIO:
                continue                    # VOLUME and friends: nothing owed
            if txf:
                txf.write(raw[4:4 + AUDIO_BYTES])
            if pos >= len(body):
                break
            os.write(fd, audio_frame(body[pos:pos + AUDIO_BYTES]))
            pos += AUDIO_BYTES
            nframes += 1
    except (BrokenPipeError, OSError) as e:
        sys.stderr.write("replay: socket closed after %d frames (%s)\n"
                         % (nframes, e))
    finally:
        if txf:
            txf.close()

    sys.stderr.write("replay: fed %d frames, %d of %d samples (%.1f s)\n"
                     % (nframes, pos // 2, len(body) // 2,
                        (pos // 2) / float(rate)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
