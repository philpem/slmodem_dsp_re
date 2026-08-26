#!/usr/bin/env python3
"""Find fields whose SIGNEDNESS we got wrong, by asking the original's compiler.

A `movswl`/`movzwl` (or `movsbl`/`movzbl`) is the compiler stating the declared
type of what it loaded.  Where the object sign-extends and we zero-extend, or
the reverse, one of the two declarations is wrong -- and a differential test
usually cannot tell, because the two agree over every value the field actually
holds.  Finding F613 is the worked example: `struct b103_hdx.mode` was `short`
here and `unsigned short` in the original, it indexes a table of function
pointers, and 1,104 tests never saw it.

THE RULE THIS TOOL EXISTS TO APPLY (finding F614).  An extension difference is
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

PRECISION, MEASURED, AND IT IS POOR (finding F2402).  On the current toolchain
-- GCC 3.4.2 exact, -O3, -mno-ieee-fp -- this reports 18 live memory-operand
candidates over 938 shared symbols.  Fourteen are traced (eleven by hand
against tools/dis.py, three by 619): THREE are real, so roughly ONE REPORT IN
FIVE is a defect.  Four are untraced and 2402 names them.  Every hit must
still be traced; nothing here is a gate.

The twelve failures are not one class.  A 16-bit compare (`cmp $0x2580,%ax`), a
signed branch on a 16-bit test, a 32-bit sum immediately narrowed by a cast, a
value masked with `and $0x3` before use -- in each the extension is real but
unobservable, and lookahead cannot see truncation.  619 already ruled that
fixing this needs real dataflow rather than another lookahead rule, and that
ruling stands; the two corrections since are BUGS removed (finding F2401), not
heuristics added.

One more class the displacement key cannot avoid: at displacement 0x0 it pairs
any zero-displacement load with any other, so `receiver mem 0x0` pairs the
object's dot-product loop against our field copy.  Treat a `mem 0x0` hit in a
large function as unpaired until the two sites are shown to be the same site.
"""

import glob
import os
import re
import subprocess
import sys

BLOB = os.environ.get("BLOB", "ref/slmodemd/dsplibs.o")
OURS = os.environ.get("TC_OUT", "build/tc_out")

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


#
# ALIGNMENT PADDING IS NOT A USE.  A multi-byte NOP is spelled as a real
# instruction -- `lea 0x0(%esi,%eiz,1),%esi`, `lea 0x0(%edi),%edi`,
# `mov %esi,%esi` -- and the liveness scan below, which asks only whether the
# register's name appears, read every one of them as a 32-bit use.  So a load
# followed by padding was reported LIVE with no use at all.
#
# `fskdemodulate` +0x14 and `V90AutoDigitalImpDetector::resetLinearMapping`
# +0xa96c were both this and nothing else.  Padding also ate the re-extension
# window, which counts ten INSTRUCTIONS -- nops included, so a padded gap hid
# the second extension the window exists to find.
#
# MEASURED, NOT ASSUMED: it costs two memory-operand reports at -O3 (20 -> 18)
# and the same two at -O2 (17 -> 15), so it is NOT a consequence of the -O3
# move.  In the decisive cases the padding is in the BLOB, whose codegen no
# flag of ours changes.  The bug was latent from the start and survived 618's
# five corrections; re-running the validation ritual is what found it.
# Finding F2401.
#
PAD = re.compile(r"nop|lea 0x0\(.*\),%e[a-z][a-z]$|mov %e(si|di),%e(si|di)$")


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
            t = re.sub(r"\s*<[^>]*>", "", re.sub(r"\s+", " ", m.group(1))).strip()
            if not PAD.match(t):
                insns.append(t)
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
        # Finding F618.
        #
        # A REDEFINITION ENDS THE WINDOW.  The filter's premise is that the SAME
        # loaded value is extended again; once the register has been written the
        # low half belongs to a different value and any later `movswl %di,...`
        # is unrelated.  Scanning past it retracted a real hit: `initdigital`
        # loads +0x0 into %edi and passes it 32-bit to a call, then reloads
        # %edi from +0x14 and re-extends THAT -- ten instructions later, which
        # the window reached once padding stopped filling it.  Finding F2401.
        low16 = LOW16.get(reg)
        if low16:
            reextended = False
            for n in insns[i + 1:i + 10]:
                if re.match(r"mov[sz][wb]l\s+%s," % re.escape(low16), n):
                    reextended = True
                    break
                if re.match(r"\S+\s+[^,]*,%s$" % re.escape(reg), n):
                    break                      # reg overwritten; value is gone
            if reextended:
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
        # far more often than not -- the defect in finding F613 reads
        # `movzwl (%ecx)` in the object and `movswl (%edx)` here.  Matching on
        # the full text never pairs those, which is why the first version of
        # this tool could not find the one defect it was written for.
        # Stack slots are excluded: those are locals, not fields.
        # Finding F618.
        #
        m2 = re.match(r"(0x[0-9a-f]+)?\((%e[a-z][a-z])\)$", src)
        if m2 and m2.group(2) != "%esp":
            out.append(("mem " + (m2.group(1) or "0x0"), mnem, live))
        elif not m2:
            out.append(("reg " + src, mnem, live))
    return out


def load_ours():
    """Objects from TC_OUT, or die.  AN EMPTY SET IS NOT A CLEAN TREE.

    This tool defaulted to `/tmp/tc_out` while `period.mk` and `compare.py`
    moved to `build/tc_out`, so with no environment set it globbed an absent
    directory, compared zero symbols, printed "(none)" and exited 0 -- the
    dead detector of finding F618 back in the tree, and no way to see it from
    the output.  Finding F2400.  Never let this fail quietly again.
    """
    objs = sorted(glob.glob(os.path.join(OURS, "*.o")))
    if not objs:
        sys.exit("extcheck: no objects in TC_OUT=%s -- run `make tc` "
                 "first.  Refusing to report a clean tree that was "
                 "never examined." % OURS)
    ours = {}
    for o in objs:
        for k, v in sizes(o).items():
            ours.setdefault(k, v)
    return ours


def main():
    show_dead = "--dead" in sys.argv
    blob = sizes(BLOB)
    ours = load_ours()

    live_hits, dead_hits = [], []
    compared = 0
    for k in sorted(ours):
        if k not in blob:
            continue
        compared += 1
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
        # and was the tell.  Finding F618.
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
    # ALWAYS say how much was examined.  "(none)" over 938 symbols is a result;
    # "(none)" over 0 is a broken invocation, and they used to print alike.
    print("\n  %d live, %d discarded as dead extensions"
          "   (%d symbols compared, TC_OUT=%s)"
          % (len(live_hits), len(dead_hits), compared, OURS))
    if show_dead:
        print()
        for k, src, mnem, other in dead_hits:
            print("  dead  %-30s %-14s object %s  ours %s" % (k, src, mnem, other))
    return 1 if live_hits else 0


if __name__ == "__main__":
    sys.exit(main())
