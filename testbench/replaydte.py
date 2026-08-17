#!/usr/bin/env python3
"""replaydte.py -- act as the DTE for a replayed call.

    replaydte.py /dev/pts/24 [seconds]

slmodemd is a modem.  It sits in command mode and starts no datapump until
something on the pty tells it to, so a replay that only feeds audio waits
forever for an equaliser that never runs -- which is exactly how the first
version of replaycmp.sh failed.  This issues the three commands that put it
into a V.34 originate, then holds the line open while the recording plays.

NO CALL IS PLACED, and that is structural rather than a promise.  On the bench
d-modem sits on the far side of slmodemd's socket and turns a dial string into
SIP.  Under replay, `replay.py` occupies that slot and never reads the dial
string at all -- slmodemd passes it as argv[1] and it is dropped.  The number
in the ATD below exists to make the command well-formed and reaches no network.

AT+MS=34,1 pins V.34 rather than letting V.8 negotiate, because a recording
cannot answer a negotiation: the far end in the file said what it said months
ago and will not adapt.  Pinning the mode is what makes the receiver run the
datapump the recording was made against.
"""

import os
import sys
import time

#
# OVERRIDABLE, because chancall.sh needs one side to ANSWER and the other to
# ORIGINATE, and a hard-coded ATD can only do one of those.  Semicolon
# separated, e.g. DTE_CMDS='ATZ;AT+MS=34,1;ATS0=1' for the answering side.
# The default is the originating sequence a replay needs.
#
CMDS = tuple((c.strip() + "\r").encode() for c in
             os.environ.get("DTE_CMDS", "ATZ;AT+MS=34,1;ATDT1902").split(";")
             if c.strip())


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: replaydte.py <pty> [hold-seconds]")
    pty = sys.argv[1]
    hold = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0

    fd = os.open(pty, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        for cmd in CMDS:
            os.write(fd, cmd)
            time.sleep(0.4)
        #
        # Drain whatever the modem says back.  A pty whose output buffer fills
        # blocks the writer, and the writer is slmodemd, so this must keep
        # reading regardless.  With DTE_ECHO=1 it is ALSO copied to stdout,
        # because the result code carrying the rate -- `CONNECT 33600` --
        # exists nowhere else: slmodemd's log records `modem report result: 1
        # (CONNECT)` and drops the speed.  chancall.sh sets DTE_ECHO and
        # redirects to `LABEL.SIDE.dte`.
        #
        # OFF BY DEFAULT, because replaycmp.sh lets this stdout reach the
        # console rather than a file and should keep the output it has always
        # had.
        #
        echo = os.environ.get("DTE_ECHO") == "1"
        end = time.time() + hold
        while time.time() < end:
            try:
                data = os.read(fd, 4096)
                if not data:
                    break
                if echo:
                    sys.stdout.write(data.decode("latin-1"))
                    sys.stdout.flush()
            except BlockingIOError:
                pass
            except OSError:
                break
            time.sleep(0.2)
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
