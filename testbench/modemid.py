#!/usr/bin/env python3
"""modemid.py -- ask a modem what it is, and derive its AT dialect from that.

    modemid.py /dev/serial/by-id/usb-...    # kind, init string, diagnostic cmd
    modemid.py <port> --field init

WHY ASK RATHER THAN LOOK IT UP.  The bench identifies modems through
`/dev/serial/by-id`, which names the **USB-RS232 adapter**, not the modem
plugged into it.  Move a modem to a different adapter and it silently inherits
that adapter's role, along with whatever AT dialect the table says that role
speaks.  Asking the modem removes the indirection: the answer comes from the
device that has to execute the commands.

WHAT GOES WRONG WHEN THE DIALECT IS ASSUMED.  `AT&A3` and `AT&B1` are
USRobotics commands.  Sent to the Oli'Net they return ERROR, and a harness that
ignores the ERROR proceeds with the modem in a state it did not choose.  The
link diagnostic is worse, because it fails silently: `ATI11` on a USR prints a
full link report, and on the Conexant it prints the product name and OK.  Four
Oli'Net calls were recorded with an empty far-end rate for exactly that reason
-- not a missing measurement, a measurement taken with the wrong command.

THE DIALECTS, and what identifies each:

  usr        ATI7 contains "Configuration Profile" / "Product type"
             init  AT&F;AT&A3;AT&B1      diag  ATI11   (link diagnostics)
  conexant   ATI3/ATI7 contains "V92 Ready", or ATI0 reports "V5.0"
             init  AT&F                  diag  AT&V1   (last-call report)
  rockwell   ATI7 contains "RCV" (e.g. RCV56DPF-PLL)
             init  AT&F                  diag  AT&V1
  unknown    init  AT&F                  diag  ATI11   -- the safe subset

`AT&F` ALONE IS THE SAFE INIT and it must come FIRST.  The harness sets
`ATS0=1` for autoanswer before sending the init string, so an `AT&F` anywhere
but the front resets S0 to its factory value and the modem never answers.
"""

import argparse
import sys
import time

DIALECTS = {
    "usr":      {"init": "AT&F;AT&A3;AT&B1", "diag": "ATI11"},
    "conexant": {"init": "AT&F",             "diag": "AT&V1"},
    "rockwell": {"init": "AT&F",             "diag": "AT&V1"},
    "unknown":  {"init": "AT&F",             "diag": "ATI11"},
}


def classify(replies):
    """replies: {command: text}. Order matters -- USR is checked first because
    its ATI7 is unmistakable, and a Rockwell part number can appear in a modem
    whose command set is someone else's."""
    blob = " ".join(replies.values())
    if "Configuration Profile" in blob or "Product type" in blob:
        return "usr"
    if "V92 Ready" in blob or "V5.0" in blob:
        return "conexant"
    if "RCV" in blob:
        return "rockwell"
    return "unknown"


def probe(port, timeout=1.0):
    try:
        import serial
    except ImportError:
        sys.exit("modemid: pyserial not installed")
    try:
        s = serial.Serial(port, 115200, timeout=timeout)
    except Exception as e:
        sys.exit("modemid: cannot open %s: %s" % (port, e))
    try:
        s.write(b"\r")
        time.sleep(0.3)
        s.reset_input_buffer()
        out = {}
        for cmd in ("ATI0", "ATI3", "ATI7"):
            s.write(cmd.encode() + b"\r")
            time.sleep(1.0)
            raw = s.read(s.in_waiting or 1).decode("latin-1", "replace")
            out[cmd] = " ".join(x.strip() for x in raw.splitlines()
                                if x.strip() and x.strip() != cmd)
        return out
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("port")
    ap.add_argument("--field", choices=("kind", "init", "diag", "all"),
                    default="all")
    args = ap.parse_args()

    replies = probe(args.port)
    kind = classify(replies)
    d = DIALECTS[kind]

    if args.field == "kind":
        print(kind)
    elif args.field in ("init", "diag"):
        print(d[args.field])
    else:
        print("kind %s" % kind)
        print("init %s" % d["init"])
        print("diag %s" % d["diag"])
        for c, t in replies.items():
            print("  %-5s %s" % (c, t[:90]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
