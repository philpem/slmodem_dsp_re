#!/usr/bin/env python3
"""
How much of dsplibs.o has been reconstructed, and how much of that is tested.

WHY THIS EXISTS

"Phase 5 is done" is not a measurement.  Two numbers are:

  translated   the share of the blob's .text that a function of the same name
               now exists for in this tree
  tested       the share of THAT which some test actually drives against the
               blob, by calling its `ref_` alias

The second matters more than the first.  A reconstructed function that
nothing compares against the original is a guess with good indentation, and
the gap between the two numbers is the honest measure of how much of the work
rests on reading alone.

HOW IT DECIDES

Not by grepping source for names, which would count a prototype, a comment or
a call as a definition.  By reading symbol tables:

  ours     every T symbol our own build defines, from build/src/**/*.o --
           or build/repro/**/*.o, whichever a build has actually filled
  blob     every T and t symbol dsplibs.o defines, with its size
  tested   every `ref_NAME` a compiled differential-test object actually
           REFERENCES, from `nm -u` on the complete expected test-driver
           object set under build/test/

That last distinction matters and was got wrong once.  Every test file opens
with a block of `extern ref_*` declarations, so grepping the sources counts a
symbol as tested the moment it is declared -- including one whose calls were
deleted in some earlier revision.  An undefined symbol in the object file
means the compiler emitted a reference to it, which only a call or an address
can do.  The test-object set is checked before it is read.  A partial tree is
not a low score: it is an incomplete measurement, and this tool refuses it.

The interop programs compile straight to executables with no intermediate
object, so those few sources are still read by name; they are listed
explicitly rather than swept up by a directory walk.

File-local symbols (`t` in nm) are counted when we have reconstructed one of
the same name -- several are, `AnalyseDialString` among them.  They used to be
reported apart as impossible to test differentially, "because objcopy cannot
rename them".  That was wrong: --globalize-symbols promotes them first and the
rename map then applies, which the Makefile now does in two passes.  So a
file-local symbol whose name is unique in the object has a `ref_` alias, can
be called by name, and counts in the `tested` denominator like anything else.
Ten names occur in more than one translation unit and cannot be globalized --
two statics of the same name are two different objects -- and those alone
stay in the reached-through-a-caller bucket.

Which is which is not re-derived from symmap.py's rules: the alias set is read
out of build/dsplibs_ref.o, the object the tests actually link.  A symbol is
drivable if and only if objcopy really made an alias for it.

WHERE `ours` COMES FROM, AND WHY IT IS A WHITELIST

Only build/src and build/repro are walked, in that order, and only ever one
of them: they are the two directories our own compiler writes to from src/,
they hold the same 1457 (name, kind) pairs when both are built, and a plain
`make` has filled only the second since 75dcc19 (#164).  An empty pair of
them is a REFUSAL and not a 0.0%.  See tools/objtree.py, findings F3055/3110.

This used to be all of build/ less `dsplibs_ref.o`
by name -- and then the two-pass rename put a SECOND copy of the blob beside
it, build/dsplibs_glob.o, with all 1782 of its symbols promoted to global.
The walk took that for our own output and the report claimed 98.0% translated
for a tree that has reconstructed 290 symbols.  Adding a second name to the
blacklist would break again the next time the Makefile leaves an intermediate
in build/; naming the one directory our compiler writes to cannot.  See
finding F222.

Usage:
    coverage.py [--obj ref/slmodemd/dsplibs.o] [--build build] [--md FILE]
"""

import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import objtree                                            # noqa: E402

# Symbols we define only IN PART, and which therefore must not be counted.
#
# A symbol's whole `st_size` lands in `translated` the moment a definition of
# that name exists, which is right for a function reconstructed in one go and
# very wrong for one being landed a dispatch case at a time: `v34handshak` is
# 61,541 bytes, 8.4% of .text, and defining a skeleton with four arms in it
# moved this report from 21.2% to 29.6% for a few hundred bytes of work.
#
# So a partial symbol is excluded from BOTH figures and listed on its own.
# An entry here is a promise to remove it: when the last arm lands, the line
# goes and the bytes arrive.  Reported per name rather than per byte because
# nobody can honestly say how many of the 61,541 are written.
#
# THE SET IS EMPTY, AND `v34handshak` IS WHAT CAME OUT OF IT.  It was here for
# five tasks' worth of work -- 61,541 bytes landed a dispatch arm at a time --
# and the criterion for taking it out was never "the last guard has gone", one
# of which is a permanent structural assertion, but "no REACHABLE arm is
# unwritten".  That is now checkable in one grep:
#
#     grep -n 't3m_notwritten(\|t3c_unwritten(' src/pump/v34/V34hshak.c
#
# which reports the two definitions, `t3c_unwritten`'s own call of
# `t3m_notwritten`, and exactly one call site -- table 3's `default:`.  That
# label is unreachable by construction: the fifteen written arms and the
# twenty-four shared ones are forty labels over 41..80, which is every value
# the range test at 0x64ac6 admits, and it stays because the range test and
# the label set are two statements of one fact.  Findings F748 and F750, and
# `docs/v34handshak.md` for the arm-by-arm record.
#
# Keep the machinery: it is how the next function of this size gets taken, and
# it is cheaper to leave an empty dict than to re-derive the exclusion.
PARTIAL = {
}

# Blob symbols confirmed COMPLETE in src/ that `our_symbols()` can never see,
# because the modern host compiler inlines them entirely into their sole
# caller -- the object's own GCC 3.4.2 did not, so the blob still carries a
# genuine standalone local symbol (`nm` shows real `t`, not zero bytes) and
# there is nothing to name for CLAUDE.md's inlining-boundary trap to catch.
# Each entry is a finding, not a guess: a wrong one would silently inflate
# `translated` forever, since nothing else would ever check it again.
INLINED_AWAY = {
    "GetNextDigitAndReturnNextState":
        "F8490 -- written in src/dialer/Dialer.c, tested indirectly through "
        "DialerProgress by t_dialerprog since every modern compiler tried "
        "here inlines it away; the object's own compiler did not, and a "
        "`ref_` alias exists (build/dsplibs_ref.o) for a test to call it "
        "directly if one is ever written.",
}

# Blob symbols that are not this reconstruction's target at all: libm's own
# `pow()`, statically linked into the object as `pow.S` rather than authored
# by dsplibs.o's own developer.  F1990 traced the object's four unordered
# float comparisons to exactly this function and called it "libm and not our
# code" -- there is no reconstruction work here to schedule, ever, and
# leaving these nine names in `blob` makes the denominator claim otherwise.
NOT_OURS = {
    "pow", "one", "zero", "inf_zero", "infinity", "minfinity",
    "mzero", "minf_mzero", "limit",
}

# Symbols we define that the object has no counterpart for, and why that is
# expected rather than drift.  Anything not matching these is reported.
BENIGN = (
    (re.compile(r"^__x86\.get_pc_thunk\."), "compiler thunk"),
    (re.compile(r"_(entry|generate|size)$"), "test accessor"),
    (re.compile(r"^RcFixed_(State|UpFactor|DownFactor)$"), "test accessor"),
    (re.compile(r"^FP_Pow_coefficient$"), "test accessor"),
    # C2/D2 are the BASE-OBJECT constructor and destructor.  For a class with
    # no virtual base they are the same code as C1/D1, and which of the pair a
    # compiler emits is a compiler-version detail, not a property of the
    # source: the 2003 build put only C1/D1 in the object, and every g++ used
    # here emits all four.  Nothing calls them -- there is no virtual base
    # anywhere in the object -- so they are dead weight rather than drift, and
    # listing them every run would bury the strays that matter.  Settled once
    # for the whole weak-symbol batch; see finding F601.
    (re.compile(r"^_Z.*[CD]2E"), "base-object ctor/dtor, no virtual bases"),
)


#
# T, t AND W.
#
# `W` is a weak text symbol -- a template instantiation or an inline the
# compiler emitted out of line -- and the blob has 79 of them, 6,301 bytes,
# living in `.gnu.linkonce.t.*` sections rather than in `.text`.  They were
# invisible to this report on BOTH sides for a long time, so `GenericIIR`'s
# 1,269 bytes counted as neither translated nor remaining even though
# src/dsp/FloatIIR.cpp reconstructs them and t_genericiir.cpp drives them.
#
# They are not a special case for testing: `objcopy` renames them like
# anything else, so `ref__ZN10GenericIIRIfdE5resetEv` exists and they are
# differentially testable.  Counted with the globals for that reason.
#
# NOT `V`, which is a weak OBJECT -- the four vtables.  Data, like D and R,
# and outside what this measures.
#
def nm_symbols(path):
    """{name: (size, kind)} for the T/t/W symbols a file defines.

    Excludes NOT_OURS everywhere this is read, blob or ours alike: libm's
    `pow()` internals are not a name any DSP function in this tree could
    legitimately collide with, so dropping them here rather than only in
    `main()` means every tool built on this function (worklist.py included)
    stops carrying them as apparent remaining work.
    """
    out = subprocess.run(["nm", "-S", "--defined-only", path],
                         capture_output=True, text=True).stdout
    syms = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4 and f[2] in "TtW" and f[3] not in NOT_OURS:
            syms[f[3]] = (int(f[1], 16), f[2])
        elif len(f) == 3 and f[1] in "TtW" and f[2] not in NOT_OURS:
            syms[f[2]] = (0, f[1])
    return syms


def blob_addresses(path):
    """{name: address} for every T/t symbol, to attribute it to a TU."""
    out = subprocess.run(["nm", "--defined-only", path],
                         capture_output=True, text=True).stdout
    addr = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[1] in "Tt":
            addr[f[2]] = int(f[0], 16)
    return addr


def our_symbols(build):
    """Text symbols our own compiler output defines: every global one, plus
    a local (`static`) one where the name is unambiguous.

    build/src, or build/repro when build/src is empty -- the two-directory
    whitelist the docstring argues for, probed rather than assumed since a
    plain `make` stopped filling the first of them.  REFUSES on an empty
    tree: this function returning an empty set made the headline number of
    the whole project read `translated 0.0%, 0 bytes, 0 symbols` at exit 0
    after a plain `make`, which is indistinguishable from a reconstruction
    that has not started.  Findings F3055 and F3110; tools/objtree.py.

    A LOCAL NAME IS COUNTED ONLY WHEN IT DEFINES ONE OBJECT, NOT WHEN IT
    APPEARS ONCE. `static void reset(void)` in two unrelated files is legal
    C and common in this tree, and a name occurring in two of our own
    object files is exactly as ambiguous as one occurring in two of the
    blob's -- `unaliasable()`'s whole argument, applied to our own side
    instead of the blob's. Counting occurrences and keeping only the
    singletons is what makes that safe: `v22_create`/`v22_delete`/
    `v22_process` are `static` in both the blob and this reconstruction (F10190)
    and were invisible to every prior version of this function, which kept
    only `T`/`W` kinds and so could never credit a correctly-`static`
    function -- 929 bytes read as unwritten while fully written and tested.
    Finding F10191.

    INLINED_AWAY's names are added on top, unconditionally: each is a blob
    symbol independently confirmed complete in src/ that no amount of
    scanning our own build's `nm` output can ever find, because the modern
    host compiler removes the standalone symbol entirely. Finding F10192.
    """
    _d, objs = objtree.read("the translated share of the blob", build)
    syms = set()
    local_count = {}
    for path in objs:
        for sym, (_size, kind) in nm_symbols(path).items():
            if kind in ("T", "W"):
                syms.add(sym)
            elif kind == "t":
                local_count[sym] = local_count.get(sym, 0) + 1
    syms.update(name for name, n in local_count.items() if n == 1)
    syms.update(INLINED_AWAY)
    return syms


def unaliasable(obj, aliased):
    """File-local symbols the two-pass rename could NOT give a `ref_` name.

    Derived the same way symmap.py derives its exclusion, and from the same
    object, so the report and the rename map cannot drift: a name used by more
    than one translation unit cannot be globalized, because two statics of the
    same name are two different objects and promoting both would make one
    symbol.  The membership test is against what objcopy actually produced,
    not against a list of names written down here.
    """
    out = subprocess.run(["nm", "--defined-only", obj],
                         capture_output=True, text=True).stdout
    fields = [line.split() for line in out.splitlines()]
    glob = {f[-1] for f in fields if len(f) >= 2 and f[-2] in "TDBRW"}
    local = {f[-1] for f in fields if len(f) >= 2 and f[-2] in "tdbr"}
    return sorted(local - glob - aliased)


def aliased_symbols(build):
    """Names reachable as `ref_NAME` in the object the tests link.

    Read from the artifact, not from symmap.py's rules, so the two cannot
    drift: whatever objcopy managed to alias is what a test can call.
    """
    ref = os.path.join(build, "dsplibs_ref.o")
    if not os.path.exists(ref):
        sys.exit("error: %s does not exist, so nothing can be said about "
                 "which symbols have a ref_ alias.  Build it first:\n"
                 "    make %s" % (ref, ref))
    out = subprocess.run(["nm", "--defined-only", ref],
                         capture_output=True, text=True).stdout
    names = {line.split()[-1] for line in out.splitlines() if line.split()}
    return {n[len("ref_"):] for n in names if n.startswith("ref_")}


# Interop sources, which have no intermediate object to read.
INTEROP_BY_NAME = ("test/interop/v8peer.c",)


# These are the same two classes of objects the differential-test link rule
# uses: every top-level t_*.c/cpp driver, and the explicit shared HARNESS
# sources from the Makefile.
# Do not walk build/test/ and hope its current contents are a complete suite:
# a failed parallel build leaves a plausible-looking prefix there, which made
# the `tested` result read 1.2% instead of 99.9% (F7586).
TEST_DRIVER_DIR = os.path.join("test", "unit")
TEST_HARNESS_SOURCES = (
    "test/harness/harness.c",
    "test/harness/runtime.c",
    "test/harness/fakedp.c",
    "test/harness/v34hsstep.c",
    "test/harness/unwritten.c",
)


def expected_test_objects(build):
    """{source: object} for every object that coverage may read.

    Keep the source selection in step with Makefile's TESTS/CXXTESTS and
    explicit HARNESS lists.  The paths are derived rather than globbed under build/ so
    a left-over object can neither conceal a missing driver nor contribute a
    stale `ref_` reference to the measurement.
    """
    sources = []
    for suffix in (".c", ".cpp"):
        sources.extend(sorted(
            os.path.join(TEST_DRIVER_DIR, name)
            for name in os.listdir(TEST_DRIVER_DIR)
            if name.startswith("t_") and name.endswith(suffix)))
    sources.extend(TEST_HARNESS_SOURCES)

    out = {}
    for source in sources:
        stem, _suffix = os.path.splitext(source)
        obj = os.path.join(build, stem + ".o")
        if obj in out.values():
            sys.exit("coverage.py: test sources map more than once to %s; "
                     "refusing an ambiguous driver set" % obj)
        out[source] = obj
    return out


def require_complete_test_objects(build):
    """Return the complete current test-driver object set or refuse.

    A missing object is enough to make `tested` a false percentage.  A source
    newer than its object is the same problem when coverage.py is run directly
    instead of through make.  Unlike objtree.py's source-object census, this
    is fatal: `tested` cannot truthfully describe a partial suite.
    """
    try:
        expected = expected_test_objects(build)
    except OSError as exc:
        sys.exit("coverage.py: cannot enumerate test drivers: %s" % exc)
    if not expected:
        sys.exit("coverage.py: NO TEST DRIVERS, so `tested` has no "
                 "denominator; refusing to report a percentage")

    missing = [source for source, obj in expected.items()
               if not os.path.isfile(obj)]
    stale = [source for source, obj in expected.items()
             if os.path.isfile(obj)
             and os.path.getmtime(source) > os.path.getmtime(obj)]
    if missing or stale:
        details = []
        if missing:
            details.append("%d missing" % len(missing))
        if stale:
            details.append("%d stale" % len(stale))
        sample = sorted(missing + stale)[:5]
        sys.exit(
            "coverage.py: REFUSING incomplete test-driver object tree: %s "
            "among %d expected object(s) under %s/.\n"
            "  Examples: %s\n"
            "  `make coverage` builds the complete set before measuring. "
            "Finding F7586."
            % (", ".join(details), len(expected),
               os.path.join(build, "test"), ", ".join(sample)))
    return list(expected.values())


def undefined(path):
    out = subprocess.run(["nm", "-u", path], capture_output=True,
                         text=True).stdout
    return {line.split()[-1] for line in out.splitlines() if line.split()}


def tested_symbols(build, extra_sources=INTEROP_BY_NAME):
    """Symbols some test actually references, not merely declares."""
    found = set()
    for path in require_complete_test_objects(build):
        for sym in undefined(path):
            if sym.startswith("ref_"):
                found.add(sym[4:])

    pat = re.compile(r"\bref_([A-Za-z_][A-Za-z0-9_]*)\s*\(")
    for src in extra_sources:
        try:
            with open(src, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in pat.finditer(text):
            found.add(m.group(1))
    return found


#
# `.text` PLUS the linkonce sections, because the weak symbols counted above
# live in those and a numerator without its denominator flatters.  83 sections,
# 6,405 bytes -- slightly more than the 6,301 the symbols account for, the
# difference being alignment padding.
#
def text_size(obj):
    out = subprocess.run(["size", "-A", obj], capture_output=True,
                         text=True).stdout
    total = 0
    for line in out.splitlines():
        f = line.split()
        if len(f) >= 2 and (f[0] == ".text"
                            or f[0].startswith(".gnu.linkonce.t.")):
            total += int(f[1])
    return total


def load_tus(path):
    """[(lo, hi, label)] from the tumap JSON, one entry per distinct span."""
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except OSError:
        return []
    spans = {}
    for name, t in data["tus"].items():
        spans.setdefault((t["lo"], t["hi"]), []).append((t["seq"], name))
    out = []
    for (lo, hi), members in spans.items():
        members.sort()
        label = members[0][1]
        if len(members) > 1:
            label += " +%d" % (len(members) - 1)
        out.append((lo, hi, label))
    out.sort()
    return out


#
# A weak/linkonce symbol has no address in `.text` -- it sits at offset 0 of
# its own `.gnu.linkonce.t.<mangled>` section -- so the TU map, which is built
# from `.text` address spans, cannot place it.  Say that rather than "?", or
# 74 symbols and 5 KB look like a gap in the map instead of a property of how
# the compiler emitted them.
#
def area_of(addr, tus, kind=None):
    if kind == "W":
        return "(weak/linkonce, no .text address to attribute)"
    for lo, hi, label in tus:
        if lo <= addr < hi:
            return label
    return "?"


def bar(frac, width=34):
    n = int(round(frac * width))
    return "[" + "#" * n + "." * (width - n) + "]"


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--obj",
                    default=os.environ.get("BLOB",
                                           "ref/slmodemd/dsplibs.o"))
    ap.add_argument("--build", default="build")
    ap.add_argument("--tumap", default="build/tumap.json")
    ap.add_argument("--md", help="also write a markdown summary here")
    args = ap.parse_args()

    blob = nm_symbols(args.obj)
    ours = our_symbols(args.build)
    tested = tested_symbols(args.build)
    aliased = aliased_symbols(args.build)
    addr = blob_addresses(args.obj)
    tus = load_tus(args.tumap)

    total = text_size(args.obj)
    # Weak counts as globally visible: it has a `ref_` alias and is drivable.
    gl = {n: s for n, (s, k) in blob.items() if k in ("T", "W")}
    lo = {n: s for n, (s, k) in blob.items() if k == "t"}

    done_g = {n: s for n, s in gl.items() if n in ours}
    done_l = {n: s for n, s in lo.items() if n in ours}

    # Partial ones out of both figures, and named below instead.
    partial = {n: s for n, s in list(done_g.items()) + list(done_l.items())
               if n in PARTIAL}
    done_g = {n: s for n, s in done_g.items() if n not in PARTIAL}
    done_l = {n: s for n, s in done_l.items() if n not in PARTIAL}

    # A file-local symbol that got a `ref_` alias can be called by name and so
    # belongs in the denominator; one that did not, cannot and does not.
    done_la = {n: s for n, s in done_l.items() if n in aliased}
    done_ln = {n: s for n, s in done_l.items() if n not in aliased}

    drivable = dict(done_g)
    drivable.update(done_la)
    done_t = {n: s for n, s in drivable.items() if n in tested}

    d_bytes = sum(done_g.values()) + sum(done_l.values())
    t_bytes = sum(done_t.values())

    lines = []
    add = lines.append
    add("dsplibs.o reconstruction coverage")
    add("")
    add("  .text                        %8d bytes, %d symbols"
        % (total, len(blob)))
    add("")
    g_bytes = sum(drivable.values())
    add("  translated  %s %5.1f%%  %8d bytes, %d symbols"
        % (bar(d_bytes / total if total else 0),
           100 * d_bytes / total if total else 0, d_bytes,
           len(done_g) + len(done_l)))
    add("  tested      %s %5.1f%%  %8d bytes, %d of %d that can be"
        % (bar(t_bytes / g_bytes if g_bytes else 0),
           100 * t_bytes / g_bytes if g_bytes else 0, t_bytes, len(done_t),
           len(drivable)))
    add("")
    add("  `tested` is the share of what we have translated that some test"
        " drives")
    add("  against the blob itself, not a self-consistency check.  Its"
        " denominator")
    add("  is what CAN be driven that way: everything with a `ref_` alias in")
    add("  build/dsplibs_ref.o, which since the Makefile globalizes first"
        " includes")
    add("  the file-local symbols too -- %d of ours (%d bytes)."
        % (len(done_la), sum(done_la.values())))
    add("")

    if partial:
        add("  defined here only IN PART, and so counted in NEITHER figure")
        add("  above -- the whole symbol size would land in `translated` the")
        add("  moment a definition exists, which for a function being written")
        add("  one dispatch case at a time is a claim nobody made:")
        for name in sorted(partial):
            add("    %-30s %6d bytes   %s"
                % (name, partial[name], PARTIAL[name]))
        add("")

    untested = sorted(((s, n) for n, s in drivable.items() if n not in tested),
                      reverse=True)
    if untested:
        add("  translated, alias exists, and NOT tested:")
        for size, name in untested:
            add("    %-44s %6d bytes%s"
                % (name, size, "   (file-local)" if name in done_la else ""))
        add("")

    stuck = unaliasable(args.obj, aliased)
    if stuck:
        add("  file-local and NOT aliasable, so reached through a caller if at")
        add("  all: each of these names is used by more than one translation")
        add("  unit, and two statics of the same name are two different")
        add("  objects -- globalizing both would make ONE symbol and the link")
        add("  would take whichever it saw first.  symmap.py excludes them")
        add("  every run.  Listed whether or not we have reconstructed one,")
        add("  because what cannot be tested directly is worth naming.")
        add("  (%d symbols; %d reconstructed here, and those %d are outside"
            % (len(stuck), len(done_ln), len(done_ln)))
        add("  the figures above):")
        for name in stuck:
            add(("    %-44s  reconstructed here, %d bytes"
                 % (name, done_ln[name])) if name in done_ln
                else ("    %s" % name))
        add("")

    strays = []
    for name in sorted(ours - set(blob)):
        why = next((w for pat, w in BENIGN if pat.search(name)), None)
        if why is None:
            strays.append(name)
    if strays:
        add("  we define these and the object has no symbol of that name --")
        add("  either a helper split out of a larger function, or drift:")
        for name in strays:
            add("    %s" % name)
        add("")

    if tus:
        rest = {}
        # Local symbols count here too.  Iterating the globals alone left the
        # file-local ones out of "what is left" as well as out of the
        # denominator, which understated both.
        for name, (size, kind) in blob.items():
            # A PARTIAL symbol is still work left, and dropping its whole
            # translation unit out of this list because a skeleton exists
            # would hide 62 KB of it.
            if name in ours and name not in PARTIAL:
                continue
            area = area_of(addr.get(name, -1), tus, kind)
            rest.setdefault(area, [0, 0])
            rest[area][0] += size
            rest[area][1] += 1
        add("  what is left, by translation-unit span:")
        for label, (size, count) in sorted(rest.items(),
                                           key=lambda kv: -kv[1][0])[:12]:
            add("    %-44s %7d bytes  %4d symbols" % (label, size, count))

    text = "\n".join(lines)
    print(text)

    if args.md:
        with open(args.md, "w", encoding="utf-8") as fh:
            fh.write("# Reconstruction coverage\n\n"
                     "Generated by `tools/coverage.py`; run `make coverage`"
                     " to refresh.\n\n```\n")
            fh.write(text)
            fh.write("\n```\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
