#!/usr/bin/env python3
"""
Interrogate the modem on a serial port: identity, capabilities, and the
settings that decide whether it will negotiate V.8 at all.

Nothing here dials.  The port is opened ONCE and held, so nothing the modem
prints is lost and DTR never drops mid-sequence.
"""
import os
import select
import subprocess
import sys
import termios
import time

# Resolve through modems.sh so a role name works and ttyUSBn renumbering
# cannot silently point this at the other modem.
if len(sys.argv) > 1:
    PORT = sys.argv[1]
else:
    PORT = subprocess.run(
        [os.path.join(os.path.dirname(os.path.abspath(__file__)), "modems.sh"),
         "resolve", "supra"],
        capture_output=True, text=True).stdout.strip() or "/dev/ttyUSB0"

QUERIES = [
    ("ATI0", "product code"),
    ("ATI1", "checksum"),
    ("ATI2", "self test"),
    ("ATI3", "firmware"),
    ("ATI4", "capabilities"),
    ("ATI5", "country/profile"),
    ("ATI6", "modem data pump"),
    ("ATI7", "manufacturer"),
    ("AT+MS?", "current modulation selection"),
    ("AT+MS=?", "SUPPORTED modulations -- V.8 needs V.34 or V.90 here"),
    ("AT+A8E?", "V.8 configuration (if the chipset implements it)"),
    ("AT+A8E=?", "V.8 configuration, permitted values"),
    ("ATS27?", "S27: modulation/handshake control on some chipsets"),
    ("ATS32?", "S32"),
    ("ATS109?", "S109: line rate/modulation on Rockwell parts"),
    ("AT&V", "ACTIVE PROFILE -- the whole configuration"),
]


def main():
    fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0
    a[1] = 0
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    a[3] = 0
    a[4] = a[5] = termios.B115200
    a[6][termios.VMIN] = 0
    a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)

    def ask(cmd, secs=1.5):
        os.write(fd, (cmd + "\r").encode())
        out = b""
        end = time.time() + secs
        while time.time() < end:
            if select.select([fd], [], [], 0.2)[0]:
                try:
                    c = os.read(fd, 4096)
                except OSError:
                    break
                if c:
                    out += c
                    end = time.time() + 0.4      # keep reading while it talks
        return out.decode(errors="replace")

    ask("ATZ", 2.0)
    ask("ATE0", 1.0)                              # echo off, so the reply is the reply
    for cmd, why in QUERIES:
        r = ask(cmd, 2.5 if cmd == "AT&V" else 1.5)
        lines = [l.strip() for l in r.replace("\r", "\n").split("\n") if l.strip()]
        print("\n%-10s %s" % (cmd, why))
        for l in lines:
            print("    %s" % l)
    os.close(fd)


main()
