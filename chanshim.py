#!/usr/bin/env python3
"""chanshim.py -- put two live datapumps on an emulated channel.

One instance runs in d-modem's place for each slmodemd, exactly as replay.py
does.  Instead of a recording, each shim forwards to the OTHER shim over a
loopback socket, applying the measured characteristics of the bench path on
the way.  Two real datapumps, reacting to each other, with no PBX and no
hardware.

    CHAN_ROLE=server CHAN_PORT=45001 slmodemd -d9 -e $PWD/chanshim.py
    CHAN_ROLE=client CHAN_PORT=45001 slmodemd -d9 -e $PWD/chanshim.py

WHY THIS AND NOT replay.py.  A replay is OPEN LOOP: the recorded far end said
what it said months ago and cannot react.  Finding 1906 measured the
consequence -- switching the pre-emphasis request from index 6 to index 0
changed nothing at all across 25 blocks, because a recording cannot comply
with a request.  Everything this project actually wants to test -- retrains,
renegotiation, the rate ladder, pre-emphasis -- needs both ends live.  This
closes the loop.

WHAT IS MODELLED, and every number is measured rather than assumed:

  * **Band limit.**  The VG204 is configured `bearer-cap 3100Hz` and measures
    flat to 3300 Hz, -6.2 dB at 3400, -15.5 at 3700, -33.6 at 3900
    (finding 1907's spectrum work).  Modelled as an FIR fitted to those points.
  * **Delay.**  The far end's own report gives 141-152 ms round trip on this
    path (finding 1916's AT&V1 / ATI11 reads), which is packetisation plus two
    jitter buffers plus the socket hop -- NOT propagation.  Configurable,
    default 70 ms each way.
  * **Jitter and loss.**  Off by default.  There to answer whether either tips
    a call from the settling mode into the thrashing mode (finding 1922).
  * **Noise.**  Off by default, and DELIBERATELY so: finding 1914 established
    that this bench cannot measure its own noise floor, so any figure here
    would be invented.  Set CHAN_SNR only to explore, never to reproduce.

WHAT IT CANNOT ANSWER.  Smart Link against Smart Link is not Smart Link
against a Rockwell or a USR.  If the retrain thrashing is an INTEROP
behaviour, two identical datapumps may handshake perfectly and reproduce
nothing -- which is itself a result, because it would localise the cause to
the real path rather than to the datapump.  Do not report an emulated rate as
a bench rate.

THE FRAME FORMAT is slmodemd's, not ours: `struct socket_frame` is a 4-byte
type plus a 320-byte union, 324 bytes, at 8000 Hz.  The audio fd is
**argv[-2]**, not argv[-1] -- D-Modem-fork passes dial string, audio socket,
then a call-info socket.  Both facts cost bench calls to learn; see replay.py.
"""

import os
import socket
import struct
import sys
import time

import numpy as np

SIP_FRAMESIZE = 160
AUDIO_BYTES = SIP_FRAMESIZE * 2
UNION_BYTES = max(AUDIO_BYTES, 256, 4)
FRAME_BYTES = 4 + UNION_BYTES
FRAME_AUDIO = 0
RATE = 8000


def band_filter(tilt_db=0.0):
    """FIR matching the VG204's measured response.

    Fitted to the points finding 1907 measured, not to a textbook template:
    0 dB through 3300, -6.2 at 3400, -15.5 at 3700, -33.6 at 3900.  A plain
    rectangular low-pass would be wrong in the one region that decides whether
    3429 baud survives.
    """
    f = np.array([0, 300, 3000, 3300, 3400, 3700, 3900, 4000], float)
    d = np.array([0, 0, 0, 0, -6.2, -15.5, -33.6, -40.0], float)
    #
    # CHAN_TILT adds a linear slope in dB across the voice band, on top of the
    # VG204's measured response.  This bench has none of its own -- finding
    # 1956 measured it flat to +/-0.4 dB from 450 to 3150 Hz -- and a channel
    # with no tilt cannot show whether a tilt corrector works.  A real
    # subscriber loop has several dB of it, which is what Table 3 exists for.
    #
    # Sign: NEGATIVE is the physical case, high frequencies attenuated.
    # The slope is applied from 300 Hz to 3400 Hz and held flat outside, so it
    # does not fight the band limit at the top or the DC block at the bottom.
    #
    if tilt_db:
        lo, hi = 300.0, 3400.0
        d = d + np.clip((f - lo) / (hi - lo), 0.0, 1.0) * tilt_db
    n = 129
    grid = np.linspace(0, RATE / 2, 512)
    mag = 10 ** (np.interp(grid, f, d) / 20.0)
    # symmetric impulse response by inverse FFT of the magnitude response
    full = np.concatenate([mag, mag[-2:0:-1]])
    h = np.real(np.fft.ifft(full))
    h = np.concatenate([h[-(n // 2):], h[:n // 2 + 1]])
    return h * np.hanning(len(h))


class Channel:
    def __init__(self):
        self.tilt_db = float(os.environ.get("CHAN_TILT", "0"))
        self.h = band_filter(self.tilt_db)
        self.tail = np.zeros(len(self.h) - 1)
        self.delay_ms = float(os.environ.get("CHAN_DELAY_MS", "70"))
        self.snr = os.environ.get("CHAN_SNR")
        self.loss = float(os.environ.get("CHAN_LOSS", "0"))
        n = int(RATE * self.delay_ms / 1000.0)
        self.delay_buf = np.zeros(n) if n > 0 else None
        #
        # SEEDABLE, and it must be.  This was a hardcoded 12345, which makes
        # every emulated call bit-identical: an A/B of 8 calls per arm came
        # back with all eight rates exactly 7200 and all eight handshake
        # counts exactly 3, i.e. n=1 reported as n=8.  Determinism is right
        # for reproducing ONE call and wrong for measuring a distribution.
        #
        # Default is still 12345 so an unset CHAN_SEED reproduces every result
        # taken before this existed.  Pass a distinct seed per call to sample.
        #
        self.rng = np.random.default_rng(int(os.environ.get("CHAN_SEED", "12345")))
        # jitter-buffer underrun model -- see slip_apply()
        self.slip = float(os.environ.get("CHAN_SLIP", "0"))     # events/second
        self.slipq = bytearray()
        self.slips = 0
        self.slip_max = int(RATE * float(os.environ.get("CHAN_SLIP_MAX_MS",
                                                        "500")) / 1000.0) * 2

    def slip_apply(self, pcm):
        """Model a JITTER BUFFER UNDERRUN, which is not the same as loss.

        `CHAN_LOSS` substitutes silence for a frame IN PHASE: 20 ms of wrong
        samples, then the real signal resumes exactly where the receiver
        expects it.  That is what a dropped RTP packet does.

        A jitter buffer underrun INSERTS.  pjmedia hands the consumer a frame
        it invented (`PJMEDIA_JB_MISSING_FRAME` -> PLC, or
        `ZERO_EMPTY_FRAME`), and everything afterwards arrives 10 ms LATER
        than it would have.  At 3429 baud that is a step of about 34 symbols
        for the timing recovery to chase, on top of the bad samples.  Finding
        1941 measured this happening 0.3-0.5 times a second on the real bench
        with zero packet loss, so it is the impairment this bench actually
        has, and the emulator did not model it at all (1948: the emulator
        would not reproduce the retraining under test).

        Concealment is a REPEAT of the previous 10 ms rather than zeroes,
        because that is what WSOLA produces -- plausible waveform carrying
        wrong symbols, which is worse for a decision-directed equaliser than
        an obvious hole.

        The queue this creates is the added delay, exactly as on the real
        path, and it is capped the way pjmedia caps it: past `jb_max` the
        oldest audio is discarded (`jbuf.c:1056`).
        """
        if not self.slip:
            return pcm
        self.slipq += pcm
        if self.rng.random() < self.slip * (SIP_FRAMESIZE / float(RATE)):
            half = AUDIO_BYTES // 2                     # 10 ms
            tail = (bytes(self.slipq[-half:]) if len(self.slipq) >= half
                    else b"\0" * half)
            self.slipq += tail
            self.slips += 1
        if len(self.slipq) > self.slip_max:
            del self.slipq[:len(self.slipq) - self.slip_max]
        out = bytes(self.slipq[:AUDIO_BYTES])
        del self.slipq[:AUDIO_BYTES]
        return out.ljust(AUDIO_BYTES, b"\0")

    def apply(self, pcm):
        x = np.frombuffer(pcm, dtype="<i2").astype(float)
        y = np.convolve(np.concatenate([self.tail, x]), self.h, "valid")
        self.tail = np.concatenate([self.tail, x])[-(len(self.h) - 1):]
        if self.delay_buf is not None and len(self.delay_buf):
            both = np.concatenate([self.delay_buf, y])
            y, self.delay_buf = both[:len(y)], both[len(y):]
        if self.snr:
            # only when explicitly asked; see the docstring on why not default
            p = np.mean(y ** 2) or 1.0
            y = y + self.rng.normal(0, (p / (10 ** (float(self.snr) / 10))) ** .5,
                                    len(y))
        return np.clip(y, -32768, 32767).astype("<i2").tobytes()


def read_exactly(fd, n):
    buf = b""
    while len(buf) < n:
        c = os.read(fd, n - len(buf))
        if not c:
            return None
        buf += c
    return buf


def peer_socket():
    role = os.environ.get("CHAN_ROLE", "client")
    port = int(os.environ.get("CHAN_PORT", "45001"))
    if role == "server":
        ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        ls.bind(("127.0.0.1", port))
        ls.listen(1)
        sys.stderr.write("chanshim: listening on %d\n" % port)
        s, _ = ls.accept()
        ls.close()
    else:
        for _ in range(200):
            try:
                s = socket.create_connection(("127.0.0.1", port), 0.5)
                break
            except OSError:
                time.sleep(0.05)
        else:
            sys.exit("chanshim: could not reach peer on %d" % port)
    #
    # CLEAR THE CONNECT TIMEOUT.  `create_connection(..., 0.5)` leaves that
    # 0.5 s on the socket for every later operation, so the first recv of a
    # frame gives up half a second in and the run ends with "1 frames
    # (timed out)" -- which looks like a datapump that would not start.
    #
    s.settimeout(None)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return s


def main():
    if len(sys.argv) < 4:
        sys.exit("chanshim: expected <dial> <audio-fd> <sip-fd>; got %r"
                 % sys.argv[1:])
    fd = int(sys.argv[-2])
    ch = Channel()
    peer = peer_socket()
    sys.stderr.write("chanshim: role=%s delay=%.0fms snr=%s loss=%.3f slip=%.2f/s "
                     "tilt=%.1fdB\n"
                     % (os.environ.get("CHAN_ROLE"), ch.delay_ms,
                        ch.snr or "off", ch.loss, ch.slip, ch.tilt_db))
    silence = b"\0" * AUDIO_BYTES
    nf = 0
    timing = os.environ.get("CHAN_TIMING") == "1"
    t_wrote = None
    lat = []
    try:
        while True:
            raw = read_exactly(fd, FRAME_BYTES)
            if raw is None:
                break
            if timing and t_wrote is not None:
                lat.append((time.monotonic() - t_wrote) * 1000.0)
                #
                # REPORTED PERIODICALLY, NOT AT EXIT.  chancall.sh tears the
                # run down by killing the process group, so an exit-time
                # summary is never reached and the first attempt at this
                # measurement produced nothing at all.
                #
                if len(lat) % 500 == 0:
                    a = np.array(lat[-500:])
                    sys.stderr.write(
                        "CHANLAT n=%d p50=%.2f p90=%.2f p99=%.2f max=%.2f ms "
                        "over20=%.1f%% over40=%.1f%%\n"
                        % (len(a), np.percentile(a, 50), np.percentile(a, 90),
                           np.percentile(a, 99), a.max(),
                           100.0 * (a > 20).mean(), 100.0 * (a > 40).mean()))
                    sys.stderr.flush()
            (ftype,) = struct.unpack_from("<i", raw, 0)
            if ftype != FRAME_AUDIO:
                continue
            out = ch.apply(raw[4:4 + AUDIO_BYTES])
            if ch.loss and ch.rng.random() < ch.loss:
                out = silence            # a lost packet is silence, not a gap
            peer.sendall(out)
            got = b""
            while len(got) < AUDIO_BYTES:
                c = peer.recv(AUDIO_BYTES - len(got))
                if not c:
                    raise ConnectionError("peer closed")
                got += c
            got = ch.slip_apply(got)
            os.write(fd, struct.pack("<i", FRAME_AUDIO)
                     + got.ljust(UNION_BYTES, b"\0"))
            #
            # HOW LONG slmodemd TOOK, measured from the instant we handed it a
            # frame to the instant it produced the next one.  This is the
            # quantity #155 is about: `dmodem_get_frame` does a BLOCKING read
            # on this socket from the conference-bridge thread, so whatever
            # distribution appears here is exactly what the real jitter buffer
            # sees as consumer burstiness.  A tight 20 ms means the buffer can
            # be shallow; a heavy tail means it cannot.
            #
            if timing:
                t_wrote = time.monotonic()
            nf += 1
    except (OSError, ConnectionError) as e:
        sys.stderr.write("chanshim: ended after %d frames (%s)\n" % (nf, e))
    finally:
        peer.close()
    sys.stderr.write("chanshim: %d frames, %.1f s of audio, %d slips\n"
                     % (nf, nf * SIP_FRAMESIZE / float(RATE), ch.slips))
    if timing and lat:
        a = np.array(lat)
        sys.stderr.write(
            "CHANLAT n=%d  p50=%.2f p90=%.2f p99=%.2f max=%.2f ms  "
            "over20ms=%.1f%% over40ms=%.2f%%\n"
            % (len(a), np.percentile(a, 50), np.percentile(a, 90),
               np.percentile(a, 99), a.max(),
               100.0 * (a > 20).mean(), 100.0 * (a > 40).mean()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
