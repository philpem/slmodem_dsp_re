#!/usr/bin/env python3
"""Per-function POSITIONAL BYTE IDENTITY: did we reproduce the function exactly?

WHY THIS EXISTS, AND WHY `compare.py` IS NOT IT

`compare.py` runs `objdump -d --no-show-raw-insn` and compares MNEMONICS with
operands dropped.  That is deliberate and it answers a real question, but it
cannot answer the first one anybody actually asks: **is our function the same
bytes in the same places as the object's?**  Two functions storing different
constants to different offsets both read as `mov mov mov` to that tool.

Its other number is worse for this purpose.  The "total code: blob N bytes,
ours M (P%)" line is a SIZE RATIO over the whole tree, and a size ratio moves
when we emit more code, not when we emit more of the right code -- `-O3` took
it from 77.3% to 89.0% while the count of matching functions did not move at
all (finding F616).  It is a rough gauge of completion and nothing else, and it
has already misled once: the MP CRC work found a source form whose BYTE COUNT
was closer while its instruction sequence was further away.

So this tool reports one number per function and one count over the tree:
**exact, or not.**

RELOCATED FIELDS ARE COMPARED BY TARGET, NOT BY VALUE, and that is not a
loosening -- it is the only correct comparison.  A four-byte field under an
`R_386_32` or `R_386_PC32` holds a placeholder the linker will overwrite; in
the blob it is frequently 0 or -4, and ours is whatever our assembler chose.
Comparing those bytes literally would report a difference that does not exist,
so each relocated field is compared by (relocation type, target symbol)
instead.  A function that is byte-equal everywhere else but CALLS SOMETHING
DIFFERENT is reported separately, as `RELOC`, because that is a real
difference this tool must not hide.

    tools/toolchain/byteident.py                  counts, then the misses
    tools/toolchain/byteident.py --list-exact     the exact set, for a diff
    BLOB=/abs/path/to/dsplibs.o TC_OUT=build/tc_out tools/toolchain/byteident.py

Denominator: the symbols the blob and `TC_OUT` both define, which is
`compare.py`'s denominator exactly, so the two numbers are comparable.
"""

import argparse
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def _default_blob():
    d = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                       capture_output=True, text=True).stdout.strip()
    if d:
        return os.path.join(os.path.dirname(os.path.abspath(d)),
                            "ref", "slmodemd", "dsplibs.o")
    return os.path.join("ref", "slmodemd", "dsplibs.o")


BLOB = os.environ.get("BLOB") or _default_blob()
OURS = os.environ.get("TC_OUT", "build/tc_out")

INSN = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)")
RELOC = re.compile(r"^\s*([0-9a-f]+):\s+(R_386_\w+)\s+(\S+)")


def sizes(path):
    out = subprocess.run(["nm", "--size-sort", "-S", path],
                         capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4 and p[2] in "TtWw":
            d[p[3]] = int(p[1], 16)
    return d


def body(path, sym):
    """(bytes, {offset: (type, target)}) for one function, offsets relative."""
    out = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + sym, path],
        capture_output=True, text=True).stdout
    data, relocs, base = {}, {}, None
    for line in out.splitlines():
        m = INSN.match(line)
        if m:
            at = int(m.group(1), 16)
            if base is None:
                base = at
            for i, byte in enumerate(m.group(2).split()):
                data[at - base + i] = int(byte, 16)
            continue
        m = RELOC.match(line)
        if m and base is not None:
            relocs[int(m.group(1), 16) - base] = (m.group(2), m.group(3))
    if base is None:
        return None, None
    n = max(data) + 1 if data else 0
    return bytes(data.get(i, 0) for i in range(n)), relocs


REG32 = {"al": "eax", "ah": "eax", "ax": "eax", "eax": "eax",
         "bl": "ebx", "bh": "ebx", "bx": "ebx", "ebx": "ebx",
         "cl": "ecx", "ch": "ecx", "cx": "ecx", "ecx": "ecx",
         "dl": "edx", "dh": "edx", "dx": "edx", "edx": "edx",
         "si": "esi", "esi": "esi", "di": "edi", "edi": "edi",
         "bp": "ebp", "ebp": "ebp", "sp": "esp", "esp": "esp"}
REGTOK = re.compile(r"%(\w+)")
BARE_REG = re.compile(r"%\w+")
#
# A DEFINITION ENDS A LIVE RANGE ONLY IF IT WRITES THE WHOLE REGISTER.
# `REG32` above folds `%al` and `%ax` onto `eax` so the two sides can be
# compared at all, and that folding is what makes this necessary: without it
# `sete %al`, `movb $0,%al` and `xor %ax,%ax` all read as redefinitions of the
# full `%eax`, and the 16 or 24 bits they leave untouched -- still carrying the
# old value, still live -- would be free to rebind.  That is a FALSE ACCEPT,
# the direction a loosening tool must never fail in.
#
WIDE_REG = frozenset(("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp"))


def _full_write(tok):
    """Is `%tok` a write of an entire 32-bit register?"""
    m = REGTOK.fullmatch(tok.strip())
    return bool(m) and m.group(1) in WIDE_REG
DEFS = {"mov", "movl", "movw", "movb", "movzbl", "movzwl", "movsbl",
        "movswl", "movzbw", "movsbw", "lea", "pop", "movd", "movq"}


PAD_LEA = re.compile(r"0x0\((%\w+)(?:,%eiz,1)?\),(%\w+)")


def _padding(mn, ops):
    """Is this row alignment padding rather than code?

    THE SELF-MOVE WAS MISSING AND IT MADE `--why` LIE.  GCC 3.4.2 fills two
    bytes at a loop head with `mov %esi,%esi`, which this did not recognise, so
    the padding-stripped counts it printed were the RAW counts plus a constant
    that differed between the two sides.  On `unitePhasesInfoOfUref` it printed
    "204 against 204" -- equal, therefore not lever 2 -- where the blob carries
    two self-moves and ours one and the true code counts are 202 against 203, a
    real extra instruction in ours.  Of the BYTES symbols then remaining, 13
    carried a self-move and 7 disagreed across the two sides, so the triage was
    unsafe for 7 of 67 and silently so.  `instrcount.py` already knew.
    """
    if mn.startswith("nop"):
        return True
    if mn == "mov":
        f = ops.split(",")
        if len(f) == 2 and f[0].strip() == f[1].strip() and BARE_REG.fullmatch(f[0].strip()):
            return True
    if mn != "lea":
        return False
    m = PAD_LEA.fullmatch(ops.strip())
    return bool(m) and m.group(1) == m.group(2)


def _fields(ops):
    """Top-level comma-separated operands.

    AT&T memory operands contain commas -- `0x56(%ebp,%ebx,2)` is ONE operand
    with two of them -- so a naive split puts the destination in the wrong
    place and every indexed instruction reads as a definition of `2`.
    """
    out, depth, cur = [], 0, ""
    for ch in ops:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return out


def insns(path, sym):
    """[(mnemonic, operand text)] with addresses and symbol comments dropped."""
    out = subprocess.run(
        ["objdump", "-dr", "--disassemble=" + sym, "--no-show-raw-insn", path],
        capture_output=True, text=True).stdout
    rows, base = [], None
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\t", line)
        if not m:
            #
            # A RELOCATION LINE BELONGS TO THE INSTRUCTION ABOVE IT, and it
            # must be applied THERE.  The first version of this did it in a
            # second loop over the whole output, so `rows[-1]` was the
            # function's LAST instruction every time -- usually a `ret` with
            # no operands -- and the normaliser silently did nothing at all
            # for 210 relocated instructions across the tree while reporting
            # a clean run.  Findings F134 and F2401: a detector that cannot be
            # seen to fire has not been shown to work.
            #
            r = RELOC.match(line)
            if r and rows:
                tag = "@" + r.group(3)
                sub, k = re.subn(r"0x[0-9a-f]+|\$0x[0-9a-f]+", tag, rows[-1][1])
                #
                # A CALL HAS NO NUMERIC LITERAL LEFT TO REPLACE -- its operand
                # is a bare address already rewritten to `.+N` above -- so a
                # substitution alone drops the target on the floor, and two
                # calls to DIFFERENT functions compare equal.  That is not
                # hypothetical: `V90PreFilter`'s destructors call
                # `FloatFIR::~FloatFIR` D2 in the blob and D1 in ours, and
                # both were being certified as "same instructions and
                # operands".  Append where nothing was replaced.
                #
                rows[-1][1] = sub if k else (rows[-1][1] + " " + tag).strip()
            continue
        at = int(m.group(1), 16)
        if base is None:
            base = at
        body = line.split("\t", 1)[1].strip()
        body = body.split("#", 1)[0].strip()
        body = re.sub(r"<[^>]*>", "", body).strip()
        parts = body.split(None, 1)
        ops = parts[1].strip() if len(parts) > 1 else ""
        #
        # A BRANCH TARGET IS AN ABSOLUTE ADDRESS AND THE TWO OBJECTS PUT THE
        # FUNCTION AT DIFFERENT OFFSETS, so `jmp 7e321` and `jmp f1` are the
        # same jump printed twice.  Comparing them literally scored 310
        # functions as different that differ in nothing at all.  Rewrite a
        # bare hex operand as an offset from the function's own first
        # instruction, which is the same number on both sides.
        #
        if re.fullmatch(r"[0-9a-f]+", ops):
            ops = ".%+d" % (int(ops, 16) - base)
        rows.append([parts[0], ops])
        continue
    #
    # A RELOCATED OPERAND'S PRINTED NUMBER IS A PLACEHOLDER, and the two
    # objects do not use the same one: the blob prints
    # `movswl 0x5740(%eax,%eax,1)` where ours prints `0x0(...)`, because the
    # blob's addend rides inline against a SECTION symbol and ours is a named
    # symbol with a zero addend (finding F604).  Comparing the printed
    # displacement scores that as a difference in code where there is none.
    # Replace every numeric literal in a relocated instruction's operands with
    # the relocation's TARGET, so two instructions relocated against the same
    # thing compare equal and two relocated against different things do not.
    #
    return [tuple(r) for r in rows]


def alpha_why(x, y):
    """Same instructions and operands up to a register renaming, PER LIVE RANGE.

    This is grade 1 and it is STRICTER than `compare.py`, which drops operands
    altogether -- there, `mov $1,%eax` and `mov $2,%ebx` both read as `mov`.
    Here every immediate and displacement must match exactly and only the
    register NAMES may differ (%eax<->%edx implies %al<->%dl, so tokens are
    folded to their 32-bit family and the width must still agree).  `%esp` and
    `%ebp` are pinned to themselves: the frame is not a free choice.

    THE RENAMING IS PER LIVE RANGE, NOT PER FUNCTION, because that is what a
    register allocator actually chooses.  One bijection held for the whole
    function rejected 20 pairs that differ in nothing but allocation: the blob
    puts two successive values in %eax while ours puts the second in %edx, and
    a function-wide map cannot express that.  A DESTINATION WRITTEN WITHOUT
    BEING READ ends the range that was in it and the pairing lapses there --
    `mov`, `lea`, `pop`, the `movzx`/`movsx` family, `set*`, `cmov*` and the
    `xor r,r` idiom.  `add %eax,%ebx` reads and writes %ebx and is NOT a
    definition; a memory destination is a store, not a definition.

    THIS IS A LOOSENING, so it is the direction that risks calling different
    code equivalent, and `--self-test` exists for that reason: four of its
    eight cases are things this must still REJECT.
    """
    #
    # A LENGTH DIFFERENCE IS A DIAGNOSIS, NOT A TOOL ARTEFACT.  This returned a
    # bare `False`, which `--why` printed verbatim and which reads exactly like
    # something the tool failed to do -- and it has already misled a reader into
    # quoting it as evidence of an absence.  Two independent passes asked for
    # this line.
    #
    # THE COUNTS ARE REPORTED BOTH WAYS ON PURPOSE.  The raw pair is ambiguous
    # between alignment padding and real code, which is the whole of finding
    # 7793: counting padding as code inverted the triage of five functions.
    # `instrcount.py` is the tool that strips, and the stripped pair is the one
    # to quote.
    #
    #
    # STRIP PADDING BEFORE THE LENGTH CHECK, NOT INSIDE THE LOOP.  The per-row
    # skip below can never run when the two sequences differ in length, because
    # this check returns first -- so two functions whose CODE matches and whose
    # alignment filler does not were rejected on length, which is a difference
    # in nothing.
    #
    x = [r for r in x if not _padding(*r)]
    y = [r for r in y if not _padding(*r)]

    if len(x) != len(y):
        sx = len(x)
        sy = len(y)
        return ("INSTRUCTION COUNT differs: blob %d, ours %d  (padding "
                "already stripped: %d against %d)\n"
                "        an absence or an extra, so this is lever 2 -- a "
                "missing or added statement -- and not a renaming"
                % (len(x), len(y), sx, sy))
    fwd, rev = {"esp": "esp", "ebp": "ebp"}, {"esp": "esp", "ebp": "ebp"}

    def bind(fu, fv):
        return fwd.setdefault(fu, fv) == fv and rev.setdefault(fv, fu) == fu

    def rebind(fu, fv):
        if fu in ("esp", "ebp") or fv in ("esp", "ebp"):
            return fu == fv
        old = fwd.pop(fu, None)
        if old is not None:
            rev.pop(old, None)
        old = rev.pop(fv, None)
        if old is not None:
            fwd.pop(old, None)
        fwd[fu], rev[fv] = fv, fu
        return True

    for i, ((mx, ox), (my, oy)) in enumerate(zip(x, y)):
        if mx != my:
            return "row %d MNEMONIC  %s vs %s" % (i, mx, my)
        #
        # ALIGNMENT PADDING IS NOT A READ, AND IT IS NOT SPELT `nop`.  The
        # first version of this guard tested `mx.startswith("nop")` and never
        # fired once, because the form that actually appears is
        # `lea 0x0(%esi,%eiz,1),%esi` -- mnemonic `lea`.  Measured over the
        # blob the family is exactly four spellings and no others --
        # `lea 0x0(%esi,%eiz,1),%esi` 925, `lea 0x0(%esi),%esi` 827, and the
        # two `%edi` twins 538 and 82 -- plus the bare `nop`.  All four are
        # ZERO displacement, base equal to destination, and either no index or
        # `%eiz`, which is GAS printing an SIB whose index field says "none".
        #
        # THE PATTERN HAS TO BE EXACTLY THAT TIGHT.  A first survey matched any
        # self-based `lea` and swept up `lea (%edx,%edx,2),%edx` -- 63 of them,
        # real code computing `edx * 3`.  Skipping those would have discarded
        # an arithmetic operand as though it were whitespace.
        #
        # Binding those register fields rejected `_iir_filter_create`, whose
        # 58 rows are otherwise a clean `%ebx`/`%esi` swap (finding F7768).
        # BOTH sides must look like padding before the row is skipped -- a
        # real `lea` opposite a padding `lea` is a genuine difference.
        #
        if _padding(mx, ox) and _padding(my, oy):
            continue
        fx, fy = _fields(ox), _fields(oy)
        if len(fx) != len(fy):
            return ("row %d OPERAND COUNT  %s | %s" % (i, ox, oy))
        dx = fx[-1].strip() if fx else ""
        dy = fy[-1].strip() if fy else ""
        #
        # A ZEROING IDIOM READS NOTHING, and treating its source as a use was
        # a real defect (finding F7762).  `xor %eax,%eax` does not depend on
        # %eax; nor does `sub r,r`; nor does `sbb r,r`, which materialises the
        # carry as 0 or -1 and was missing from the set entirely.  Stripping
        # only the DESTINATION left the identical source operand to be bound
        # against the stale map -- so the idiom failed precisely when a
        # register had been renamed, which is the only case it exists for.
        #
        idiom = (mx in ("xor", "sub", "sbb") and len(fx) == 2
                 and fx[0].strip() == dx and fy[0].strip() == dy
                 and _full_write(dx) and _full_write(dy))
        isdef = bool(fx) and _full_write(dx) and _full_write(dy) and (
            mx in DEFS or mx.startswith("set") or mx.startswith("cmov") or idiom)
        ux = ([] if idiom else fx[:-1]) if isdef else fx
        uy = ([] if idiom else fy[:-1]) if isdef else fy
        rx = REGTOK.findall(" ".join(ux))
        ry = REGTOK.findall(" ".join(uy))
        if len(rx) != len(ry):
            return ("row %d REGISTER COUNT  %s | %s" % (i, ox, oy))
        for u, v in zip(rx, ry):
            fu, fv = REG32.get(u), REG32.get(v)
            if fu is None or fv is None:
                if u != v:
                    return ("row %d NON-REGISTER  %s vs %s  | %s | %s"
                            % (i, u, v, ox, oy))
                continue
            if len(u) != len(v):
                return ("row %d WIDTH  %s vs %s  | %s | %s" % (i, u, v, ox, oy))
            if not bind(fu, fv):
                return ("row %d USE CONFLICT  %s wants %s, already bound to %s\n"
                        "        blob %s\n        ours %s"
                        % (i, fu, fv, fwd.get(fu, rev.get(fv)), ox, oy))
        if isdef:
            u, v = dx[1:], dy[1:]
            fu, fv = REG32.get(u), REG32.get(v)
            if fu is None or fv is None:
                if u != v:
                    return ("row %d DEST NON-REGISTER  %s vs %s" % (i, u, v))
            elif len(u) != len(v) or not rebind(fu, fv):
                return ("row %d DEST WIDTH  %s vs %s" % (i, u, v))
        if REGTOK.sub("%r", ox) != REGTOK.sub("%r", oy):
            return ("row %d NON-REGISTER OPERAND  %s | %s" % (i, ox, oy))
    return None


def alpha_equal(x, y):
    """Grade 1: same instructions and operands under a register bijection.

    ONE implementation, wrapped.  The rejection-row explainer began life as a
    separate script with its own copy of this loop, and within an hour it had
    gone stale -- it still tested `mx.startswith("nop")` after the padding rule
    moved on, and reported a rejection the real function no longer made.  A
    second copy of a comparison is a second answer to the same question.
    """
    return alpha_why(x, y) is None


def verdict(a, ra, b, rb):
    if a is None or b is None:
        return "NODATA", 0
    if len(a) != len(b):
        return "SIZE", abs(len(a) - len(b))
    masked = set()
    for off in set(ra) | set(rb):
        masked.update(range(off, min(off + 4, len(a))))
    diff = sum(1 for i in range(len(a)) if i not in masked and a[i] != b[i])
    if diff:
        return "BYTES", diff
    if ra != rb:
        #
        # A SECTION-RELATIVE RELOCATION AND A NAMED-SYMBOL ONE CAN NAME THE
        # SAME THING.  `objdump` prints the blob's string and table references
        # as `R_386_32 .rodata` with the offset as an inline addend, and ours
        # as `R_386_32 v8_costab` -- finding F604.  Comparing the printed names
        # scores that as a differing target when nothing differs, so a pair
        # where one side names a section is reported as UNRESOLVED and NOT as
        # a difference.  Resolving it properly needs the addend, which
        # `objdump -dr` does not print.
        #
        sections = (".text", ".rodata", ".data", ".bss")
        hard = soft = 0
        for k in set(ra) | set(rb):
            x, y = ra.get(k), rb.get(k)
            if x == y:
                continue
            if (x and x[1].startswith(sections)) or (y and y[1].startswith(sections)):
                soft += 1
            else:
                hard += 1
        if hard:
            return "RELOC", hard
        if soft:
            return "UNRESOLVED", soft
    return "EXACT", 0


SELF_TESTS = [
    ("pure renaming accepted", True,
     ["mov 0x4(%esp),%eax", "add $0x1,%eax", "mov %eax,(%edx)"],
     ["mov 0x4(%esp),%ecx", "add $0x1,%ecx", "mov %ecx,(%edx)"]),
    ("reuse after a redefinition accepted -- the whole point", True,
     ["mov (%esi),%eax", "mov %eax,(%edi)",
      "mov 0x4(%esi),%eax", "mov %eax,0x4(%edi)"],
     ["mov (%esi),%eax", "mov %eax,(%edi)",
      "mov 0x4(%esi),%edx", "mov %edx,0x4(%edi)"]),
    ("read-modify-write is NOT a redefinition", False,
     ["mov (%esi),%eax", "add $0x1,%eax", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "add $0x1,%edx", "mov %edx,(%edi)"]),
    ("different immediate rejected", False,
     ["mov $0x1,%eax"], ["mov $0x2,%eax"]),
    ("different displacement rejected", False,
     ["mov 0x4(%edx),%eax"], ["mov 0x8(%edx),%eax"]),
    ("crossed live ranges rejected", False,
     ["mov (%esi),%eax", "mov 0x4(%esi),%ecx", "add %eax,%ecx"],
     ["mov (%esi),%eax", "mov 0x4(%esi),%ecx", "add %ecx,%eax"]),
    ("%esp is pinned", False, ["mov %esp,%eax"], ["mov %ebx,%eax"]),
    ("xor r,r kills, so it survives a rename (7762)", True,
     ["mov (%esi),%eax", "xor %eax,%eax", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "xor %ecx,%ecx", "mov %ecx,(%edi)"]),
    ("sbb r,r kills too, and was missing entirely (7762)", True,
     ["mov (%esi),%eax", "sbb %eax,%eax", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "sbb %ecx,%ecx", "mov %ecx,(%edi)"]),
    #
    # THE SOURCE OF A NON-IDIOM `xor` IS READ, so it must bind.  The first
    # spelling of this case was wrong and the self-test said so: with the
    # source register bound to nothing else, `xor %ebx,%eax` against
    # `xor %ecx,%eax` IS a valid renaming and returning True was right.  The
    # case only tests anything once the source carries a live binding.
    #
    ("xor of DIFFERENT registers reads its source, which must bind", False,
     ["mov (%esi),%ebx", "xor %ebx,%eax", "mov %eax,(%edi)"],
     ["mov (%esi),%ecx", "xor %edx,%eax", "mov %eax,(%edi)"]),
    #
    # PARTIAL WRITES DO NOT END A LIVE RANGE.  REG32 folds %al and %ax onto
    # eax so the two sides can be compared; the cost is that an 8- or 16-bit
    # write looks like a redefinition of all 32.  These three would each be a
    # FALSE ACCEPT -- the dangerous direction for a loosening tool.
    #
    ("xor %ax,%ax leaves the top half of %eax live", False,
     ["mov (%esi),%eax", "xor %ax,%ax", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "xor %bx,%bx", "mov %ebx,(%edi)"]),
    ("sete %al does not redefine %eax", False,
     ["mov (%esi),%eax", "sete %al", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "sete %bl", "mov %ebx,(%edi)"]),
    ("movb into %al does not redefine %eax", False,
     ["mov (%esi),%eax", "movb $0x1,%al", "mov %eax,(%edi)"],
     ["mov (%esi),%eax", "movb $0x1,%bl", "mov %ebx,(%edi)"]),
    ("the padding `lea` is padding, not a use (7768)", True,
     ["mov (%ebx),%eax", "lea 0x0(%esi,%eiz,1),%esi", "mov %ebx,(%eax)"],
     ["mov (%esi),%eax", "lea 0x0(%edi,%eiz,1),%edi", "mov %esi,(%eax)"]),
    #
    # A MUST-REJECT CASE MUST BIND THE REGISTER FIRST.  Twice now the first
    # draft of a rejection case used registers bound to nothing else, where
    # any renaming is legal and True is the RIGHT answer -- the self-test
    # caught both.  The trap is that such a case looks like it tests the
    # operand and actually tests nothing.  Here row 0 pins the blob's `%edx`
    # to our `%ecx`, so row 1's index operand has something to contradict.
    #
    ("a self-based lea with an index is arithmetic, not padding", False,
     ["mov (%esi),%edx", "lea (%edx,%edx,2),%edx", "mov %edx,(%eax)"],
     ["mov (%esi),%ecx", "lea (%eax,%eax,2),%edx", "mov %edx,(%eax)"]),
    ("a self-move is two-byte loop-head padding, not code", True,
     ["mov (%esi),%eax", "mov %esi,%esi", "mov %eax,(%edi)"],
     ["mov (%esi),%eax",                  "mov %eax,(%edi)"]),
    ("a real move between DIFFERENT registers is not padding", False,
     ["mov (%esi),%ebx", "mov %ebx,%eax", "mov %eax,(%edi)"],
     ["mov (%esi),%ebx", "mov %ecx,%eax", "mov %eax,(%edi)"]),
    ("the 3-byte `lea 0x0(%R),%R` padding counts too", True,
     ["mov (%ebx),%eax", "lea 0x0(%esi),%esi", "mov %ebx,(%eax)"],
     ["mov (%esi),%eax", "lea 0x0(%edi),%edi", "mov %esi,(%eax)"]),
    ("a REAL lea opposite a padding lea is still a difference", False,
     ["mov (%ebx),%eax", "lea 0x0(%esi,%eiz,1),%esi", "mov %esi,(%eax)"],
     ["mov (%ebx),%eax", "lea 0x4(%esi),%edi",        "mov %edi,(%eax)"]),
    ("indexed memory is one operand, not a definition of the scale", False,
     ["movswl 0x56(%ebp,%ebx,2),%eax"], ["movswl 0x56(%ebp,%ebx,4),%eax"]),
]


def self_test():
    """Half of these must FAIL to match.  A looser check needs both halves."""
    def rows(src):
        return [tuple(s.split(None, 1)) if " " in s else (s, "") for s in src]
    bad = 0
    for name, want, a, b in SELF_TESTS:
        got = alpha_equal(rows(a), rows(b))
        if got != want:
            bad += 1
        print("  %s  %-56s want=%-5s got=%s"
              % ("ok  " if got == want else "FAIL", name, want, got))
    print("\n  %d case(s), %d accept, %d reject, %d failure(s)"
          % (len(SELF_TESTS), sum(1 for c in SELF_TESTS if c[1]),
             sum(1 for c in SELF_TESTS if not c[1]), bad))
    return 1 if bad else 0


def _staleness():
    """Newest source and newest period object, when the source is newer.

    A SILENT STALE MEASUREMENT IS THE FAILURE MODE THIS TOOL IS MOST PRONE TO.
    `build/tc_out` is written only by `tools/toolchain/build.sh`; the gate does
    not build it and no Makefile rule depends on it, so any change to `src/`
    leaves it behind while every count here keeps rendering as a clean,
    plausible, WRONG number.  That happened: a merge's grades were read off
    objects compiled before the merge, and the figures looked entirely normal.
    Findings F2400 and F2401 are the same shape -- a detector reporting on
    nothing and rendering as a pass.
    """
    newest_obj = newest_src = None
    for o in glob.glob(os.path.join(OURS, "*.o")):
        m = os.path.getmtime(o)
        if newest_obj is None or m > newest_obj[0]:
            newest_obj = (m, o)
    if newest_obj is None:
        return None, None
    for root in ("src", "include"):
        for dp, _, fns in os.walk(root):
            for fn in fns:
                if not fn.endswith((".c", ".cpp", ".h", ".hpp", ".inc")):
                    continue
                f = os.path.join(dp, fn)
                m = os.path.getmtime(f)
                if newest_src is None or m > newest_src[0]:
                    newest_src = (m, f)
    if newest_src is None or newest_src[0] <= newest_obj[0]:
        return None, None
    fmt = lambda p: "%s  (%s)" % (
        p[1], __import__("time").strftime("%Y-%m-%d %H:%M:%S",
                                         __import__("time").localtime(p[0])))
    return fmt(newest_src), fmt(newest_obj)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list-exact", action="store_true")
    ap.add_argument("--near", type=int, metavar="N", default=0,
                    help="list the N symbols closest to closing, by |instruction "
                         "delta| with alignment padding stripped")
    ap.add_argument("--why", metavar="SYMBOL",
                    help="print the row alpha_equal rejects SYMBOL on")
    ap.add_argument("--self-test", action="store_true",
                    help="prove alpha_equal both accepts and REJECTS")
    ap.add_argument("--limit", type=int, default=25)
    a = ap.parse_args()

    if a.self_test:
        return self_test()

    if a.near:
        #
        # SORT BY DELTA, NOT BY SIZE.  A brief that picked targets by BLOB SIZE
        # sent an agent at symbols whose deltas were -99, +16, -40, +20 and -32
        # while calling them "the closest to reach"; the real near misses were
        # plus or minus one to three and none of them was on the list (7865).
        # Size is how big the function is.  Delta is how far away we are.
        #
        blob = sizes(BLOB)
        ours = {}
        for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
            for s in sizes(o):
                ours.setdefault(s, o)
        rows = []
        for k in sorted(x for x in ours if x in blob):
            ab, ar = body(BLOB, k)
            bb, br = body(ours[k], k)
            v, nd = verdict(ab, ar, bb, br)
            if v in ("EXACT", "UNRESOLVED", "NODATA"):
                continue
            try:
                ia = [r for r in insns(BLOB, k) if not _padding(*r)]
                ib = [r for r in insns(ours[k], k) if not _padding(*r)]
            except Exception:
                continue
            rows.append((abs(len(ia) - len(ib)), len(ib) - len(ia), v, blob[k], k))
        rows.sort(key=lambda r: (r[0], -r[3]))
        print("  %d symbol(s) not yet exact, nearest first by |instruction delta|"
              "\n  (delta is OURS minus BLOB, padding stripped)\n" % len(rows))
        print("  %-6s %-9s %8s   %s" % ("delta", "grade", "blob B", "symbol"))
        for _, d, v, sz, k in rows[:a.near]:
            print("  %+6d %-9s %8d   %s" % (d, v, sz, k[:70]))
        return 0

    if a.why:
        k = a.why
        ours = {}
        for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
            for s in sizes(o):
                ours.setdefault(s, o)
        if k not in ours:
            sys.exit("byteident.py: %s is not defined by any object in %s"
                     % (k, OURS))
        if k not in sizes(BLOB):
            sys.exit("byteident.py: the blob does not define %s" % k)
        ab, ar = body(BLOB, k)
        bb, br = body(ours[k], k)
        v, nd = verdict(ab, ar, bb, br)
        print("  %s\n  grade 0 verdict: %s%s"
              % (k, v, "" if not nd else "  (%d byte(s) differ)" % nd))
        why = alpha_why(insns(BLOB, k), insns(ours[k], k))
        print("  grade 1 verdict: %s" % ("ACCEPT" if why is None else "REJECT"))
        if why is not None:
            print("    %s" % why)
        return 0

    blob = sizes(BLOB)
    if not blob:
        sys.exit("byteident.py: NO SYMBOLS read from the blob at %s.\n"
                 "  Every count below would be computed against NOTHING and\n"
                 "  would render as a clean zero.  From a worktree, BLOB must\n"
                 "  be explicit.  Findings F2400, F2401." % BLOB)
    ours = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, o)
    if not ours:
        sys.exit("byteident.py: no objects in %s -- run "
                 "tools/toolchain/build.sh first." % OURS)
    stale_src, stale_obj = _staleness()
    if stale_src:
        sys.exit(
            "byteident.py: %s IS STALE and every number below would be a\n"
            "  measurement of a tree that no longer exists.\n\n"
            "    newest source : %s\n"
            "    newest object : %s\n\n"
            "  `make phase` does NOT build %s -- it is written only by\n"
            "  tools/toolchain/build.sh, so a merge that changes src/ leaves\n"
            "  these objects behind without touching anything the gate reads.\n"
            "  This reported pre-merge grades as current once already.\n\n"
            "  Run:  sh tools/toolchain/build.sh"
            % (OURS, stale_src, stale_obj, OURS))

    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit("byteident.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % OURS)

    buckets = {"EXACT": [], "UNRESOLVED": [], "REGALLOC": [], "RELOC": [],
               "BYTES": [], "SIZE": [], "NODATA": []}
    for k in common:
        ab, ar = body(BLOB, k)
        bb, br = body(ours[k], k)
        v, n = verdict(ab, ar, bb, br)
        #
        # GRADE 1 IS CHECKED ONLY WHERE GRADE 0 FAILED.  A byte-identical
        # function is trivially alpha-equal and asking again costs two
        # disassemblies for no information.
        #
        #
        # RELOC IS NOT A CANDIDATE FOR GRADE 1.  A differing relocation target
        # means the function calls or reads something else, which no amount of
        # register renaming makes equivalent; letting `alpha_equal` overrule it
        # promoted two destructors that call a different symbol.
        #
        if v not in ("EXACT", "UNRESOLVED", "NODATA", "RELOC"):
            if alpha_equal(insns(BLOB, k), insns(ours[k], k)):
                v, n = "REGALLOC", 0
        buckets[v].append((n, blob[k], k))

    n = len(common)
    print("  blob    : %s" % BLOB)
    print("  ours    : %s\n" % OURS)
    print("Positional byte identity over %d symbol(s) both define.\n" % n)
    ex = len(buckets["EXACT"])
    un = len(buckets["UNRESOLVED"])
    ra = len(buckets["REGALLOC"])
    print("  grade 0  EXACT      same bytes, same places  : %4d  (%.1f%%)"
          % (ex, 100.0 * ex / n))
    print("           UNRESOLVED as EXACT, but a section  : %4d" % un)
    print("                      relocation cannot be")
    print("                      compared by name (604)")
    print("  grade 1  REGALLOC   same instructions and    : %4d" % ra)
    print("                      operands, renamed per")
    print("                      live range")
    print("           ------------------------------------------")
    print("           grade 0 or 1                        : %4d  (%.1f%%)"
          % (ex + un + ra, 100.0 * (ex + un + ra) / n))
    print("  RELOC   -- bytes agree, a target differs : %4d" % len(buckets["RELOC"]))
    print("  BYTES   -- same size, bytes differ       : %4d" % len(buckets["BYTES"]))
    print("  SIZE    -- different size                : %4d" % len(buckets["SIZE"]))
    if buckets["NODATA"]:
        print("  NODATA  -- could not be disassembled     : %4d" % len(buckets["NODATA"]))

    if a.list_exact:
        print("\nEXACT:")
        for _, sz, k in sorted(buckets["EXACT"], key=lambda r: -r[1]):
            print("  %6d  %s" % (sz, k))
        return 0

    for name, label in (("RELOC", "differing relocation target(s)"),
                        ("BYTES", "differing byte(s)")):
        rows = sorted(buckets[name], key=lambda r: r[0])[:a.limit]
        if rows:
            print("\n%s, closest first -- %s:" % (name, label))
            for d, sz, k in rows:
                print("  %5d of %5d  %s" % (d, sz, k))
    return 0


if __name__ == "__main__":
    sys.exit(main())
