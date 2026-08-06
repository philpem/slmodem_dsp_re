#!/usr/bin/env python3
"""
Does each mutation still mutate the thing its label names?

WHY THIS EXISTS, AND WHY IT IS NOT `reanchor.py`

`mutate.py` matches `find` as a substring of the whole file, so when a second
arm of `v34handshak` lands an anchor written against the first can start
matching twice.  That is reported UNUSABLE and `tools/reanchor.py` repairs it.

Finding 432 is the other half, and it is worse.  An anchor can stay UNIQUE and
silently come to point somewhere else:

  * two anchors here were "the last `t3m_txblock` before the 0x64ad2 comment",
    which was arm 59's exit until arm 58 landed after it; and
  * seven labelled `the shared reset (47, 49, 50) ...` were mutating arm 51's
    record fill, having been made unique at an earlier repair by DEEPENING the
    anchor a tab rather than by moving it.

All nine were reported CAUGHT -- at claims nobody made.  `src.count(find) == 1`
holds for every one of them, so uniqueness is not the property that matters:
WHERE the anchor lands is.  Neither `mutate.py`, `reanchor.py` nor
`refcheck.py` can see this by construction.

WHAT THIS CHECKS, AND WHAT IT CANNOT

Labels in these suites name a microstate -- "58's head", "the shared reset
(47, 49, 50)", "51's gain shifts".  The dispatch binds each microstate to one
arm function.  So: if a label names microstates, and the anchor lands inside
an arm function belonging to a DIFFERENT microstate, that is a re-pointing.

It is deliberately quiet about two legitimate cases:

  * an anchor in a SHARED helper, whose name encodes no microstate -- a label
    naming three arms and pointing into `t3m_errrec_core` is correct; and
  * a label with no microstate number in it at all.

So this is a heuristic over prose, and a clean run is not a proof.  It is the
difference between checking nothing and checking the case that has already
gone wrong nine times.
"""

import argparse
import json
import os
import re
import sys

#
# THE MAP COMES FROM THE HEADER, NOT FROM A COMMENT.
#
# A first version read the microstate number out of the `/* 41, 0x669a4 */`
# comment beside each case label.  Two arms carry no such comment and one
# switch elsewhere in the file produced two phantom entries, so the map had
# four entries of which two were wrong and four arms were missing -- a check
# with a hole exactly where the file is busiest.  `V34HS_DET_SYNC` is defined
# once, in v34hshak.h, and that is what the case label says.
#
CASE = re.compile(
    r"^\s*case\s+(V34HS_\w+):[^\n]*\n"
    r"((?:\s*case\s+V34HS_\w+:[^\n]*\n)*)"
    r"\s*(?:/\*(?:[^*]|\*(?!/))*\*/\s*)?"
    r"\s*([a-z_][a-z_0-9]*)\(", re.M)
CASE_NAME = re.compile(r"case\s+(V34HS_\w+):")
STATE_DEF = re.compile(r"^#define\s+(V34HS_\w+)\s+(\d+)\s*$", re.M)
# a definition in this tree's style: return type on its own line, name at col 0
DEFN = re.compile(r"^([a-z_][a-z_0-9]*)\(", re.M)
NUMS = re.compile(r"\b(\d{2})\b")


def defn_index(src):
    return sorted((m.start(), m.group(1)) for m in DEFN.finditer(src))


def enclosing(index, pos):
    lo, best = 0, None
    for start, name in index:
        if start > pos:
            break
        best = name
    return best


def state_values():
    """V34HS_* -> its number, from the one place each is defined."""
    try:
        h = open(os.path.join("include", "dsplib", "v34hshak.h")).read()
    except OSError:
        return {}
    return {m.group(1): int(m.group(2)) for m in STATE_DEF.finditer(h)}


def arm_map(src, values=None):
    """microstate number -> the function its case label calls."""
    values = values if values is not None else state_values()
    out = {}
    for m in CASE.finditer(src):
        names = [m.group(1)] + CASE_NAME.findall(m.group(2) or "")
        fn = m.group(3)
        for n in names:
            if n in values:
                out[values[n]] = fn
    return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("suite", nargs="*",
                    help="suites to check; default every registered one")
    args = ap.parse_args()

    reg = json.load(open(os.path.join("test", "mutations", "suites.json")))
    names = args.suite or [k for k in reg if not k.startswith("_")]
    suspect = 0
    for name in sorted(names):
        entry = reg.get(name)
        if not entry or not isinstance(entry, list) or len(entry) != 2:
            continue
        source = entry[0]
        if not os.path.exists(source):
            continue
        path = os.path.join("test", "mutations", name + ".json")
        if not os.path.exists(path):
            continue
        try:
            muts = json.load(open(path))
        except ValueError:
            continue
        src = open(source).read()
        idx, arms = defn_index(src), arm_map(src)
        if not arms:
            continue
        #
        # ONE FUNCTION CAN OWN MANY MICROSTATES.  Twenty-four of the forty
        # share a single arm, so a fn -> number dict would keep whichever won
        # the race and call every anchor in that arm a re-pointing.  fn ->
        # SET, and a label is wrong only if it names none of the set.
        #
        owner = {}
        for n, fn in arms.items():
            owner.setdefault(fn, set()).add(n)
        for m in muts:
            if not isinstance(m, dict) or "find" not in m:
                continue
            hits = [i for i in range(len(src))
                    if src.startswith(m["find"], i)]
            if len(hits) != 1:
                continue                      # reanchor.py's problem, not ours
            fn = enclosing(idx, hits[0])
            if fn not in owner:
                continue                      # shared helper: legitimate
            here = owner[fn]
            label = m.get("label", "")
            claimed = {int(x) for x in NUMS.findall(label)
                       if int(x) in arms}
            if claimed and not (claimed & here):
                print("  RE-POINTED  %s" % name)
                print("      label    %s" % label)
                print("      names    %s" % sorted(claimed))
                print("      lands in %s, which is microstate%s %s\n"
                      % (fn, "" if len(here) == 1 else "s",
                         ", ".join(str(x) for x in sorted(here))))
                suspect += 1
    print("  %d anchor(s) land in an arm their label does not name" % suspect)
    return 1 if suspect else 0


if __name__ == "__main__":
    sys.exit(main())
