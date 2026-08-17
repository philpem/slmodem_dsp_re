#!/usr/bin/env python3
"""
Partition one large function into the code each dispatch case owns.

WHY THIS EXISTS

`v34handshak` is 61,541 bytes -- one function holding a state machine with 87
named states and three jump tables.  Planning the reconstruction needs to know
what each state actually costs, and the obvious estimate is wrong in a way that
looks right: sort the jump-table targets and call the distance to the next one
the block size.  That silently hands every byte between the last target and the
end of the function to whichever state happens to sit highest, and it assumes a
compiler lays each case out contiguously, which GCC does not.  Done three ways
here it produced three different 45-55 KB "states", none of which exist.

What a case owns has to come from the control-flow graph:

  exclusive   blocks reachable from this dispatch target and no other
  shared      blocks reachable from two or more -- the common machinery,
              which is a module in its own right and not any state's cost
  unreached   blocks no dispatch target reaches: the prologue, the epilogue,
              and anything the tables do not cover

The shared figure is the one that decides the shape of the work.  If a phase's
states are thin drivers over a large shared engine, the engine is the subtask
and the states are an afternoon; if each state owns its code outright, the
split is per state.  Those are different plans and the distance-to-next-target
estimate cannot tell them apart.

HOW IT FINDS THE TABLES

An indirect jump `jmp *0x2da0(,%eax,4)` carries a relocation naming the section
the table lives in, and the two instructions before it are the range check the
compiler emits: `sub $0x5,%eax` then `cmp $0x51,%eax`, giving both the first
case value and how many entries to read.  So the tables need not be supplied by
hand.  Pass --table to override when the pattern does not match.

Usage:
    cfgsplit.py --func v34handshak
    cfgsplit.py --func v34handshak --states .data:0x6c00:87
    cfgsplit.py --func FOO --table .rodata:0x2da0:82:5
"""

import argparse
import re
import struct
import subprocess
import sys
import tempfile

INSN = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*(\S+)\s*(.*)$")
#
# AND ITS CONTINUATION.  objdump wraps the hex column at SEVEN bytes and puts
# the remainder on a line of its own -- an address, a tab, and bytes, with no
# mnemonic:
#
#     65486:	66 83 bf 92 35 00 00 	cmpw   $0x34,0x3592(%edi)
#     6548d:	34
#
# `INSN` cannot match that -- it requires a mnemonic -- so before this regex
# existed those bytes were DISCARDED and the instruction was recorded seven
# bytes long instead of eight.  131 lines in `v34handshak` alone, and the
# report said so on every run and was read as a remark:
#
#     61410 bytes accounted of 61541
#
# Every walk-derived byte count in this tree was therefore a LOWER BOUND,
# which is the likeliest source of two corrections made by hand against
# address arithmetic (findings 719 and 727).  See `--selftest`.
CONT = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2}\s*)+)$")
RELOC = re.compile(r"^\s+([0-9a-f]+):\s+(R_386_\S+)\s+(\S+)")
TARGET = re.compile(r"^([0-9a-f]+)\s")
INDIRECT = re.compile(r"^\*(0x[0-9a-f]+)\(,%e[a-z]{2},([0-9])\)")

# Anything that ends a basic block.  `call` does not: it comes back.
UNCOND = ("jmp", "ret", "retl", "hlt", "ud2", "leave.ret")
COND = re.compile(r"^j(?!mp$)[a-z]+$")


def section_bytes(obj, section):
    with tempfile.NamedTemporaryFile(suffix=".bin") as tmp:
        subprocess.run(["objcopy", "-O", "binary", "--only-section=" + section,
                        obj, tmp.name], check=True, capture_output=True)
        with open(tmp.name, "rb") as fh:
            return fh.read()


def symbol(obj, name):
    out = subprocess.run(["nm", "-S", "--defined-only", obj],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[3] == name:
            return int(f[0], 16), int(f[1], 16)
    return None, None


class Insn(object):
    __slots__ = ("addr", "size", "mnem", "operand", "reloc")

    def __init__(self, addr, size, mnem, operand):
        self.addr = addr
        self.size = size
        self.mnem = mnem
        self.operand = operand
        self.reloc = None


def disassemble(obj, lo, hi):
    out = subprocess.run(["objdump", "-dr", "-j", ".text",
                          "--start-address=0x%x" % lo,
                          "--stop-address=0x%x" % hi, obj],
                         capture_output=True, text=True).stdout
    insns = []
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            raw = m.group(2).replace(" ", "")
            insns.append(Insn(int(m.group(1), 16), len(raw) // 2,
                              m.group(3), m.group(4).strip()))
            continue
        m = CONT.match(line)
        if m and insns:
            # The tail of the instruction above, not an instruction of its own.
            continue
        m = RELOC.match(line)
        if m and insns:
            insns[-1].reloc = m.group(3)
    #
    # SIZE FROM ADDRESS DELTAS, not from the bytes objdump printed.  Parsing
    # the hex column cannot be made reliable: objdump wraps it at seven bytes
    # and the continuation line carries no mnemonic, so a regex that demands
    # one drops the tail and calls an eight-byte instruction seven bytes long.
    # Recovering the wrapped bytes by hand got 109 of the 131 that
    # `v34handshak` loses and still left 22 -- the encodings vary and chasing
    # them is fitting the formatter.
    #
    # The distance to the next instruction is what "size" MEANS for this
    # tool's accounting, it needs no formatting assumption at all, and it is
    # exact by construction.  `--selftest` is the demonstration.
    #
    for i in range(len(insns) - 1):
        gap = insns[i + 1].addr - insns[i].addr
        if gap > 0:
            insns[i].size = gap
    return insns


def direct_target(insn, lo, hi):
    m = TARGET.match(insn.operand)
    if not m:
        return None
    addr = int(m.group(1), 16)
    return addr if lo <= addr < hi else None


def find_tables(obj, insns, lo, hi):
    """[(offset, count, first_case, section)] for each indirect dispatch."""
    tables = []
    for i, insn in enumerate(insns):
        if insn.mnem != "jmp":
            continue
        m = INDIRECT.match(insn.operand)
        if not m or insn.reloc is None:
            continue
        off = int(m.group(1), 16)
        count = base = None
        for back in insns[max(0, i - 8):i]:
            if back.mnem in ("cmp", "cmpl") and back.operand.startswith("$"):
                count = int(back.operand.split(",")[0][1:], 16) + 1
            elif back.mnem == "sub" and back.operand.startswith("$"):
                base = int(back.operand.split(",")[0][1:], 16)
        if count is None:
            print("cfgsplit: no range check before the jump at 0x%x" % insn.addr,
                  file=sys.stderr)
            continue
        tables.append((off, count, base or 0, insn.reloc))
    return tables


def read_table(obj, section, off, count):
    raw = section_bytes(obj, section)
    return [struct.unpack_from("<I", raw, off + 4 * i)[0] for i in range(count)]


def build_blocks(insns, leaders, lo, hi):
    """{start: (size, [successors])}"""
    index = {insn.addr: i for i, insn in enumerate(insns)}
    starts = sorted(leaders)
    blocks = {}
    for j, start in enumerate(starts):
        end = starts[j + 1] if j + 1 < len(starts) else hi
        i = index.get(start)
        if i is None:
            continue
        size = 0
        succ = []
        while i < len(insns) and insns[i].addr < end:
            insn = insns[i]
            size += insn.size
            nxt = insn.addr + insn.size
            if insn.mnem in UNCOND or insn.mnem.startswith("ret"):
                t = direct_target(insn, lo, hi)
                succ = [t] if t is not None else []
                break
            if COND.match(insn.mnem):
                t = direct_target(insn, lo, hi)
                succ = ([t] if t is not None else []) + \
                       ([nxt] if nxt < hi else [])
                break
            i += 1
        else:
            succ = [end] if end < hi else []
            blocks[start] = (size, succ)
            continue
        if i < len(insns) and insns[i].addr + insns[i].size < end:
            # the block was cut short by a branch; the rest is dead bytes
            # (alignment padding) and belongs to nobody
            pass
        blocks[start] = (size, [s for s in succ if s is not None])
    return blocks


def reachable(blocks, entry):
    seen = set()
    stack = [entry]
    while stack:
        b = stack.pop()
        if b in seen or b not in blocks:
            continue
        seen.add(b)
        stack.extend(blocks[b][1])
    return seen


def selftest(obj):
    """
    Two claims, both demonstrated rather than asserted.

    1. THE ACCOUNTING IS COMPLETE.  Every byte of the range is attributed.
    2. THE CHECK CAN FAIL.  With sizes taken from objdump's hex column again
       -- the pre-fix behaviour -- `v34handshak` loses 131 bytes and the
       check reports it.  Without this half, a clean run proves nothing:
       `extcheck` printed "(none)" through four broken versions and there was
       no way to tell a clean tree from a dead detector (finding 134).
    """
    ok = True
    for func in ("v34handshak", "receiver", "v34handshakinit"):
        lo, sz = symbol(obj, func)
        if lo is None:
            print("  SKIP  %-18s not defined in the object" % func)
            continue
        hi = lo + sz
        insns = disassemble(obj, lo, hi)
        total = sum(i.size for i in insns)
        good = total == hi - lo
        ok &= good
        print("  %-4s  %-18s %6d of %6d bytes"
              % ("ok" if good else "FAIL", func, total, hi - lo))

    # The negative control: parse sizes the old way and watch the gap appear.
    lo, sz = symbol(obj, "v34handshak")
    hi = lo + sz
    out = subprocess.run(["objdump", "-dr", "-j", ".text",
                          "--start-address=0x%x" % lo,
                          "--stop-address=0x%x" % hi, obj],
                         capture_output=True, text=True).stdout
    old_total = 0
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            old_total += len(m.group(2).replace(" ", "")) // 2
    lost = (hi - lo) - old_total
    print("  %-4s  %-18s %6d of %6d bytes  <- the defect, reproduced"
          % ("ok" if lost > 0 else "FAIL", "(hex column)", old_total, hi - lo))
    if lost <= 0:
        print("\n  ERROR: the negative control did not fail, so a pass above\n"
              "  is not evidence of anything.")
        ok = False
    else:
        # Bytes, not lines: the line count would come from CONT, and CONT's
        # coverage is exactly what could not be made reliable.  The loss is
        # measured against the symbol's own size, which needs no regex.
        print("\n  %d bytes were being dropped." % lost)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obj", default="ref/slmodemd/dsplibs.o")
    ap.add_argument("--func")
    ap.add_argument("--selftest", action="store_true",
                    help="prove the byte accounting is complete, and prove "
                         "the check that says so can FAIL. A tool nobody has "
                         "seen fire is not a tool -- gates.md rule 3.")
    ap.add_argument("--states", metavar="SEC:OFF:N",
                    help="table of state-name pointers, to label the cases")
    ap.add_argument("--table", action="append", default=[],
                    metavar="SEC:OFF:N:FIRST", help="override table detection")
    ap.add_argument("--min", type=int, default=0,
                    help="only list cases owning at least this many bytes")
    ap.add_argument("--entries", metavar="LABEL=ADDR[,LABEL=ADDR...]",
                    help="dispatch targets given explicitly, for a function "
                         "that selects with a compare/branch chain rather "
                         "than a jump table.  V34SetupModulator is the "
                         "reason this exists: it dispatches on seven baud "
                         "rates with cmp/je and find_tables correctly "
                         "reports no tables, which left the whole function "
                         "as 'reached by no case'.")
    args = ap.parse_args()
    if args.selftest:
        return selftest(args.obj)
    if not args.func:
        ap.error("--func is required (or --selftest)")

    lo, size = symbol(args.obj, args.func)
    if lo is None:
        print("cfgsplit: no symbol %r" % args.func, file=sys.stderr)
        return 1
    hi = lo + size

    names = None
    if args.states:
        sec, off, n = args.states.split(":")
        off, n = int(off, 0), int(n, 0)
        data = section_bytes(args.obj, sec)
        strs = section_bytes(args.obj, ".rodata.str1.1")
        names = []
        for i in range(n):
            p = struct.unpack_from("<I", data, off + 4 * i)[0]
            names.append(strs[p:strs.index(b"\0", p)].decode("latin1"))

    insns = disassemble(args.obj, lo, hi)
    if args.table:
        tables = []
        for spec in args.table:
            sec, off, n, first = spec.split(":")
            tables.append((int(off, 0), int(n, 0), int(first, 0), sec))
    else:
        tables = find_tables(args.obj, insns, lo, hi)

    # Every dispatch target, and which cases select it.
    cases = {}
    if args.entries:
        names = []
        for i, spec in enumerate(args.entries.split(",")):
            label, addr = spec.split("=")
            cases.setdefault(int(addr, 0), []).append(i)
            names.append(label)
        tables = []
    for off, count, first, section in tables:
        entries = read_table(args.obj, section, off, count)
        for i, target in enumerate(entries):
            cases.setdefault(target, []).append(first + i)

    leaders = {lo}
    leaders.update(t for t in cases if lo <= t < hi)
    for i, insn in enumerate(insns):
        nxt = insn.addr + insn.size
        if insn.mnem in UNCOND or insn.mnem.startswith("ret") \
                or COND.match(insn.mnem):
            t = direct_target(insn, lo, hi)
            if t is not None:
                leaders.add(t)
            if nxt < hi:
                leaders.add(nxt)

    blocks = build_blocks(insns, leaders, lo, hi)
    entries = sorted(t for t in cases if t in blocks)

    reach = {e: reachable(blocks, e) for e in entries}
    count = {}
    for e in entries:
        for b in reach[e]:
            count[b] = count.get(b, 0) + 1

    exclusive = {}
    for e in entries:
        exclusive[e] = sum(blocks[b][0] for b in reach[e] if count[b] == 1)
    shared = sum(blocks[b][0] for b, n in count.items() if n > 1)
    covered = set()
    for e in entries:
        covered |= reach[e]
    unreached = sum(sz for b, (sz, _s) in blocks.items() if b not in covered)

    total = sum(sz for sz, _s in blocks.values())
    print("%s: 0x%x..0x%x, %d bytes, %d blocks, %d dispatch targets in %d "
          "table(s)" % (args.func, lo, hi, size, len(blocks), len(entries),
                        len(tables)))
    print()
    print("  exclusive to one case  %7d bytes" % sum(exclusive.values()))
    print("  shared by two or more  %7d bytes   <- the common engine" % shared)
    print("  reached by no case     %7d bytes   (prologue, epilogue, padding)"
          % unreached)
    print("  %s %7d bytes accounted of %d"
          % (" " * 21, total, size))
    #
    # AND IT IS A FAILURE, not a remark.  gates.md's rule 1: make the tool
    # COUNT what it examined and FAIL on the difference.  This line printed a
    # 131-byte shortfall on every run of `v34handshak` for as long as the tool
    # has existed, and nobody read it as a defect because nothing failed.
    #
    if total != size:
        print("\n  ERROR: %d bytes of %s were not accounted for.\n"
              "  The walk is INCOMPLETE and every byte count derived from it\n"
              "  is a lower bound.  Do not quote it." % (size - total, func))
        return 1
    return 0
    print()
    print("  per case, exclusive bytes only:")
    rows = []
    for e in entries:
        who = cases[e]
        label = "/".join(names[c] if names and c < len(names) else str(c)
                         for c in who)
        rows.append((exclusive[e], e, label, len(reach[e])))
    for excl, addr, label, nblocks in sorted(rows, reverse=True):
        if excl < args.min:
            continue
        print("    %7d  0x%05x  %3d blocks  %s" % (excl, addr, nblocks, label))
    return 0


if __name__ == "__main__":
    sys.exit(main())
