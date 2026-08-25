#!/usr/bin/env python3
"""lastlink.py -- ask a modem what its last connection actually was.

    lastlink.py supra
    lastlink.py /dev/serial/by-id/usb-FTDI_... --label originator

WHY.  Every measurement on this bench so far has been made from OUTSIDE the
modem -- the result codes it happened to print, and the audio.  Both modems
keep a far better record than that and nobody has ever read it:

    ATI6   last-connection report.  On a Rockwell: rate, protocol, retrains,
           and the line statistics it trained on.  On a USR Courier: the link
           diagnostics block, which lists RECEIVE and TRANSMIT rates
           SEPARATELY -- and an asymmetric V.34 link is exactly what this
           bench is trying to measure (finding F1466).
    ATI4   the active configuration on a Rockwell; the stored profile on a USR
    ATS86? Rockwell only: why the last call ended.  0 normal, 4 loss of
           carrier, 5 V.42 negotiation failed, 9 no common protocol, 12 the
           remote hung up, 13 no response after 10 retransmissions, 14
           protocol violation.  (AT reference p. 4-20.)
    ATS91? Rockwell only: PSTN transmit attenuation, never yet read here.

TIMING IS THE WHOLE TRICK.  All of these describe the LAST call, and all of
them are cleared or overwritten by the next one.  `ATZ` and `AT&F` reload a
profile and lose the lot.  So this runs AFTER the call has dropped and BEFORE
anything resets the modem -- `call.py` tears down with `+++` and `ATH`, which
leaves them intact, and that is the window.

A COMMAND THAT ERRORS IS INFORMATION, NOT A FAILURE.  The Courier is a USR and
has no S86; the Supra has no USR link-diagnostics block.  Each is asked for
everything and the ERRORs are printed as ERRORs, because a bench that silences
them is how `AT+MS=B103` went unnoticed for a session.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from atprobe import open_raw, command                      # noqa: E402

QUERIES = ["ATI6", "ATI4", "ATI3", "ATS86?", "ATS91?", "ATI11", "AT&V1"]
#
# AT&V1 IS THE CONEXANT/ROCKWELL LINK REPORT and it is the one that matters
# on anything that is not a USR.  ATI11 does not error on the Conexant -- it
# prints the product name and OK -- so its absence from this list looked like
# a modem with no diagnostics rather than a query we never sent.  &V1 gives
# TERMINATION REASON, LAST/HIGHEST TX and RX rate, which caught a link
# reaching 19200 and collapsing to 4800 on a retrain failure while the
# CONNECT string said 14400 (finding F1916).



def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("port", help="a /dev path, or a role name resolved by hand")
    ap.add_argument("--label", default="")
    ap.add_argument("--only", help="comma-separated subset of the queries")
    args = ap.parse_args()

    queries = args.only.split(",") if args.only else QUERIES
    fd = open_raw(args.port)
    try:
        for q in queries:
            try:
                out = command(fd, q + "\r", 0.6, deadline=6.0)
            except Exception as e:                          # noqa: BLE001
                out = "(%s)" % e
            body = "\n".join("        " + l for l in
                             str(out).replace("\r", "\n").split("\n")
                             if l.strip())
            print("    %s %s" % (args.label, q))
            print(body if body.strip() else "        (no reply)")
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
