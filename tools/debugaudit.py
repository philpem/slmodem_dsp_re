#!/usr/bin/env python3
"""
Which diagnostic call sites are missing, and what they were going to say.

WHY THIS EXISTS

`debug.h` states the policy: carry the `dsplibs_debug_printf` call sites,
because the gate is real control flow and the format strings are the original
author's own words.  Finding F134 counted them and found the policy had not
been followed -- 1670 calls in the blob across 399 functions, 22 in the
tree, and 242 missing from functions that ARE reconstructed.

Nothing could have caught that.  `dsplibs_debug_level` ships at zero and
every gate is `> 1`, so a missing call site and a present one behave
identically under every test here.  The differential harness is structurally
blind to it, which is why it needs a tool rather than a test.

WHAT IT REPORTS

  --missing   per function, how many call sites the blob has and this tree
              does not.  The work queue for task #50.
  --absent    the same question asked of the STRINGS rather than the counts:
              which of the object's format strings appear nowhere in `src/`.
              A count -- per function or per file -- is distorted by wherever
              we chose to put a helper; a string is content and does not move
              when we re-factor, so this is what separates a real gap from a
              factoring artefact.  Read it before restoring anything: two of
              the three largest rows in `--missing` are artefacts (findings
              F2600 and F2950).  Necessary and not sufficient, exactly as
              `--invented` is -- see the note at its implementation.
  --strings   the format strings themselves, per function.  Useful BEFORE
              restoring anything: the strings are the annotation, and they
              routinely name fields and conditions the reconstruction is
              otherwise guessing at (finding F136 is an example).
  --invented  the opposite direction, and the one --missing cannot see: every
              string literal THIS TREE carries that does not appear anywhere
              in the object's .rodata or .data.  Such a string was written
              rather than read -- usually from the function's own name -- and
              no test catches it unless something compares that function's
              transcript.  Four were found this way after `V34EchoCleanUp`
              turned up printing its own name (finding F180).

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
              on TU boundaries that symbol ordering cannot give (finding F135).

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
import os
import glob
import re
import ast
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


def show_sites(obj, tabs, func, window):
    """
    Every diagnostic call site in one function, with its argument setup.

    `--strings` resolves a format by walking back to the nearest .rodata
    relocation, which is wrong wherever more than one string is pushed before
    a single call -- `CALLPROG_Progress` has nine such sites and they come out
    as <unresolved>.  This prints the instructions instead and resolves EVERY
    string in the window, leaving the reading to a human.  That is the right
    division of labour: which push is the format and which are arguments is a
    calling-convention question, and cdecl pushes the format at (%esp).
    """
    out = run("objdump", "-dr", "--section=.text", obj)
    lines = out.split("\n")

    # The function's own lines, with each relocation folded into the
    # instruction it belongs to -- objdump prints them separately, which makes
    # every "the instruction before this one" question off by one.
    body, cur = [], None
    for line in lines:
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
        if m:
            cur = m.group(1)
            continue
        if cur != func or not line.strip():
            continue
        if re.match(r"^\s+[0-9a-f]+:\s+R_386", line) and body:
            body[-1] = body[-1] + "   <== " + line.split(None, 1)[1].strip()
        else:
            body.append(line.rstrip())
    if not body:
        sys.exit("no such function in %s: %s" % (obj, func))

    def annotate(line):
        """Resolve every .rodata address in the line to its string."""
        m = re.search(r"R_386_32\s+(\.rodata[^\s]*)", line)
        if not m:
            return line
        im = re.search(r"\$0x([0-9a-f]+)", line)
        if not im:
            return line
        s = string_at(tabs, m.group(1), int(im.group(1), 16))
        return line if s is None else "%s\n        %r" % (line, s)

    sites = [i for i, l in enumerate(body) if "R_386_PC32" in l and DBG in l]
    print("%s -- %d diagnostic call site%s\n"
          % (func, len(sites), "" if len(sites) == 1 else "s"))
    for n, i in enumerate(sites):
        start = max(0, i - window)
        # Do not run back past the previous call: those are another site's
        # arguments, not this one's.
        for j in range(i - 1, start, -1):
            if re.search(r"\bcall\b", body[j]):
                start = j + 1
                break
        print("  --- site %d of %d ---" % (n + 1, len(sites)))
        for line in body[start:i + 1]:
            print("   %s" % annotate(line).rstrip())
        # Where it goes next.  GCC moves these blocks out of line, so the
        # jump AFTER the call is what says where the site sits in the source;
        # without it every cold block looks like it belongs at the end.
        for line in body[i + 1:i + 3]:
            print("   %s" % line.rstrip())
            if re.search(r"\bjmp\b|\bret\b", line):
                break
        print()

    # The gates: every read of dsplibs_debug_level, with its branch.
    gates = [l for l in body if "dsplibs_debug_level" in l]
    print("  gates on dsplibs_debug_level (%d):" % len(gates))
    for g in gates:
        print("   %s" % g.rstrip())
    print("\n  A gate is `cmpl $0x1` + `ja`/`jbe` where the site fires at 2 "
          "and above.\n  Anything else is a threshold we do not reproduce -- "
          "see finding F150.")
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


#
# AN ASM TEMPLATE IS NOT A DIAGNOSTIC STRING.
#
# `__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x))` carries
# two string literals -- the instruction template and the constraint -- and
# neither is in the blob's .rodata, because neither is data.  This gate exists
# to catch INVENTED DIAGNOSTICS (findings F180, F201): a format string that
# behaves identically to the right one because `dsplibs_debug_level` ships at
# zero.  An instruction template has no such failure mode; it is checked by
# the differential test like any other code.  So asm statements are blanked
# before the scan, exactly as comments and preprocessor lines already are.
#
ASM_HEAD = re.compile(r"\b(?:__asm__|asm)\b\s*(?:__volatile__|volatile)?\s*\(")


def strip_asm(src):
    """Blank the body of every asm statement, keeping the line count."""
    out, i = [], 0
    while True:
        m = ASM_HEAD.search(src, i)
        if not m:
            out.append(src[i:])
            return "".join(out)
        out.append(src[i:m.start()])
        depth, j = 0, m.end() - 1          # at the opening paren
        while j < len(src):
            if src[j] == "(":
                depth += 1
            elif src[j] == ")":
                depth -= 1
                if depth == 0:
                    j += 1
                    break
            j += 1
        out.append("\n" * src.count("\n", m.start(), j))
        i = j


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
        #
        # Preprocessor lines are emptied rather than skipped, because the
        # scan below runs over the WHOLE file and cannot be told which line
        # it is on.  Their content is the only source of literals that are
        # legitimately not the blob's -- every one is an `#include` path.
        #
        src = "\n".join("" if l.lstrip().startswith("#") else l
                        for l in src.split("\n"))
        #
        # And the asm statements, for the reason above the helper.
        #
        src = strip_asm(src)
        #
        # ONE PASS OVER THE FILE, NOT ONE PER LINE.  `RUN` separates adjacent
        # literals with `\s*`, which spans newlines -- but only if it is
        # given them.  Scanned line by line, a message split across source
        # lines came back as its fragments, and each fragment is a substring
        # of the whole, so a TRUNCATED format string matched and passed.
        # That is how the K56Flex mutation of finding F195 got past this
        # sweep.  142 of 592 strings were fragments.
        #
        for m in RUN.finditer(src):
            s = unescape("".join(LIT.findall(m.group(0))))
            if s:
                out.append((p, src.count("\n", 0, m.start()) + 1, s))
    return out


def main():
    ap = argparse.ArgumentParser(
        description="Audit dsplibs.o's diagnostic call sites against this "
                    "reconstruction.")
    ap.add_argument("--obj",
                    default=os.environ.get("BLOB",
                                           "ref/slmodemd/dsplibs.o"))
    ap.add_argument("--src", nargs="*", default=None)
    ap.add_argument("--missing", action="store_true")
    ap.add_argument("--strings", metavar="FUNC", nargs="?", const="",
                    help="print format strings, optionally for one function")
    ap.add_argument("--stamps", action="store_true")
    ap.add_argument("--sites", metavar="FUNC",
                    help="one function's call sites in full: the gate, the "
                         "argument setup, and every string resolved")
    ap.add_argument("--window", type=int, default=14,
                    help="instructions of argument setup to show (--sites)")
    ap.add_argument("--invented", action="store_true",
                    help="our format strings that are not in the object")
    ap.add_argument("--absent", metavar="FUNC", nargs="?", const="",
                    help="the object's format strings that appear NOWHERE in "
                         "this tree, optionally for one function")
    args = ap.parse_args()

    tabs = rodata_strings(args.obj)
    sites = blob_sites(args.obj, tabs)

    if args.src is None:
        #
        # .cpp AS WELL AS .c.  The reconstruction has C++ translation units
        # because the object has them -- FloatIIR.cpp, and VPcmV34Main.cpp's
        # one mangled entry point in v34pcmmain.cpp -- and this check globbed
        # only "*.c" from the day it was written.  FloatIIR.cpp prints
        # nothing, so the gap was invisible until a C++ file arrived carrying
        # seven format strings, which is finding F134's own argument about a
        # check that reports clean because it cannot fail.
        #
        args.src = (glob.glob("src/**/*.c", recursive=True)
                    + glob.glob("src/**/*.cpp", recursive=True))
    ours, where = our_sites(args.src)

    if args.stamps:
        print("Build stamps baked into .rodata -- one per TU that printed "
              "__DATE__/__TIME__.\nThe seconds distinguish translation units "
              "(finding F135).\n")
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

    if args.sites:
        show_sites(args.obj, tabs, args.sites, args.window)
        return
    if args.invented:
        haystack = b"".join(blob for _, blob in tabs.values())
        haystack += subprocess.run(
            ["objcopy", "-O", "binary", "--only-section=.data",
             args.obj, "/dev/stdout"], capture_output=True).stdout
        found = our_strings(args.src)
        #
        # NUL-ANCHORED, not a bare substring.  Every string in .rodata is
        # terminated, so requiring the terminator demands that our literal be
        # a WHOLE string of the object's rather than any run of bytes inside
        # one.  A substring test accepts a TRUNCATION -- which is exactly what
        # a mis-transcribed format string looks like -- and that is how the
        # K56Flex mutation of finding F195 passed this sweep.
        #
        # It only became possible once `our_strings` glued literals across
        # source lines: before that, every multi-line message arrived here as
        # its fragments and the substring test was the only thing making them
        # match.  The two halves of this check have to move together.
        #
        bad = [(p, n, t) for p, n, t in found
               if t.encode("latin1") + b"\0" not in haystack]
        #
        # DECLARED BRANCH-ONLY STRINGS.  A branch may add instrumentation the
        # blob never had; it declares it in docs/invented_strings.txt and the
        # entry is reported separately instead of failing.  An UNdeclared
        # string still fails, which is the whole value of the gate.  See that
        # file's header for why the declaration is not just an allow-list.
        #
        declared = {}
        decl_path = os.path.join(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__))), "docs", "invented_strings.txt")
        try:
            for line in open(decl_path):
                if line.startswith("#") or not line.strip():
                    continue
                br, lit, why = line.rstrip("\n").split("\t", 2)
                declared[ast.literal_eval(lit)] = (br, why)
        except OSError:
            pass
        ok = [(p, n, t) for p, n, t in bad if t in declared]
        bad = [(p, n, t) for p, n, t in bad if t not in declared]
        print("String literals in this tree that are not WHOLE strings of "
              "the object's\n.rodata or .data -- so they were invented, or "
              "truncated.  Findings F180, F201.\n")
        for p, n, t in bad:
            print("  INVENTED  %s:%d\n            %r" % (p, n, t))
        for p, n, t in ok:
            print("  declared  %s:%d  [%s]\n            %r"
                  % (p, n, declared[t][0], t))
        print("\n  %d checked, %d not present in the object, "
              "%d of those declared in docs/invented_strings.txt"
              % (len(found), len(bad) + len(ok), len(ok)))
        return 1 if bad else 0

    if args.absent is not None:
        #
        # WHICH OF THE OBJECT'S DIAGNOSTICS ARE REALLY NOT HERE.
        #
        # Neither count above can answer that.  The per-function table is
        # distorted by any helper we factor out, and the per-file table -- the
        # one this tool's own comment below calls the one "an inlining
        # boundary cannot distort" -- is distorted by a helper we factor into
        # ANOTHER FILE, because the blob's sites are attributed to the file
        # holding our function of the same name and ours are counted where we
        # put them.  `v8handshak.c` read as `-8 (blob 10, ours 2)` with all
        # ten of the object's strings present, eight of them in `v8hsrx.c`,
        # whose `+8 (blob 0, ours 8)` is the other half of the same number.
        # Finding F2950.
        #
        # A string is content and does not move when we re-factor, so this
        # asks the question the counts were standing in for.
        #
        # ITS LIMIT IS `--invented`'s, MIRRORED, and it is the reason this
        # prints a queue and not a verdict: a string being somewhere in the
        # tree is NECESSARY and not sufficient.  It does not show the site is
        # in the right function, under the right condition, or carrying the
        # right arguments -- only a transcript comparison does that (findings
        # 126, 2600).  So an empty report means "nothing to restore", never
        # "these sites are right".
        #
        # AND IT COUNTS DISTINCT STRINGS, NOT SITES.  Where the object prints
        # one message from several places -- `probe_preemph`'s three strings
        # over ten inlined copies (finding F2600) -- carrying it once satisfies
        # this check.  That is deliberate, because the number of copies is
        # precisely what inlining decides and what we are trying not to
        # measure, but it means a row of `0 absent` bounds the gap at "no
        # message was lost" and not at "no call site was lost".
        #
        which = args.absent
        have = set(t for _, _, t in our_strings(args.src))
        print("Format strings the object has and this tree does not carry "
              "ANYWHERE.\nString presence is necessary, not sufficient: it "
              "says a site was not\ndropped, never that it is in the right "
              "place.  Finding F2950.\n")
        rows, unres = [], 0
        for fn in sorted(sites):
            if which and fn != which:
                continue
            if not which and fn not in ours:
                continue            # only reconstructed ones, unless named
            miss = sorted(set(s for s in sites[fn] if s and s not in have))
            n_un = sum(1 for s in sites[fn] if not s)
            unres += n_un
            if miss or n_un:
                rows.append((len(miss), n_un, fn, where.get(fn, "?"), miss))
        rows.sort(reverse=True)
        for n, n_un, fn, f, miss in rows:
            print("  %-30s %s" % (fn, f))
            print("      %d absent, %d unresolvable" % (n, n_un))
            for s in miss:
                print("        %r" % s)
        print("\n  %d absent over %d function%s; %d call sites could not be "
              "resolved to a\n  string at all and are not judged either way "
              "(see the --strings caveat)."
              % (sum(r[0] for r in rows), len(rows),
                 "" if len(rows) == 1 else "s", unres))
        if not rows:
            print("  every one of the object's resolvable format strings is "
                  "somewhere in src/.")
        return 0

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
          "call and a\npresent one behave identically.  Finding F134.\n")
    for gap, n, g, fn, p in rows:
        print("  %3d missing  (blob %2d, ours %2d)  %-30s %s"
              % (gap, n, g, fn, p))
    print("\n  %d functions, %d call sites, %d total in the blob"
          % (len(rows), sum(r[0] for r in rows),
             sum(len(v) for v in sites.values())))

    #
    # THE SAME COUNTS PER FILE, which is the only comparison an inlining
    # boundary cannot distort.
    #
    # The per-function table above compares a blob function against OUR
    # function of the same name.  Where the reconstruction split one of the
    # original's functions into static helpers -- because the original's
    # compiler inlined a helper the source really had, or because the function
    # was too big to read in one piece -- our sites sit in functions the blob
    # has no symbol for.  They are then counted against neither side: they
    # vanish from `ours`, and the blob's function shows the whole difference as
    # missing.  `callprog.c` is the worked example: `CALLPROG_Progress` reads
    # as 15 sites short, and every one of them is present a few lines away in
    # `request_state`, `detect`, `apply_event` or `run_timeouts`.  See finding
    # 605.
    #
    # THIS TABLE HAS A BOUNDARY OF ITS OWN, and it used to say here that it
    # had none.  It falls through wherever the helper is in a DIFFERENT FILE:
    # the blob's sites are attributed to whichever file holds our function of
    # the same name, and ours are counted where we actually put them, so a
    # split across files debits one file and credits the other.  `v8handshak.c`
    # read as `-8 (blob 10, ours 2)` with every one of the object's ten
    # strings present -- eight of them in `v8hsrx.c`, whose `+8 (blob 0,
    # ours 8)` is the same eight sites counted the other way.  Finding F2950.
    #
    # `--absent` is the check with no such boundary, because it compares
    # content rather than counts.  Read it before restoring anything.
    #
    by_file_blob, by_file_ours = {}, {}
    for fn, lst in sites.items():
        f = where.get(fn)
        if f:
            by_file_blob[f] = by_file_blob.get(f, 0) + len(lst)
    for fn, n in ours.items():
        f = where.get(fn)
        if f:
            by_file_ours[f] = by_file_ours.get(f, 0) + n
    frows = []
    for f in sorted(set(by_file_blob) | set(by_file_ours)):
        b, o = by_file_blob.get(f, 0), by_file_ours.get(f, 0)
        if b != o:
            frows.append((b - o, b, o, f))
    frows.sort(reverse=True)
    print("\nPER FILE, where a helper in the SAME file cannot hide a site."
          "\nA helper in another file still can -- run --absent before "
          "restoring (2950).")
    if not frows:
        print("  every reconstructed file matches the blob's count exactly")
    for gap, b, o, f in frows:
        print("  %+4d  (blob %3d, ours %3d)  %s" % (-gap, b, o, f))


if __name__ == "__main__":
    sys.exit(main() or 0)
