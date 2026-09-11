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
`R_386_32` or `R_386_PC32` holds an inline addend that the linker combines with
the relocation destination.  Different section layouts can encode the same
destination differently, so each relocated field is compared by relocation
type and canonical destination, retaining the addend for both kinds.
A function that is byte-equal everywhere else but CALLS SOMETHING
DIFFERENT is reported separately, as `RELOC`, because that is a real
difference this tool must not hide.

Function identity here compares relocation destination identity.  Symbol
binding and visibility remain separate whole-object checks; canonicalizing a
destination does not prove the same symbol interposition or export behavior.

    tools/toolchain/byteident.py                  counts, then the misses
    tools/toolchain/byteident.py --list-exact     the exact set, for a diff
    BLOB=/abs/path/to/dsplibs.o TC_OUT=build/tc_out tools/toolchain/byteident.py

Denominator: the symbols the blob and `TC_OUT` both define, which is
`compare.py`'s denominator exactly, so the two numbers are comparable.
"""

import argparse
import functools
import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

#
# THE RATCHET, mirroring `compare.py`'s -- same reasoning, a different metric.
#
# `compare.py --ratchet` catches the reconstruction moving away from the
# original's CODE GENERATION (mnemonic sequence) with no differential test
# able to see it.  This one catches the same failure mode one grade stricter:
# a symbol that was byte-for-byte EXACT losing that status -- to a size
# change, a relocation change, or merely a different register allocation --
# while `make period` stays green throughout, because a differential test
# proves behavioural equivalence, not code-generation equivalence, and this
# tree's own history (findings F2155, F612, F616, 7796, 7800, 7770) is a
# record of exactly that gap.  Only EXACT ratchets, for the reason
# `compare.py`'s own comment gives for its `identical`: REGALLOC (grade 1)
# falls when a function GRADUATES to EXACT, so gating on it fails on
# progress.  REGALLOC is reported with its direction and is informational.
#
# The floor is the exact SYMBOL SET, not only its cardinality.  An aggregate
# count lets one function quietly fall out of EXACT while another enters it:
# the headline remains unchanged and the regression is certified.  That is
# precisely backwards for a ratchet -- every symbol it has banked must remain
# banked.  Keep the counts as redundant, human-readable integrity checks and
# for the existing REGALLOC note, but decide grade 0 from membership.
#
RATCHET = os.path.join(HERE, "byteident_ratchet.json")
RATCHET_EXACT_SYMBOLS = "exact_symbols"


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


@functools.lru_cache(maxsize=None)
def section_symbols(path):
    """Section names and all symbol starts, retaining ambiguity, per object.

    Read SECTION records as well as ordinary symbols: equal numeric values in
    different sections do not identify the same address.  Count records, not
    just distinct names, so duplicate local names cannot create a false match.
    """
    out = subprocess.run(["readelf", "--syms", "--wide", path],
                         capture_output=True, text=True, check=True).stdout
    sections, starts, objects = {}, {}, {}
    for line in out.splitlines():
        fields = line.split()
        if len(fields) != 8 or not fields[0].endswith(":"):
            continue
        _, value, size, kind, _, _, index, name = fields
        if not index.isdigit():
            continue
        if kind == "SECTION":
            sections.setdefault(name, []).append(index)
        elif kind in ("OBJECT", "FUNC", "NOTYPE"):
            starts.setdefault((index, int(value, 16)), []).append(name)
            if kind == "OBJECT" and int(size, 0):
                objects.setdefault(index, []).append(
                    (int(value, 16), int(size, 0), name))
    return sections, starts, mergeable_entries(path), objects


def mergeable_entries(path):
    """ELF SHF_MERGE entries: (entry size, is string, bytes), by section."""
    out = subprocess.run(["readelf", "--section-headers", "--wide", path],
                         capture_output=True, text=True, check=True).stdout
    entries = {}
    with open(path, "rb") as obj:
        for line in out.splitlines():
            match = re.match(r"\s*\[\s*(\d+)\]\s+(.*)", line)
            if not match:
                continue
            fields = match.group(2).split()
            if len(fields) < 10:
                continue
            _, kind, _, offset, size, entsize, flags = fields[:7]
            if kind != "PROGBITS" or "M" not in flags:
                continue
            entry_size, is_string = int(entsize, 16), "S" in flags
            if not entry_size or (is_string and entry_size != 1):
                continue
            obj.seek(int(offset, 16))
            data = obj.read(int(size, 16))
            if len(data) != int(size, 16):
                raise ValueError("truncated ELF merge section in " + path)
            entries[match.group(1)] = (entry_size, is_string, data)
    return entries


def relocation_target(kind, target, field, symbols):
    """Canonical R_386_32 target from unique symbols or ELF merge entries.

    ELF/i386 REL stores its addend in the four relocated bytes, not in the
    relocation record.  Object interiors require a unique nonzero-sized OBJECT
    interval; nearest symbols, overlapping objects and aliases do not resolve.
    Unresolved section offsets and named-symbol addends remain in tagged,
    hashable tuples, so neither byte masking nor synthetic-looking symbol
    names can erase a distinction.  PC-relative addends are retained but
    section-relative PC32 destinations are not resolved yet.
    """
    sections, starts, entries, objects = symbols
    tag = "section" if target in sections else "symbol"
    if kind not in ("R_386_32", "R_386_PC32"):
        return (tag, target, 0)
    if len(field) != 4:
        raise ValueError(kind + " relocation needs four addend bytes")
    addend = int.from_bytes(field, "little")
    indices = sections.get(target, [])
    if kind == "R_386_32" and len(indices) == 1:
        names = starts.get((indices[0], addend), [])
        containing = [(start, size, name)
                      for start, size, name in objects.get(indices[0], [])
                      if start <= addend < start + size]
        if len(names) == 1 and (not containing or
                               len(containing) == 1 and
                               containing[0][0] == addend and
                               containing[0][2] == names[0]):
            return ("symbol", names[0], 0)
        if not names and len(containing) == 1:
            start, _, name = containing[0]
            if starts.get((indices[0], start)) == [name]:
                return ("symbol", name, addend - start)
        # Linkers may merge identical strings and suffixes at different
        # offsets.  SHF_MERGE also licenses fixed-size entries, compared in
        # full together with the offset inside the entry.  Ordinary rodata
        # never takes this path, nor do ambiguous symbol starts/intervals.
        if not names and not containing and indices[0] in entries:
            entry_size, is_string, data = entries[indices[0]]
            if is_string:
                end = data.find(b"\0", addend)
                if end >= 0:
                    return ("merge-string", data[addend:end + 1])
            elif entry_size and addend < len(data):
                start = addend - addend % entry_size
                end = start + entry_size
                if end <= len(data):
                    return ("merge-entry", entry_size, data[start:end], addend - start)
    return (tag, target, addend)


def render_relocation(target):
    """Text for grade-1 operands/diagnostics; grade 0 compares the tuples.

    Encode byte payloads as hex so a string containing register-like text
    cannot participate in register renaming.  Tuple tags keep the rendering
    distinct from real symbol names even when they resemble synthetic names.
    """
    return repr(tuple(item.hex() if isinstance(item, bytes) else item
                      for item in target)).replace("%", r"\x25")


# Grade 1 reuses the canonical grade-0 body for every non-exact candidate.
@functools.lru_cache(maxsize=None)
def body(path, sym):
    """(bytes, {offset: (type, canonical target)}), offsets relative."""
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
    raw = bytes(data.get(i, 0) for i in range(n))
    symbols = section_symbols(path) if relocs else ({}, {}, {}, {})
    relocs = {off: (kind, relocation_target(kind, target, raw[off:off + 4],
                                           symbols))
              for off, (kind, target) in relocs.items()}
    return raw, relocs


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
    _, relocs = body(path, sym)
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
                off = int(r.group(1), 16) - base
                tag = "@%s:%s" % (relocs[off][0], render_relocation(relocs[off][1]))
                literal = r"\$?0x[0-9a-f]+"
                candidates = list(re.finditer(literal, rows[-1][1]))
                #
                # A CALL HAS NO NUMERIC LITERAL LEFT TO REPLACE -- its operand
                # is a bare address already rewritten to `.+N` above -- so a
                # substitution alone drops the target on the floor, and two
                # calls to DIFFERENT functions compare equal.  That is not
                # hypothetical: `V90PreFilter`'s destructors call
                # `FloatFIR::~FloatFIR` D2 in the blob and D1 in ours, and
                # both were being certified as "same instructions and
                # operands".  Append where nothing was replaced.  Multiple
                # hex literals can include unrelated immediates (cmpl $1,
                # address); preserve all operands and append in that case.
                # A conservative false rejection is safer than erasing $1.
                #
                if len(candidates) == 1:
                    rows[-1][1] = re.sub(literal, lambda _: tag, rows[-1][1])
                else:
                    rows[-1][1] = (rows[-1][1] + " " + tag).strip()
            continue
        at = int(m.group(1), 16)
        if base is None:
            base = at
        instruction = line.split("\t", 1)[1].strip()
        instruction = instruction.split("#", 1)[0].strip()
        instruction = re.sub(r"<[^>]*>", "", instruction).strip()
        parts = instruction.split(None, 1)
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
    # Replace a sole hex literal with the relocation's canonical TARGET.
    # Where the operands contain several literals, keep them all and append
    # the target: choosing one without decoding its byte field could erase a
    # different immediate.  Zero literals also requires appending the target.
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
    # A section tag means destination identity was not proved.  Equal section
    # names and offsets in separate objects do not supply that missing proof,
    # so inspect these even when the relocation dictionaries compare equal.
    hard = soft = 0
    for k in set(ra) | set(rb):
        x, y = ra.get(k), rb.get(k)
        if x is None or y is None or x[0] != y[0]:
            hard += 1
        elif x[1][0] == "section" or y[1][0] == "section":
            soft += 1
        elif x != y:
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
    return 1 if relocation_self_test() or bad else 0


def relocation_self_test():
    """Proven symbol/merge identities must fire, and near misses must refuse."""
    from io import BytesIO
    from types import SimpleNamespace
    from unittest.mock import patch

    symbols = ({".data": ["1"], ".bss": ["2"], ".rodata.str1.1": ["3"]}, {
        ("1", 0x30): ["vpcm_op"],
        ("1", 0x3a0): ["v92TxPreFilter"],
        ("2", 0x8ec): ["bInternalBeepInProgress"],
        ("1", 0x100): ["first", "alias"],
        ("1", 0x200): ["duplicate", "duplicate"],
    }, {"3": (1, True, b"same\0different\0same\0unterminated")}, {})
    cases = [
        ("operation table resolves", "R_386_32", ".data", 0x30, ("symbol", "vpcm_op", 0)),
        ("filter resolves", "R_386_32", ".data", 0x3a0, ("symbol", "v92TxPreFilter", 0)),
        ("beep flag resolves", "R_386_32", ".bss", 0x8ec,
         ("symbol", "bInternalBeepInProgress", 0)),
        ("inside unsized symbol refused", "R_386_32", ".data", 0x31, ("section", ".data", 0x31)),
        ("wrong section refused", "R_386_32", ".bss", 0x30, ("section", ".bss", 0x30)),
        ("two names refused", "R_386_32", ".data", 0x100, ("section", ".data", 0x100)),
        ("duplicate names refused", "R_386_32", ".data", 0x200, ("section", ".data", 0x200)),
        ("named addend retained", "R_386_32", "vpcm_op", 4, ("symbol", "vpcm_op", 4)),
        ("full unsigned addend retained", "R_386_32", "vpcm_op", 0xffffffff,
         ("symbol", "vpcm_op", 0xffffffff)),
        ("PC-relative resolution excluded", "R_386_PC32", ".data", 0x30,
         ("section", ".data", 0x30)),
        ("PC-relative addend retained", "R_386_PC32", "callee", 0xfffffffc,
         ("symbol", "callee", 0xfffffffc)),
        ("mergeable string resolves by bytes", "R_386_32", ".rodata.str1.1", 0,
         ("merge-string", b"same\0")),
        ("unterminated string refused", "R_386_32", ".rodata.str1.1", 20,
         ("section", ".rodata.str1.1", 20)),
        ("out-of-section string offset refused", "R_386_32", ".rodata.str1.1", 99,
         ("section", ".rodata.str1.1", 99)),
    ]
    checks = []
    for label, kind, target, addend, want in cases:
        got = relocation_target(kind, target, addend.to_bytes(4, "little"), symbols)
        checks.append((label, got == want))
    ambiguous_sections = ({".data": ["1", "3"]}, *symbols[1:])
    checks.append(("duplicate section names refused", relocation_target(
        "R_386_32", ".data", (0x30).to_bytes(4, "little"), ambiguous_sections)
        == ("section", ".data", 0x30)))
    for kind in ("R_386_32", "R_386_PC32"):
        try:
            relocation_target(kind, ".data", b"\x30\x00\x00", symbols)
        except ValueError:
            checks.append(("truncated " + kind + " addend refused", True))
        else:
            checks.append(("truncated " + kind + " addend refused", False))

    def sample(target, addend, kind="R_386_32"):
        raw = b"\xa1" + addend.to_bytes(4, "little") + b"\xc3"
        canonical = relocation_target(kind, target, raw[1:5], symbols)
        return raw, {1: (kind, canonical)}

    for label, left, right, want in [
            ("resolved section is EXACT", (".data", 0x30), ("vpcm_op", 0), "EXACT"),
            ("wrong section addend is UNRESOLVED", (".data", 0x31),
             ("vpcm_op", 0), "UNRESOLVED"),
            ("ambiguous start is UNRESOLVED", (".data", 0x100),
             ("first", 0), "UNRESOLVED"),
            ("equal ambiguous starts remain UNRESOLVED", (".data", 0x100),
             (".data", 0x100), "UNRESOLVED"),
            ("equal unproved section offsets remain UNRESOLVED", (".data", 0x31),
             (".data", 0x31), "UNRESOLVED"),
            ("named addend difference is RELOC", ("vpcm_op", 0),
             ("vpcm_op", 4), "RELOC"),
            ("section addend difference is UNRESOLVED", (".data", 0x31),
             (".data", 0x32), "UNRESOLVED"),
            ("equal strings at different offsets are EXACT", (".rodata.str1.1", 0),
             (".rodata.str1.1", 15), "EXACT"),
            ("different string bytes are RELOC", (".rodata.str1.1", 0),
             (".rodata.str1.1", 5), "RELOC"),
            ("PC32 addend -4 versus zero is RELOC", ("callee", 0xfffffffc, "R_386_PC32"),
             ("callee", 0, "R_386_PC32"), "RELOC"),
            ("equal PC32 addends are EXACT", ("callee", 0xfffffffc, "R_386_PC32"),
             ("callee", 0xfffffffc, "R_386_PC32"), "EXACT"),
            ("differing relocation kinds are RELOC", ("callee", 0, "R_386_32"),
             ("callee", 0, "R_386_PC32"), "RELOC"),
            ("section-looking symbol names are RELOC", (".data_fake", 0),
             ("other", 0), "RELOC"),
            ("symbol and merged-string names cannot collide", ("string:73616d6500", 0),
             (".rodata.str1.1", 0), "RELOC"),
            ("symbol addend and literal name cannot collide", ("vpcm_op+0x4", 0),
             ("vpcm_op", 4), "RELOC")]:
        checks.append((label, verdict(*sample(*left), *sample(*right))[0] == want))

    def merge_target(data, entry_size, offset):
        metadata = ({".rodata.cst": ["1"]}, {},
                    {"1": (entry_size, False, data)}, {})
        return relocation_target("R_386_32", ".rodata.cst",
                                 offset.to_bytes(4, "little"), metadata)

    for label, left, right, equal in [
            ("identical full merge entries resolve", (b"abcdabcd", 4, 1),
             (b"abcdabcd", 4, 5), True),
            ("differing full merge entries refuse", (b"abcd", 4, 1),
             (b"zbcd", 4, 1), False),
            ("merge entry sizes must match", (b"abcd", 4, 1),
             (b"abcd", 2, 1), False),
            ("merge intra-entry offsets must match", (b"abcd", 4, 1),
             (b"abcd", 4, 2), False)]:
        checks.append((label, (merge_target(*left) == merge_target(*right)) == equal))
    for label, data, entry_size, offset in [
            ("truncated merge entry refused", b"abcdef", 4, 5),
            ("merge section end refused", b"abcd", 4, 4),
            ("merge offset out of range refused", b"abcd", 4, 10),
            ("zero merge entry size refused", b"abcd", 0, 1)]:
        checks.append((label, merge_target(data, entry_size, offset) ==
                       ("section", ".rodata.cst", offset)))

    object_symbols = ({".data": ["1"]}, {
        ("1", 0x10): ["FIFO_CFG"], ("1", 0x30): ["fixedRc_UpFact"],
        ("1", 0x50): ["FrameNames"], ("1", 0x70): ["wide"],
        ("1", 0x78): ["overlap"], ("1", 0x90): ["alias_a", "alias_b"],
    }, {}, {"1": [(0x10, 8, "FIFO_CFG"), (0x30, 16, "fixedRc_UpFact"),
                  (0x50, 8, "FrameNames"), (0x70, 16, "wide"),
                  (0x78, 8, "overlap"), (0x90, 8, "alias_a"),
                  (0x90, 8, "alias_b")]})
    for label, offset, want in [
            ("FIFO_CFG interior resolves", 0x14, ("symbol", "FIFO_CFG", 4)),
            ("fixedRc_UpFact interior resolves", 0x38, ("symbol", "fixedRc_UpFact", 8)),
            ("FrameNames interior resolves", 0x54, ("symbol", "FrameNames", 4)),
            ("sized object exact start resolves", 0x10, ("symbol", "FIFO_CFG", 0)),
            ("object boundary end refused", 0x18, ("section", ".data", 0x18)),
            ("overlapping object interiors refused", 0x7c, ("section", ".data", 0x7c)),
            ("overlapping object start refused", 0x78, ("section", ".data", 0x78)),
            ("aliased object interior refused", 0x94, ("section", ".data", 0x94))]:
        checks.append((label, relocation_target("R_386_32", ".data",
                       offset.to_bytes(4, "little"), object_symbols) == want))

    collision_targets = [
        ("symbol", "string:73616d6500", 0), ("merge-string", b"same\0"),
        ("symbol", "merge:4:73616d65+0x1", 0), ("merge-entry", 4, b"same", 1),
        ("symbol", "x+0x4", 0), ("symbol", "x", 4), ("section", "x", 4),
    ]
    checks.append(("tagged destinations are hashable and collision-free",
                   len(set(collision_targets)) == len(collision_targets)))
    checks.append(("diagnostic rendering keeps destination kinds distinct",
                   len({render_relocation(t) for t in collision_targets}) ==
                   len(collision_targets)))

    # Exercise both objdump consumers and the ELF metadata reader together.
    # The fixture has identical bytes in mergeable and ordinary rodata; only
    # the former may be interpreted as a string.
    def fixture_run(command, **kwargs):
        if "--syms" in command:
            output = ("1: 00000000 0 SECTION LOCAL DEFAULT 1 .data\n"
                      "2: 00000030 24 OBJECT LOCAL DEFAULT 1 vpcm_op\n"
                      "3: 00000000 0 SECTION LOCAL DEFAULT 3 .rodata.str1.1\n"
                      "4: 00000000 0 SECTION LOCAL DEFAULT 4 .rodata\n"
                      "5: 00000000 0 SECTION LOCAL DEFAULT 5 .rodata.cst4\n")
        elif "--section-headers" in command:
            output = ("[ 3] .rodata.str1.1 PROGBITS 00000000 000000 000005 01 AMS 0 0 1\n"
                      "[ 4] .rodata PROGBITS 00000000 000000 000005 00 A 0 0 1\n"
                      "[ 5] .rodata.cst4 PROGBITS 00000000 000000 000004 04 AM 0 0 4\n")
        elif any(arg in ("--disassemble=compare1", "--disassemble=compare2")
                 for arg in command):
            immediate = 1 if "--disassemble=compare1" in command else 2
            if "--no-show-raw-insn" in command:
                output = ("0:\tcmpl $0x%x,0x30\n 2: R_386_32 .data\n7:\tret\n"
                          % immediate)
            else:
                output = ("0:\t83 3d 30 00 00 00 %02x \tcmpl $0x%x,0x30\n"
                          " 2: R_386_32 .data\n7:\tc3 \tret\n"
                          % (immediate, immediate))
        elif "--disassemble=call" in command:
            if "--no-show-raw-insn" in command:
                output = "0:\tcall 1\n 1: R_386_PC32 callee\n5:\tret\n"
            else:
                output = "0:\te8 fc ff ff ff \tcall 1\n 1: R_386_PC32 callee\n5:\tc3 \tret\n"
        elif any(arg in ("--disassemble=absolute", "--disassemble=relative")
                 for arg in command):
            relative = "--disassemble=relative" in command
            register = "ecx" if relative else "eax"
            kind = "R_386_PC32" if relative else "R_386_32"
            if "--no-show-raw-insn" in command:
                output = ("0:\tmov 0x0,%%%s\n 2: %s vpcm_op\n6:\tret\n"
                          % (register, kind))
            else:
                output = ("0:\t8b %s 00 00 00 00 \tmov 0x0,%%%s\n"
                          " 2: %s vpcm_op\n6:\tc3 \tret\n"
                          % ("0d" if relative else "05", register, kind))
        elif "--no-show-raw-insn" in command:
            output = "0:\tmov 0x30,%eax\n 1: R_386_32 .data\n5:\tret\n"
        else:
            output = "0:\ta1 30 00 00 00 \tmov 0x30,%eax\n 1: R_386_32 .data\n5:\tc3 \tret\n"
        return SimpleNamespace(stdout=output)

    fixture_path = "<byteident-relocation-self-test>"
    with patch.object(subprocess, "run", side_effect=fixture_run), \
            patch("builtins.open", return_value=BytesIO(b"same\0")):
        raw, relocs = body(fixture_path, "fixture")
        checks.append(("body preserves bytes and resolves inline addend",
                       raw == b"\xa1\x30\0\0\0\xc3" and
                       relocs == {1: ("R_386_32", ("symbol", "vpcm_op", 0))}))
        checks.append(("instruction operands use canonical relocation",
                       insns(fixture_path, "fixture") ==
                       [("mov", "@R_386_32:('symbol', 'vpcm_op', 0),%eax"), ("ret", "")]))
        compare1 = insns(fixture_path, "compare1")
        compare2 = insns(fixture_path, "compare2")
        checks.append(("relocated compare preserves unrelated immediate",
                       compare1[0] == ("cmpl", "$0x1,0x30 @R_386_32:('symbol', 'vpcm_op', 0)")))
        checks.append(("relocated cmpl $1 versus $2 must reject grade 1",
                       not alpha_equal(compare1, compare2)))
        checks.append(("zero-literal PC32 call retains destination and addend",
                       insns(fixture_path, "call") ==
                       [("call", ".+1 @R_386_PC32:('symbol', 'callee', 4294967292)"), ("ret", "")]))
        checks.append(("register change cannot hide a relocation-kind change",
                       verdict(*body(fixture_path, "absolute"),
                               *body(fixture_path, "relative"))[0] == "BYTES" and
                       not alpha_equal(insns(fixture_path, "absolute"),
                                       insns(fixture_path, "relative"))))
        parsed = section_symbols(fixture_path)
        checks.append(("ELF symbol sizes license object interior resolution",
                       relocation_target("R_386_32", ".data", b"\x34\0\0\0",
                                         parsed) == ("symbol", "vpcm_op", 4)))
        checks.append(("ELF merge flag and entry size license fixed entries",
                       relocation_target("R_386_32", ".rodata.cst4", b"\1\0\0\0",
                                         parsed) == ("merge-entry", 4, b"same", 1)))
        checks.append(("ELF flags restrict string resolution",
                       relocation_target("R_386_32", ".rodata.str1.1", b"\0" * 4,
                                         parsed) == ("merge-string", b"same\0") and
                       relocation_target("R_386_32", ".rodata", b"\0" * 4,
                                         parsed) == ("section", ".rodata", 0)))
    section_symbols.cache_clear()
    bad = sum(not ok for _, ok in checks)
    for label, ok in checks:
        print("  %s  %s" % ("ok  " if ok else "FAIL", label))
    print("\n  %d relocation case(s), %d failure(s)" % (len(checks), bad))
    return 1 if bad else 0


def _staleness():
    """Newest source and newest period object, when the source is newer.

    A SILENT STALE MEASUREMENT IS THE FAILURE MODE THIS TOOL IS MOST PRONE TO.
    `build/tc_out` is written by `tools/toolchain/period.mk` and by nothing
    else.  `make phase` still does not build it -- it needs docker and the
    toolchain image, which not every checkout has.  `make byteident` DOES
    now, and used not to, which is why this guard was written: any change to
    `src/` left the objects behind while every count here kept rendering as a
    clean, plausible, WRONG number.  That happened: a merge's grades were
    read off objects compiled before the merge, and the figures looked
    entirely normal.  The dependency is the real fix; this stays because it
    also catches the directory being read by something that is not make.
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


def _ratchet_membership(was, now):
    """Validate a membership baseline and return (lost, gained) exact symbols.

    Older ratchets stored only an aggregate `exact` count.  They cannot tell
    whether a replacement symbol concealed a regression, so accepting one
    would weaken this check exactly where it is meant to be strict.  Refuse
    it with a re-blessing instruction rather than silently retaining the old
    count-only behaviour.
    """
    if not isinstance(was, dict):
        raise ValueError("baseline is not a JSON object")
    if RATCHET_EXACT_SYMBOLS not in was:
        raise ValueError(
            "baseline is legacy aggregate-only data (no %r); run --update "
            "to record the current exact-symbol set" % RATCHET_EXACT_SYMBOLS)
    for label, row in (("baseline", was), ("current result", now)):
        symbols = row.get(RATCHET_EXACT_SYMBOLS)
        if (not isinstance(symbols, list) or
                any(not isinstance(k, str) for k in symbols)):
            raise ValueError("%s %r must be a list of symbol names"
                             % (label, RATCHET_EXACT_SYMBOLS))
        if len(set(symbols)) != len(symbols):
            raise ValueError("%s %r contains duplicate symbol names"
                             % (label, RATCHET_EXACT_SYMBOLS))
        for field in ("exact", "regalloc", "compared"):
            value = row.get(field)
            if type(value) is not int or value < 0:
                raise ValueError("%s %r must be a non-negative integer"
                                 % (label, field))
        if row["exact"] != len(symbols):
            raise ValueError("%s exact count (%d) disagrees with its %r (%d)"
                             % (label, row["exact"], RATCHET_EXACT_SYMBOLS,
                                len(symbols)))
    return (sorted(set(was[RATCHET_EXACT_SYMBOLS]) -
                   set(now[RATCHET_EXACT_SYMBOLS])),
            sorted(set(now[RATCHET_EXACT_SYMBOLS]) -
                   set(was[RATCHET_EXACT_SYMBOLS])))


def ratchet_self_test():
    """Exercise the strict direction: equal counts must not hide a loss."""
    def row(symbols):
        return {"exact": len(symbols), "regalloc": 53, "compared": 1852,
                RATCHET_EXACT_SYMBOLS: symbols}

    cases = [
        ("a replacement cannot conceal an exact-symbol loss", row(["a", "b"]),
         row(["b", "c"]), (["a"], ["c"])),
        ("an exact-set gain loses nothing", row(["a", "b"]),
         row(["a", "b", "c"]), ([], ["c"])),
    ]
    bad = 0
    for name, was, now, want in cases:
        got = _ratchet_membership(was, now)
        ok = got == want
        bad += not ok
        print("  %s  %-56s want=%s got=%s"
              % ("ok  " if ok else "FAIL", name, want, got))
    try:
        _ratchet_membership({"exact": 2, "regalloc": 53, "compared": 1852},
                            row(["a", "b"]))
        ok = False
    except ValueError as e:
        ok = "legacy aggregate-only" in str(e)
    bad += not ok
    print("  %s  %-56s legacy count-only baseline is rejected"
          % ("ok  " if ok else "FAIL", ""))
    print("\n  %d case(s), %d failure(s)" % (len(cases) + 1, bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list-exact", action="store_true")
    ap.add_argument("--comdat", action="store_true",
                    help="list symbols whose defining objects disagree")
    ap.add_argument("--near", type=int, metavar="N", default=0,
                    help="list the N symbols closest to closing, by |instruction "
                         "delta| with alignment padding stripped")
    ap.add_argument("--why", metavar="SYMBOL",
                    help="print the row alpha_equal rejects SYMBOL on")
    ap.add_argument("--self-test", action="store_true",
                    help="prove alpha_equal and relocation resolution accept and REJECT")
    ap.add_argument("--ratchet-self-test", action="store_true",
                    help="prove the exact-symbol ratchet rejects a masked loss")
    ap.add_argument("--limit", type=int, default=25)
    ap.add_argument("--ratchet", action="store_true",
                    help="fail if any previously grade-0 EXACT symbol regresses")
    ap.add_argument("--update", action="store_true",
                    help="record the current exact-symbol set as the new floor")
    ap.add_argument("--json-out", metavar="PATH",
                    help="write the final exact/regalloc counts as JSON")
    a = ap.parse_args()

    if a.self_test:
        return self_test()
    if a.ratchet_self_test:
        return ratchet_self_test()

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
    #
    # A COMDAT SYMBOL IS DEFINED BY EVERY TU THAT INSTANTIATES IT, and this
    # used to `setdefault` over a sorted glob -- scoring whichever copy the
    # filesystem named first and silently ignoring the rest.  **34 symbols are
    # multiply defined and 4 of them get DIFFERENT verdicts depending on which
    # copy is read**, so the grade was decided by glob order (F8146).
    #
    # Every defining object is scored and the WORST verdict wins.  The blob has
    # one copy because the linker picked one; we cannot know which, so claiming
    # the best would be claiming the luckiest.  `--comdat` lists the ones that
    # disagree.
    #
    ours = {}
    allobjs = {}
    for o in sorted(glob.glob(os.path.join(OURS, "*.o"))):
        for k, v in sizes(o).items():
            ours.setdefault(k, o)
            allobjs.setdefault(k, []).append(o)
    if not ours:
        sys.exit("byteident.py: no objects in %s -- run "
                 "`make tc` first." % OURS)
    stale_src, stale_obj = _staleness()
    if stale_src:
        sys.exit(
            "byteident.py: %s IS STALE and every number below would be a\n"
            "  measurement of a tree that no longer exists.\n\n"
            "    newest source : %s\n"
            "    newest object : %s\n\n"
            "  `make phase` does NOT build %s -- it is written only by\n"
            "  tools/toolchain/period.mk, so a merge that changes src/ leaves\n"
            "  these objects behind without touching anything the gate reads.\n"
            "  This reported pre-merge grades as current once already.\n\n"
            "  Run:  make tc"
            % (OURS, stale_src, stale_obj, OURS))

    common = sorted(k for k in ours if k in blob)
    if not common:
        sys.exit("byteident.py: blob and %s share NO symbols; the denominator "
                 "is zero, which is not a score." % OURS)

    buckets = {"EXACT": [], "UNRESOLVED": [], "REGALLOC": [], "RELOC": [],
               "BYTES": [], "SIZE": [], "NODATA": []}
    #
    # Worst-first, so `max` over this key picks the copy furthest from exact.
    #
    RANK = {"EXACT": 0, "UNRESOLVED": 1, "REGALLOC": 2, "RELOC": 3,
            "BYTES": 4, "SIZE": 5, "NODATA": 6}
    comdat_split = []
    for k in common:
        ab, ar = body(BLOB, k)
        objs = allobjs.get(k, [ours[k]])
        scored = []
        for o in objs:
            bb2, br2 = body(o, k)
            scored.append((verdict(ab, ar, bb2, br2), o))
        if len({s[0][0] for s in scored}) > 1:
            comdat_split.append((k, sorted({s[0][0] for s in scored})))
        (v, n), worst_obj = max(scored, key=lambda s: RANK.get(s[0][0], 9))
        ours[k] = worst_obj
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
    if comdat_split:
        print("  NOTE: %d COMDAT symbol(s) get different verdicts from different\n"
              "        defining objects; the WORST is scored.  --comdat lists them."
              % len(comdat_split))
    if a.comdat:
        print("\n  COMDAT symbols whose defining objects DISAGREE:\n")
        for k, vs in sorted(comdat_split):
            print("    %-14s %s" % ("/".join(vs), k))
            for o in sorted(allobjs.get(k, [])):
                ab2, ar2 = body(BLOB, k)
                bb2, br2 = body(o, k)
                vv, nn = verdict(ab2, ar2, bb2, br2)
                print("        %-11s %5d  %s" % (vv, nn, os.path.basename(o)))
        print("\n  The blob has ONE copy because the linker picked one, and we\n"
              "  cannot know which.  Scoring the best would be scoring the\n"
              "  luckiest, so the worst is scored (F8146).")
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

    exact_bytes = sum(blob[k] for _, _, k in buckets["EXACT"])
    compared_bytes = sum(blob[k] for k in common)
    if exact_bytes > compared_bytes:
        raise AssertionError("exact reference bytes exceed compared reference bytes")
    now = {"exact": ex, "regalloc": ra, "compared": n,
           "reference_functions": len(blob),
           "exact_bytes": exact_bytes, "compared_bytes": compared_bytes,
           "unresolved": len(buckets["UNRESOLVED"]),
           "reloc": len(buckets["RELOC"]),
           "bytes": len(buckets["BYTES"]),
           "size": len(buckets["SIZE"]),
           "nodata": len(buckets["NODATA"]),
           RATCHET_EXACT_SYMBOLS: sorted(k for _, _, k in buckets["EXACT"])}
    if a.json_out:
        with open(a.json_out, "w") as f:
            json.dump(now, f, indent=2, sort_keys=True)
            f.write("\n")
    if a.update:
        with open(RATCHET, "w") as f:
            json.dump(now, f, indent=2, sort_keys=True)
            f.write("\n")
        print("\nratchet updated: %d exact symbol(s), %d compared, %d regalloc"
              % (now["exact"], now["compared"], now["regalloc"]))
        return 0
    if a.ratchet:
        try:
            was = json.load(open(RATCHET))
        except (OSError, ValueError):
            sys.exit("no %s -- run with --update to set the floor" % RATCHET)
        try:
            lost, gained_symbols = _ratchet_membership(was, now)
        except ValueError as e:
            sys.exit("byteident.py: invalid ratchet baseline %s: %s"
                     % (RATCHET, e))
        if lost:
            print("\nRATCHET FAILED -- a previously grade-0 EXACT symbol regressed"
                  "\nwhile another gain could have kept the aggregate count level."
                  "\n`make period` proves BEHAVIOUR, not CODE GENERATION.")
            print("    exact  was %d, now %d" % (was["exact"], now["exact"]))
            print("\n  Lost exact symbol(s):")
            for k in lost:
                print("    %s" % k)
            print("\n  If the change was deliberate (e.g. a genuine behavioural"
                  "\n  fix that necessarily changes codegen), re-bless with"
                  "\n  --update and say in the commit message which exact symbols"
                  "\n  changed and why.  If it was a pure rename or comment change,"
                  "\n  it should not have moved this set at all -- find out what"
                  "\n  else changed before re-blessing.")
            return 1
        if now["regalloc"] < was["regalloc"]:
            print("\n  note: regalloc %d -> %d.  A FALL IS AMBIGUOUS -- a symbol"
                  "\n  leaves that bucket both by GRADUATING to exact and by"
                  "\n  ceasing to match at all.  exact went %d -> %d over the"
                  "\n  same period; a fall in regalloc alongside a rise in exact"
                  "\n  is progress, not regression."
                  % (was["regalloc"], now["regalloc"], was["exact"], now["exact"]))
        gained = bool(gained_symbols) or now["regalloc"] > was["regalloc"]
        print("\nratchet OK%s" % ("" if not gained else
              " (exact %d -> %d, regalloc %d -> %d)"
              % (was["exact"], now["exact"], was["regalloc"], now["regalloc"])))

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
