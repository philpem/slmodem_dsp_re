#!/usr/bin/env python3
"""exectrace.py -- which UNWRITTEN symbols does a real call actually execute?

    tools/exectrace.py build/test/t_v34link VPcmV34Progress
    tools/exectrace.py build/test/t_v34link VPcmV34Progress --all

WHY A STATIC CLOSURE IS THE WRONG ANSWER TO THIS QUESTION.

`tools/closure.py <entry> --missing` answers "what could this reach that
nobody has written", and for a dispatcher it over-answers enormously.
`VPcmV34Progress` is the progress entry for the whole V.PCM family, so its
closure is 168 symbols and 145,706 bytes -- and 97% of that is the V.90 and
V.92 arms, which a 33,600 V.34 call never enters.  `tools/callgraph.py` is no
better here and for the same reason: it reads R_386_PC32 relocations, so it
is reachability too.  No static tool can tell the taken arm of a switch from
the untaken one.

So run the call and see.  A test that drives a real connection is the
specification of what that connection needs; everything else in the closure
is a different protocol's problem.

HOW, AND THE TRAP THAT COST A WRONG ANSWER FIRST TIME.

Callgrind, then map executed addresses back through the binary's symbol table
-- the blob's symbols are `ref_*` in a differential binary, so anything with
that prefix is code we have not written unless our own copy is linked too.

**`--dump-instr=yes` IS NOT OPTIONAL.**  Callgrind's default output records
CALL instructions, and GCC 3.4 tail-calls heavily: a function entered by
`jmp` never appears, its cost charged to whoever jumped.  The first run of
this analysis used the default and saw 141 blob functions; with instruction
dumping the same call shows 282.  It happened to reach the same conclusion,
which is worse than failing -- a validation is what caught it.

WHICH IS WHY --check EXISTS.  Name symbols the call MUST run and symbols it
must NOT, and refuse to report until both hold.  `v34handshak` and `receiver`
were the ones that exposed the tail-call problem: a V.34 connect cannot
happen without them, and the first trace did not see either.

WHAT IT DOES NOT TELL YOU.  One run of one test over one wire.  A path the
test never provokes -- a retrain, a rate renegotiation, a different symbol
rate -- is unwritten work this will not list.  The output is a floor, and a
tight one; it is not a promise.
"""

import argparse
import bisect
import os
import re
import subprocess
import sys
import tempfile


def symbol_table(binary):
    out = subprocess.run(["nm", "--defined-only", binary],
                         capture_output=True, text=True).stdout
    syms = sorted((int(p[0], 16), p[2])
                  for p in (l.split() for l in out.splitlines())
                  if len(p) == 3 and p[1] in "tT")
    if not syms:
        sys.exit("no text symbols in %s -- stripped?" % binary)
    return syms


def executed_symbols(binary, keep=None):
    syms = symbol_table(binary)
    addrs = [a for a, _ in syms]
    out = keep or os.path.join(tempfile.mkdtemp(), "cg.out")
    r = subprocess.run(["valgrind", "--tool=callgrind", "--dump-instr=yes",
                        "--collect-jumps=yes", "--cache-sim=no",
                        "--branch-sim=no", "--callgrind-out-file=" + out,
                        binary], capture_output=True, text=True)
    if not os.path.exists(out):
        sys.exit("callgrind produced nothing:\n%s" % r.stderr[-800:])
    seen = set()
    for line in open(out):
        m = re.match(r"^0x([0-9a-fA-F]+)\s", line)
        if not m:
            continue
        i = bisect.bisect_right(addrs, int(m.group(1), 16)) - 1
        if i >= 0:
            seen.add(syms[i][1])
    return seen


def unwritten_closure(entry):
    out = subprocess.run(["python3", "tools/closure.py", entry, "--missing"],
                         capture_output=True, text=True).stdout
    return {s: int(b) for b, s in re.findall(r"^\s+\*\s+(\d+)\s+(\S+)$",
                                             out, re.M)}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("binary", help="a differential test that drives a call")
    ap.add_argument("entry", help="the entry point whose closure to filter")
    ap.add_argument("--all", action="store_true",
                    help="also list what was NOT executed")
    ap.add_argument("--must", action="append", default=[],
                    help="symbol the call must run; repeatable")
    ap.add_argument("--must-not", action="append", default=[],
                    help="symbol the call must NOT run; repeatable")
    ap.add_argument("--keep", metavar="PATH", help="keep the callgrind output")
    args = ap.parse_args()

    seen = executed_symbols(args.binary, args.keep)
    blob = {n[4:] for n in seen if n.startswith("ref_")}
    print("executed: %d functions, %d of them the blob's" % (len(seen), len(blob)))

    bad = False
    for s in args.must:
        ok = s in blob or s in seen
        print("  MUST run      %-40s %s" % (s, "seen" if ok else "NOT SEEN"))
        bad |= not ok
    for s in args.must_not:
        ok = s not in blob and s not in seen
        print("  MUST NOT run  %-40s %s" % (s, "absent" if ok else "SEEN"))
        bad |= not ok
    if bad:
        sys.exit("\n  the trace failed its own check -- do not use this list.\n"
                 "  a missing MUST is usually tail calls: is --dump-instr on?")

    unw = unwritten_closure(args.entry)
    hit = {s: b for s, b in unw.items() if s in blob}
    miss = {s: b for s, b in unw.items() if s not in blob}
    print("\nunwritten in %s's closure : %3d  %7d bytes"
          % (args.entry, len(unw), sum(unw.values())))
    print("  EXECUTED by this call%s: %3d  %7d bytes"
          % (" " * max(0, len(args.entry) - 8), len(hit), sum(hit.values())))
    print("  never entered%s: %3d  %7d bytes"
          % (" " * max(0, len(args.entry) + 8), len(miss), sum(miss.values())))
    print("\nTHE WORK LIST -- executed here and not written:")
    for s, b in sorted(hit.items(), key=lambda kv: -kv[1]):
        print("  %7d  %s" % (b, s))
    if args.all:
        print("\nNOT entered by this call (a different protocol's arm):")
        for s, b in sorted(miss.items(), key=lambda kv: -kv[1]):
            print("  %7d  %s" % (b, s))
    return 0


if __name__ == "__main__":
    sys.exit(main())
