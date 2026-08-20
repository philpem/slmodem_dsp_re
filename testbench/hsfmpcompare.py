#!/usr/bin/env python3
"""Compare SmartLink V.34 MP/MP' decisions from two HSF interop logs.

The test bench's noisy HSF controls are useful only when the two SmartLink
implementations actually made the same negotiation decision.  This extracts
one compact record for every final-rate decision, including the MP word that
SmartLink sent and whether the next event was a local or a peer retrain.

Usage:
    hsfmpcompare.py blob.sl.log reconstruction.sl.log

Exit status is non-zero when the two logs differ in a completed MP exchange.
It deliberately does *not* compare DFT/MP receive words or equaliser error:
those are sampled signals and may differ slightly while the on-wire proposal
and negotiated configuration are identical.
"""

import argparse
import re
import sys
from dataclasses import dataclass


STAMP = r"<\s*([0-9.]+)>"
RE_CHOICE = re.compile(STAMP +
                       r".*Final choice data rate = (\d+),")
RE_MP = re.compile(STAMP +
                   r".*V34DATARATE, txmp bits (0x[0-9a-f]+),(0x[0-9a-f]+),"
                   r"(0x[0-9a-f]+),(0x[0-9a-f]+),(0x[0-9a-f]+)", re.I)
RE_FINAL = re.compile(STAMP +
                      r".*V34DATARATE, finally txbitrate (\d+),rxbitrate (\d+)")
RE_PEER = re.compile(STAMP + r".*V34RETRAIN, retrain request detected")
RE_LOCAL = re.compile(STAMP + r".*V34RTNCOUNT, triggered")


@dataclass
class Attempt:
    rate: int | None = None
    mp: tuple[str, str, str, str, str] | None = None
    final_at: float | None = None
    tx: int | None = None
    rx: int | None = None
    peer_delay: float | None = None
    local_delay: float | None = None

    def complete(self):
        return self.rate is not None and self.mp is not None and self.final_at is not None


def attempts(path):
    """Return completed final-rate attempts in chronological order."""
    out, current = [], Attempt()
    with open(path, "rb") as fh:
        for raw in fh:
            line = raw.decode("latin-1")
            m = RE_CHOICE.search(line)
            if m:
                # A new decision starts the next exchange.  Keep an incomplete
                # preceding record out of the comparison; it never put an MP
                # proposal on the wire.
                if current.complete():
                    out.append(current)
                current = Attempt(rate=int(m.group(2)))
                continue
            m = RE_MP.search(line)
            if m:
                current.mp = tuple(m.group(i).lower() for i in range(2, 7))
                continue
            m = RE_FINAL.search(line)
            if m:
                current.final_at = float(m.group(1))
                current.tx, current.rx = int(m.group(2)), int(m.group(3))
                continue
            m = RE_PEER.search(line)
            if m and current.final_at is not None and current.peer_delay is None:
                current.peer_delay = float(m.group(1)) - current.final_at
                continue
            m = RE_LOCAL.search(line)
            if m and current.final_at is not None and current.local_delay is None:
                current.local_delay = float(m.group(1)) - current.final_at
    if current.complete():
        out.append(current)
    return out


def fmt(a):
    link = "%s/%s" % (a.tx, a.rx)
    events = []
    if a.peer_delay is not None:
        events.append("peer +%.3fs" % a.peer_delay)
    if a.local_delay is not None:
        events.append("local +%.3fs" % a.local_delay)
    when = ", ".join(events) if events else "-"
    return "rate-index %-2d  MP %-34s link %-11s %s" % (
        a.rate, ",".join(a.mp), link, when)


def same(a, b):
    # The exact on-wire fields and configuration are the discriminator.  The
    # local stillness counter is bench instrumentation that the blob does not
    # emit, so it is reported but is not a protocol comparison.  A peer retrain
    # may be logged a few samples apart, hence 20 ms tolerance.
    if (a.rate, a.mp, a.tx, a.rx, a.peer_delay is not None) != \
       (b.rate, b.mp, b.tx, b.rx, b.peer_delay is not None):
        return False
    if a.peer_delay is not None and abs(a.peer_delay - b.peer_delay) > 0.020:
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("left", help="first SmartLink -d9 log (normally blob)")
    ap.add_argument("right", help="second SmartLink -d9 log (normally reconstruction)")
    args = ap.parse_args()
    left, right = attempts(args.left), attempts(args.right)

    print("V.34 MP/MP' comparison")
    print("  left : %s (%d complete exchanges)" % (args.left, len(left)))
    print("  right: %s (%d complete exchanges)\n" % (args.right, len(right)))
    if not left or not right:
        print("REFUSING: no completed MP exchange in one input.")
        return 2

    ok = len(left) == len(right)
    for i, (a, b) in enumerate(zip(left, right), 1):
        verdict = "MATCH" if same(a, b) else "DIFFER"
        print("  attempt %d: %s" % (i, verdict))
        print("    left  " + fmt(a))
        print("    right " + fmt(b))
        ok = ok and verdict == "MATCH"
    if len(left) != len(right):
        print("  exchange count differs; unmatched tail is not silently ignored.")
    print("\n%s" % ("MATCH" if ok else "DIFFER"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
