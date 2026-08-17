#!/usr/bin/env python3
"""atprobe.py -- is there a MODEM behind this device node, and which one?

    atprobe.py /dev/serial/by-id/usb-FTDI_... [--timeout 2] [--quiet]

Exit 0 and print `OK|<identity>` if a modem answered; exit 1 and print a
diagnosis to stderr otherwise.

WHY THIS EXISTS.  `asymmetry.sh` ran five calls into an empty line and printed
five rows of `no connect`.  The modems had physically disconnected -- the whole
USB hub went -- and nothing in the harness noticed, because the harness's only
question was "did the open() succeed", asked separately inside each call, after
the dial had already gone out.  A row that says `no connect` because the modem
is unplugged is indistinguishable in the summary from one that says `no
connect` because V.34 failed, and that is the exact confusion this bench has
already been burned by once.

So the check has to be stronger than "the node exists" in two ways:

  * A NODE IS NOT A MODEM.  The adapter can enumerate perfectly while the modem
    behind it is powered off, in a hung state, or simply not plugged into the
    adapter.  Only a reply proves there is a modem.
  * A MODEM IS NOT THE RIGHT MODEM.  `ttyUSBn` is assigned in enumeration
    order, so a replug can silently swap the SupraExpress and the Courier.  The
    identity string is what turns "something answered" into "the modem I meant
    answered", which is why `--expect` exists and why `modems.sh` passes it.

State is left as close to untouched as a probe can manage: `AT` alone on the
first attempt, and `ATQ0V1` only if the first attempt drew silence -- which is
what a modem left in quiet mode by a previous run looks like.  No `ATZ`, no
`AT&F`; restoring the profile is the caller's business and `call.py` already
does it.
"""

import argparse
import os
import re
import select
import sys
import termios
import time


def open_raw(path, baud=termios.B115200):
    """Open a serial port with no line discipline in the way.

    CLOCAL matters: without it the open blocks until DCD asserts, which for an
    idle modem is never.  VMIN/VTIME 0 make reads non-blocking so the timeout
    is ours to enforce rather than the kernel's.
    """
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0                                        # iflag
    a[1] = 0                                        # oflag
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[3] = 0                                        # lflag: raw, no echo
    a[4] = a[5] = baud
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


# Terminal result codes.  A command is finished when one of these arrives, not
# when the port merely goes quiet.
_RESULT = re.compile(r"\b(OK|ERROR|NO CARRIER|NO DIAL ?TONE|BUSY|NO ANSWER)\b",
                     re.I)


def command(fd, cmd, timeout, deadline=None):
    """Send one AT command and read the reply.

    `timeout` is the QUIET window -- how long to wait after the last byte
    before deciding the modem has finished.  `deadline` is a hard cap in
    seconds for commands whose answer takes far longer than their echo.

    THE ECHO IS A TRAP.  With E1 the modem echoes the command within
    milliseconds; that is a byte, so a naive quiet-timer restarts and expires
    0.4 s later, long before a slow result arrives.  `ATD` is exactly that
    case: the echo is instant and `NO DIAL TONE` takes ~4 s.  The first
    version of the line check returned "no verdict" for BOTH modems, including
    the one already proven to have no line -- a detector that cannot fail is
    worth nothing, which is how this was caught.

    So: return as soon as a terminal result code appears, and otherwise run to
    `deadline` rather than to the quiet window.
    """
    termios.tcflush(fd, termios.TCIOFLUSH)
    os.write(fd, cmd.encode() + b"\r")
    out = b""
    hard = time.time() + (deadline if deadline else timeout)
    end = time.time() + timeout
    while time.time() < min(end, hard) if not deadline else time.time() < hard:
        if select.select([fd], [], [], 0.2)[0]:
            try:
                d = os.read(fd, 4096)
            except OSError:
                break
            if d:
                out += d
                if _RESULT.search(out.decode(errors="replace")):
                    # DRAIN THE TAIL before returning.  Breaking the instant a
                    # result code appears leaves whatever the modem is still
                    # sending in the kernel buffer, and the NEXT command's
                    # tcflush cannot remove bytes that have not arrived yet --
                    # so they surface as the head of the next reply.  That is
                    # how `ATI3` came back as the fragment `I3` and got
                    # accepted as the Courier's product name.  The pre-flight
                    # identity check refused it, which is the only reason it
                    # was noticed rather than tabulated.
                    quiet = time.time() + 0.25
                    while time.time() < quiet:
                        if select.select([fd], [], [], 0.05)[0]:
                            try:
                                more = os.read(fd, 4096)
                            except OSError:
                                break
                            if more:
                                out += more
                                quiet = time.time() + 0.25
                    break
                end = time.time() + 0.4
    return out.decode(errors="replace")


def lines(text):
    return [s.strip() for s in text.replace("\r", "\n").split("\n") if s.strip()]


# A line of a settings dump rather than a product name: `B0  C1  E1  F1` or
# `&A1  &B0  &C1` from the two modems' ATI4, or anything with an `=` in it
# (`SPEED=115200`).
_SETTINGS = re.compile(r"^(&?[A-Z]\d+\s+){2,}|=")


def identify(fd, timeout):
    """Best-effort product string.  Never fatal -- not every modem has ATI3.

    THE ORDER IS NOT ARBITRARY and neither is the filter.  On the Rockwell,
    `ATI3` is the product ID and gives `Rev 2.000-01 ... SupraExpress 56e PRO`.
    On the USR Courier `ATI3` is the duration of the last call -- `00:00:19` --
    which is a perfectly good reply that identifies nothing, and would have been
    pinned into `modems.conf` as this modem's name had the first non-empty
    answer been taken.  So a candidate must contain a LETTER to count, which
    drops the timer and falls through to `ATI4` (`USRobotics Courier HST Dual
    Standard V.34 ...`) and then `ATI7` (`Product type  UK External MSK`).
    """
    for cmd in ("ATI3", "ATI4", "ATI7", "ATI"):
        # Any suffix of the command is an echo fragment, not a product name:
        # a split read turns `ATI3` into `I3`, which passes every other test.
        echoes = {cmd[i:] for i in range(len(cmd))}
        got = [s for s in lines(command(fd, cmd, timeout))
               if s not in ("OK", "ERROR") and not s.startswith("AT")
               and s not in echoes
               and re.search(r"[A-Za-z]", s) and not _SETTINGS.search(s)]
        if got:
            return (" / ".join(got[:2]))[:90]
    return ""


def line_check(fd, timeout):
    """Is there dial tone on this modem's TELEPHONE line?

    A third failure class, distinct from the two `probe` covers, and it cost a
    whole comparison run: the USR Courier answered `AT` perfectly, identified
    itself, and had no phone line.  The PBX rang extension 1902, `slmodemd` saw
    `180 Ringing`, and the modem heard nothing, because its LINE jack was not
    connected to the ATA.  Five calls of `no connect` and no way to tell that
    from a negotiation failure.

    `ATD` WITH NO DIGITS dials nothing.  It seizes the line, waits for dial
    tone, and reports.  That matters here: the PBX is live and can reach the
    PSTN, and the standing rule is that this bench dials 1901, 1902 and 4242
    and nothing else.  A number-less `ATD` cannot violate that -- there is no
    number.

    Returns (ok, detail).  `X0` modems do not detect dial tone at all and
    answer OK blindly, so an OK is reported as unverified rather than as proof.
    """
    got = command(fd, "ATD", timeout, deadline=20).upper()
    if "NO DIAL" in got:                            # NO DIALTONE / NO DIAL TONE
        return False, "no dial tone -- is the LINE jack connected to the ATA?"
    command(fd, "ATH", timeout)                     # back on hook either way
    if "ERROR" in got:
        return True, "modem refused ATD; line NOT checked"
    # Either a clean result, or it sat off-hook waiting until the deadline --
    # which is what a modem with dial tone and nothing to dial does.  Neither
    # is proof: an X0 modem does not listen for dial tone at all and never
    # complains.  So this is "not contradicted", never "verified".
    return True, "dial tone not contradicted"


def probe(path, timeout):
    """(ok, identity, diagnosis)."""
    if not os.path.exists(path):
        return False, "", "no such device node"
    try:
        fd = open_raw(path)
    except OSError as e:
        return False, "", "cannot open: %s" % e

    try:
        # Nudge the modem out of any half-typed command left by a previous run,
        # then discard whatever that produced.
        os.write(fd, b"\r")
        time.sleep(0.2)
        termios.tcflush(fd, termios.TCIOFLUSH)

        got = command(fd, "AT", timeout)
        if "OK" not in got:
            # Silence is what Q1 (result codes suppressed) looks like, and it
            # survives a power cycle in a stored profile.  One retry, with the
            # smallest command that could fix it.
            command(fd, "ATQ0V1", timeout)
            got = command(fd, "AT", timeout)
        if "OK" not in got:
            if got.strip():
                return False, "", ("no OK; port returned %r"
                                   % got.strip()[:60])
            return False, "", "no response to AT (modem off, or wrong port?)"
        return True, identify(fd, timeout), ""
    finally:
        os.close(fd)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("device")
    ap.add_argument("--timeout", type=float, default=2.0,
                    help="seconds to wait for each reply (default 2)")
    ap.add_argument("--expect", default="",
                    help="substring the identity must contain; a mismatch is "
                         "a FAILURE, because it means the wrong modem is on "
                         "this path")
    ap.add_argument("--line", action="store_true",
                    help="also check for dial tone.  Uses a number-less ATD, "
                         "which seizes the line and dials NOTHING")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    ok, ident, why = probe(args.device, args.timeout)
    if not ok:
        print("atprobe: %s: %s" % (args.device, why), file=sys.stderr)
        return 1
    if args.expect and args.expect.lower() not in ident.lower():
        print("atprobe: %s: answered, but identifies as %r, expected %r"
              % (args.device, ident, args.expect), file=sys.stderr)
        return 1
    if args.line:
        fd = open_raw(args.device)
        try:
            lok, ldetail = line_check(fd, args.timeout)
        finally:
            os.close(fd)
        if not lok:
            print("atprobe: %s: %s" % (args.device, ldetail), file=sys.stderr)
            return 2
        ident = "%s [line: %s]" % (ident, ldetail)
    if not args.quiet:
        print("OK|%s" % ident)
    return 0


if __name__ == "__main__":
    sys.exit(main())
