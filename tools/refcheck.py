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

import json
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

#
# Itanium ABI constructor and destructor variant tags, which collide with
# deviation numbering.  `C1`/`C2` are the complete- and base-object
# constructors and `D0`/`D1`/`D2` the deleting, complete and base destructors,
# so a sentence about a virtual destructor contains all three of the strings
# `D0`, `D1` and `D2` -- and the latter two are also real deviations
# (FPM_sqrt's short table and its Q15 clamp).
#
# So the damage was not only the `D0` that dangled loudly.  `D1` and `D2`
# RESOLVED, silently, to two unrelated deviations: exactly the failure this
# checker cannot otherwise catch, and the reason CLAUDE.md says a reference
# that still resolves is the dangerous kind.
#
# Two shapes are masked.  A run of two or more variant tags joined by `/` or
# `,` is never prose about a deviation.  A single backticked tag is the way
# this tree writes one in running text; real deviation references are written
# bare (D4, D17, D31) and there is no backticked one anywhere else, so the
# backticks are a reliable discriminator rather than a guess.
#
ABI_VARIANT = re.compile(
    r"`[CD][012]`"
    r"|`?\b[CD][012]\b`?(?:\s*[/,]\s*`?\b[CD][012]\b`?)+")
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
    masked = [m.span() for m in ABI_VARIANT.finditer(flat)]
    for m in DEV_REF.finditer(flat):
        if any(a <= m.start() < b for a, b in masked):
            continue
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
    #
    # DEDUPED, because `git ls-files` lists an UNMERGED path once per stage.
    # During a merge conflict -- which is the one situation this tool exists
    # for -- a conflicted `docs/findings.md` comes back three times and every
    # reference in it is counted three times with it.  Measured: 790 became
    # 1437, which is 790 + 2 x 324, and two commit messages carry the
    # inflated figure.  The verdict was right both times; the number was not,
    # and a checker that miscounts in the case it was built for is one nobody
    # should have to second-guess.
    #
    out = subprocess.run(["git", "ls-files"], capture_output=True, text=True)
    seen, paths = set(), []
    for p in out.stdout.split("\n"):
        if p in seen or not p.endswith(SCAN_EXT) or not os.path.exists(p):
            continue
        seen.add(p)
        paths.append(p)
    return paths


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


CONFLICT = re.compile(r"(?m)^(<{7} |={7}$|>{7} )")


def renumber(old, new, nth=None):
    """Move one entry and every citation of it, in one pass.

    Doing this by hand is what the six missed references in finding 196 were.
    The heading and the citations have to move together or the tree is left in
    the state that reads correct and is not.

    IT REFUSES WHEN THE NUMBER IS CLAIMED TWICE, which is the case you reach
    it from -- a merge collision.  It used to accept that and produce
    something worse than the collision: with two `### 195.` headings it moved
    the FIRST, which is the established entry rather than the new one, and
    rewrote EVERY citation of 195 including the ones belonging to the entry
    that kept the number.  Exit status 0.  Measured, not supposed.

    The ambiguity is real and not the tool's to guess: with two entries
    answering to one number, a bare `finding 195` in some third file names
    both.  What it can do is move ONE of them by position -- `nth`, where -1
    is the last, which is where a merge appends -- and rewrite only the
    citations INSIDE that entry's own section, then list every other citation
    of the number so the author can place them by hand.
    """
    kind = "D" if old.startswith("D") else "finding"
    o = old[1:] if kind == "D" else old
    n = new[1:] if new.startswith("D") else new
    path, head = ((DEVIATIONS, DEV_HEAD) if kind == "D"
                  else (FINDINGS, FINDING_HEAD))

    for f in (FINDINGS, DEVIATIONS):
        if CONFLICT.search(read(f)):
            sys.exit("%s still has conflict markers -- resolve the merge "
                     "first, taking BOTH sides, then renumber." % f)

    nums = [num for num, _ in head.findall(read(path))]
    have = set(nums)
    if o not in have:
        sys.exit("no such entry: %s" % old)
    if nums.count(o) > 1 and nth is None:
        titles_ = [t for num, t in head.findall(read(path)) if num == o]
        sys.exit(
            "%s%s is claimed by %d entries:\n%s\n"
            "Citations of it are ambiguous -- a bare `%s %s` names both -- so "
            "moving\nthem all is wrong however it is done.  Say which entry "
            "with --nth:\n"
            "      tools/refcheck.py --renumber %s %s --nth -1\n"
            "which moves the LAST (where a merge appends) and rewrites only "
            "the\ncitations inside that entry's own section, then lists the "
            "rest."
            % ("D" if kind == "D" else "", o, nums.count(o),
               "\n".join("      %s" % t[:70] for t in titles_),
               kind, o, old, new))
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

    def move_refs(text):
        return (rpat.sub(rrep, text) if kind == "D"
                else rpat.sub(lambda m: m.group(1) + n, text))

    #
    # THE DISAMBIGUATED PATH.  One heading, chosen by position, and only the
    # citations inside its own section -- everything else that names the
    # number belongs to the entry that is staying, or cannot be told apart
    # from it, and is listed rather than touched.
    #
    if nth is not None:
        doc = read(path)
        spans = [m.start() for m in head.finditer(doc)]
        mine = [i for i, m in enumerate(head.finditer(doc))
                if m.group(1) == o]
        try:
            at = mine[nth]
        except IndexError:
            sys.exit("--nth %d: %s%s has only %d entries"
                     % (nth, "D" if kind == "D" else "", o, len(mine)))
        start = spans[at]
        end = spans[at + 1] if at + 1 < len(spans) else len(doc)

        body = doc[start:end]
        body = hpat.sub(lambda m: m.group(1) + (rrep if kind == "D"
                                                else n + "."), body, count=1)
        body = move_refs(body)
        open(path, "w", encoding="utf-8").write(doc[:start] + body + doc[end:])

        left = []
        for f in tracked():
            for k2, num, line, _ in refs_in(f, read(f)):
                if k2 == kind and num == o:
                    left.append((f, line))
        print("  %s -> %s: the heading and %d citation(s) inside its own "
              "section." % (old, new, len(list(rpat.finditer(doc[start:end])))))
        if left:
            print("\n  Still naming %s -- these belong to the entry that kept "
                  "the number,\n  or cannot be told from it.  Place them by "
                  "hand:" % old)
            for f, line in left:
                print("      %s:%d" % (f, line))
        return 0

    touched = 0
    for f in tracked():
        text = orig = read(f)
        if f == path:
            text = hpat.sub(lambda m: m.group(1) + (rrep if kind == "D"
                                                    else n + "."), text, count=1)
        text = move_refs(text)
        if text != orig:
            open(f, "w", encoding="utf-8").write(text)
            touched += 1
    print("  %s -> %s across %d file(s).  Re-run to confirm, and check "
          "`--since` against the merge parent." % (old, new, touched))
    return 0


#
# A CONFLICT MARKER IS NOT A REFERENCE PROBLEM, AND IT BELONGS HERE ANYWAY.
#
# `docs/findings.md` reached `origin` with `<<<<<<< HEAD` and `>>>>>>> w1e_dil`
# in it: a merge resolved by script, staged, and committed.  Every gate passed.
# `make phase` compiles and runs, and neither it nor this tool reads prose, so
# a document with markers in it is a document that builds.  The record is the
# deliverable here, so a marker in it is as much a defect as a failing test --
# and this is the one gate that already walks every tracked file.
#
CONFLICT_MARK = re.compile(r"^(?:<{7}|={7}|>{7})(?:\s|$)", re.M)


def check_conflict_markers():
    bad = []
    for path in tracked():
        if not path.endswith(SCAN_EXT):
            continue
        for m in CONFLICT_MARK.finditer(read(path) or ""):
            line = (read(path) or "").count("\n", 0, m.start()) + 1
            bad.append((path, line, m.group(0).strip()))
    for path, line, mark in bad:
        print("  CONFLICT  %s:%d  %s" % (path, line, mark))
    return bad


#
# THE MUTATION REGISTRY IS JSON AND NOTHING PARSED IT.
#
# `test/mutations/suites.json` is the list every `mutate.py --suite` reads.
# Merging two branches that each appended a line to it produces a trailing
# comma about one time in three, and a malformed registry makes EVERY suite
# unrunnable -- while `make phase` stays green, because nothing in the phase
# boundary opens the file.  A test suite that cannot be run reports no
# failures, which is finding 134's argument in its purest form.
#
# Checked here because this is the gate that already walks the tree, and
# because the same merge that breaks it is the one that breaks references.
# Finding 346.
#
def check_suites():
    path = os.path.join("test", "mutations", "suites.json")
    if not os.path.exists(path):
        return []
    try:
        reg = json.load(open(path))
    except ValueError as e:
        print("  MALFORMED  %s: %s" % (path, e))
        return [path]
    bad = []
    for name, entry in sorted(reg.items()):
        # `_` holds the file's own prose header, which is a list of strings
        # and not a suite.  Any leading-underscore key is documentation.
        if name.startswith("_"):
            continue
        if (not isinstance(entry, list) or len(entry) != 2
                or not all(isinstance(x, str) for x in entry)):
            print("  MALFORMED  %s: suite %r is not [source, binary]"
                  % (path, name))
            bad.append(name)
            continue
        src = entry[0]
        if not os.path.exists(src):
            print("  MISSING    %s: suite %r names %s, which does not exist"
                  % (path, name, src))
            bad.append(name)
    return bad


#
# A MUTATION RUN THAT DIES LEAVES ITS MUTANT IN THE SOURCE.
#
# `mutate.py` edits the file, builds, runs, and puts the file back.  Kill it
# between the first and the last -- a timeout, a disconnect, a Ctrl-C -- and
# the mutant stays.  `git add -A` then commits it, and if the commit's
# `make phase` ran BEFORE the mutation run rather than after, nothing objects.
#
# That happened: merge commit 92e565e captured `47 tests the counter before
# incrementing it` into src/pump/v34/v34hshak_t3mid.c.  The differential test
# does catch it -- 84 of 31,328 checks -- so the tree was one `make phase`
# away from noticing, and the ordering of two commands was the whole defect.
#
# Detection is exact rather than heuristic: for a correctly restored source
# every mutation's `find` string is present.  If `find` is ABSENT and
# `replace` is PRESENT, that mutation is live in the tree.  Finding 349.
#
def check_live_mutants():
    reg_path = os.path.join("test", "mutations", "suites.json")
    if not os.path.exists(reg_path):
        return []
    try:
        reg = json.load(open(reg_path))
    except ValueError:
        return []                       # check_suites() reports this
    live = []
    for name, entry in sorted(reg.items()):
        if name.startswith("_") or not isinstance(entry, list) or len(entry) != 2:
            continue
        path = os.path.join("test", "mutations", name + ".json")
        if not os.path.exists(path) or not os.path.exists(entry[0]):
            continue
        try:
            muts = json.load(open(path))
        except ValueError:
            print("  MALFORMED  %s" % path)
            live.append(path)
            continue
        text = open(entry[0]).read()
        for m in muts:
            if not isinstance(m, dict) or "find" not in m or "replace" not in m:
                continue
            if m["find"] not in text and m["replace"] in text:
                print("  LIVE MUTANT  %s: %s"
                      % (entry[0], m.get("label", "(unlabelled)")))
                live.append(entry[0])
    return live


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
    marks = check_conflict_markers()
    suites = check_suites()
    live = check_live_mutants()
    print("\n  %d references checked, %d resolve to nothing%s%s%s"
          % (total, len(bad),
             "" if not marks else ", %d conflict marker(s)" % len(marks),
             "" if not suites else ", %d bad mutation suite(s)" % len(suites),
             "" if not live else ", %d LIVE MUTANT(S)" % len(live)))
    return 1 if (bad or marks or suites or live) else 0


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
    ap.add_argument("--nth", type=int, metavar="N",
                    help="with --renumber, and only when the number is "
                         "claimed twice: which entry to move, 0 for the "
                         "first and -1 for the last, which is where a merge "
                         "appends")
    ap.add_argument("--renumber", nargs=2, metavar=("OLD", "NEW"),
                    help="move an entry and every citation of it together, "
                         "e.g. --renumber 192 195, or --renumber D44 D48")
    ap.add_argument("--since", metavar="REV",
                    help="also: no reference kept its number while its "
                         "target changed title.  Use a merge parent.")
    args = ap.parse_args()

    if args.renumber:
        return renumber(*args.renumber, nth=args.nth)
    if args.nth is not None:
        sys.exit("--nth is only meaningful with --renumber")
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
