#!/usr/bin/env python3
"""
Does each mutation still mutate the thing its label names?

WHY THIS EXISTS, AND WHY IT IS NOT `reanchor.py`

`mutate.py` matches `find` as a substring of the whole file, so when a second
arm of `v34handshak` lands an anchor written against the first can start
matching twice.  That is reported UNUSABLE and `tools/reanchor.py` repairs it.

Finding F432 is the other half, and it is worse.  An anchor can stay UNIQUE and
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
#
# THE SECOND RULE IS OPT-IN, BECAUSE THE PROSE VERSION DID NOT WORK.
#
# Finding F455: `reanchor.py` repaired three anchors in the `v34pcmif` set,
# reported "0 left for a human", and pointed all three at the WRONG FUNCTION.
# They would still have read CAUGHT.  Rule 1 cannot see it -- `v34pcmif.c` has
# no `case V34HS_*` dispatch, so the arm map is empty and the suite is skipped.
#
# The obvious generalisation was to read the label's `tag:` prefix -- `reneg:`
# for VPcmV34InitiateRateRenegotiation -- and require the enclosing function's
# name to contain it.  MEASURED ON THE TREE: six flags, all six false.  The
# tags are topical, not abbreviations: `tail:` means the tail of
# `v34handshakinit`'s setup and merely substring-matches `t3c_block_tail`;
# `probe:` means the probe sequence and matches `probeselect`.  Zero true
# positives.  A check that cries wolf is worse than no check, which is the
# same lesson the comment-derived arm map taught one commit earlier.
#
# So the rule is exact and opt-in instead: a mutation may carry
#
#     "fn": "VPcmV34InitiateRateRenegotiation"
#
# and the anchor must land in that function.  No inference, no false
# positives, and it protects exactly the entries whose author asked for it.
#
STATE_DEF = re.compile(r"^#define\s+(V34HS_\w+)\s+(\d+)\s*$", re.M)
#
# A definition in this tree's style: return type on its own line, name at
# column 0.  Three versions of this regex have had silent holes, so the rule
# is now structural rather than lexical.
#
#   1. It required a LOWERCASE first letter, so every `VPcmV34*` and `V34*`
#      function was invisible and `v34pcmif.c` looked like a file with no
#      functions in it.
#   2. It stopped at `[A-Za-z_0-9]*`, which does not include `::`, so every
#      QUALIFIED C++ METHOD was invisible too -- and because a bare
#      `^NAME(` also matches a MACRO INVOCATION at column 0, what it found
#      instead was noise.  `VPcmFloModem.cpp` reported 45 "definitions", all
#      45 of them `VPCM_OFF(...)` and not one of them a function.  Rule 1's
#      `enclosing()` therefore returned a macro name for every anchor in
#      every C++ file, and Rule 2's `"fn"` could only ever report BAD fn.
#
# So: match the name, walk to the matching close paren, and require the next
# non-space character to be `{`.  That is what separates a definition from a
# macro call (`;`) and from a forward declaration (`;`).  The name recorded
# is the trailing `::` component, because that is what a `"fn"` field spells.
#
DEFN = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*(?:::[A-Za-z_~][A-Za-z_0-9]*)*)\(",
                  re.M)
NUMS = re.compile(r"\b(\d{2})\b")


def defn_index(src):
    """Every function DEFINITION in `src`, as (offset, trailing name)."""
    out = []
    for m in DEFN.finditer(src):
        i, depth = m.end() - 1, 0
        while i < len(src):
            if src[i] == "(":
                depth += 1
            elif src[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        else:
            continue
        j = i + 1
        while j < len(src) and src[j] in " \t\r\n":
            j += 1
        if j < len(src) and src[j] == "{":      # a body, so a definition
            out.append((m.start(), m.group(1).split("::")[-1]))
    return sorted(out)


def label_of(m):
    return m.get("label", "(no label)")


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
    #
    # A SKIP IS A DEFECT, NOT A NON-EVENT.
    #
    # Every `continue` below used to be silent, and the tree was about to run
    # a batch that DELETES a registered source file (v34hshak_t3mid.c, which
    # 455 mutations are pinned to).  With a silent skip that batch comes back
    # `0 anchor(s) land in an arm their label does not name` and exit 0, with
    # a third of the mutation coverage checked by nothing.  That is finding
    # 134's argument -- a detector that cannot be told apart from a clean
    # tree is not a detector -- and it is the same shape as `extcheck`
    # printing "(none)" through four broken versions.
    #
    # So each skip is counted, named and fails the run.
    #
    skipped, checked, seen, nonuniq, vacuous = [], 0, 0, [], []
    for name in sorted(names):
        entry = reg.get(name)
        if not entry or not isinstance(entry, list) or len(entry) != 2:
            skipped.append((name, "suites.json entry is not [source, binary]"))
            continue
        source = entry[0]
        if not os.path.exists(source):
            skipped.append((name, "source %s does not exist" % source))
            continue
        path = os.path.join("test", "mutations", name + ".json")
        if not os.path.exists(path):
            skipped.append((name, "registered, but there is no %s" % path))
            continue
        try:
            muts = json.load(open(path))
        except ValueError as e:
            skipped.append((name, "%s is malformed: %s" % (path, e)))
            continue
        checked += 1
        src = open(source).read()
        idx, arms = defn_index(src), arm_map(src)
        defined = {n for _, n in idx}
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
            seen += 1
            #
            # A MUTATION THAT DOES NOT CHANGE THE FILE IS NOT A MUTATION.
            #
            # `mutate.py` now reports these (finding F572: re-anchoring a
            # suite after a refactor made five of them in one pass, all
            # carrying `equivalent: true`, so they printed `survived,
            # equivalent` and the totals did not move).  But it reports them
            # as UNUSABLE, and unusable DOES NOT FAIL A RUN -- which is the
            # property that cost four batches their mutations (finding F347).
            #
            # This is the static half, and it is free: no build, no suite,
            # just a string compare.  It fails.
            #
            if m.get("replace") == m["find"]:
                vacuous.append((name, label_of(m)))
                continue
            hits = [i for i in range(len(src))
                    if src.startswith(m["find"], i)]
            if len(hits) != 1:
                #
                # `reanchor.py` repairs these, but nothing FAILED on them:
                # `mutate.py` prints ANCHOR MATCHES n TIMES and carries on,
                # and an unusable mutation still leaves the suite reporting
                # `0 NOT caught` (finding F347, four batches lost mutations
                # that way).  Counting them here is what makes a lost anchor
                # visible without running a suite that takes an hour.
                #
                nonuniq.append((name, label_of(m), len(hits)))
                continue
            fn = enclosing(idx, hits[0])
            label = m.get("label", "")

            #
            # RULE 2, opt-in: `"fn"` says which function the anchor belongs
            # in, and is checked exactly.  Applies to every suite.
            #
            want = m.get("fn")
            if want:
                if want not in defined:
                    print("  BAD fn      %s: %r names no function in %s"
                          % (name, want, source))
                    suspect += 1
                    continue
                if fn != want:
                    print("  RE-POINTED  %s" % name)
                    print("      label    %s" % label)
                    print("      fn says  %s" % want)
                    print("      lands in %s\n" % fn)
                    suspect += 1
                continue

            if fn not in owner:
                continue                      # shared helper: legitimate
            here = owner[fn]
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
    for name, why in skipped:
        print("  SKIPPED     %s: %s" % (name, why))
    for name, label, n in nonuniq:
        print("  NOT UNIQUE  %s: %-46s matches %d time(s)"
              % (name, label[:46], n))
    for name, label in vacuous:
        print("  VACUOUS     %s: %-46s replace == find" % (name, label[:46]))
    print("\n  %d suite(s) checked, %d mutation(s), %d skipped"
          % (checked, seen, len(skipped)))
    print("  %d anchor(s) match other than exactly once" % len(nonuniq))
    print("  %d mutation(s) whose replace equals their find" % len(vacuous))
    print("  %d anchor(s) land in an arm their label does not name" % suspect)
    return 1 if (suspect or skipped or nonuniq or vacuous) else 0


if __name__ == "__main__":
    sys.exit(main())
