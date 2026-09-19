#!/usr/bin/env python3
"""linkdiverge.py -- declare a differential fixture the MODERN host compiler
cannot LINK, because a symbol the period object emits is absent from the
modern object.

    tools/linkdiverge.py --list
    tools/linkdiverge.py --check --build build
    tools/linkdiverge.py --run t_agc
    tools/linkdiverge.py --test t_agc -- <link command>

WHY THIS EXISTS.

`make period` builds and links with GCC 3.4.2 -- the compiler that built the
object -- and every fixture links.  The modern portability tier builds the
same source with GCC 13+ and a handful of fixtures name a file-LOCAL symbol
that GCC 13 either RENAMES or DROPS:

  * RENAMED -- `AnalyseDialString` (Dialer.c) and `bValidateEnergyValue`
    (Fdspkrnl.c) are partially inlined / constant-propagated by GCC 13.
    APPARATUS flags on the host tree close these (HOSTPORTFLAGS in the
    Makefile), so they are NOT registered here -- finding F11359.
  * DROPPED, AND FLAG-CLOSABLE -- `pGlobalFDSPObj`/`uCorrelationReportsNo`
    (Fdsp.c), `v34initialbauds` (VpcmFloModem.cpp) and the V.22/V.32 static
    tables are eliminated because nothing takes their address; the same
    HOSTPORTFLAGS `-fno-toplevel-reorder` keeps them.  F11359 tried
    `-fno-tree-dce`/`-fno-dce`/`-fkeep-static-consts` and found no flag;
    `-fno-toplevel-reorder` was not tried there and is the correction.  These
    are NOT registered here either.
  * DROPPED, AND NOT FLAG-CLOSABLE -- the header-only template weak copies
    (`Agc`, `DiffCoder`, `LowPassFIR`, `Queue`, `Scrambler`, `SineWave`).
    GCC 13+ inlines every call and emits no out-of-line copy; only
    `-fno-inline` keeps them, and that changes every translation unit.  These
    fixtures cannot link and are DECLARED here.  Issue #77.

THE DISCIPLINE, the same as tools/gccdiverge.json and for the same reason:

  * IT NAMES A FIXTURE AND ITS SYMBOLS.  An entry excuses the fixture only
    when the linker's undefined set is a SUBSET of the symbols it names; an
    unexpected undefined reference, or a failure that is not an undefined
    reference at all, still fails the build.
  * A STALE ENTRY IS AN ERROR.  If a declared fixture now LINKS, `--check`
    exits non-zero and says so.  Otherwise the register accumulates excuses
    for problems that fixed themselves.
  * IT APPLIES TO THE MODERN BUILD ONLY.  `make period` never consults it.
  * EVERY ENTRY CITES A FINDING.
  * THE FIXTURE IS NOT RUN.  No stub binary is produced and nothing is
    executed: an excused fixture prints LINK-EXCUSED and is counted.  It is
    not a pass and it is not a tolerance.

An entry is a statement that MODERN GCC CANNOT BUILD WHAT THE PERIOD COMPILER
BUILDS, not that the fixture is inconvenient.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REGISTER = os.path.join(HERE, "linkdiverge.json")

#
# Both the first occurrence and the "more undefined references to `X' follow"
# summary line, so a fixture's full symbol set is collected in one pass.
#
UNDEF = re.compile(r"undefined references? to `([^']+)'")

#
# A link failure is excusable ONLY if every error line is an undefined
# reference (or the linker's own "collect2: ... ld returned 1" summary).  A
# `multiple definition`, a truncated relocation or a missing library must not
# be swallowed by an entry that happens to name a symbol the linker also
# mentioned.
#
COLLECT2 = "collect2: error: ld returned 1 exit status"


def load():
    try:
        with open(REGISTER) as f:
            return json.load(f)
    except OSError:
        return {}


def demangle(names):
    """Normalise both sides of the comparison.

    Recent binutils demangles C++ names in its undefined-reference messages;
    older ones do not.  `c++filt` is idempotent on an already-demangled name
    and decodes a mangled one, so the register can be written in the readable
    form and still match either linker.  If `c++filt` is absent the strings
    are compared as-is -- which fails LOUDLY, in the direction that stops the
    gate, rather than quietly excusing the wrong thing.
    """
    if not names:
        return []
    if shutil.which("c++filt"):
        p = subprocess.run(["c++filt"], input="\n".join(names),
                           capture_output=True, text=True)
        out = p.stdout.splitlines()
        if p.returncode == 0 and len(out) == len(names):
            return out
    return list(names)


def bad_errors(out):
    """Error lines that are NOT undefined references, if any."""
    bad = []
    for line in out.splitlines():
        if "error:" not in line:
            continue
        if "undefined reference" in line or COLLECT2 in line:
            continue
        bad.append(line.strip())
    return bad


def cmd_link(reg, args):
    r = subprocess.run(args.command, capture_output=True, text=True)
    out = r.stdout + r.stderr
    sys.stdout.write(out)
    entry = reg.get(args.test)

    if r.returncode == 0:
        if entry:
            print("  STALE  %s is declared link-excused in "
                  "tools/linkdiverge.json and now LINKS." % args.test)
            print("         Delete the entry.  If the reason expired, say so "
                  "in finding %s." % entry.get("finding", "?"))
            return 1
        return 0

    if not entry:
        return r.returncode

    other = bad_errors(out)
    if other:
        print("  %s is declared link-excused, but the link failed for a "
              "reason that is not an undefined reference:" % args.test)
        for line in other[:10]:
            print("      %s" % line)
        return r.returncode

    found = set(demangle(UNDEF.findall(out)))
    if not found:
        print("  %s is declared link-excused, but no undefined reference was "
              "reported -- refusing to excuse a different failure."
              % args.test)
        return r.returncode

    allowed = set(demangle(entry.get("symbols", [])))
    unexpected = found - allowed
    if unexpected:
        print("  %s is declared link-excused, but these undefined symbols are "
              "NOT covered:" % args.test)
        for s in sorted(unexpected):
            print("      %s" % s)
        print("  An entry excuses the symbols it names and nothing else.")
        return r.returncode

    print("  LINK-EXCUSED  %s: %d symbol(s) the period object emits and the "
          "modern object does not." % (args.test, len(found)))
    for s in sorted(found):
        print("      %s" % s)
    print("           %s" % entry.get("why", "").strip())
    print("           `make period` is authoritative here.  Finding %s."
          % entry.get("finding", "?"))
    return 0


def cmd_run(reg, args):
    entry = reg.get(args.test)
    if not entry:
        print("linkdiverge: %s is NOT link-excused and its binary does not "
              "exist -- refusing to treat a missing binary as a pass."
              % args.test)
        return 1
    print("  LINK-EXCUSED  %s: %d symbol(s) absent from the modern object; "
          "not run." % (args.test, len(entry.get("symbols", []))))
    return 0


def cmd_check(reg, args):
    testdir = os.path.join(args.build, "test")
    if not os.path.isdir(testdir):
        print("linkdiverge: REFUSING -- %s does not exist, so nothing was "
              "built and every entry would read as excused." % testdir)
        return 1
    binaries = [f for f in os.listdir(testdir)
                if os.path.isfile(os.path.join(testdir, f))
                and os.access(os.path.join(testdir, f), os.X_OK)]
    if not binaries:
        print("linkdiverge: REFUSING -- %s holds no executables, so the "
              "test tier did not run (findings 134, 2401)." % testdir)
        return 1

    stale, excused = [], []
    for name in sorted(reg):
        binary = os.path.join(testdir, name)
        (stale if os.path.exists(binary) else excused).append(name)

    if stale:
        print("linkdiverge: STALE entries -- these now LINK and must be "
              "deleted from tools/linkdiverge.json:")
        for name in stale:
            print("    %s" % name)
        return 1

    line = ("link exceptions: %d declared, %d excused, 0 stale "
            "(%d binaries built)" % (len(reg), len(excused), len(binaries)))
    print(line)
    if args.counts:
        with open(args.counts, "w") as f:
            f.write(line + "\n")
    return 0


def cmd_list(reg, args):
    if not reg:
        print("linkdiverge: no entries -- the modern build links every "
              "fixture it is tested against")
        return 0
    print("linkdiverge: %d declared fixture(s)" % len(reg))
    for name, e in sorted(reg.items()):
        print("%s  (finding %s)" % (name, e.get("finding", "?")))
        for s in e.get("symbols", []):
            print("    %s" % s)
        print("    %s" % e.get("why", "").strip())
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--run", metavar="TEST")
    ap.add_argument("--test", metavar="TEST")
    ap.add_argument("--build", default="build")
    ap.add_argument("--counts", metavar="PATH",
                    help="write the --check summary line here")
    ap.add_argument("command", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    reg = load()
    if args.list:
        return cmd_list(reg, args)
    if args.check:
        return cmd_check(reg, args)
    if args.run:
        args.test = args.run
        return cmd_run(reg, args)
    if args.test:
        if args.command and args.command[0] == "--":
            args.command = args.command[1:]
        if not args.command:
            ap.error("--test needs the link command after --")
        return cmd_link(reg, args)
    ap.error("nothing to do: use --list, --check, --run or --test")


if __name__ == "__main__":
    sys.exit(main())
