#!/usr/bin/env python3
"""
Drive both ends of a test call and log both, continuously.

WHY THIS EXISTS.  The first version of the harness opened /dev/ttyUSB1, sent an
AT command, and closed it again for each interaction.  That is wrong twice over:

  * While the port is closed the kernel DISCARDS everything the modem sends, so
    RING, CONNECT <rate> and NO CARRIER -- the entire answering-side record of
    what happened -- were thrown away.  Three calls were analysed without it,
    and the log read "(nothing buffered)", which looks like "the modem said
    nothing" and is really "we were not listening".
  * Closing the port drops DTR, which a modem may take as a hang-up or a reset,
    so the act of observing changed the thing being observed.

So: open both ends ONCE, hold them open for the whole call, and timestamp every
line from either.  The two ends are
    tty  -- the real modem on a serial port (SupraExpress 56e PRO)
    pty  -- slmodemd's pseudo-terminal, i.e. the blob (or our reconstruction)
and either may originate.
"""
import argparse
import os
import select
import sys
import termios
import time


def open_raw(path, speed=termios.B115200):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0                                     # iflag: no xon/xoff, no cr/nl
    a[1] = 0                                     # oflag: no post-processing
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[3] = 0                                     # lflag: raw, no echo
    a[4] = a[5] = speed
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


class End:
    """One end of the call: a file descriptor plus its line-assembly buffer."""

    def __init__(self, name, fd, log):
        self.name, self.fd, self.log = name, fd, log
        self.buf = b""
        self.text = ""          # everything seen, for CONNECT/CARRIER tests
        self.connected = False
        self.rx = b""           # payload seen after CONNECT, for the data test

    def send(self, s):
        os.write(self.fd, s.encode() if isinstance(s, str) else s)
        self.log.write("%8.3f %s >> %r\n" % (time.time() - T0, self.name, s))
        self.log.flush()

    def drain(self):
        try:
            c = os.read(self.fd, 4096)
        except OSError:
            return
        if not c:
            return
        self.text += c.decode(errors="replace")
        if self.connected:
            self.rx += c
        self.buf += c
        while b"\r" in self.buf or b"\n" in self.buf:
            i = min((self.buf.index(b) for b in (b"\r", b"\n") if b in self.buf))
            line, self.buf = self.buf[:i], self.buf[i + 1:]
            line = line.decode(errors="replace").strip()
            if line:
                stamp = time.time() - T0
                self.log.write("%8.3f %s << %s\n" % (stamp, self.name, line))
                self.log.flush()
                print("   %7.2f %-5s %s" % (stamp, self.name, line[:100]))
                sys.stdout.flush()
                if "CONNECT" in line:
                    self.connected = True


def pump(ends, seconds, until=None):
    """Read both ends for `seconds`, or until `until(ends)` returns True."""
    end_at = time.time() + seconds
    while time.time() < end_at:
        r, _, _ = select.select([e.fd for e in ends], [], [], 0.2)
        for e in ends:
            if e.fd in r:
                e.drain()
        if until and until(ends):
            return True
    return False


T0 = time.time()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tty", required=True, help="real modem, e.g. a /dev/serial/by-id/ path")
    ap.add_argument("--pty", required=True, help="slmodemd's pseudo-terminal")
    ap.add_argument("--originator", choices=("pty", "tty"), required=True)
    ap.add_argument("--dial", required=True, help="number the originator dials")
    ap.add_argument("--log", required=True, help="path for the combined log")
    ap.add_argument("--wait", type=float, default=75.0, help="seconds to wait for CONNECT")
    ap.add_argument("--extra", default="", help="extra AT setup for the answering end")
    ap.add_argument("--quiet", action="store_true", help="do not turn the modem speaker up")
    ap.add_argument("--pty-extra", default="", help="AT setup for slmodemd, applied after ATZ; semicolon-separated")
    ap.add_argument("--tty-extra", default="", help="AT setup for the hardware modem, applied after ATZ; semicolon-separated")
    ap.add_argument("--hold", type=float, metavar="SECS",
                    help="after CONNECT, hold the carrier for SECS writing "
                         "NOTHING, then hang up; use to tell a link that dies "
                         "on the first byte from one that times out")
    ap.add_argument("--tty-retrain", type=int, default=0,
                    help="after connecting, force N retrains from the hardware "
                         "modem with +++ / ATO1, logging the rate each time")
    args = ap.parse_args()

    log = open(args.log, "w")
    tty = End("tty", open_raw(args.tty), log)
    pty = End("pty", open_raw(args.pty), log)
    ends = [tty, pty]
    origin, answer = (pty, tty) if args.originator == "pty" else (tty, pty)

    s0_orig = None
    try:
        # --- setup.  Both ports stay open from here to the end of the call.
        for e in ends:
            e.send("ATZ\r")
        pump(ends, 2.0)

        answer.send("ATS0?\r")                  # remember auto-answer to restore it
        pump(ends, 1.5)
        digits = "".join(c for c in answer.text.split("ATS0?")[-1] if c.isdigit())
        s0_orig = digits[:3] if digits else None

        answer.send("ATS0=1\r")                 # answer after one ring

        # Speaker on for the WHOLE call at maximum volume, on the real modem.
        # M2 keeps it live past carrier detect rather than muting at CONNECT
        # (M1), so the handshake, the retrains and the failure are all audible
        # in the room -- which is a diagnostic channel the logs do not have.
        if not args.quiet:
            tty.send("ATM2L3\r")

        # Setup for the hardware modem, applied here rather than left in its
        # stored profile because the ATZ above would undo it.  This is where
        # AT+A8E=1,1 goes: the SupraExpress ships with V.8 origination AND
        # answer negotiation both disabled, which is why it sent no CM and no
        # JM in the first five calls.
        if args.tty_extra:
            for cmd in args.tty_extra.split(";"):
                tty.send(cmd.strip() + "\r")
                pump(ends, 0.8)
        if args.pty_extra:
            for cmd in args.pty_extra.split(";"):
                pty.send(cmd.strip() + "\r")
                pump(ends, 0.8)
        if args.extra:
            answer.send(args.extra + "\r")
        pump(ends, 1.5)

        # RE-ASSERT AUTO-ANSWER, AFTER THE EXTRAS, AND VERIFY IT.
        #
        # ATS0=1 is sent above, BEFORE --tty-extra.  Any extra that reloads a
        # profile undoes it -- `AT&F` and `ATZ` both do -- and the answering
        # modem then never picks up.  That is exactly how a Courier comparison
        # produced five rows of `no connect` with `AT&F` as the only far-end
        # command: the modem was present, answered AT, and was simply not in
        # auto-answer.  No RING appears anywhere in the log, which is the
        # signature.
        #
        # Reading it back is the point.  Setting it again without checking
        # would leave the same failure available to any future extra that
        # clears S0 by a route nobody predicted.
        answer.send("ATS0=1\r")
        pump(ends, 0.8)
        answer.send("ATS0?\r")
        pump(ends, 1.2)
        s0_now = "".join(c for c in answer.text.split("ATS0?")[-1] if c.isdigit())
        if not s0_now[:3].lstrip("0"):
            print("=== ABORT: %s is not in auto-answer (S0=%s) after --tty-extra."
                  % (answer.name, s0_now[:3] or "?"))
            print("    A far-end command reset it -- AT&F and ATZ both do.")
            print("    It would never answer, and the run would look like a")
            print("    negotiation failure.  Not dialling.")
            return 4

        # --- the call
        print("=== %s dials %s" % (origin.name, args.dial))
        origin.send("ATDT%s\r" % args.dial)

        both = pump(ends, args.wait, until=lambda es: all(e.connected for e in es))
        print("=== connected: " + ", ".join("%s=%s" % (e.name, e.connected) for e in ends))

        if both:
            # The bar is data BOTH WAYS -- a CONNECT that carries nothing is not
            # a working link (see task #81).
            # Wait for the error-control layer before probing.  CONNECT is
            # reported when the DATA PUMP has trained; on an /ARQ link V.42
            # LAPM negotiation then runs on top of it, and anything written
            # during that window is discarded.  One second was enough for the
            # unprotected V.22/V.32 links and not for V.34/ARQ, which is why
            # rows 11 and 12 connected but read nothing -- including row 11,
            # which had no blob in it at all, so it was never a blob problem.
            time.sleep(5.0)
            for e in ends:
                e.rx = b""

            #
            # --hold: CONNECT, sit there, hang up.  NOTHING IS WRITTEN.
            #
            # The Supra drops a Bell 103 call at the exact millisecond the
            # first probe byte is written (#124), which makes "does the write
            # cause it" the whole question -- and there was no way to ask,
            # because this function always writes.  With --hold the carrier
            # is simply held, so a call that survives N seconds of silence
            # and dies on a write has answered it.
            #
            if args.hold:
                print("   HOLDING %.1f s with NO data written" % args.hold)
                pump(ends, args.hold)
                for e in ends:
                    alive = b"NO CARRIER" not in e.rx
                    print("   %s still connected after the hold: %s"
                          % (e.name, "YES" if alive else "NO -- dropped unprompted"))
                print("=== HOLD ONLY, no data probe attempted")
                return 0
            probe = {"tty": b"FROM-TTY 0123456789 the quick brown fox\r",
                     "pty": b"FROM-PTY 9876543210 the quick brown fox\r"}
            for e in ends:
                e.send(probe[e.name])
            pump(ends, 10.0)
            ok = True
            for e in ends:
                other = probe[[x for x in ends if x is not e][0].name][:8]
                got = other in e.rx
                print("   %s received the other end's probe: %s" % (e.name, "YES" if got else "NO"))
                ok = ok and got
            print("=== DATA BOTH WAYS: %s" % ("PASS" if ok else "FAIL"))
        else:
            print("=== NO CONNECT")

        # FORCED RETRAIN, task #109.  `ATO1` is "return to online data mode
        # AND retrain" -- the host-side way to make the link run Phase 3
        # again without touching the blob.
        #
        # The design is PAIRED: the rate at the first CONNECT and the rate
        # after each retrain come from the same call, over the same path,
        # seconds apart.  Every between-call confounder this bench has been
        # burned by -- and there have been four -- is held constant by
        # construction.
        #
        # Finding 1209: eighteen single-pass calls never exceeded 14400, and
        # both calls that did ran Phase 3 twice.  If that is causal, the rate
        # after ATO1 should beat the rate before it.  If it is not, this
        # measures nothing and says so.
        for k in range(args.tty_retrain):
            print("=== forcing retrain %d of %d (+++ ATO1)"
                  % (k + 1, args.tty_retrain))
            time.sleep(1.2)                     # escape guard time
            tty.send("+++")
            time.sleep(1.2)
            tty.send("ATO1\r")
            # A V.34 retrain runs the whole handshake again; give it room.
            pump(ends, 45.0)

    finally:
        for e in ends:
            try:
                e.send("+++")
                time.sleep(1.2)
                e.send("ATH\r")
            except OSError:
                pass
        pump(ends, 2.0)
        if s0_orig is not None:
            answer.send("ATS0=%s\r" % s0_orig)
            pump(ends, 1.5)
            print("=== S0 restored to '%s'" % s0_orig)
        for e in ends:
            os.close(e.fd)
        log.close()


sys.exit(main() or 0)
