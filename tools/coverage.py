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
  tested   every `ref_NAME` mentioned anywhere under test/

File-local symbols (`t` in nm) are counted when we have reconstructed one of
the same name -- several are, `AnalyseDialString` among them -- but they can
never be *differentially* tested directly, because objcopy cannot rename them
and there is nothing to link against.  They are reported apart for that
reason, not left out.

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
    syms = set()
    for root, _dirs, files in os.walk(build):
        if "test" in root.split(os.sep):
            continue
        for name in files:
            if not name.endswith(".o") or name == "dsplibs_ref.o":
                continue
            for sym, (_size, kind) in nm_symbols(os.path.join(root,
                                                             name)).items():
                if kind == "T":
                    syms.add(sym)
    return syms


def tested_symbols(testdir):
    pat = re.compile(r"\bref_([A-Za-z_][A-Za-z0-9_]*)")
    found = set()
    for root, _dirs, files in os.walk(testdir):
        for name in files:
            if not name.endswith((".c", ".cpp", ".h")):
                continue
            with open(os.path.join(root, name), encoding="utf-8",
                      errors="replace") as fh:
                for m in pat.finditer(fh.read()):
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
    ap.add_argument("--tests", default="test")
    ap.add_argument("--tumap", default="build/tumap.json")
    ap.add_argument("--md", help="also write a markdown summary here")
    args = ap.parse_args()

    blob = nm_symbols(args.obj)
    ours = our_symbols(args.build)
    tested = tested_symbols(args.tests)
    addr = blob_addresses(args.obj)
    tus = load_tus(args.tumap)

    total = text_size(args.obj)
    gl = {n: s for n, (s, k) in blob.items() if k == "T"}
    lo = {n: s for n, (s, k) in blob.items() if k == "t"}

    done_g = {n: s for n, s in gl.items() if n in ours}
    done_l = {n: s for n, s in lo.items() if n in ours}
    done_t = {n: s for n, s in done_g.items() if n in tested}

    d_bytes = sum(done_g.values()) + sum(done_l.values())
    t_bytes = sum(done_t.values())

    lines = []
    add = lines.append
    add("dsplibs.o reconstruction coverage")
    add("")
    add("  .text                        %8d bytes, %d symbols"
        % (total, len(blob)))
    add("")
    g_bytes = sum(done_g.values())
    add("  translated  %s %5.1f%%  %8d bytes, %d symbols"
        % (bar(d_bytes / total if total else 0),
           100 * d_bytes / total if total else 0, d_bytes,
           len(done_g) + len(done_l)))
    add("  tested      %s %5.1f%%  %8d bytes, %d of %d that can be"
        % (bar(t_bytes / g_bytes if g_bytes else 0),
           100 * t_bytes / g_bytes if g_bytes else 0, t_bytes, len(done_t),
           len(done_g)))
    add("")
    add("  `tested` is the share of what we have translated that some test"
        " drives")
    add("  against the blob itself, not a self-consistency check.  Its"
        " denominator")
    add("  is what CAN be driven that way: a function the object keeps"
        " file-local")
    add("  has no `ref_` alias to link against, because objcopy cannot"
        " rename it.")
    add("")

    untested = sorted(((s, n) for n, s in done_g.items() if n not in tested),
                      reverse=True)
    if untested:
        add("  translated, globally visible, and NOT tested:")
        for size, name in untested:
            add("    %-44s %6d bytes" % (name, size))
        add("")

    if done_l:
        add("  file-local in the object, so reached through a caller"
            " instead")
        add("  (%d symbols, %d bytes -- outside the figure above):"
            % (len(done_l), sum(done_l.values())))
        for name in sorted(done_l):
            add("    %-44s %6d bytes" % (name, done_l[name]))
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
        for name, size in gl.items():
            if name in ours:
                continue
            rest.setdefault(area_of(addr.get(name, -1), tus),
                            [0, 0])
            rest[area_of(addr.get(name, -1), tus)][0] += size
            rest[area_of(addr.get(name, -1), tus)][1] += 1
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
