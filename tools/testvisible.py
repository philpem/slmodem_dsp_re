#!/usr/bin/env python3
"""Emit the file-local reconstruction symbols a differential test must reach BY NAME.

The problem
-----------
The reference object records many top-level tables and datapump entry points as
file-LOCAL.  To match it the reconstruction defines them `static`, but a
differential test that compares `RATEv32[i]` against `ref_RATEv32[i]` then has
no symbol to name: a static definition in one object cannot satisfy another
object's undefined reference, and the link fails.

This is the mirror of `symmap.py`.  That tool globalizes the BLOB's file-local
symbols so `ref_*` can be named; this one globalizes OURS so the plain name can
be named.  It never touches `src/` or the partial-link candidate: it emits a
list of names, and the build links a globalized COPY of each object for the
test binaries only.

What it keeps
-------------
A name is emitted only when it is

  * defined LOCAL in some reconstructed object, and
  * defined in exactly ONE of them (a name used by two translation units
    cannot be globalized -- both would become the same global and the linker
    would take one at random), and
  * named somewhere under `test/`.

The third test is a token scan of the sources, so it over-approximates rather
than under-approximates: a name mentioned only in a comment is still emitted,
which is harmless, while a name a test needs is never omitted.  The build fails
LOUDLY (undefined reference) if the scan ever misses one, which is the safe
direction.

Usage:
    testvisible.py --objects build/repro --tests test -o build/test_visible.txt
"""

import argparse
import os
import re
import subprocess
import sys


def defined_locals(obj):
    """Names this object defines, split into its LOCAL and GLOBAL sets."""
    out = subprocess.run(["nm", "--defined-only", obj], capture_output=True,
                         text=True)
    if out.returncode != 0:
        sys.exit("error: nm failed on %s:\n%s" % (obj, out.stderr))
    local, global_ = [], []
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        name = parts[-1]
        if parts[-2] in "tdrb":
            local.append(name)
        elif parts[-2] in "TDRBC":
            global_.append(name)
    return local, global_


def objects_under(root):
    found = []
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            if name.endswith(".o"):
                found.append(os.path.join(dirpath, name))
    return sorted(found)


def referenced_tokens(test_root):
    tokens = set()
    word = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
    for dirpath, _dirs, files in os.walk(test_root):
        for name in files:
            if not name.endswith((".c", ".cpp", ".h")):
                continue
            try:
                with open(os.path.join(dirpath, name), errors="replace") as f:
                    tokens.update(word.findall(f.read()))
            except OSError:
                pass
    return tokens


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--objects", help="directory tree of reconstructed objects")
    src.add_argument("--from-list", metavar="FILE",
                     help="a file listing object paths, one per line")
    ap.add_argument("--tests", default="test",
                    help="directory tree whose source tokens count as a use")
    ap.add_argument("-o", "--output", required=True)
    args = ap.parse_args()

    if args.from_list:
        with open(args.from_list) as f:
            objs = [line.strip() for line in f if line.strip()]
    else:
        objs = objects_under(args.objects)
    if not objs:
        sys.exit("error: no objects given (empty list) -- refusing to write an "
                 "empty list")
    for obj in objs:
        if not os.path.exists(obj):
            sys.exit("error: object %s does not exist" % obj)

    seen = {}
    globals_ = set()
    for obj in objs:
        local, global_ = defined_locals(obj)
        globals_.update(global_)
        for name in local:
            seen[name] = seen.get(name, 0) + 1
    if not seen:
        sys.exit("error: no file-local symbols found in %d object(s) -- "
                 "refusing to write an empty list" % len(objs))

    # A name is globalizable only if it is defined local exactly once AND is
    # not also defined GLOBAL anywhere.  A name that is global in one object
    # and local in another would collide if the local copy were promoted, which
    # is exactly the `ToneLPF` shape (Fdspkrnl.c and fpm_tone_cfg.c).
    dup = sorted(n for n, c in seen.items() if c > 1 or n in globals_)
    used = referenced_tokens(args.tests)
    visible = sorted(n for n, c in seen.items()
                     if c == 1 and n not in globals_ and n in used)

    with open(args.output, "w") as f:
        f.write("".join(n + "\n" for n in visible))

    print("test-visible: %d of %d local names (%d unique, %d ambiguous) from "
          "%d objects; %d names in the list"
          % (len(visible), len(seen), sum(1 for c in seen.values() if c == 1),
             len(dup), len(objs), len(visible)))
    if dup:
        print("  left local, name used by more than one TU: %s"
              % " ".join(dup))


if __name__ == "__main__":
    main()
