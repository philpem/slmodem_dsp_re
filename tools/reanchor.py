#!/usr/bin/env python3
"""
Make a mutation set's `find` strings unique again after a new arm lands.

WHY THIS EXISTS

`mutate.py` matches `find` as a substring of the whole source file.  The arms
of `v34handshak` are near-identical at their edges -- almost every one ends by
setting a transmit state and a microstate through `hs_setstate` and falling
into the block dispatch -- so the moment a second arm lands, a one- or two-line
anchor written against the first matches twice.

A doubly-matching anchor is reported UNUSABLE, and UNUSABLE DOES NOT FAIL A
RUN.  The suite still prints `0 NOT caught`.  Four batches have now silently
lost mutations that way (finding 347), and the repair each time was the same
mechanical edit: extend the anchor by whole lines until it is unique again.

WHICH OCCURRENCE

Extending upward from the wrong occurrence would silently re-point a mutation
at a different claim, which is worse than leaving it unusable.  So the choice
is made and REPORTED, never guessed silently:

  * the preferred occurrence is the one whose surrounding lines mention the
    suite's own macro prefix most often -- `T44_` for a suite whose source
    region is microstate 44's arm, and so on, which is exactly what the
    per-arm prefixes introduced by finding 325 are good for; and
  * every choice is printed with its line number, so a reader can check it.

If no prefix distinguishes them, the mutation is left alone and named.  A
mutation that cannot be placed is a decision for whoever owns the arm.

THE TIE GUARD IS UNCONDITIONAL, AND IT USED NOT TO BE

That last paragraph was a promise the tool did not keep.  The guard was
gated on `--prefix` having been given, and the sort's second key was the
file offset, descending -- so with no `--prefix` every occurrence scored 0,
nothing could ever reach the guard, and the LAST textual match won every
time.  Run against `v90p3ddec`, that re-anchored nine mutations out of
`getV90Decision` and into `getV92Decision` and called it "9 re-anchored, 0
left for a human".  A relocated mutation is worse than a lost one: the
suite stays green while nine of its claims have changed which code they
test (finding 2120, and 455 is the same picked-the-wrong-occurrence one
occurrence at a time).

So: the sort is keyed on the SCORE ALONE -- the offset is no longer a
tiebreak, because there is no defensible reason to prefer the later
occurrence over the earlier one -- and the guard runs whether or not
`--prefix` was given.  With no prefix, every ambiguous anchor now reads
STUCK, which is the honest answer: nothing was supplied that could tell
the occurrences apart.
"""

import argparse
import collections
import json
import os
import re
import sys


def occurrences(src, needle):
    out, i = [], src.find(needle)
    while i >= 0:
        out.append(i)
        i = src.find(needle, i + 1)
    return out


def line_of(src, pos):
    return src.count("\n", 0, pos) + 1


def prefix_score(src, pos, prefixes, window=1200):
    """How strongly the text just before `pos` belongs to this suite."""
    lo = max(0, pos - window)
    chunk = src[lo:pos]
    return sum(chunk.count(p) for p in prefixes)


def extend(src, needle, pos, limit=12):
    """Grow the anchor upward by whole lines until it matches only once."""
    start = pos
    for _ in range(limit):
        prev = src.rfind("\n", 0, start - 1)
        if prev < 0:
            break
        start = prev + 1
        cand = src[start:pos] + needle
        if len(occurrences(src, cand)) == 1:
            return cand
    return None


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("suite", help="a name in test/mutations/suites.json")
    ap.add_argument("--prefix", action="append", default=[],
                    help="macro prefix that marks this suite's own arm, "
                         "e.g. --prefix T44_ (repeatable)")
    ap.add_argument("--write", action="store_true",
                    help="rewrite the JSON; without it, report only")
    args = ap.parse_args()

    reg = json.load(open(os.path.join("test", "mutations", "suites.json")))
    if args.suite not in reg:
        sys.exit("no suite %r" % args.suite)
    source = reg[args.suite][0]
    src = open(source).read()
    path = os.path.join("test", "mutations", args.suite + ".json")
    muts = json.load(open(path))

    fixed = stuck = 0
    for m in muts:
        if "find" not in m:
            continue
        hits = occurrences(src, m["find"])
        if len(hits) <= 1:
            continue
        label = m.get("label", "?")
        scored = sorted(((prefix_score(src, h, args.prefix), h) for h in hits),
                        key=lambda t: t[0], reverse=True)
        if len(scored) > 1 and scored[0][0] == scored[1][0]:
            print("  STUCK    %-52s %d sites, %s"
                  % (label[:52], len(hits),
                     "prefix does not separate them" if args.prefix
                     else "no --prefix given, so nothing separates them"))
            stuck += 1
            continue
        pos = scored[0][1]
        grown = extend(src, m["find"], pos)
        if grown is None:
            print("  STUCK    %-52s %d sites, no unique anchor within 12 lines"
                  % (label[:52], len(hits)))
            stuck += 1
            continue
        lead = grown[:-len(m["find"])]
        print("  anchored %-52s to line %d (+%d line(s) of context)"
              % (label[:52], line_of(src, pos), lead.count("\n")))
        m["find"] = grown
        m["replace"] = lead + m["replace"]
        m.setdefault("note", "")
        m["note"] = (m["note"] + " " if m["note"] else "") + (
            "Anchor carries %d line(s) above it: another arm of this function "
            "spells the same statements, and a doubly-matching anchor reads as "
            "UNUSABLE rather than as a failure. Finding 347."
            % lead.count("\n"))
        fixed += 1

    print("\n  %d re-anchored, %d left for a human" % (fixed, stuck))
    if args.write and fixed:
        json.dump(muts, open(path, "w"), indent=1)
        print("  wrote %s" % path)
    elif fixed:
        print("  (dry run; pass --write)")
    return 1 if stuck else 0


if __name__ == "__main__":
    sys.exit(main())
