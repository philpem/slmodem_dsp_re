#!/usr/bin/env python3
"""
Decide whether a candidate DATA symbol can be made file-local (static).

WHY THIS EXISTS

F11448's `dataaudit.py` showed the remaining missing data symbols are the
blob's LOCALS -- `FixedRC.c`'s `rc*_filter`, the `Dialer`/`Callprog` families --
which this tree defines under other names, often as GLOBALs.  Reconciling them
means making ours `static` under the blob's name.  But the blob's LOCAL binding
proves no OTHER translation unit references the symbol, so if ours has a
cross-TU reference the change breaks the link -- a hard failure, not a metric
win (`dataaudit.py`'s own CANDIDATE MATCHES cannot see that; size and section
are not enough).

This tool reads the per-TU object tree (`build/tc_repro/*.o`, the objects that
go into `build/partial/dsplibs.o`) and, for each queried symbol, reports the
object(s) that DEFINE it and every object that REFERENCES it (an undefined
symbol).  A symbol with a reference from a non-defining object cannot become
`static` without moving the referrer into the same TU first.

DENOMINATOR.  Every run prints how many objects were scanned and how many
symbols were queried, and separates "safe to make static" from "cross-TU
reference forbids it".  `--self-test` assembles a defining and a referencing
object, proves the reference is found, proves an unreferenced symbol reports
none, then removes the reference and proves it is gone.

Usage:
    staticcheck.py [--objdir DIR] [--symbol NAME]... [--file PATH]
    staticcheck.py --self-test

`--file` reads one symbol name per line (`#` comments allowed); this is how a
`dataaudit.py --json` missing list is fed in.
"""

import argparse
import collections
import os
import re
import shutil
import subprocess
import sys
import tempfile


DEFAULT_OBJDIR = "build/tc_repro"


def nm_scan(objdir):
    """Return (objects, defined, referenced).

    defined:   name -> sorted [relative object path]
    referenced: name -> sorted [relative object path] (undefined there)
    """
    objs = sorted(f for f in os.listdir(objdir) if f.endswith(".o"))
    defined = collections.defaultdict(list)
    referenced = collections.defaultdict(list)
    for obj in objs:
        path = os.path.join(objdir, obj)
        out = subprocess.run(["nm", "-P", path], capture_output=True,
                             text=True)
        if out.returncode != 0:
            sys.exit("staticcheck: nm failed on %s: %s"
                     % (path, out.stderr.strip()))
        for line in out.stdout.splitlines():
            parts = line.split()
            if len(parts) < 2:
                continue
            name, typ = parts[0], parts[1]
            if typ == "U":
                referenced[name].append(obj)
            elif typ != "?" and typ != "N":
                defined[name].append(obj)
    return objs, defined, referenced


def query(symbols, objs, defined, referenced):
    rows = []
    for name in symbols:
        defs = defined.get(name, [])
        refs = referenced.get(name, [])
        outside = sorted(set(refs) - set(defs))
        rows.append({"name": name, "defined": defs, "referenced": refs,
                     "outside": outside})
    return rows


def report(rows, nobjs):
    print("staticcheck: %d object(s) scanned, %d symbol(s) queried"
          % (nobjs, len(rows)))
    safe = [r for r in rows if not r["outside"]]
    blocked = [r for r in rows if r["outside"]]
    print("  can be static: %d" % len(safe))
    for r in safe:
        print("    %-30s defined=%s refs=%s"
              % (r["name"], ",".join(r["defined"]) or "-",
                 ",".join(r["referenced"]) or "none"))
    print("  cross-TU reference forbids static: %d" % len(blocked))
    for r in blocked:
        print("    %-30s defined=%s referenced-by=%s"
              % (r["name"], ",".join(r["defined"]) or "?",
                 ",".join(r["outside"])))
    print("  verdict: %d safe / %d blocked / %d queried"
          % (len(safe), len(blocked), len(rows)))


# --------------------------------------------------------------------------
# self-test

def _assemble(d, name, text):
    src = os.path.join(d, name + ".s")
    obj = os.path.join(d, name + ".o")
    with open(src, "w") as f:
        f.write(text)
    subprocess.run(["as", "--32", "-o", obj, src], check=True,
                   capture_output=True)
    return obj


def self_test():
    if not (shutil.which("as") and shutil.which("nm")):
        sys.exit("staticcheck --self-test needs as and nm")
    with tempfile.TemporaryDirectory(prefix="staticcheck-") as d:
        _assemble(d, "definer",
                  '.file "definer.c"\n.section .rodata\n'
                  '.globl shared_sym\n.type shared_sym,@object\n'
                  'shared_sym: .long 1\n.size shared_sym,.-shared_sym\n'
                  '.globl lonely_sym\n.type lonely_sym,@object\n'
                  'lonely_sym: .long 2\n.size lonely_sym,.-lonely_sym\n')
        _assemble(d, "user",
                  '.file "user.c"\n.text\n.globl use\n.type use,@function\n'
                  'use: movl $shared_sym, %eax\nret\n.size use,.-use\n')
        objs, defined, referenced = nm_scan(d)
        rows = query(["shared_sym", "lonely_sym"], objs, defined, referenced)
        by = {r["name"]: r for r in rows}
        assert by["shared_sym"]["outside"], rows
        assert not by["lonely_sym"]["outside"], rows
        report(rows, len(objs))
        print("  self-test: shared_sym reference found, lonely_sym clean")

        _assemble(d, "user",
                  '.file "user.c"\n.text\n.globl use\n.type use,@function\n'
                  'use: movl $0, %eax\nret\n.size use,.-use\n')
        objs, defined, referenced = nm_scan(d)
        rows = query(["shared_sym"], objs, defined, referenced)
        assert not rows[0]["outside"], rows
        print("  self-test: reference removed, shared_sym now clean")


# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--objdir", default=DEFAULT_OBJDIR)
    ap.add_argument("--symbol", action="append", default=[])
    ap.add_argument("--file")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        self_test()
        return 0

    symbols = list(args.symbol)
    if args.file:
        with open(args.file) as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    symbols.append(line)
    if not symbols:
        sys.exit("staticcheck: no --symbol and no --file; refusing to report "
                 "a denominator of zero (F134/F2400/F2401)")
    if not os.path.isdir(args.objdir):
        sys.exit("staticcheck: no object directory %s" % args.objdir)

    objs, defined, referenced = nm_scan(args.objdir)
    if not objs:
        sys.exit("staticcheck: %s holds zero objects (F134)" % args.objdir)
    rows = query(symbols, objs, defined, referenced)
    report(rows, len(objs))
    missing = [r for r in rows if not r["defined"]]
    if missing:
        print("  NOTE: %d queried symbol(s) are defined by no object: %s"
              % (len(missing), ", ".join(r["name"] for r in missing)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        sys.exit("staticcheck: %s" % error)
