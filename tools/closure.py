#!/usr/bin/env python3
"""
Everything a batch must define before it will link -- all four kinds at once.

WHY THIS EXISTS

Every test binary links all of $(OBJ), the harness and the renamed reference
object, and the reference object cannot satisfy a reference from our side
because every symbol it defines has been renamed.  So one undefined symbol
does not fail the new test: it fails all 92 binaries, and `make` stops at the
first, which is `t_encode` and names nothing involved.

Work therefore lands in CLOSED batches -- a set where every dependency of
every member is in the set or already written.  Computing that set has four
parts, and one session found them one per batch, each the expensive way:

    calls                T symbols reached through .rel.text        free
    static data members  D and R symbols reached through .rel.text  ~20 min
    template members     W symbols, in .gnu.linkonce.t.* sections   a batch
    data referring to    D and R symbols reached through .rel.data  an hour,
    other data                                                      via a
                                                                    wrong split

`callgraph.py` answers the first.  It walks calls, so a class whose method
loads a static coefficient table looks like a leaf and is not, and a table
whose entries are pointers to other tables looks like a terminus and is not.
This walks all four in one pass.

WHAT IT DOES NOT KNOW

Whether a symbol is reachable at RUN time.  This is the link closure, which
is the thing that decides whether a commit builds; it is deliberately
pessimistic and will list a branch nothing takes.

Whether a named data symbol needs to exist under that name.  `V90Jd` reaches
`prop_dsp_version`, a seven-byte string in .rodata; our reconstruction is
free to write the literal inline, in which case nothing is undefined and the
report is a false alarm.  Reported anyway: the alternative is a rule for
which data symbols to ignore, and a rule like that is how the third and
fourth closures went unnoticed in the first place.

USAGE

    tools/closure.py V90PreFilter                 by class, any spelling
    tools/closure.py _ZN12V90PreFilter12setParamEia6Ev
    tools/closure.py probeselect --missing        only what src/ lacks
    tools/closure.py V90Jd V92Jd --batch          is this set closed?
"""

import argparse
import collections
import glob
import os
import re
import struct
import subprocess
import sys

_here = os.path.dirname(os.path.abspath(__file__))
sys.path[:] = [p for p in sys.path if os.path.abspath(p or ".") != _here]

BLOB = os.environ.get("BLOB", "../slmodemd/dsplibs.o")


def readelf_sections(obj):
    """{index: (name, kind)} so a relocation's target section can be named."""
    out = subprocess.run(["readelf", "-SW", obj], capture_output=True,
                         text=True).stdout
    sec = {}
    for m in re.finditer(r"^\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)", out, re.M):
        sec[int(m.group(1))] = (m.group(2), m.group(3))
    return sec


def symbols(obj):
    """{name: (size, kind, section)} for everything the object defines."""
    out = subprocess.run(["readelf", "-sW", obj], capture_output=True,
                         text=True).stdout
    syms, order = {}, []
    for line in out.splitlines():
        f = line.split()
        if len(f) < 8 or not f[0].endswith(":"):
            continue
        value, size, typ, bind, _vis, ndx, name = (f[1], f[2], f[3], f[4],
                                                   f[5], f[6], f[7])
        # SECTION symbols are not symbols for this purpose: `.rodata` names
        # the section, has size 0, and is what a relocation targets when the
        # real owner is file-local.  Keeping them made every closure end in a
        # phantom zero-byte "unwritten" entry and a batch that is closed
        # report that it is not.
        if ndx in ("UND", "ABS", "COM") or not name or typ == "SECTION":
            continue
        try:
            syms[name] = (int(size), typ, int(ndx), int(value, 16))
        except ValueError:
            continue
        order.append(name)
    return syms


#
# WHICH SYMBOL OWNS AN ADDRESS.
#
# A relocation against a section plus an addend -- `.rodata+0x2da0` -- is how
# the object refers to a static table, to a string, and to anything file-local.
# Turning that back into a name needs the symbol table sorted by section and
# address, which is what this builds once.
#
def address_index(syms):
    by_sec = collections.defaultdict(list)
    for name, (size, typ, ndx, val) in syms.items():
        by_sec[ndx].append((val, size, name))
    for ndx in by_sec:
        by_sec[ndx].sort()
    return by_sec


def owner(by_sec, ndx, addr):
    best = None
    for val, size, name in by_sec.get(ndx, ()):
        if val > addr:
            break
        if val == addr or val + max(size, 1) > addr:
            best = name
    return best


#
# THE ADDEND IS NOT IN THE RELOCATION.
#
# This object is ELF32 REL, not RELA: `r_addend` does not exist, and the
# addend is the value already stored at the relocation site.  `readelf -r`
# therefore prints no `+ 0x...` column for any of the 4,794 relocations here
# that name a section rather than a symbol -- and reading the addend as zero
# resolves every one of them to whatever symbol sits at offset 0 of that
# section.  In this blob that is `prop_dp_init` for `.text`,
# `_ZZN5V92CP10bitsToInfoEhE5gamma` for `.bss` and `prop_dsp_version` for
# `.rodata`, which is why those three used to appear in every closure the
# tool computed.  All 4,794 are R_386_32, so the stored value IS the target
# offset within the section; no PC-relative correction arises.  Finding 330.
#
def section_file_offsets(obj):
    """{section index: (file offset, size)} straight out of the ELF header."""
    with open(obj, "rb") as f:
        blob = f.read()
    if blob[:4] != b"\x7fELF" or blob[4] != 1:
        return {}, blob
    shoff, shentsize, shnum = struct.unpack_from("<I", blob, 0x20)[0], \
        struct.unpack_from("<H", blob, 0x2e)[0], \
        struct.unpack_from("<H", blob, 0x30)[0]
    out = {}
    for i in range(shnum):
        base = shoff + i * shentsize
        sh_type, _fl, _ad, sh_off, sh_size = struct.unpack_from(
            "<IIIII", blob, base + 4)
        out[i] = (sh_off, sh_size, sh_type)
    return out, blob


def inplace_addend(shdrs, blob, ndx, off):
    """The four bytes at `off` in section `ndx`, or None if unreadable."""
    got = shdrs.get(ndx)
    if not got:
        return None
    sh_off, sh_size, sh_type = got
    if sh_type == 8 or off + 4 > sh_size:        # SHT_NOBITS, or past the end
        return None
    return struct.unpack_from("<i", blob, sh_off + off)[0]


def relocations(obj, sec):
    """[(from_section_index, target_symbol_or_section, addend)] for the object.

    Both .rel.text* and .rel.data* and .rel.rodata*, which is the whole point:
    the fourth closure lives in .rel.data and nothing that only reads
    .rel.text will see it.
    """
    out = subprocess.run(["readelf", "-rW", obj], capture_output=True,
                         text=True).stdout
    cur, rels = None, []
    name_to_ndx = {v[0]: k for k, v in sec.items()}
    for line in out.splitlines():
        m = re.match(r"^Relocation section '(\S+)'", line)
        if m:
            target = m.group(1)
            for pfx in (".rel.", ".rela."):
                if target.startswith(pfx):
                    target = target[len(pfx) - 1:]
            cur = name_to_ndx.get(target)
            continue
        f = line.split()
        if cur is None or len(f) < 5 or not re.match(r"^[0-9a-f]+$", f[0]):
            continue
        sym = f[4]
        add = 0
        if len(f) > 5 and f[5] in ("+", "-"):
            add = int(f[6], 16) if len(f) > 6 else 0
        m = re.match(r"^(\S+)\s*\+\s*([0-9a-fx]+)$", " ".join(f[4:]))
        if m:
            sym, add = m.group(1), int(m.group(2), 0)
        rels.append((cur, sym, add))
    return rels


def build_graph():
    sec = readelf_sections(BLOB)
    syms = symbols(BLOB)
    by_sec = address_index(syms)
    name_to_ndx = {v[0]: k for k, v in sec.items()}
    shdrs, raw = section_file_offsets(BLOB)

    # Which symbol each relocation sits INSIDE, so an edge has a source.
    # readelf gives the offset within the section, so the same address
    # lookup answers it.
    out = subprocess.run(["readelf", "-rW", BLOB], capture_output=True,
                         text=True).stdout
    edges = collections.defaultdict(set)
    cur = None
    for line in out.splitlines():
        m = re.match(r"^Relocation section '(\S+)'", line)
        if m:
            t = m.group(1)
            for pfx in (".rel.", ".rela."):
                if t.startswith(pfx):
                    t = "." + t[len(pfx):]
            cur = name_to_ndx.get(t)
            continue
        f = line.split()
        if cur is None or len(f) < 5 or not re.match(r"^[0-9a-f]+$", f[0]):
            continue
        roff = int(f[0], 16)
        src = owner(by_sec, cur, roff)
        if src is None:
            continue
        tgt, add = f[4], 0
        rest = " ".join(f[4:])
        m = re.match(r"^(\S+)\s*\+\s*([0-9a-fA-Fx]+)", rest)
        if m:
            tgt, add = m.group(1), int(m.group(2), 0)
        if tgt in syms:
            edges[src].add(tgt)
        elif tgt in name_to_ndx:                 # a section, so resolve it
            if not m:                            # REL: the addend is in place
                got = inplace_addend(shdrs, raw, cur, roff)
                if got is None:
                    continue
                add = got
            got = owner(by_sec, name_to_ndx[tgt], add)
            if got:
                edges[src].add(got)
    return syms, sec, edges


def ours():
    found = set()
    for o in glob.glob("build/src/**/*.o", recursive=True):
        out = subprocess.run(["nm", "--defined-only", o], capture_output=True,
                             text=True).stdout
        for line in out.splitlines():
            f = line.split()
            if len(f) >= 3:
                found.add(f[-1])
    return found


def kind_of(syms, sec, name):
    size, typ, ndx, _val = syms[name]
    sname = sec.get(ndx, ("?", "?"))[0]
    if sname.startswith(".gnu.linkonce.t"):
        return "template"
    if typ == "FUNC":
        return "call"
    if sname.startswith((".data", ".bss")):
        return "data"
    if sname.startswith(".rodata"):
        return "rodata"
    return typ.lower()


#
# THE WALK STOPS AT WHAT IS ALREADY WRITTEN.
#
# A link closure asks what a batch will leave undefined.  A symbol `src/`
# already defines cannot leave anything undefined -- the tree builds, so its
# own callees are satisfied -- and the blob's version of it is not the one
# that will be linked.  Walking through it imports the BLOB's callees, which
# is how one call to the tree's own `edprintf` used to drag `call_op` and the
# `dp_*_init` family into every closure computed here.  Roots are always
# expanded, so asking about a function that is already written still works.
# Finding 330.
#
def expand(roots, syms, edges, have=()):
    seen, stack = set(), list(roots)
    while stack:
        n = stack.pop()
        if n in seen or n not in syms:
            continue
        seen.add(n)
        if n in have and n not in roots:
            continue
        stack += [c for c in edges.get(n, ()) if c not in seen]
    return seen


def resolve(spec, syms):
    """A class name, a mangled name, or a plain name.

    ANCHORED.  A class's own members begin `_ZN<len><name>` (or `_ZNK` for a
    const one); the same `<len><name>` appears mid-symbol whenever any other
    class takes a pointer to it as a parameter.  An unanchored search pulled
    `V90Equalizer`'s constructor into `V90PreFilter`'s closure, because it
    takes a `V90PreFilter*`, and inflated the answer by four functions.
    """
    if spec in syms:
        return [spec]
    # No trailing guard is needed or wanted: the `<len>` prefix already pins
    # the name exactly, and what follows a member's class name is the length
    # prefix of the method -- a digit, which a `(?![A-Za-z0-9_])` lookahead
    # rejects, matching nothing at all.
    pat = re.compile(r"^_Z(TV|TI|TS|NK|N)%d%s" % (len(spec), re.escape(spec)))
    hits = [n for n in syms if pat.match(n)]
    return hits or [n for n in syms if n == spec]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="+")
    ap.add_argument("--missing", action="store_true",
                    help="only symbols src/ does not define")
    ap.add_argument("--batch", action="store_true",
                    help="report whether the named set is closed")
    args = ap.parse_args()

    syms, sec, edges = build_graph()
    have = ours()

    #
    # AN UNBUILT TREE MAKES EVERY CLOSURE LOOK ENORMOUS.
    #
    # `ours()` reads what src/ defines out of build/src/**/*.o, so in a fresh
    # `git worktree add` -- where build/ does not exist yet -- the have-set is
    # empty and everything this tree has already written is reported missing.
    # One agent read 21 symbols and 9,187 bytes for a function whose real
    # closure is itself.  The answer is not wrong so much as answering a
    # different question, which is the worst kind.  Finding 271.
    #
    if not have:
        sys.stderr.write(
            "closure.py: build/src/**/*.o defines nothing, so NOTHING counts\n"
            "            as already written and this closure is meaningless.\n"
            "            Run `make` first.  (Finding 271.)\n")

    roots = []
    for spec in args.names:
        got = resolve(spec, syms)
        if not got:
            sys.exit("no symbol or class matching %r in %s" % (spec, BLOB))
        roots += got

    reached = expand(roots, syms, edges, have)
    missing = sorted(n for n in reached if n not in have)

    if args.batch:
        outside = [n for n in missing if n not in roots]
        print("%d root symbol(s); closure reaches %d; %d unwritten"
              % (len(roots), len(reached), len(missing)))
        if not outside:
            print("CLOSED -- everything the roots reach is written or is a root")
            return 0
        print("NOT CLOSED -- %d unwritten symbol(s) outside the set:" % len(outside))
        by = collections.Counter(kind_of(syms, sec, n) for n in outside)
        print("   by kind: %s" % ", ".join("%s %d" % kv for kv in by.most_common()))
        for n in sorted(outside, key=lambda x: -syms[x][0])[:40]:
            print("   %-9s %6d  %s" % (kind_of(syms, sec, n), syms[n][0], n))
        return 1

    show = missing if args.missing else sorted(reached)
    tot = sum(syms[n][0] for n in show)
    print("closure of %s: %d symbols, %d bytes%s\n"
          % (" ".join(args.names), len(show), tot,
             " (unwritten only)" if args.missing else ""))
    by = collections.defaultdict(list)
    for n in show:
        by[kind_of(syms, sec, n)].append(n)
    for kind in ("call", "template", "data", "rodata"):
        if kind not in by:
            continue
        b = sum(syms[n][0] for n in by[kind])
        print("  %s -- %d symbols, %d bytes" % (kind, len(by[kind]), b))
        for n in sorted(by[kind], key=lambda x: -syms[x][0]):
            mark = "  " if n in have else " *"
            print("   %s %7d  %s" % (mark, syms[n][0], n))
        print()
    for kind in sorted(set(by) - {"call", "template", "data", "rodata"}):
        print("  %s -- %s" % (kind, " ".join(by[kind])))
    if not args.missing:
        print("  `*` marks a symbol src/ does not define.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
