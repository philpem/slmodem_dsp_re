#!/usr/bin/env python3
"""
Inventory the "thin" comments in one source file, and say which of them a
mutation anchor depends on byte-for-byte.

A thin comment is one whose entire text is addresses and punctuation --
`/* 0x6abae */`, `/* 0x6aa0b, 0x6c9aa */`, `/* else 0x6c133 */`.  It names a
place in the object and says nothing about it, which is what the comment pass
is here to fix.

WHY THE SECOND COLUMN MATTERS.  `tools/mutate.py` locates each mutation by an
exact substring of the source, and `test/mutations/v34hsrx72.json`'s own note
says the anchors carry addresses *because the source lines repeat*.  So 168 of
this file's thin comments are inside some suite's `find` string, and rewriting
one there makes the mutation UNUSABLE -- finding 347, where an unusable
mutation does not fail its own run and the suite silently stops testing what
it claims to.  (The 168 was measured when the file had 270 thin comments and
task #65 has since taken it to 256; run the tool rather than quoting either.)

`make phase` DOES catch that, which was worth checking rather than assuming:
`anchorcheck.py` runs in the `refs` target and returns 1 on any anchor
matching other than exactly once.  Rewriting one frozen thin comment in a
throwaway copy of the tree takes it from "0 anchor(s) match other than exactly
once" to two NOT UNIQUE lines and exit 1.  So the hazard is a red gate, not a
silent loss.

What this tool adds is that the gate is retrospective and this is not.  It
says WHICH comments are load-bearing before anything is edited, and where a
new line may be inserted without splitting an anchor, so a comment pass over a
file with 3,611 anchors against it does not have to proceed by trial.

    tools/cmtsites.py src/pump/v34/v34hshak.c --json /tmp/sites.json
"""

import argparse
import bisect
import glob
import json
import os
import re
import sys

THIN = re.compile(
    r"/\*\s*(?:\+?0x[0-9a-fA-F]{3,6}|and|or|,|\.|else|:|;|-{1,2}|\s)+\*/")


def anchor_covered_lines(src, path, mutdir):
    """Line indices of `src` that some mutation `find` string spans."""
    starts, off = [], 0
    for line in src.split("\n"):
        starts.append(off)
        off += len(line) + 1
    covered = set()
    for jf in sorted(glob.glob(os.path.join(mutdir, "*.json"))):
        try:
            doc = json.load(open(jf))
        except ValueError:
            continue
        if not isinstance(doc, list):
            continue
        for ent in doc:
            if not isinstance(ent, dict) or "find" not in ent:
                continue
            at = src.find(ent["find"])
            while at >= 0:
                a = bisect.bisect_right(starts, at) - 1
                b = bisect.bisect_right(starts, at + len(ent["find"]) - 1) - 1
                covered.update(range(a, b + 1))
                at = src.find(ent["find"], at + 1)
    return covered


def anchor_spans(src, mutdir):
    """(first, last) line index of every mutation `find` occurrence."""
    starts, off = [], 0
    for line in src.split("\n"):
        starts.append(off)
        off += len(line) + 1
    spans = []
    for jf in sorted(glob.glob(os.path.join(mutdir, "*.json"))):
        try:
            doc = json.load(open(jf))
        except ValueError:
            continue
        if not isinstance(doc, list):
            continue
        for ent in doc:
            if not isinstance(ent, dict) or "find" not in ent:
                continue
            at = src.find(ent["find"])
            while at >= 0:
                a = bisect.bisect_right(starts, at) - 1
                b = bisect.bisect_right(starts, at + len(ent["find"]) - 1) - 1
                spans.append((a, b))
                at = src.find(ent["find"], at + 1)
    return spans


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--mutations", default="test/mutations")
    ap.add_argument("--json")
    args = ap.parse_args()

    src = open(args.source).read()
    lines = src.split("\n")
    covered = anchor_covered_lines(src, args.source, args.mutations)

    #
    # Where a new line may be inserted.  Inserting before line P splits any
    # anchor whose span starts above P and ends at or below it, so those
    # positions are barred; everything else is free, including immediately
    # above an anchor's own first line.
    #
    barred = set()
    for a, b in anchor_spans(src, args.mutations):
        barred.update(range(a + 1, b + 1))

    sites = []
    for i, line in enumerate(lines):
        for m in THIN.finditer(line):
            at = i
            while at in barred:
                at -= 1
            sites.append({"line": i + 1, "text": m.group(0),
                          "frozen": i in covered,
                          "append_ok": i not in covered,
                          "insert_above": at + 1, "code": line})
    #
    # PAIRED.  The thin comment itself stays -- the address is a real
    # cross-reference into the disassembly and 168 of them are load-bearing
    # for a mutation anchor besides.  What the comment pass changes is
    # whether the address is ALSO explained: a site counts as paired when
    # its address appears in a comment that is not itself thin.
    #
    # A distant mention does not pair -- it has to be close enough to read
    # with the code it is about, so the window is forty lines either way.
    NEAR = 40
    prose = []                       # (line, text) of every non-thin comment
    for m in re.finditer(r"/\*(?:[^*]|\*(?!/))*\*/", src, re.S):
        if not THIN.fullmatch(m.group(0)):
            prose.append((src[:m.start()].count("\n") + 1, m.group(0)))
    for s in sites:
        addrs = re.findall(r"0x[0-9a-fA-F]{3,6}", s["text"])
        near = "\n".join(t for ln, t in prose
                         if abs(ln - s["line"]) <= NEAR)
        s["paired"] = bool(addrs) and all(a in near for a in addrs)

    frozen = sum(1 for s in sites if s["frozen"])
    paired = sum(1 for s in sites if s["paired"])
    print("%s: %d thin comments, %d frozen by a mutation anchor, %d free"
          % (args.source, len(sites), frozen, len(sites) - frozen))
    print("%s  %d have their address explained in prose within %d lines, "
          "%d are bare" % (" " * len(args.source), paired, NEAR,
                           len(sites) - paired))
    if args.json:
        json.dump(sites, open(args.json, "w"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
