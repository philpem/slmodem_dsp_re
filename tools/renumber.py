#!/usr/bin/env python3
"""Renumber a branch's findings at merge, without touching anything else.

WHY THIS EXISTS

Parallel batches allocate findings numbers at write time and the parent
renumbers them at merge. That was done with a regex over the branch's files
for eight merges, and on the ninth it did this:

    src/callprog/elliptic.c:75    8192, -14430,   7822,   ->   7829
    src/callprog/elliptic.c:236   8192, -15415,   7823,   ->   7830

-- two elliptic filter COEFFICIENTS, in a file the branch had never touched,
because a finding number and a filter coefficient are the same four digits.
The same sweep repointed `finding F7820` in a sibling wave's source comment and
two citations in `refinement.md` that pointed at the finding those sentences
were about. **`refcheck.py` passed throughout**, because all four still
RESOLVED -- findings F212/213's failure mode, and the one thing that checker
structurally cannot see. Finding F7833.

WHAT THIS DOES INSTEAD, AND WHAT IT REFUSES

1. **Only files the BRANCH changed.** Derived from git, never a glob. A file
   the branch did not touch cannot contain a citation the branch owns.
2. **Only CITATIONS, never a bare integer.** In prose (`.md`) a number in
   parentheses or after "finding" is a citation. In CODE it must be preceded
   by `finding`/`findings` -- a four-digit number on its own in a `.c` is a
   coefficient until proved otherwise, and proving otherwise is not this
   tool's job.
3. **Every change is printed** with its file, line and both spellings, because
   a summary is what let the coefficients through.

It is deliberately conservative: it would rather leave a citation for a human
than rewrite a constant. `refcheck.py` catches what it leaves; nothing catches
what it would have broken.
"""

import argparse
import os
import re
import subprocess
import sys

#
# A CITATION IN PROSE.  `### 7820.` is a heading, `(7820)` a parenthetical,
# `finding F7820` / `findings F7820-7825` explicit, `7820's` possessive.
#
PROSE = [
    (re.compile(r"(?<=^### )(\d{3,5})(?=\.)", re.M), "heading"),
    (re.compile(r"(?<=\()(\d{3,5})(?=\))"), "parenthetical"),
    (re.compile(r"(?<=\bfinding )(\d{3,5})\b", re.I), "finding N"),
    (re.compile(r"(?<=\bfindings )(\d{3,5})\b", re.I), "findings N"),
    (re.compile(r"(?<=\bfindings \d{4}[-–])(\d{3,5})\b", re.I), "range end"),
    (re.compile(r"(\d{3,5})(?='s\b)"), "possessive"),
]

#
# A CITATION IN CODE.  ONLY where the word `finding` precedes it.  This is the
# whole point: `8192, -14430, 7822,` must never match, and it cannot.
#
CODE = [
    (re.compile(r"(?<=\bfinding )(\d{3,5})\b", re.I), "finding N"),
    (re.compile(r"(?<=\bfindings )(\d{3,5})\b", re.I), "findings N"),
    (re.compile(r"(?<=\bfindings \d{4}[-–])(\d{3,5})\b", re.I), "range end"),
]

PROSE_EXT = (".md", ".txt")
FINDINGS = "docs/findings.md"


def changed_files(branch, base):
    """Files the branch itself changed -- git's answer, not a glob."""
    r = subprocess.run(["git", "diff", "--name-only", "%s...%s" % (base, branch)],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("renumber.py: git diff failed for %s...%s" % (base, branch))
    return [f for f in r.stdout.split() if os.path.exists(f)]


def rewrite(path, mapping, apply_it):
    rules = PROSE if path.endswith(PROSE_EXT) else CODE
    lines = open(path, encoding="utf-8", errors="surrogateescape").read().split("\n")
    hits = []
    for i, line in enumerate(lines):
        new = line
        for rx, why in rules:
            def sub(m):
                old = m.group(1)
                return mapping.get(old, old)
            new = rx.sub(sub, new)
        if new != line:
            hits.append((i + 1, line.strip(), new.strip()))
            lines[i] = new
    if hits and apply_it:
        open(path, "w", encoding="utf-8",
             errors="surrogateescape").write("\n".join(lines))
    return hits


def self_test():
    """Prove it rewrites a citation and REFUSES a coefficient."""
    cases = [
        # (filename, line, mapping, expected)
        ("docs/findings.md", "### 7820. A HEADING", {"7820": "7827"},
         "### 7827. A HEADING"),
        ("docs/method/x.md", "were extra code (7823) -- see", {"7823": "7830"},
         "were extra code (7830) -- see"),
        ("docs/method/x.md", "as finding F7820 measured", {"7820": "7827"},
         "as finding F7827 measured"),
        ("src/a.cpp", " * ENUMERATED (finding F7820).  The map", {"7820": "7827"},
         " * ENUMERATED (finding F7827).  The map"),
        #
        # THE CASE THIS TOOL EXISTS FOR.  A coefficient, in a code file, whose
        # value happens to be a live finding number.
        #
        ("src/callprog/elliptic.c", "\t  8192, -14430,   7822,   8192,",
         {"7822": "7829"}, "\t  8192, -14430,   7822,   8192,"),
        ("src/callprog/CPfiltrs.c", "\t8192, -14686, 7832,", {"7832": "7839"},
         "\t8192, -14686, 7832,"),
        #
        # A bare parenthetical in CODE is not enough either -- it could be an
        # array bound or a magic number.
        #
        ("src/b.c", "\tif (x == 7820) return;", {"7820": "7827"},
         "\tif (x == 7820) return;"),
    ]
    bad = 0
    for path, line, mapping, want in cases:
        rules = PROSE if path.endswith(PROSE_EXT) else CODE
        got = line
        for rx, _ in rules:
            got = rx.sub(lambda m: mapping.get(m.group(1), m.group(1)), got)
        ok = got == want
        bad += not ok
        print("  %-5s %-28s %s" % ("ok" if ok else "FAIL", os.path.basename(path),
                                   got.strip()))
    print("\n  %d case(s), %d must-rewrite, %d must-REFUSE, %d failure(s)"
          % (len(cases), 4, 3, bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--branch", help="the branch whose findings are being renumbered")
    ap.add_argument("--base", default="master", help="merge base side (default master)")
    ap.add_argument("--map", action="append", default=[], metavar="OLD:NEW",
                    help="a single mapping; repeatable")
    ap.add_argument("--shift", nargs=3, type=int, metavar=("FIRST", "LAST", "NEWFIRST"),
                    help="shift a contiguous block")
    ap.add_argument("--apply", action="store_true",
                    help="write the changes (default is a dry run)")
    ap.add_argument("--post-merge", action="store_true",
                    help="allow a post-merge run (you must read every hit)")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()

    if a.self_test:
        return self_test()
    if not a.branch:
        sys.exit("renumber.py: --branch is required (or --self-test)")

    mapping = {}
    for m in a.map:
        old, new = m.split(":")
        mapping[old.strip()] = new.strip()
    if a.shift:
        first, last, newfirst = a.shift
        for k in range(first, last + 1):
            mapping[str(k)] = str(newfirst + (k - first))
    if not mapping:
        sys.exit("renumber.py: no mapping given -- use --map or --shift")

    #
    # RUN THIS ON THE BRANCH, BEFORE THE MERGE.  Scope and citation-matching
    # kill the coefficient class outright, but they cannot tell WHOSE number a
    # citation is once both sets live in one file: after a merge `docs/
    # findings.md` holds the branch's `### 7823.` and master's `### 7823.`
    # side by side and nothing in the text distinguishes them.  That is how
    # two of master's citations got repointed even with the scope fixed.
    #
    # On the branch there is no ambiguity -- master's numbers are not there
    # yet.  So renumber first, then merge.  This refuses when the tree already
    # contains a heading for a number the mapping wants to CREATE, because
    # that is the signature of a post-merge run.
    #
    if os.path.exists(FINDINGS) and not a.post_merge:
        text = open(FINDINGS, encoding="utf-8", errors="surrogateescape").read()
        taken = [new for new in sorted(set(mapping.values()))
                 if re.search(r"^### %s\." % re.escape(new), text, re.M)]
        if taken:
            sys.exit(
                "renumber.py: REFUSING -- %s already has a heading for %s.\n"
                "  That means both sets of numbers are in one file, which is a\n"
                "  post-merge tree, and a citation there cannot be attributed to\n"
                "  one side.  Renumber ON THE BRANCH and then merge; that is the\n"
                "  only ordering in which the question has an answer (7833).\n"
                "  Override with --post-merge only if you have read every hit."
                % (FINDINGS, ", ".join(taken)))

    files = changed_files(a.branch, a.base)
    if not files:
        sys.exit("renumber.py: %s changed NO files against %s -- nothing to "
                 "renumber, and a silent zero is not an answer." % (a.branch, a.base))

    print("renumber: %d mapping(s) over %d file(s) the branch changed%s\n"
          % (len(mapping), len(files), "" if a.apply else "   [DRY RUN]"))
    total = 0
    for f in sorted(files):
        hits = rewrite(f, mapping, a.apply)
        if not hits:
            continue
        kind = "prose" if f.endswith(PROSE_EXT) else "CODE "
        print("  %s %s" % (kind, f))
        for ln, before, after in hits:
            print("      %5d  %s" % (ln, before[:96]))
            print("            -> %s" % after[:96])
        total += len(hits)
    print("\n  %d citation(s) rewritten over %d file(s)." % (total, len(files)))
    if not a.apply:
        print("  DRY RUN -- re-run with --apply to write.")
    print("  Numbers in CODE files were rewritten only where the word "
          "`finding` precedes\n  them; a bare four-digit constant is a "
          "coefficient here and is left alone (7833).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
