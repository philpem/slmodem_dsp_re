#!/usr/bin/env python3
"""Find fields whose SIGNEDNESS we got wrong, by asking the original's compiler.

A `movswl`/`movzwl` (or `movsbl`/`movzbl`) is the compiler stating the declared
type of what it loaded.  Where the object sign-extends and we zero-extend, or
the reverse, one of the two declarations is wrong -- and a differential test
usually cannot tell, because the two agree over every value the field actually
holds.  Finding 353 is the worked example: `struct b103_hdx.mode` was `short`
here and `unsigned short` in the original, it indexes a table of function
pointers, and 1,104 tests never saw it.

THE RULE THIS TOOL EXISTS TO APPLY (finding 354).  An extension difference is
evidence ONLY IF THE 32-BIT RESULT IS USED.  Where the loaded value is stored
straight back as 16 bits -- a field copy, a filter history shifting along --
the upper half is discarded and the compiler was free to pick either
instruction.  Five of the first six differences found this way were exactly
that, and retyping those struct fields would have been wrong.

So each load is classified:

    LIVE    the destination register is later read as a full 32-bit value --
            an index, a multiply, a compare, a 32-bit store.  The extension
            reached the result, and a disagreement here is a defect.
    DEAD    the only later use is a 16-bit store of the low half.  The
            extension is unobservable; ignore it.

Liveness is approximated by scanning forward to the next definition of the
register, which is enough for the small leaf functions this finds and errs
towards reporting.  Read the disassembly before changing a declaration.

    tools/toolchain/extcheck.py [--dead]

`--dead` also lists the discarded ones, to show what is being filtered.
"""

import glob
import os
import re
import subprocess
import sys

BLOB = os.environ.get("BLOB", "../slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "/tmp/tc_out")

LOW16 = {"%eax": "%ax", "%ebx": "%bx", "%ecx": "%cx", "%edx": "%dx",
         "%esi": "%si", "%edi": "%di", "%ebp": "%bp"}
LOW8 = {"%eax": "%al", "%ebx": "%bl", "%ecx": "%cl", "%edx": "%dl"}


def sizes(path):
    d = {}
    for line in subprocess.run(["nm", "--size-sort", "-S", path],
                               capture_output=True, text=True).stdout.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = (int(p[1], 16), path)
    return d


def disasm(path, sym):
    out = subprocess.run(
        ["objdump", "-d", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    insns = []
    for line in out.splitlines():
        if line.startswith("Disassembly"):
            continue
        m = re.match(r"^\s*[0-9a-f]+:\s+(.*?)\s*$", line)
        if m:
            insns.append(re.sub(r"\s*<[^>]*>", "", re.sub(r"\s+", " ", m.group(1))).strip())
    return insns


def extensions(insns):
    """[(operand, mnemonic, live)] for each extending load."""
    out = []
    for i, ins in enumerate(insns):
        m = re.match(r"(mov[sz][wb]l)\s+([^,]+),(%e[a-z][a-z])$", ins)
        if not m:
            continue
        mnem, src, reg = m.groups()

        #
        # RE-EXTENSION KILLS THE EVIDENCE.  If the low half of the destination
        # is extended again straight afterwards -- `movzwl 0x8(%ebx),%eax`
        # followed by `movswl %ax,%edx` -- then the load's own extension is not
        # the field's declared type.  The compiler wanted the raw 16 bits for
        # something else and re-extended a copy the way the type actually
        # requires.  Reading the first instruction as the type gets it exactly
        # backwards, which is how `fskdemodulate`'s four "hits" arose.
        # Finding 358.
        #
        low16 = LOW16.get(reg)
        if low16 and any(re.match(r"mov[sz][wb]l\s+%s," % re.escape(low16), n)
                         for n in insns[i + 1:i + 10]):
            continue

        live = False
        for nxt in insns[i + 1:]:
            # Any full-width mention of the register that is not the
            # low-half store below counts as a real use.
            low = LOW16.get(reg), LOW8.get(reg)
            if re.match(r"mov[wb]?\s+%s," % re.escape(low[0] or "\0"), nxt) \
               or (low[1] and re.match(r"mov[b]?\s+%s," % re.escape(low[1]), nxt)):
                break                      # 16/8-bit store of the low half
            if reg in nxt:
                # Redefinition without a read: `mov ...,%reg` ends it.
                if re.match(r"\S+\s+[^,]+,%s$" % re.escape(reg), nxt) \
                   and not re.search(r"%s\s*[,)]" % re.escape(reg), nxt.split(",")[0]):
                    break
                live = True
                break
        #
        # KEY ON THE DISPLACEMENT, NOT THE OPERAND TEXT.  The base register is
        # whatever the allocator picked, and it differs between the two builds
        # far more often than not -- the defect in finding 353 reads
        # `movzwl (%ecx)` in the object and `movswl (%edx)` here.  Matching on
        # the full text never pairs those, which is why the first version of
        # this tool could not find the one defect it was written for.
        # Stack slots are excluded: those are locals, not fields.
        # Finding 358.
        #
        m2 = re.match(r"(0x[0-9a-f]+)?\((%e[a-z][a-z])\)$", src)
        if m2 and m2.group(2) != "%esp":
            out.append(("mem " + (m2.group(1) or "0x0"), mnem, live))
        elif not m2:
            out.append(("reg " + src, mnem, live))
    return out


def main():
    show_dead = "--dead" in sys.argv
    blob = sizes(BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, v)

    live_hits, dead_hits = [], []
    for k in sorted(ours):
        if k not in blob:
            continue
        ea, eb = extensions(disasm(BLOB, k)), extensions(disasm(ours[k][1], k))
        a = {(s, m): l for s, m, l in ea}
        b = {(s, m): l for s, m, l in eb}

        #
        # AN OPERAND LOADED BOTH WAYS ON EITHER SIDE PROVES NOTHING.  The same
        # field read once signed and once unsigned in the same function is
        # ordinary -- one use needs the value, another needs the raw bits --
        # and comparing the two sides as SETS then manufactures a pair in each
        # direction.  `demapFrame`'s 0x144c and `receiver`'s 0x210..0x216 each
        # appeared twice, in opposite directions, which is self-contradictory
        # and was the tell.  Finding 358.
        #
        # Per SIDE.  Unioning the two sides' mnemonics would make every genuine
        # disagreement filter itself out, which it did.
        both = set()
        for side in (ea, eb):
            for s in {x[0] for x in side}:
                if len({m for (s2, m, _) in side if s2 == s}) > 1:
                    both.add(s)

        for (src, mnem), live in a.items():
            if src in both:
                continue
            other = mnem.replace("s", "z", 1) if "movs" in mnem else mnem.replace("z", "s", 1)
            if (src, other) in b and (src, mnem) not in b:
                (live_hits if (live or b[(src, other)]) else dead_hits).append(
                    (k, src, mnem, other))

    #
    # MEMORY operands are the ones that state a FIELD's declared type.  A
    # register operand -- `movzwl %ax,%eax` -- is the compiler materialising a
    # cast of a computed value, which reflects an intermediate expression's
    # type rather than a declaration, and is far weaker evidence.  They are
    # separated rather than dropped: a systematic disagreement among them still
    # says something, just not which field to retype.
    #
    mem = [h for h in live_hits if h[1].startswith("mem ")]
    reg = [h for h in live_hits if h[1].startswith("reg ")]

    print("LIVE, MEMORY OPERAND -- a field's declared type differs:\n")
    for k, src, mnem, other in mem:
        print("  %-34s %-14s object %s   ours %s" % (k, src, mnem, other))
    if not mem:
        print("  (none)")
    print("\nLIVE, register operand -- an intermediate cast, weaker evidence:"
          "  %d" % len(reg))
    print("\n  %d live, %d discarded as dead extensions" % (len(live_hits), len(dead_hits)))
    if show_dead:
        print()
        for k, src, mnem, other in dead_hits:
            print("  dead  %-30s %-14s object %s  ours %s" % (k, src, mnem, other))
    return 1 if live_hits else 0


if __name__ == "__main__":
    sys.exit(main())
