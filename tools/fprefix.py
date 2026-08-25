#!/usr/bin/env python3
"""One-shot migration: give findings an `F` prefix, as deviations have `D`.

WHY

A finding number is four bare digits and a filter coefficient is four bare
digits, so nothing in the text tells them apart. Renumbering a branch's
findings at merge with a regex rewrote

    src/callprog/elliptic.c:75    8192, -14430,   7822,   ->   7829

and `refcheck.py` passed, because the reference still RESOLVED (7833).
Deviations never had this problem: `D250` cannot be a coefficient.

**THE TREE ALREADY KNEW.** `refcheck.py`'s `FINDING_REF` requires the word
`finding` before a number, and the comment above it cites finding F543 --
"the coefficient rows that would be corrupted by treating them as one". The
rule existed and was encoded in the checker; the merge procedure simply did
not follow it. A prefix makes the rule structural instead of a convention
each tool has to re-implement, which is `gates.md`'s rule 5.

WHAT IS REWRITTEN, AND WHAT IS NOT

The set of KNOWN finding numbers is read from the headings themselves, and
nothing outside that set is ever touched. On top of that:

- **In prose** (`.md`): headings, and `finding N` / `findings N, M and K`
  using **`refcheck.py`'s own regex**, imported rather than re-implemented, so
  the migration and the checker cannot disagree about what a citation is.
- **Bare `(N)` and `N's` are NEVER rewritten**, and being a known finding
  number is not enough to make one a citation -- `fcomp %st(1)` is an x87
  stack register, and 2400 is both a live finding and a baud rate. See the
  note at rule 3.
- **In code** (`.c`, `.cpp`, `.h`, `.py`, `.json`): ONLY the anchored
  `finding N` forms. A bare number in a code file is a coefficient here --
  708 of them are -- and this tool cannot and must not judge otherwise.

A JSON ANCHOR EMBEDS SOURCE TEXT WITH ESCAPED NEWLINES, AND THAT HID FOUR
CITATIONS

`test/mutations/*.json` anchors quote source verbatim, but with `\n` as two
literal characters. A citation split across a line --

    * ... on either side.  Finding
    * 722.

-- matches in the `.c`, where the separator is a real newline the pattern's
emphasis class can span, and does NOT match in the JSON, where it is a
backslash. So four anchors had their source rewritten underneath them and
stopped matching, while `refcheck` stayed green because the citations still
resolved. Repaired by decoding each JSON string, migrating it, and putting it
back re-escaped, which is the only level at which the two agree.

**Any text-level migration over this tree has to do the JSON separately.**

VERIFICATION, WHICH IS THE POINT

This touches comments and prose only, so **the period objects must be
byte-identical before and after**. Diff `build/tc_out` both ways -- the
objects, not the grade counts. A single byte that moves means a rewrite
escaped into code, and the migration must be abandoned rather than patched.
"""

import argparse
import importlib.util
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FINDINGS = "docs/findings.md"

#
# refcheck.py's OWN definition of a citation, imported.  Writing a second one
# is how a migration and its checker come to disagree, which is 7773's rule.
#
_spec = importlib.util.spec_from_file_location("refcheck",
                                               os.path.join(HERE, "refcheck.py"))
_refcheck = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_refcheck)
FINDING_REF = _refcheck.FINDING_REF
FINDING_HEAD = _refcheck.FINDING_HEAD

PROSE_EXT = (".md", ".txt")
CODE_EXT = (".c", ".cpp", ".h", ".hpp", ".py", ".json", ".sh", ".mk")

HEAD_RX = re.compile(r"^(#{2,4} )(\d+[a-z]?)(\.)", re.M)
NUM_IN_LIST = re.compile(r"\b(\d+[a-z]?)\b")


def known_numbers():
    text = open(FINDINGS, encoding="utf-8", errors="surrogateescape").read()
    return {n for n, _ in FINDING_HEAD.findall(text)}


def migrate_text(text, known, prose):
    """Return (new_text, hits). `prose` widens what counts as a citation."""
    hits = []

    def note(before, after):
        if before != after:
            hits.append((before, after))

    #
    # 1. HEADINGS, prose only.  `### 7833.` -> `### F7833.`
    #
    if prose:
        def head(m):
            new = m.group(1) + "F" + m.group(2) + m.group(3)
            note(m.group(0), new)
            return new
        text = HEAD_RX.sub(head, text)

    #
    # 2. THE ANCHORED FORMS, using refcheck's regex so the two agree.  The
    #    match may carry a list -- `findings F134, F2400 and F2401` -- and every
    #    member of it is a citation.
    #
    def anchored(m):
        whole = m.group(0)
        body = m.group(1)
        def one(mm):
            n = mm.group(1)
            return ("F" + n) if n in known else n
        new_body = NUM_IN_LIST.sub(one, body)
        new = whole[: len(whole) - len(body)] + new_body
        note(whole, new)
        return new
    text = FINDING_REF.sub(anchored, text)

    #
    # 3. BARE `(N)` AND `N's` ARE NOT REWRITTEN, ANYWHERE, and the reason is
    #    worth keeping.  Membership in the known-findings set is NOT enough to
    #    make a bare number a citation:
    #
    #        docs/method/refinement.md:893   `fcomp %st(1)`+`jae` (5701)
    #        docs/method/conformance-plan.md "start-bit (0) ... stop-bit (1)"
    #        docs/method/equivalence.md:41   plus (1) accounts for the entire
    #
    #    `%st(1)` is an x87 stack register and would have become `%st(F1)`.
    #    At four digits it is no better: 2400 and 9600 are both live finding
    #    numbers AND baud rates, and this tree writes about both.
    #
    #    So the migration rewrites ONLY what `refcheck.py` already tracks --
    #    headings and the anchored `finding N` forms. The roughly one hundred
    #    informal bare citations stay bare, which loses nothing that was ever
    #    machine-checked, and afterwards they are VISIBLY unprefixed among
    #    prefixed neighbours: a reader can convert them to `finding N` and the
    #    checker will then see them for the first time.
    #
    return text, hits


def targets():
    r = subprocess.run(["git", "ls-files"], capture_output=True, text=True)
    out = []
    for f in r.stdout.split():
        if not os.path.exists(f):
            continue
        if f.endswith(PROSE_EXT) or f.endswith(CODE_EXT):
            out.append(f)
    return out


def self_test():
    known = {"7822", "7833", "543", "134", "2400", "2401", "7820", "5701", "1"}
    cases = [
        ("prose", "### 7833. A HEADING", "### F7833. A HEADING"),
        ("prose", "see finding F7833 for why", "see finding F7833 for why"),
        ("prose", "findings F134, F2400 and F2401 are the same shape",
         "findings F134, F2400 and F2401 are the same shape"),
        # A BARE PARENTHETICAL IS LEFT ALONE EVEN WHEN IT IS A KNOWN NUMBER.
        ("prose", "a bare (7833) citation", "a bare (7833) citation"),
        ("prose", "7820's harness wrote through a hardlink",
         "7820's harness wrote through a hardlink"),
        ("prose", "the window is (5) samples wide", "the window is (5) samples wide"),
        # the case that killed the rule: an x87 stack register
        ("prose", "`fcomp %st(1)`+`jae` (5701)", "`fcomp %st(1)`+`jae` (5701)"),
        # and a baud rate that is also a finding number
        ("prose", "the (2400) baud arm", "the (2400) baud arm"),
        #
        # THE CASES THIS EXISTS FOR.
        #
        ("code", "\t  8192, -14430,   7822,   8192,", "\t  8192, -14430,   7822,   8192,"),
        ("code", "\t8192, -14686, 7832,", "\t8192, -14686, 7832,"),
        ("code", " * ENUMERATED (finding F7820).  The map",
         " * ENUMERATED (finding F7820).  The map"),
        ("code", "\tif (x == 7822) return;", "\tif (x == 7822) return;"),
        ("code", " * see 7822's note", " * see 7822's note"),
    ]
    bad = 0
    for kind, line, want in cases:
        got, _ = migrate_text(line, known, kind == "prose")
        ok = got == want
        bad += not ok
        print("  %-5s %-5s %s" % ("ok" if ok else "FAIL", kind, got.strip()[:78]))
    rewrites = sum(1 for k, l, w in cases if l != w)
    print("\n  %d case(s), %d must-rewrite, %d must-LEAVE, %d failure(s)"
          % (len(cases), rewrites, len(cases) - rewrites, bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true", help="write (default: dry run)")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--show", type=int, default=6, help="sample hits per file")
    a = ap.parse_args()

    if a.self_test:
        return self_test()

    if not os.path.exists(FINDINGS):
        sys.exit("fprefix.py: %s not found -- run from the repository root." % FINDINGS)
    known = known_numbers()
    if not known:
        sys.exit("fprefix.py: NO finding headings parsed, so the known set is "
                 "empty and nothing would be rewritten -- a clean zero that "
                 "means the parser is broken, not that the tree is migrated.")
    print("fprefix: %d known finding number(s)%s\n"
          % (len(known), "" if a.apply else "   [DRY RUN]"))

    files = targets()
    total = touched = 0
    for f in sorted(files):
        prose = f.endswith(PROSE_EXT)
        text = open(f, encoding="utf-8", errors="surrogateescape").read()
        new, hits = migrate_text(text, known, prose)
        if not hits:
            continue
        touched += 1
        total += len(hits)
        print("  %-5s %-52s %4d" % ("prose" if prose else "CODE ", f, len(hits)))
        for before, after in hits[:a.show]:
            print("        %-44s -> %s" % (before[:44], after[:44]))
        if len(hits) > a.show:
            print("        ... %d more" % (len(hits) - a.show))
        if a.apply:
            open(f, "w", encoding="utf-8", errors="surrogateescape").write(new)

    print("\n  %d citation(s) over %d file(s)." % (total, touched))
    if not a.apply:
        print("  DRY RUN -- re-run with --apply.")
    print("\n  AFTERWARDS: rebuild build/tc_out and diff the OBJECTS against\n"
          "  the pre-migration ones.  This touches comments and prose only, so\n"
          "  a single byte that moves means a rewrite escaped into code.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
