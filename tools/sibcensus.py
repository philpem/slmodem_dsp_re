#!/usr/bin/env python3
"""Pairwise sibling-call census: where does the blob tail-call and we do not?

WHY THIS EXISTS

`docs/method/refinement.md` lever 7 turns on one instruction.  At a
destructor's LAST free the blob makes an ordinary `call sysdep_free`; an
explicit `if (p != 0) sysdep_free(p)` makes a sibling `jmp`.  `delete[]` is
the only spelling of eight that reproduces the object (finding 7786), so
knowing WHICH functions disagree is what tells you where the lever can pay.

A RAW POPULATION RATIO IS THE WRONG MEASUREMENT.  The two objects define
different symbol sets, so "the blob sibcalls out of 4 functions and we out of
50" mixes "the blob does not sibcall here" with "the blob has no such
function".  The comparison has to be PAIRWISE over the symbols both objects
define -- which is `byteident.py`'s own denominator -- and it has to report
that denominator, because a detector that cannot be seen to fire is
indistinguishable from a broken one (findings 134, 2400, 2401).

TWO THINGS THIS GETS RIGHT THAT ARE EASY TO GET WRONG

1.  **A function's span is its `nm -S` size, not its objdump block.**  GCC pads
    between functions with `jmp <next symbol>` plus nops, and that padding sits
    inside the symbol's disassembly block.  Counting it reads alignment as a
    tail call -- the same artefact that invented a call edge in refinement.md
    lever 3a.

2.  **An indirect `jmp *TABLE(,%eax,4)` is a switch, not a tail call**, and it
    carries an `R_386_32` relocation against `.rodata` exactly as a relocated
    direct jump carries one against its callee.  The first version of this
    script rewrote the operand from the relocation before testing for the `*`,
    and reported SEVEN switch statements as sibling calls: `TimingV34`,
    `setfinalrate` and `v90Phase34` on our side, `B103FP_modem`,
    `RcFixed_Create`, `cadence_create` and `v8_process` on the blob's.  That
    inflated both disagreement columns by five.

Usage:
    tools/sibcensus.py                 the three counts
    tools/sibcensus.py --detail        every disagreeing symbol, both call sets
    tools/sibcensus.py --sym SYMBOL    one symbol, both sides
    tools/sibcensus.py --self-test     prove it accepts AND rejects
"""
import argparse
import glob
import os
import re
import subprocess
import sys


def _default_blob():
    d = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                       capture_output=True, text=True).stdout.strip()
    if d:
        return os.path.join(os.path.dirname(os.path.abspath(d)),
                            "ref", "slmodemd", "dsplibs.o")
    return os.path.join("ref", "slmodemd", "dsplibs.o")


BLOB = os.environ.get("BLOB") or _default_blob()
OURS = os.environ.get("TC_OUT", "build/tc_out")

HDR = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:")
INSN = re.compile(r"^\s*([0-9a-f]+):\t(?:[0-9a-f ]+\t)?\s*(\S+)\s*(.*)$")
RELOC = re.compile(r"^\s*([0-9a-f]+):\s+(R_386_\w+)\s+(\S+)")
TARGET = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>$")

#
# A sentinel address no function's span can contain, so a relocated target is
# never mistaken for an intra-function branch.
#
FAR = "7fffffff"


def nm_sizes(path):
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def scan(path):
    """{sym: {'sib': set(names), 'call': set(names)}} for one object."""
    sizes = nm_sizes(path)
    out = subprocess.run(["objdump", "-dr", "--no-show-raw-insn", path],
                         capture_output=True, text=True).stdout
    res, cur, rows = {}, None, []

    def flush():
        if cur is None:
            return
        name, start, end = cur
        sib, calls = set(), set()
        for addr, mnem, ops in rows:
            if addr >= end:
                continue                      # alignment padding past nm's size
            m = TARGET.match(ops.strip())
            if not m:
                continue
            tgt = m.group(2).split("+")[0].split("-")[0]
            if mnem == "jmp":
                if start <= int(m.group(1), 16) < end:
                    continue                  # intra-function branch
                sib.add(tgt)
            elif mnem == "call":
                calls.add(tgt)
        r = res.setdefault(name, {"sib": set(), "call": set()})
        r["sib"] |= sib
        r["call"] |= calls

    for line in out.splitlines():
        m = HDR.match(line)
        if m:
            flush()
            rows = []
            name, start = m.group(2), int(m.group(1), 16)
            sz = sizes.get(name)
            cur = (name, start, start + sz) if sz is not None else None
            continue
        if cur is None:
            continue
        m = RELOC.match(line)
        if m and rows:
            addr, mnem, ops = rows[-1]
            if mnem in ("jmp", "call"):
                ops = "%s <%s>" % (FAR, m.group(3))
            rows[-1] = (addr, mnem, ops)
            continue
        m = INSN.match(line)
        if m:
            mnem = m.group(2)
            ops = m.group(3).split("#", 1)[0].strip()
            #
            # RENAME AN INDIRECT JUMP HERE, before the relocation handler above
            # can turn its `.rodata` table reference into a target.  See the
            # module docstring: doing it afterwards scored seven switches as
            # tail calls.
            #
            if mnem == "jmp" and ops.startswith("*"):
                mnem = "jmp*"
            rows.append((int(m.group(1), 16), mnem, ops))
    flush()
    return res


def ours_scan():
    res = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in scan(o).items():
            res.setdefault(k, v)
    return res


def self_test(blob, ours):
    """Prove the detector both FIRES and STAYS QUIET, on named symbols.

    Finding 134's argument: `extcheck` printed "(none)" through four broken
    versions and nobody could tell a clean tree from a dead detector.  Two of
    these three cases are things this must NOT report.
    """
    cases = [
        # symbol, blob sibcalls?, we sibcall?, why it is here
        ("_ZN3PsdD1Ev", False, False,
         "closed by wave 4b -- both sides plain `call`, must read AGREE"),
        ("V92deleteConstellations", False, True,
         "open -- we sibcall and the blob calls, must read DISAGREE"),
        ("v90Phase34", False, False,
         "a switch, `jmp *TABLE(,%eax,4)` against .rodata; NOT a tail call"),
    ]
    bad = 0
    for sym, want_b, want_o, why in cases:
        if sym not in blob or sym not in ours:
            print("  SKIP  %-28s not defined by both objects" % sym)
            bad += 1
            continue
        got_b, got_o = bool(blob[sym]["sib"]), bool(ours[sym]["sib"])
        ok = (got_b, got_o) == (want_b, want_o)
        bad += 0 if ok else 1
        print("  %-5s %-28s blob %-5s ours %-5s   %s"
              % ("ok" if ok else "FAIL", sym, got_b, got_o, why))
    print("\n  sibcensus self-test: %d case(s), %d failed" % (len(cases), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--detail", action="store_true",
                    help="every disagreeing symbol, with BOTH call sets")
    ap.add_argument("--sym", metavar="SYMBOL", help="one symbol, both sides")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the detector fires AND stays quiet")
    a = ap.parse_args()

    blob = scan(BLOB)
    if not blob:
        sys.exit("sibcensus.py: NO SYMBOLS read from the blob at %s.  Every\n"
                 "  count below would be computed against NOTHING.  From a\n"
                 "  worktree, BLOB must be explicit.  Findings 2400, 2401."
                 % BLOB)
    ours = ours_scan()
    if not ours:
        sys.exit("sibcensus.py: no objects in %s -- run "
                 "tools/toolchain/build.sh first." % OURS)
    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit("sibcensus.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % OURS)

    if a.self_test:
        return self_test(blob, ours)

    if a.sym:
        for side, d in (("blob", blob), ("ours", ours)):
            r = d.get(a.sym)
            print("%s %s:\n    sib  : %s\n    call : %s"
                  % (side, a.sym,
                     ",".join(sorted(r["sib"])) if r else "(not defined)",
                     ",".join(sorted(r["call"])) if r else "(not defined)"))
        return 0

    agree, we_only, blob_only = [], [], []
    for k in common:
        b, o = bool(blob[k]["sib"]), bool(ours[k]["sib"])
        (agree if b == o else we_only if o else blob_only).append(k)

    sf = [k for k in we_only if "sysdep_free" in ours[k]["sib"]]
    print("  blob : %s" % BLOB)
    print("  ours : %s\n" % OURS)
    print("Sibling calls over %d symbol(s) both objects define.\n" % len(common))
    print("  agree on whether the function sibcalls : %4d" % len(agree))
    print("  WE sibcall, the blob does not          : %4d" % len(we_only))
    print("     of those, the target is sysdep_free : %4d" % len(sf))
    print("  the BLOB sibcalls and we do not        : %4d" % len(blob_only))

    if a.detail:
        for title, group, mine in (
                ("WE SIBCALL, THE BLOB DOES NOT", we_only, True),
                ("THE BLOB SIBCALLS AND WE DO NOT", blob_only, False)):
            print("\n=== %s (%d) ===" % (title, len(group)))
            for k in sorted(group):
                src = ours[k] if mine else blob[k]
                print("  %s" % k)
                print("      sib(%s) : %s" % ("ours" if mine else "blob",
                                              ",".join(sorted(src["sib"])) or "-"))
                print("      calls blob: %s" % (",".join(sorted(blob[k]["call"])) or "-"))
                print("      calls ours: %s" % (",".join(sorted(ours[k]["call"])) or "-"))
                #
                # A call the BLOB makes and we do not is an ABSENCE in our
                # body, and no change of free spelling reaches it.  Printing
                # only one side hides exactly the sites where `delete[]` would
                # be a fit over something missing.
                #
                extra = sorted(blob[k]["call"] - ours[k]["call"] - ours[k]["sib"])
                if extra:
                    print("      >> BLOB CALLS WE DO NOT MAKE: %s" % ",".join(extra))
    return 0


if __name__ == "__main__":
    sys.exit(main())
