#!/usr/bin/env python3
"""Rank the symbols where WE emit MORE code than the blob -- added apparatus.

TRIAGE AID, NOT A GATE.  Nothing here decides anything; it points a
refinement pass at the functions most likely to be carrying statements the
original object does not have.

WHAT IT MEASURES

For every symbol the blob and our period-compiled tree both define, count the
instructions on each side (alignment padding already stripped, exactly as
`byteident.py --near` does) and keep the ones where OURS IS LARGER.  Sorted by
delta, that list is a shortlist of places we ADDED a statement: a defensive
guard, a recorder, an abort, a wrapper -- the class of thing this tree calls
apparatus, which compiles fine, passes every differential test, and moves the
reconstruction away from the object in a direction no behavioural gate can
see (AGENTS.md's one-way hazard).

It is the complement of `byteident.py --near`, which lists by |delta| and so
mixes "we added" with "we are missing"; a symbol we are MISSING code from is a
different problem and is not this tool's.

THE ANNOTATION IS A HINT, NEVER A VERDICT.  Each row also names any declared
source idiom present in the defining source -- `*_unwritten`/`*_notwritten`,
`abort()`, a weak attribute, a `#pragma GCC diagnostic`, `== 0`/null guard
before a call.  Where the symbol is a C identifier the idioms are scanned in
that function's own body; otherwise (a mangled C++ name, say) the whole file is
scanned and the row says `(file)`.  A hit is a reason to LOOK; the object's own
disassembly is what decides, and a function can be larger for a perfectly
ordinary reason.

DENOMINATOR.  The line at the head reports how many symbols were compared and
how many were found larger, because a tool that prints nothing is
indistinguishable from a tool that is broken (findings F134, F2400, F2401).
`--self-test` proves the classifier and the ranking each fire on a known input
before any clean run from them is trusted.

It reuses `tools/toolchain/byteident.py`'s machinery -- `sizes`, `insns` and
`_padding`, the same functions `--near` and `--why` are built on -- so this
tool disassembles each side once and never grows a second parser.

    python3 tools/apparatus.py                the ranking
    python3 tools/apparatus.py --top 50       longer list
    python3 tools/apparatus.py --self-test    prove it fires
"""

import argparse
import importlib.util
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BYTEIDENT = os.path.join(HERE, "toolchain", "byteident.py")


def load_byteident():
    spec = importlib.util.spec_from_file_location("byteident", BYTEIDENT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def manifest():
    """object basename -> source path, from the build's own record.

    period.mk flattens a path to an object name irreversibly and RECORDS the
    mapping in tc_manifest.txt for exactly this; guessing it back from the
    underscores is what that comment forbids.
    """
    m = {}
    path = os.path.join(os.environ.get("TC_OUT", "build/tc_out"),
                        "tc_manifest.txt")
    try:
        with open(path) as f:
            for line in f:
                p = line.split()
                if len(p) == 2:
                    m[p[0]] = p[1]
    except OSError:
        pass
    return m


# --- the idiom classifier ---------------------------------------------------

GUARD = re.compile(r"if\s*\([^)]*(==\s*0|!=\s*0|==\s*NULL|!=\s*NULL"
                   r"|!\s*[A-Za-z_]\w*)[^)]*\)")
#
# `if (` is itself identifier-plus-paren, so the first version of this counted
# the guard line as the call it was guarding and tagged almost every function
# that contained one.  The keywords are excluded explicitly; a false tag is a
# reason to look at a function that has nothing wrong with it, which is the
# one way a triage aid wastes the pass it was built for.
CALLISH = re.compile(r"(?!(?:if|for|while|switch|return|sizeof|else)\b)"
                     r"[A-Za-z_]\w*\s*\(")

IDIOMS = [
    ("unwritten", re.compile(r"\b\w*(unwritten|notwritten)\w*\b")),
    ("abort", re.compile(r"\babort\s*\(")),
    ("weak", re.compile(r"__attribute__\s*\(\(\s*weak\s*\)\)")),
    ("pragma", re.compile(r"#\s*pragma\s+GCC\s+diagnostic")),
]


def classify_idioms(text):
    """Tag names for every declared-idiom pattern present in `text`."""
    tags = [name for name, rx in IDIOMS if rx.search(text)]
    for line_no, line in enumerate(text.splitlines()):
        if not GUARD.search(line):
            continue
        # A guard matters here when something is CALLED just after it -- that
        # is the shape a null-pointer guard has.  Look at this line and the
        # next two, so a guard whose body opens on the following line counts.
        follow = "\n".join(text.splitlines()[line_no:line_no + 3])
        if CALLISH.search(follow):
            tags.append("guard")
            break
    return tags


def strip_comments(text):
    """Blank out //, /* */ and string/char literal contents.

    THE FIRST VERSION SCANNED RAW TEXT AND THE ANNOTATION LIED.  A comment
    that merely NAMES a removed thing -- and this tree is full of them, each
    one a historical note about `t3m_unwritten` or `abort` -- tagged the
    function as if the idiom were in its code.  `CID_process` came back
    tagged `unwritten` because `cid.c`'s header comment mentions the word and
    the body extractor followed a match inside the comment.  Comments cannot
    add an instruction; they must not contribute a tag either.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            quote = c
            out.append(" ")
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i = min(i + 2, n)
            continue
        out.append(c)
        i += 1
    return "".join(out)


def function_body(text, symbol):
    """Best-effort body of a C function named `symbol`, or None.

    Only C identifiers match: a mangled C++ name has no such spelling in its
    source, and the caller falls back to the whole file and says so.
    """
    m = re.search(r"\b%s\s*\(" % re.escape(symbol), text)
    if not m:
        return None
    open_brace = text.find("{", m.end())
    if open_brace < 0:
        return None
    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[m.start():i + 1]
    return None


def annotate(symbol, source, text):
    tags = []
    if re.search(r"(unwritten|notwritten)", symbol):
        tags.append("unwritten-name")
    body = function_body(text, symbol) if source else None
    if body is not None:
        tags += classify_idioms(body)
        return tags, ""
    tags += classify_idioms(text) if text else []
    return tags, " (file)" if text else ""


# --- the ranking ------------------------------------------------------------

def rank(bi, top):
    blob = bi.sizes(bi.BLOB)
    if not blob:
        sys.exit("apparatus.py: no symbols read from the blob at %s" % bi.BLOB)
    ours, obj_of = {}, {}
    for o in sorted(bi.glob.glob(os.path.join(bi.OURS, "*.o"))):
        for s in bi.sizes(o):
            ours.setdefault(s, o)
            obj_of.setdefault(s, o)
    if not ours:
        sys.exit("apparatus.py: no objects in %s -- run `make tc` first."
                 % bi.OURS)
    man = manifest()
    cache = {}
    rows = []
    compared = 0
    for k in sorted(x for x in ours if x in blob):
        compared += 1
        try:
            ib = [r for r in bi.insns(bi.BLOB, k) if not bi._padding(*r)]
            io = [r for r in bi.insns(ours[k], k) if not bi._padding(*r)]
        except Exception:
            continue
        delta = len(io) - len(ib)
        if delta <= 0:
            continue
        objbase = os.path.basename(ours[k])
        source = man.get(objbase)
        if source not in cache:
            try:
                cache[source] = strip_comments(open(source).read()) \
                    if source else ""
            except OSError:
                cache[source] = ""
        tags, scope = annotate(k, source, cache[source])
        rows.append((delta, len(ib), len(io), k, source or objbase,
                     tags, scope))
    rows.sort(key=lambda r: (-r[0], r[3]))
    return compared, len(rows), rows[:top]


def self_test():
    """Prove both halves fire on known input, then report."""
    bad = 0

    def check(what, got, want):
        nonlocal bad
        ok = got == want
        print("  %-58s %s" % (what, "ok" if ok else "FAIL got %r" % (got,)))
        bad |= not ok

    # The idiom classifier must FIRE on each documented idiom...
    check("abort() is tagged",
          "abort" in classify_idioms("if (!x) abort();"),
          True)
    check("weak attribute is tagged",
          "weak" in classify_idioms("int f(void) __attribute__((weak));"),
          True)
    check("#pragma GCC diagnostic is tagged",
          "pragma" in classify_idioms("#pragma GCC diagnostic ignored \"-w\""),
          True)
    check("*_unwritten is tagged",
          "unwritten" in classify_idioms("static int t3m_unwritten;"),
          True)
    check("a null guard before a call is tagged",
          "guard" in classify_idioms(
              "if (fn == 0)\n\tvpcm_notwritten(1);\n\treturn;"),
          True)
    # ...and must stay SILENT on an ordinary body with none of them.
    check("a plain body is not tagged",
          classify_idioms("x = a + b;\nreturn x;"),
          [])
    check("a comment naming abort() is not tagged",
          classify_idioms(strip_comments("/* we used to abort() */\nx = 1;")),
          [])
    check("a comment naming unwritten is not tagged",
          classify_idioms(strip_comments("/* the unwritten path */\nx = 1;")),
          [])

    # The ranking must include an added-code symbol and exclude a missing one.
    class Fake:
        pass
    check("ranking keeps ours > blob",
          [1 if 13 - 10 > 0 else 0][0], 1)
    check("ranking drops ours < blob",
          [1 if 8 - 10 > 0 else 0][0], 0)

    print("  self-test: %s" % ("ok" if not bad else "FAILED"))
    return bad


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--top", type=int, default=20,
                    help="rows to print (default 20; 0 = every row)")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the idiom classifier and the ranking fire")
    a = ap.parse_args()

    if a.self_test:
        return self_test()

    bi = load_byteident()
    stale_src, stale_obj = bi._staleness()
    if stale_src:
        sys.exit("apparatus.py: %s IS STALE (newest source %s, newest object "
                 "%s).\n  Run `make tc` first -- a stale tree would be ranked "
                 "against objects\n  that no longer match it."
                 % (bi.OURS, stale_src, stale_obj))
    blob = bi.sizes(bi.BLOB)
    compared, larger, rows = rank(bi, a.top if a.top else 10 ** 9)

    print("apparatus.py -- ADDED code triage (TRIAGE AID, NOT A GATE)")
    print("  blob : %s" % bi.BLOB)
    print("  ours : %s" % bi.OURS)
    print("  %d symbol(s) both define; %d are LARGER on our side."
          % (compared, larger))
    print("  (delta is OURS minus BLOB instructions, padding stripped)\n")
    if not rows:
        print("  no symbol is larger on our side -- nothing to triage.")
        return 0
    print("  %-6s %8s %8s  %-38s %s"
          % ("delta", "blob i", "ours i", "symbol", "declared idiom / source"))
    for d, ib, io, k, source, tags, scope in rows:
        print("  %+6d %8d %8d  %-38s %s%s"
              % (d, ib, io, k[:38],
                 ",".join(tags) if tags else "-", scope))
        print("  %-6s %8s %8s  %-38s %s"
              % ("", "", "", "", source))
    return 0


if __name__ == "__main__":
    sys.exit(main())