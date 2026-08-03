#!/usr/bin/env python3
"""
Which diagnostic call sites are missing, and what they were going to say.

WHY THIS EXISTS

`debug.h` states the policy: carry the `dsplibs_debug_printf` call sites,
because the gate is real control flow and the format strings are the original
author's own words.  Finding 134 counted them and found the policy had not
been followed -- 1670 calls in the blob across 399 functions, 22 in the
tree, and 242 missing from functions that ARE reconstructed.

Nothing could have caught that.  `dsplibs_debug_level` ships at zero and
every gate is `> 1`, so a missing call site and a present one behave
identically under every test here.  The differential harness is structurally
blind to it, which is why it needs a tool rather than a test.

WHAT IT REPORTS

  --missing   per function, how many call sites the blob has and this tree
              does not.  The work queue for task #50.
  --strings   the format strings themselves, per function.  Useful BEFORE
              restoring anything: the strings are the annotation, and they
              routinely name fields and conditions the reconstruction is
              otherwise guessing at (finding 136 is an example).
  --invented  the opposite direction, and the one --missing cannot see: every
              string literal THIS TREE carries that does not appear anywhere
              in the object's .rodata or .data.  Such a string was written
              rather than read -- usually from the function's own name -- and
              no test catches it unless something compares that function's
              transcript.  Four were found this way after `V34EchoCleanUp`
              turned up printing its own name (finding 156).

              EVERY literal, not just the ones at a printf call site.  A
              format reached through a variable has no literal at the call:
              `agc_gain_sample` takes `fmt` as a parameter and `hs_setstate`
              indexes a `fmt[]` table, so a call-site scan silently skips
              both -- and those are exactly the sites a reader would assume
              were covered.  Scanning everything also covers name tables like
              `StateName`, which no call-site scan could ever reach.

              Two limits, both real.  It is a NECESSARY condition only: a
              string that is in .rodata but belongs to a different function
              still passes, so this retires "invented from thin air" and not
              "attached to the wrong site".  And it says nothing about the
              ARGUMENTS, which is the half that bit in V34EchoReportCoeff --
              only a transcript comparison covers those.
  --stamps    the __DATE__/__TIME__ pairs.  Six translation units baked their
              build time into .rodata; the seconds are an independent check
              on TU boundaries that symbol ordering cannot give (finding 135).

CAVEATS

Counting is per function name, so a call site moved between functions during
reconstruction shows as one missing and one extra.  The counts are a queue,
not a proof.

`--strings` resolves each format by walking back to the nearest preceding
`.rodata` relocation, which is right for a one-argument call and WRONG where
several strings are pushed before one call -- it will name whichever came
last, not the format.  `FPM_AGC_init`'s two-string message is the example.
Treat the output as a shopping list of what a function says, not as an
argument list; read the disassembly before restoring.
"""

import argparse
import glob
import re
import subprocess
import sys
from collections import defaultdict

DBG = "dsplibs_debug_printf"


def run(*cmd):
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit("failed: %s\n%s" % (" ".join(cmd), out.stderr))
    return out.stdout


def rodata_strings(obj):
    """(section, addr) -> the NUL-terminated string there."""
    tabs = {}
    for sec in (".rodata.str1.1", ".rodata.str1.4", ".rodata"):
        out = subprocess.run(["readelf", "-x", sec, obj],
                             capture_output=True, text=True).stdout
        data = {}
        for line in out.split("\n"):
            m = re.match(r"\s+0x([0-9a-f]+) ((?:[0-9a-f]{2,8} ){1,4})", line)
            if m:
                data[int(m.group(1), 16)] = bytes.fromhex(
                    m.group(2).replace(" ", ""))
        if data:
            tabs[sec] = (min(data),
                         b"".join(data[k] for k in sorted(data)))
    return tabs


def string_at(tabs, sec, addr):
    if sec not in tabs:
        return None
    base, blob = tabs[sec]
    i = addr - base
    if i < 0 or i >= len(blob):
        return None
    end = blob.find(b"\0", i)
    if end < 0:
        return None
    return blob[i:end].decode("latin1")


def blob_sites(obj, tabs):
    """function -> list of format strings (None where not resolvable)."""
    out = run("objdump", "-dr", "--section=.text", obj)
    lines = out.split("\n")
    sites = defaultdict(list)
    cur = None
    for i, line in enumerate(lines):
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            cur = m.group(1)
            continue
        if "R_386_PC32" not in line or DBG not in line:
            continue
        # Walk back for the most recent .rodata string relocation; its
        # addend is the immediate on the instruction line above it.
        fmt = None
        for j in range(i - 1, max(0, i - 16), -1):
            sm = re.search(r"R_386_32\s+(\.rodata[^\s]*)", lines[j])
            if sm:
                im = re.search(r"\$0x([0-9a-f]+)", lines[j - 1])
                if im:
                    fmt = string_at(tabs, sm.group(1), int(im.group(1), 16))
                break
        sites[cur].append(fmt)
    return sites


def our_sites(paths):
    """function -> count of debug calls in its body, and the file it is in."""
    counts, where = {}, {}
    for p in paths:
        try:
            src = open(p).read()
        except OSError:
            continue
        parts = re.split(r"^(\w+)\s*\(", src, flags=re.M)
        for i in range(1, len(parts), 2):
            fn, body = parts[i], parts[i + 1]
            counts[fn] = counts.get(fn, 0) + body.count(DBG)
            where[fn] = p
    return counts, where


COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
RUN = re.compile(r'(?:"(?:[^"\\\n]|\\.)*"\s*)+')
LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
ESC = {"n": "\n", "r": "\r", "t": "\t", "0": "\0", "a": "\a", "b": "\b",
       "f": "\f", "v": "\v", '"': '"', "\\": "\\", "'": "'", "?": "?"}


def unescape(raw):
    out, i = [], 0
    while i < len(raw):
        if raw[i] == "\\" and i + 1 < len(raw):
            out.append(ESC.get(raw[i + 1], raw[i + 1]))
            i += 2
        else:
            out.append(raw[i])
            i += 1
    return "".join(out)


def our_strings(paths):
    """(path, line, string) for EVERY string literal this tree carries.

    Not just the ones at a `dsplibs_debug_printf` call site.  A format reached
    through a variable -- `agc_gain_sample`'s `fmt` parameter, `hs_setstate`'s
    `fmt[]` table -- has no literal at the call, so a call-site scan cannot see
    it, and those are exactly the sites a reader would assume were covered.
    Scanning every literal also picks up name tables like `StateName`.

    Comments are stripped and preprocessor lines skipped; the latter is the
    only source of legitimate literals that are not the blob's, since every
    one of them is an `#include` path.  Adjacent literals are glued the way
    the compiler does, so a message split across source lines is checked as
    the one string it becomes.
    """
    out = []
    for p in sorted(paths):
        try:
            src = open(p).read()
        except OSError:
            continue
        src = COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), src)
        for n, line in enumerate(src.split("\n"), 1):
            if line.lstrip().startswith("#"):
                continue
            for m in RUN.finditer(line):
                s = unescape("".join(LIT.findall(m.group(0))))
                if s:
                    out.append((p, n, s))
    return out


def main():
    ap = argparse.ArgumentParser(
        description="Audit dsplibs.o's diagnostic call sites against this "
                    "reconstruction.")
    ap.add_argument("--obj", default="../slmodemd/dsplibs.o")
    ap.add_argument("--src", nargs="*", default=None)
    ap.add_argument("--missing", action="store_true")
    ap.add_argument("--strings", metavar="FUNC", nargs="?", const="",
                    help="print format strings, optionally for one function")
    ap.add_argument("--stamps", action="store_true")
    ap.add_argument("--invented", action="store_true",
                    help="our format strings that are not in the object")
    args = ap.parse_args()

    tabs = rodata_strings(args.obj)
    sites = blob_sites(args.obj, tabs)

    if args.src is None:
        args.src = glob.glob("src/**/*.c", recursive=True)
    ours, where = our_sites(args.src)

    if args.stamps:
        print("Build stamps baked into .rodata -- one per TU that printed "
              "__DATE__/__TIME__.\nThe seconds distinguish translation units "
              "(finding 135).\n")
        for sec, (base, blob) in tabs.items():
            for m in re.finditer(
                    rb"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) "
                    rb"[ 0-9][0-9] [12][0-9]{3}\x00", blob):
                print("   %-16s 0x%05x  %s"
                      % (sec, base + m.start(), m.group(0)[:-1].decode()))
            for m in re.finditer(rb"[0-2][0-9]:[0-5][0-9]:[0-5][0-9]\x00",
                                 blob):
                print("   %-16s 0x%05x  %s"
                      % (sec, base + m.start(), m.group(0)[:-1].decode()))
        return

    if args.invented:
        haystack = b"".join(blob for _, blob in tabs.values())
        haystack += subprocess.run(
            ["objcopy", "-O", "binary", "--only-section=.data",
             args.obj, "/dev/stdout"], capture_output=True).stdout
        found = our_strings(args.src)
        bad = [(p, n, t) for p, n, t in found
               if t.encode("latin1") not in haystack]
        print("String literals in this tree that appear NOWHERE in the "
              "object's\n.rodata or .data -- so they were invented rather than "
              "read.  Finding 156.\n")
        for p, n, t in bad:
            print("  INVENTED  %s:%d\n            %r" % (p, n, t))
        print("\n  %d checked, %d not present in the object"
              % (len(found), len(bad)))
        return 1 if bad else 0

    if args.strings is not None:
        which = args.strings
        for fn in sorted(sites):
            if which and fn != which:
                continue
            if not which and fn not in ours:
                continue            # only reconstructed ones, unless named
            print("%s  (blob %d, ours %d)"
                  % (fn, len(sites[fn]), ours.get(fn, 0)))
            for s in sites[fn]:
                print("    %s" % (repr(s) if s else "<unresolved>"))
            print()
        return

    rows = []
    for fn, lst in sites.items():
        if fn not in ours:
            continue
        gap = len(lst) - ours[fn]
        if gap:
            rows.append((gap, len(lst), ours[fn], fn, where[fn]))
    rows.sort(reverse=True)
    print("MISSING diagnostic call sites, in functions already reconstructed."
          "\nNo test can see these: the level ships at zero, so a missing "
          "call and a\npresent one behave identically.  Finding 134.\n")
    for gap, n, g, fn, p in rows:
        print("  %3d missing  (blob %2d, ours %2d)  %-30s %s"
              % (gap, n, g, fn, p))
    print("\n  %d functions, %d call sites, %d total in the blob"
          % (len(rows), sum(r[0] for r in rows),
             sum(len(v) for v in sites.values())))


if __name__ == "__main__":
    sys.exit(main() or 0)
