#!/usr/bin/env python3
"""
Hold the tree to its own cross-references.

WHY THIS EXISTS

Every claim in this reconstruction is argued somewhere and cited everywhere
else: `finding 171`, `see D48`.  Nothing checked those numbers.  `offcheck.py`
holds the compiler to the `/* +0xNNN */` annotations and the differential
tests hold the code to the blob, but a reference into `docs/findings.md` is
prose pointing at prose, and prose is renumbered by hand.

A merge is when that happens.  Merging two sessions that each appended
findings means renumbering one side, and the last one renumbered 146-156 and
D35-D38, rewriting nearly every reference correctly -- and missing six.

MISDIRECTION IS WORSE THAN DANGLING.  A dangling reference is loud: the
number is not there.  A missed renumber still RESOLVES, to an entry about
something else, and reads exactly like a correct citation.  All six survivors
of that merge were of the second kind: `finding 155` had meant the
register-relative offsets and now meant cadence's gates, and the sentence
around it still parsed.

So this tool has two modes, and the cheap one is not the important one.

    tools/refcheck.py                    every reference resolves  (--dangling)
    tools/refcheck.py --since REV        ...and still means what it did at REV

`--dangling` runs in `make test`.  `--since` cannot: it needs a revision to
compare against, and the revision that matters is a merge parent.  Run it
against each parent after every merge:

    git log --merges -1 --format=%P | tr ' ' '\\n' | \\
        xargs -I{} tools/refcheck.py --since {}

WHAT COUNTS AS A REFERENCE

`finding N`, `findings N, M and K`, and a bare `DN`.  Numbers may carry a
letter suffix -- 116a, 116b, 121k are all real entries.

LINES ARE JOINED BEFORE MATCHING, which is the whole trick.  References wrap:

        * be swept from one variable.  That is the fixture defect of findings 116b,
        * 123 and 171, and it turned up three times ...

A line-based scan sees `116b` and silently drops `123 and 171` -- and reports
clean, which is the one output a checker must never give wrongly.  Comment
leaders are stripped by language before joining, so the `*` above does not
end up inside the list.

WHAT IT DOES NOT CATCH

A reference by bare number with no keyword.  "finding 162, which corrects
158" cites 158 and this sees only 162, and the 120a-121k narrative refers to
its own sub-findings that way throughout -- "the four fixes from 121c", "as
121g predicted".  About twenty of those.

The keyword stays required because the alternative does not survive contact
with the tree.  A bare `\\d+[a-z]` matches `1u`, `0f`, `02x`, `400s` and every
printf width in the test suite -- 111 hits on `0x` alone -- and this runs in
`make test`, where a false positive is worse than a miss.  Even restricted to
three digits and a letter it takes `837k` out of `P(k) = -21k^2 + 837k - 354`
in v34rx.c.  So: write "finding 158" and it is covered; write "158" and it is
not.

THE `--since` WINDOW IS 48 CHARACTERS EITHER SIDE, with numbers blanked.  Two
things that cost, both learned by getting them wrong:

Without the context, `docs/findings.md` reports eleven references that are
perfectly correct.  Both sides of a merge append to that file, so "it cited
152 before and cites 152 now" is true of two unrelated sentences that arrived
from opposite parents -- and the one from the OTHER parent gets judged
against this parent's numbering.

Without blanking the numbers INSIDE the window, one sentence citing two
findings loses both the moment either is renumbered.  "Finding 149's trap,
and finding 152's" became "Finding 173's trap, and finding 152's": the 149
was corrected, and correcting it hid the 152 next to it, which was not.
"""

import argparse
import os
import re
import subprocess
import sys

os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

FINDINGS = "docs/findings.md"
DEVIATIONS = "docs/deviations.md"

#
# Both heading levels are in use in findings.md -- 121 at `##` and 99 at
# `###` -- and matching only one silently halves the target set, which turns
# every reference into the other half into a false dangling report.
#
FINDING_HEAD = re.compile(r"^#{2,4} (\d+[a-z]?)\.\s*(.*)$", re.M)
DEV_HEAD = re.compile(r"^## D(\d+[a-z]?)\b\s*(.*)$", re.M)

FINDING_REF = re.compile(
    r"\bfindings?\s+(\d+[a-z]?(?:\s*(?:,|and)\s*\d+[a-z]?)*)", re.I)
DEV_REF = re.compile(r"\bD(\d+[a-z]?)\b")
NUM = re.compile(r"\d+[a-z]?")

SCAN_EXT = (".c", ".h", ".md", ".py")

#
# Comment leaders, by language.  Stripping `#` from markdown would eat the
# headings this same file is scanned for, and stripping `*` from markdown
# would eat emphasis, so the set is chosen per extension rather than tried
# all at once.
#
LEADERS = {
    ".c": ("*", "//"),
    ".h": ("*", "//"),
    ".py": ("#",),
    ".md": (),
}


def flatten(text, leaders):
    """One long line, plus a map from offset back to the source line.

    Each newline becomes a single space; the indentation and one comment
    leader that follow it are dropped, so a reference broken across two
    lines of a block comment reads as one string.
    """
    out, lines = [], []
    line, i, n = 1, 0, len(text)
    while i < n:
        if text[i] != "\n":
            out.append(text[i])
            lines.append(line)
            i += 1
            continue
        line += 1
        j = i + 1
        while j < n and text[j] in " \t":
            j += 1
        #
        # `*/` closes the comment -- it is not a continuation leader, and
        # eating the `*` would leave a stray `/` glued to the next word.
        #
        if not text.startswith("*/", j):
            for lead in leaders:
                if text.startswith(lead, j):
                    j += len(lead)
                    if j < n and text[j] in " \t":
                        j += 1
                    break
        out.append(" ")
        lines.append(line - 1)
        i = j
    return "".join(out), lines


#
# How much text around a reference identifies it.  A reference has to be
# recognised as THE SAME ONE across two revisions, and its file and number
# are not enough: `docs/findings.md` is appended to by both sides of every
# merge, so "this file cited 152 before and cites 152 now" is true of two
# unrelated sentences that arrived from opposite parents.  Comparing the
# words around it distinguishes them.
#
# Wide enough to be unique, narrow enough to survive an unrelated edit to
# the same paragraph.  An edit that does reach inside the window loses the
# match, which costs a missed report rather than a wrong one.
#
CONTEXT = 48
SPACES = re.compile(r"\s+")
DIGITS = re.compile(r"\d+")


def refs_in(path, text):
    """Every (kind, number, line, context) this file cites."""
    flat, lines = flatten(text, LEADERS.get(os.path.splitext(path)[1], ()))

    #
    # NUMBERS INSIDE THE WINDOW ARE BLANKED, the cited one included -- it is
    # carried in the key beside the context, so nothing is lost.  Without
    # this, one sentence citing two findings loses the match for BOTH the
    # moment the merge renumbers either: "Finding 149's trap, and finding
    # 152's" became "Finding 173's trap, and finding 152's", and the 152 --
    # which is the one that was WRONG -- went unreported because the 173
    # next to it had been fixed correctly.
    #
    def ctx(a, b):
        w = SPACES.sub(" ", flat[max(0, a - CONTEXT):b + CONTEXT]).strip()
        return DIGITS.sub("#", w)

    found = []
    for m in FINDING_REF.finditer(flat):
        for nm in NUM.finditer(m.group(1)):
            at = m.start(1) + nm.start()
            found.append(("finding", nm.group(0), lines[at],
                          ctx(m.start(), m.end())))
    for m in DEV_REF.finditer(flat):
        found.append(("D", m.group(1), lines[m.start()],
                      ctx(m.start(), m.end())))
    return found


def titles(findings_text, deviations_text):
    t = {("finding", n): ttl.strip()
         for n, ttl in FINDING_HEAD.findall(findings_text)}
    t.update({("D", n): ttl.strip()
              for n, ttl in DEV_HEAD.findall(deviations_text)})
    return t


def tracked():
    out = subprocess.run(["git", "ls-files"], capture_output=True, text=True)
    return [p for p in out.stdout.split("\n")
            if p.endswith(SCAN_EXT) and os.path.exists(p)]


def at_rev(rev, path):
    r = subprocess.run(["git", "show", "%s:%s" % (rev, path)],
                       capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None


def read(path):
    return open(path, encoding="utf-8").read()


def check_duplicates():
    """Two entries sharing a number, which is what a merge produces.

    THE COLLISION IS THE CAUSE AND DANGLING IS ONLY THE SYMPTOM.  Two sessions
    branch from the same tip, both allocate from `max + 1`, and both are right
    when they do it; the merge then puts two `### 192.` in one file and every
    citation of 192 becomes ambiguous.  It has happened three times here --
    146-156, 146-151, and 192-194.

    Nothing caught any of them, and the reason is one line: `titles()` builds a
    dict, so the second entry silently replaces the first and every reference
    still resolves.  The tree reads as consistent while two different findings
    answer to one number.
    """
    dupes = []
    for path, head, label in ((FINDINGS, FINDING_HEAD, "finding"),
                              (DEVIATIONS, DEV_HEAD, "D")):
        seen, high = {}, 0
        for num, title in head.findall(read(path)):
            n = int(re.match(r"\d+", num).group(0))
            #
            # NUMBERED LISTS INSIDE A FINDING USE THE SAME MARKUP.  "### 1.
            # What six LSB actually costs" sits inside finding 20-odd and is
            # not finding 1; the heading level does not distinguish them,
            # because real entries use both ## and ###.  What does is that
            # the entries climb and a list restarts: anything far below the
            # running high-water mark is a list item.  A genuine collision is
            # a REPEAT OF A RECENT NUMBER -- the merge that caused all three
            # of them appended 192, 193, 194 after 192, 193, 194 -- so it
            # lands inside the window and a list at 1..9 does not.
            #
            if n < high - 20:
                continue
            high = max(high, n)
            seen.setdefault(num, []).append(title.strip())
        dupes += [(label, n, t) for n, t in sorted(seen.items()) if len(t) > 1]
    for label, num, ts in dupes:
        print("  DUPLICATE %s %s claimed by %d entries:" % (label, num, len(ts)))
        for t in ts:
            print("      %s" % t[:70])
    if dupes:
        print("\n  A merge allocated the same number twice.  Renumber the "
              "later side:\n      tools/refcheck.py --renumber %s%s NEW\n"
              "  which moves the heading and every citation together."
              % ("D" if dupes[0][0] == "D" else "", dupes[0][1]))
    return len(dupes)


def renumber(old, new):
    """Move one entry and every citation of it, in one pass.

    Doing this by hand is what the six missed references in finding 196 were.
    The heading and the citations have to move together or the tree is left in
    the state that reads correct and is not.
    """
    kind = "D" if old.startswith("D") else "finding"
    o = old[1:] if kind == "D" else old
    n = new[1:] if new.startswith("D") else new
    path, head = ((DEVIATIONS, DEV_HEAD) if kind == "D"
                  else (FINDINGS, FINDING_HEAD))

    have = {num for num, _ in head.findall(read(path))}
    if o not in have:
        sys.exit("no such entry: %s" % old)
    if n in have:
        sys.exit("%s%s already exists -- pick a free number (next is %s)"
                 % ("D" if kind == "D" else "", n,
                    max(int(re.match(r"\d+", x).group(0)) for x in have) + 1))

    if kind == "D":
        hpat = re.compile(r"(?m)^(## )D%s\b" % re.escape(o))
        rpat = re.compile(r"\bD%s\b" % re.escape(o))
        rrep = "D" + n
    else:
        hpat = re.compile(r"(?m)^(#{2,4} )%s\." % re.escape(o))
        rpat = re.compile(r"([Ff]indings?\s+(?:\d+[a-z]?(?:\s*(?:,|and)\s*)?)*?)"
                          r"\b%s\b" % re.escape(o))
        rrep = None

    touched = 0
    for f in tracked():
        text = orig = read(f)
        if f == path:
            text = hpat.sub(lambda m: m.group(1) + (rrep if kind == "D"
                                                    else n + "."), text, count=1)
        if kind == "D":
            text = rpat.sub(rrep, text)
        else:
            text = rpat.sub(lambda m: m.group(1) + n, text)
        if text != orig:
            open(f, "w", encoding="utf-8").write(text)
            touched += 1
    print("  %s -> %s across %d file(s).  Re-run to confirm, and check "
          "`--since` against the merge parent." % (old, new, touched))
    return 0


def check_dangling():
    known = titles(read(FINDINGS), read(DEVIATIONS))
    bad = []
    total = 0
    for path in tracked():
        for kind, num, line, _ in refs_in(path, read(path)):
            total += 1
            if (kind, num) not in known:
                bad.append((path, line, kind, num))
    for path, line, kind, num in bad:
        print("  DANGLING  %s:%d  %s"
              % (path, line, num if kind == "D" else "finding " + num))
    print("\n  %d references checked, %d resolve to nothing" % (total, len(bad)))
    return 1 if bad else 0


def check_since(rev):
    """References that stayed put while what they point at moved.

    A reference that was correctly renumbered no longer appears at REV with
    that number in that context, so it is not flagged.  One that was missed
    sits in the same sentence it did at REV, still citing the same number,
    and that number now has a different title.

    The context is what makes this work on `docs/findings.md`, which both
    sides of a merge append to: without it, a sentence that arrived from
    the OTHER parent gets checked against this parent's numbering and is
    reported wrongly.  Eleven of seventeen first reports were that.
    """
    now = titles(read(FINDINGS), read(DEVIATIONS))
    old_f, old_d = at_rev(rev, FINDINGS), at_rev(rev, DEVIATIONS)
    if old_f is None or old_d is None:
        sys.exit("cannot read the docs at %s" % rev)
    then = titles(old_f, old_d)

    bad, total = [], 0
    for path in tracked():
        here = refs_in(path, read(path))
        if not here:
            continue
        before = at_rev(rev, path)
        was = set() if before is None else {
            (k, n, c) for k, n, _, c in refs_in(path, before)}
        for kind, num, line, context in here:
            total += 1
            if (kind, num, context) not in was:
                continue                      # new, moved, or renumbered
            a, b = then.get((kind, num)), now.get((kind, num))
            if a is not None and b is not None and a != b:
                bad.append((path, line, kind, num, a, b))

    for path, line, kind, num, a, b in bad:
        print("  MISDIRECTED  %s:%d  %s"
              % (path, line, num if kind == "D" else "finding " + num))
        print("      at %s:  %s" % (rev[:9], a[:66]))
        print("      now:        %s" % b[:66])
    print("\n  %d references checked against %s, %d now point elsewhere"
          % (total, rev[:9], len(bad)))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(
        description="Check that every `finding N` and `DN` in the tree "
                    "resolves, and still means what it did.")
    ap.add_argument("--dangling", action="store_true",
                    help="every reference resolves to an entry (the default)")
    ap.add_argument("--renumber", nargs=2, metavar=("OLD", "NEW"),
                    help="move an entry and every citation of it together, "
                         "e.g. --renumber 192 195, or --renumber D44 D48")
    ap.add_argument("--since", metavar="REV",
                    help="also: no reference kept its number while its "
                         "target changed title.  Use a merge parent.")
    args = ap.parse_args()

    if args.renumber:
        return renumber(*args.renumber)
    if args.since:
        return check_since(args.since)
    #
    # Duplicates first: a collision makes every citation of that number
    # ambiguous, so reporting dangling references beside it would be noise
    # about a tree whose numbering does not mean anything yet.
    #
    return check_duplicates() or check_dangling()


if __name__ == "__main__":
    sys.exit(main())
