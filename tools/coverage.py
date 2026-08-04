#!/usr/bin/env python3
"""
How much of dsplibs.o has been reconstructed, and how much of that is tested.

WHY THIS EXISTS

"Phase 5 is done" is not a measurement.  Two numbers are:

  translated   the share of the blob's .text that a function of the same name
               now exists for in this tree
  tested       the share of THAT which some test actually drives against the
               blob, by calling its `ref_` alias

The second matters more than the first.  A reconstructed function that
nothing compares against the original is a guess with good indentation, and
the gap between the two numbers is the honest measure of how much of the work
rests on reading alone.

HOW IT DECIDES

Not by grepping source for names, which would count a prototype, a comment or
a call as a definition.  By reading symbol tables:

  ours     every T symbol our own build defines, from build/src/**/*.o
  blob     every T and t symbol dsplibs.o defines, with its size
  tested   every `ref_NAME` a compiled test object actually REFERENCES,
           from `nm -u` on build/test/**/*.o

That last distinction matters and was got wrong once.  Every test file opens
with a block of `extern ref_*` declarations, so grepping the sources counts a
symbol as tested the moment it is declared -- including one whose calls were
deleted in some earlier revision.  An undefined symbol in the object file
means the compiler emitted a reference to it, which only a call or an address
can do.

The interop programs compile straight to executables with no intermediate
object, so those few sources are still read by name; they are listed
explicitly rather than swept up by a directory walk.

File-local symbols (`t` in nm) are counted when we have reconstructed one of
the same name -- several are, `AnalyseDialString` among them.  They used to be
reported apart as impossible to test differentially, "because objcopy cannot
rename them".  That was wrong: --globalize-symbols promotes them first and the
rename map then applies, which the Makefile now does in two passes.  So a
file-local symbol whose name is unique in the object has a `ref_` alias, can
be called by name, and counts in the `tested` denominator like anything else.
Ten names occur in more than one translation unit and cannot be globalized --
two statics of the same name are two different objects -- and those alone
stay in the reached-through-a-caller bucket.

Which is which is not re-derived from symmap.py's rules: the alias set is read
out of build/dsplibs_ref.o, the object the tests actually link.  A symbol is
drivable if and only if objcopy really made an alias for it.

WHERE `ours` COMES FROM, AND WHY IT IS A WHITELIST

Only build/src is walked.  This used to be all of build/ less `dsplibs_ref.o`
by name -- and then the two-pass rename put a SECOND copy of the blob beside
it, build/dsplibs_glob.o, with all 1782 of its symbols promoted to global.
The walk took that for our own output and the report claimed 98.0% translated
for a tree that has reconstructed 290 symbols.  Adding a second name to the
blacklist would break again the next time the Makefile leaves an intermediate
in build/; naming the one directory our compiler writes to cannot.  See
finding 222.

Usage:
    coverage.py [--obj ../slmodemd/dsplibs.o] [--build build] [--md FILE]
"""

import argparse
import json
import os
import re
import subprocess
import sys

# Symbols we define that the object has no counterpart for, and why that is
# expected rather than drift.  Anything not matching these is reported.
BENIGN = (
    (re.compile(r"^__x86\.get_pc_thunk\."), "compiler thunk"),
    (re.compile(r"_(entry|generate|size)$"), "test accessor"),
    (re.compile(r"^RcFixed_(State|UpFactor|DownFactor)$"), "test accessor"),
    (re.compile(r"^FP_Pow_coefficient$"), "test accessor"),
)


def nm_symbols(path):
    """{name: (size, kind)} for the T/t symbols a file defines."""
    out = subprocess.run(["nm", "-S", "--defined-only", path],
                         capture_output=True, text=True).stdout
    syms = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[2] in "Tt":
            syms[f[3]] = (int(f[1], 16), f[2])
        elif len(f) == 3 and f[1] in "Tt":
            syms[f[2]] = (0, f[1])
    return syms


def blob_addresses(path):
    """{name: address} for every T/t symbol, to attribute it to a TU."""
    out = subprocess.run(["nm", "--defined-only", path],
                         capture_output=True, text=True).stdout
    addr = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[1] in "Tt":
            addr[f[2]] = int(f[0], 16)
    return addr


def our_symbols(build):
    """Global text symbols our own compiler output defines.

    build/src and nothing else -- the whitelist the docstring argues for.
    """
    syms = set()
    for root, _dirs, files in os.walk(os.path.join(build, "src")):
        for name in files:
            if not name.endswith(".o"):
                continue
            for sym, (_size, kind) in nm_symbols(os.path.join(root,
                                                             name)).items():
                if kind == "T":
                    syms.add(sym)
    return syms


def aliased_symbols(build):
    """Names reachable as `ref_NAME` in the object the tests link.

    Read from the artifact, not from symmap.py's rules, so the two cannot
    drift: whatever objcopy managed to alias is what a test can call.
    """
    ref = os.path.join(build, "dsplibs_ref.o")
    if not os.path.exists(ref):
        sys.exit("error: %s does not exist, so nothing can be said about "
                 "which symbols have a ref_ alias.  Build it first:\n"
                 "    make %s" % (ref, ref))
    out = subprocess.run(["nm", "--defined-only", ref],
                         capture_output=True, text=True).stdout
    names = {line.split()[-1] for line in out.splitlines() if line.split()}
    return {n[len("ref_"):] for n in names if n.startswith("ref_")}


# Interop sources, which have no intermediate object to read.
INTEROP_BY_NAME = ("test/interop/v8peer.c",)


def undefined(path):
    out = subprocess.run(["nm", "-u", path], capture_output=True,
                         text=True).stdout
    return {line.split()[-1] for line in out.splitlines() if line.split()}


def tested_symbols(build, extra_sources=INTEROP_BY_NAME):
    """Symbols some test actually references, not merely declares."""
    found = set()
    for root, _dirs, files in os.walk(os.path.join(build, "test")):
        for name in files:
            if not name.endswith(".o"):
                continue
            for sym in undefined(os.path.join(root, name)):
                if sym.startswith("ref_"):
                    found.add(sym[4:])

    pat = re.compile(r"\bref_([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    for src in extra_sources:
        try:
            with open(src, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in pat.finditer(text):
            found.add(m.group(1))
    return found


def text_size(obj):
    out = subprocess.run(["size", "-A", obj], capture_output=True,
                         text=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 2 and f[0] == ".text":
            return int(f[1])
    return 0


def load_tus(path):
    """[(lo, hi, label)] from the tumap JSON, one entry per distinct span."""
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except OSError:
        return []
    spans = {}
    for name, t in data["tus"].items():
        spans.setdefault((t["lo"], t["hi"]), []).append((t["seq"], name))
    out = []
    for (lo, hi), members in spans.items():
        members.sort()
        label = members[0][1]
        if len(members) > 1:
            label += " +%d" % (len(members) - 1)
        out.append((lo, hi, label))
    out.sort()
    return out


def area_of(addr, tus):
    for lo, hi, label in tus:
        if lo <= addr < hi:
            return label
    return "?"


def bar(frac, width=34):
    n = int(round(frac * width))
    return "[" + "#" * n + "." * (width - n) + "]"


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obj", default="../slmodemd/dsplibs.o")
    ap.add_argument("--build", default="build")
    ap.add_argument("--tumap", default="build/tumap.json")
    ap.add_argument("--md", help="also write a markdown summary here")
    args = ap.parse_args()

    blob = nm_symbols(args.obj)
    ours = our_symbols(args.build)
    tested = tested_symbols(args.build)
    aliased = aliased_symbols(args.build)
    addr = blob_addresses(args.obj)
    tus = load_tus(args.tumap)

    total = text_size(args.obj)
    gl = {n: s for n, (s, k) in blob.items() if k == "T"}
    lo = {n: s for n, (s, k) in blob.items() if k == "t"}

    done_g = {n: s for n, s in gl.items() if n in ours}
    done_l = {n: s for n, s in lo.items() if n in ours}

    # A file-local symbol that got a `ref_` alias can be called by name and so
    # belongs in the denominator; one that did not, cannot and does not.
    done_la = {n: s for n, s in done_l.items() if n in aliased}
    done_ln = {n: s for n, s in done_l.items() if n not in aliased}

    drivable = dict(done_g)
    drivable.update(done_la)
    done_t = {n: s for n, s in drivable.items() if n in tested}

    d_bytes = sum(done_g.values()) + sum(done_l.values())
    t_bytes = sum(done_t.values())

    lines = []
    add = lines.append
    add("dsplibs.o reconstruction coverage")
    add("")
    add("  .text                        %8d bytes, %d symbols"
        % (total, len(blob)))
    add("")
    g_bytes = sum(drivable.values())
    add("  translated  %s %5.1f%%  %8d bytes, %d symbols"
        % (bar(d_bytes / total if total else 0),
           100 * d_bytes / total if total else 0, d_bytes,
           len(done_g) + len(done_l)))
    add("  tested      %s %5.1f%%  %8d bytes, %d of %d that can be"
        % (bar(t_bytes / g_bytes if g_bytes else 0),
           100 * t_bytes / g_bytes if g_bytes else 0, t_bytes, len(done_t),
           len(drivable)))
    add("")
    add("  `tested` is the share of what we have translated that some test"
        " drives")
    add("  against the blob itself, not a self-consistency check.  Its"
        " denominator")
    add("  is what CAN be driven that way: everything with a `ref_` alias in")
    add("  build/dsplibs_ref.o, which since the Makefile globalizes first"
        " includes")
    add("  the file-local symbols too -- %d of ours (%d bytes)."
        % (len(done_la), sum(done_la.values())))
    add("")

    untested = sorted(((s, n) for n, s in drivable.items() if n not in tested),
                      reverse=True)
    if untested:
        add("  translated, alias exists, and NOT tested:")
        for size, name in untested:
            add("    %-44s %6d bytes%s"
                % (name, size, "   (file-local)" if name in done_la else ""))
        add("")

    if done_ln:
        add("  file-local AND named in more than one translation unit, so no")
        add("  alias is possible -- reached through a caller instead")
        add("  (%d symbols, %d bytes -- outside the figure above):"
            % (len(done_ln), sum(done_ln.values())))
        for name in sorted(done_ln):
            add("    %-44s %6d bytes" % (name, done_ln[name]))
        add("")
    else:
        add("  nothing we have reconstructed is stuck without an alias: every")
        add("  file-local symbol of ours is drivable by name.")
        add("")

    strays = []
    for name in sorted(ours - set(blob)):
        why = next((w for pat, w in BENIGN if pat.search(name)), None)
        if why is None:
            strays.append(name)
    if strays:
        add("  we define these and the object has no symbol of that name --")
        add("  either a helper split out of a larger function, or drift:")
        for name in strays:
            add("    %s" % name)
        add("")

    if tus:
        rest = {}
        # Local symbols count here too.  Iterating the globals alone left the
        # file-local ones out of "what is left" as well as out of the
        # denominator, which understated both.
        for name, (size, _kind) in blob.items():
            if name in ours:
                continue
            area = area_of(addr.get(name, -1), tus)
            rest.setdefault(area, [0, 0])
            rest[area][0] += size
            rest[area][1] += 1
        add("  what is left, by translation-unit span:")
        for label, (size, count) in sorted(rest.items(),
                                           key=lambda kv: -kv[1][0])[:12]:
            add("    %-44s %7d bytes  %4d symbols" % (label, size, count))

    text = "\n".join(lines)
    print(text)

    if args.md:
        with open(args.md, "w", encoding="utf-8") as fh:
            fh.write("# Reconstruction coverage\n\n"
                     "Generated by `tools/coverage.py`; run `make coverage`"
                     " to refresh.\n\n```\n")
            fh.write(text)
            fh.write("\n```\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
