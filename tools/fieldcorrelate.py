#!/usr/bin/env python3
"""
Value-correlation mining: does an unnamed field's VALUE track an
already-named sibling's, across the real differential-test corpus?

WHY THIS EXISTS

Six field-naming waves worked the standard evidence order -- a format string,
a typed caller, static usage inference -- and the residue that is left has,
per finding F10186, none of the first two: no debug print reaches it and no
typed callee ever receives it.  Those are STATIC properties.  This tool asks
a question about RUNTIME behaviour instead: across every checkpoint in the
whole `make test`/`make period` corpus, what value did the candidate field
actually hold, and does that value covary with an already-named sibling
field of the same object at the same moment?

This is real evidence, but it sits BELOW a typed caller and ABOVE bare usage
inference in CLAUDE.md's evidence order.  A correlation is not a proof: two
fields can move together for a reason that has nothing to do with either
one's meaning, especially in a small state space (two independent booleans
that both happen to be set during initialisation correlate perfectly over a
corpus that never exercises the divergent case).  Read every result with the
"how many joint states did we actually SEE" figure this tool prints for
exactly that reason, and never promote a correlation to a name without an
independent check that the mechanism makes sense.

HOW THE DATA GETS HERE

`test/harness/harness.c`'s `diff_eq_obj_` -- the function under EVERY
`diff_eq_obj()` call in the whole suite -- now calls a capture hook that is a
silent no-op unless both `DSPLIB_FIELDLOG_TYPE` and `DSPLIB_FIELDLOG_OUT` are
set.  When they are, and the checkpoint's `type` string matches, it appends
one JSON line with the reference (blob) side's raw bytes.  `capture` below
drives that: point it at a type and a log path and it runs every built test
binary, so whatever fraction of the corpus already exercises that struct
gets captured with ZERO new test inputs.

`analyze` resolves byte ranges back into fields using the same DWARF
tools/whichfield.py already reads (a linked build/test/t_* binary, built
`-g`), so this carries no struct layout of its own to drift from the object.

USAGE

    tools/fieldcorrelate.py capture  --type "V90CP" --out /tmp/v90cp.jsonl
    tools/fieldcorrelate.py analyze  --type "V90CP" --log /tmp/v90cp.jsonl \\
                                      --candidate groupSize

`--type` is the exact string a `diff_eq_obj`/`diff_eq_obj_` call site passes
as its `type` argument -- usually the bare C++ class name or `struct foo`,
whatever the call site itself wrote; `analyze`'s DWARF lookup (a separate
`--dwarf-type` if it differs) uses tools/whichfield.py's own conventions.

`--candidate` is a field name from the struct's layout (see
`tools/whichfield.py TYPE --layout`) or a raw byte offset.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys

_here = os.path.dirname(os.path.abspath(__file__))
# whichfield.py already carries the fix for tools/dis.py shadowing the
# standard library's `dis` (which `inspect`, which pyelftools uses via
# `elftools`, imports on the way up) -- importing it before anything else
# reaching for pyelftools keeps this script off that landmine too.
if _here not in sys.path:
    sys.path.insert(0, _here)
import whichfield as wf  # noqa: E402


# ---------------------------------------------------------------------------
# Layout: reuse tools/whichfield.py's DWARF walk rather than re-deriving one.
# ---------------------------------------------------------------------------

def find_complete_type(want):
    """
    Like whichfield.find_type, but does not settle for the first DIE that
    matches the name -- most classes here are forward-declared (`class
    V92CP;`) in far more translation units than they are fully defined in,
    and whichfield's own `find_type` returns whichever DIE it meets FIRST
    across every build/test/t_* binary in glob order, incomplete or not.
    That is fine for its own job (an offset lookup against a binary that
    DOES have the full type), and wrong for this tool's, which has no
    particular binary in mind -- it needs the definition, from wherever it
    can find one.
    """
    tag = None
    bare = want
    for kw, t in (("struct ", "DW_TAG_structure_type"),
                  ("union ", "DW_TAG_union_type"),
                  ("class ", "DW_TAG_class_type")):
        if want.startswith(kw):
            tag, bare = t, want[len(kw):]
    fallback = None
    for die, cu in wf.dies():
        if wf.name(die) != bare:
            continue
        if tag and die.tag != tag:
            continue
        if not tag and die.tag not in ("DW_TAG_structure_type",
                                        "DW_TAG_union_type",
                                        "DW_TAG_class_type",
                                        "DW_TAG_typedef"):
            continue
        if die.attributes.get("DW_AT_byte_size") is not None:
            return die, cu
        if fallback is None:
            fallback = (die, cu)
    return fallback if fallback else (None, None)


def get_layout(want):
    """[(offset, path, size, typename), ...] for every field of `want`."""
    die, cu = find_complete_type(want)
    if die is None:
        sys.exit("no DWARF for %r under build/test -- build the test "
                  "binaries that exercise it first (`make test`)." % want)
    total = wf.size(die, cu)
    fields = []
    for o in range(total):
        path, rem, leaf = wf.walk(die, cu, o)
        if rem == 0 and path:
            fields.append((o, path, wf.size(leaf, cu) or 1, wf.typename(leaf, cu)))
    return total, fields


# `pad_NNNN`, and the `type_NNNN` family (`short_2800`, `flags_0217`,
# `ptr_49b4`, ...): CLAUDE.md's own taxonomy of what still carries no name.
# A bare bottom-level `fNNNN` counts too. Matched on the LAST path segment,
# with any trailing `[n]` stripped, so `foo.word_18[3]` is still a candidate.
_PLACEHOLDER_PREFIXES = (
    "pad", "type", "short", "byte", "word", "dword", "qword", "flag",
    "flags", "ptr", "long", "ulong", "uchar", "ushort", "char", "uint",
    "int8", "int16", "int32", "double", "float", "dw", "sbyte", "sshort",
)
_PLACEHOLDER_RE = re.compile(
    r"^(?:%s)_[0-9a-fA-F]+$" % "|".join(_PLACEHOLDER_PREFIXES))
_BARE_F_RE = re.compile(r"^f[0-9a-fA-F]{2,}$")


def is_placeholder(path):
    leaf = path.rsplit(".", 1)[-1]
    leaf = re.sub(r"\[\d+\]$", "", leaf)
    return bool(_PLACEHOLDER_RE.match(leaf) or _BARE_F_RE.match(leaf))


# ---------------------------------------------------------------------------
# capture
# ---------------------------------------------------------------------------

def cmd_capture(args):
    testbins = args.tests or sorted(glob.glob(os.path.join(
        args.build, "test", "t_*")))
    testbins = [t for t in testbins
                if os.path.isfile(t) and os.access(t, os.X_OK)
                and not t.endswith((".o", ".d"))]
    if not testbins:
        sys.exit("no test binaries found under %s/test -- run `make test` "
                  "first." % args.build)

    if not args.append and os.path.exists(args.out):
        os.remove(args.out)

    env = dict(os.environ)
    env["DSPLIB_FIELDLOG_TYPE"] = args.type
    env["DSPLIB_FIELDLOG_OUT"] = os.path.abspath(args.out)

    ran = 0
    for t in testbins:
        try:
            subprocess.run([t], env=env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL, timeout=args.timeout)
            ran += 1
        except (OSError, subprocess.TimeoutExpired):
            continue

    n = 0
    if os.path.exists(args.out):
        with open(args.out) as f:
            n = sum(1 for _ in f)
    print("capture: ran %d test binaries, %d checkpoints of type %r logged "
          "to %s" % (ran, n, args.type, args.out))
    if n == 0:
        sys.exit("zero checkpoints captured -- the type string must match "
                  "EXACTLY what the call site passes to diff_eq_obj (check "
                  "quoting/spelling), and something has to actually call "
                  "diff_eq_obj on it. This is not a clean result, it is the "
                  "tool measuring nothing (CLAUDE.md, 'a detector must "
                  "report its denominator').")


# ---------------------------------------------------------------------------
# analyze
# ---------------------------------------------------------------------------

def load_log(path, expect_n=None, test_filter=None):
    samples = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rec = json.loads(line)
            raw = bytes.fromhex(rec["hex"])
            if expect_n is not None and len(raw) != expect_n:
                continue  # a different sizeof at this checkpoint; skip it
            if test_filter and test_filter not in rec["test"]:
                continue
            samples.append((rec["test"], rec["input"], raw))
    return samples


def field_value(raw, off, size):
    return int.from_bytes(raw[off:off + size], "little", signed=False)


def relationship(cvals, svals):
    """
    Characterise how svals (named sibling) relates to cvals (candidate),
    over paired samples. Returns (kind, detail, strength) or None.
    strength is an ordering key, highest = strongest evidence.
    """
    n = len(cvals)
    if n == 0:
        return None
    # Both sides have to actually MOVE, or every relationship below is a
    # trivial artefact of one side being a constant (a heap pointer that
    # never changes within one process "correlates" with everything, and
    # says nothing) -- CLAUDE.md's own point about a small state space
    # applies most sharply exactly here.
    if len(set(cvals)) < 2 or len(set(svals)) < 2:
        return None

    # A NEAR match is real evidence too, and often the MOST informative
    # shape: the V90ConnectionEvaluator validation case (F7485/F9480 --
    # `delayedRetrainArmed` is the copy `getV90CpBits` makes of
    # `delayedRetrainRequest`, and the copy runs inside one function) is
    # equal in 676 of 684 same-checkpoint samples and diverges only on the
    # handful of deliberately-adversarial test inputs built to probe the
    # exception -- a STRONGER signal than exactness over a corpus too small
    # to ever have exercised that exception. Below NEAR_THRESH this stops
    # being informative and falls through to the Pearson fallback instead.
    NEAR_THRESH = 0.90

    def best_majority(keyfn):
        """The most common key over the paired samples, and its fraction."""
        counts = {}
        for c, s in zip(cvals, svals):
            k = keyfn(c, s)
            counts[k] = counts.get(k, 0) + 1
        val, hits = max(counts.items(), key=lambda kv: kv[1])
        return val, hits / n, n - hits

    def near(label, keyfn, fmt, base_strength, skip=None):
        val, frac, exceptions = best_majority(keyfn)
        if skip is not None and val == skip:
            return None
        if frac < NEAR_THRESH:
            return None
        if frac == 1.0:
            return (label, "%s, every sample" % fmt(val), base_strength)
        return (label + "~", "%s in %.1f%% of %d samples, %d exception(s) "
                "-- worth reading which inputs those are before naming"
                % (fmt(val), frac * 100, n, exceptions),
                base_strength * frac)

    rel = near("equal", lambda c, s: c == s,
               lambda v: "candidate == sibling" if v else
               "candidate != sibling (most common state)", 100, skip=False)
    if rel:
        return rel

    # Fixed additive offset (covers "one ahead of", "mirrors minus one", ...).
    rel = near("affine", lambda c, s: c - s,
               lambda v: "candidate - sibling == %d" % v, 95)
    if rel:
        return rel

    # Fixed XOR mask -- catches bitwise inversion/complement relationships
    # a plain additive check misses.
    rel = near("xor", lambda c, s: c ^ s,
               lambda v: "candidate ^ sibling == 0x%x" % v, 90, skip=0)
    if rel:
        return rel

    # Deterministic function sibling -> candidate (many-to-one is fine, a
    # sibling value never maps to two different candidate values).
    fwd = {}
    consistent_fwd = True
    for c, s in zip(cvals, svals):
        if s in fwd and fwd[s] != c:
            consistent_fwd = False
            break
        fwd[s] = c
    bwd = {}
    consistent_bwd = True
    for c, s in zip(cvals, svals):
        if c in bwd and bwd[c] != s:
            consistent_bwd = False
            break
        bwd[c] = s

    if consistent_fwd and len(set(svals)) > 1:
        return ("function(sibling)", "candidate is a fixed function of "
                "sibling over %d distinct sibling values" % len(set(svals)),
                80)
    if consistent_bwd and len(set(cvals)) > 1:
        return ("function(candidate)", "sibling is a fixed function of "
                "candidate over %d distinct candidate values"
                % len(set(cvals)), 75)

    # Fallback: linear (Pearson) correlation, for genuinely numeric fields.
    if len(set(cvals)) > 1 and len(set(svals)) > 1:
        mc = sum(cvals) / n
        ms = sum(svals) / n
        cov = sum((c - mc) * (s - ms) for c, s in zip(cvals, svals))
        vc = sum((c - mc) ** 2 for c in cvals)
        vs = sum((s - ms) ** 2 for s in svals)
        if vc > 0 and vs > 0:
            r = cov / (vc ** 0.5 * vs ** 0.5)
            if abs(r) >= 0.9:
                return ("pearson", "r = %+.3f (linear, not exact)" % r,
                        50 + abs(r) * 10)
    return None


def joint_relationship(cvals, s1vals, s2vals, min_tuples=3):
    """
    Is candidate a consistent function of the PAIR (sibling1, sibling2)?
    Needed for a relationship like V90CP's calcSequenceLength -- the result
    depends on two fields jointly, so no single-sibling pairwise check in
    `relationship()` above can ever find it; that is a real limitation of
    single-sibling correlation, not a defect, and this is the mitigation for
    it. Declines (returns None) if either field alone already determines the
    candidate -- that is already reported as the stronger single-sibling
    result, and a joint report would just be restating it.
    """
    if len(set(cvals)) < 2 or len(set(s1vals)) < 2 or len(set(s2vals)) < 2:
        return None
    fwd = {}
    counts = {}
    for c, s1, s2 in zip(cvals, s1vals, s2vals):
        key = (s1, s2)
        if key in fwd and fwd[key] != c:
            return None
        fwd[key] = c
        counts[key] = counts.get(key, 0) + 1
    distinct_tuples = len(fwd)
    if distinct_tuples < min_tuples:
        return None
    # A near-continuous field (a float in particular) makes almost every
    # joint (s1, s2) pair UNIQUE across the corpus, so "no key ever mapped to
    # two different candidate values" is true VACUOUSLY -- it was never
    # actually tested twice. Real evidence needs the SAME joint state to
    # recur, with the SAME candidate value each time; require several keys
    # to have actually repeated, not just a low distinct-tuple count (a
    # handful of repeats among thousands of singletons would still slip
    # through a bare `distinct_tuples < n` check).
    repeated = sum(1 for k in counts if counts[k] >= 2)
    if repeated < min_tuples:
        return None
    # Decline if s1 alone, or s2 alone, already determines candidate -- that
    # is single-sibling territory and already covered.
    for svals in (s1vals, s2vals):
        single = {}
        ok = True
        for c, s in zip(cvals, svals):
            if s in single and single[s] != c:
                ok = False
                break
            single[s] = c
        if ok:
            return None
    return ("function(pair)", "candidate is a fixed function of the PAIR "
            "over %d distinct joint (sibling1, sibling2) values seen"
            % distinct_tuples, 70)


def cmd_analyze(args):
    dwarf_type = args.dwarf_type or args.type
    total, fields = get_layout(dwarf_type)
    print("%s: %d bytes, %d modelled fields (DWARF)"
          % (dwarf_type, total, len(fields)))

    # Resolve the candidate: a field name, or a raw offset.
    cand = None
    for off, path, size, tname in fields:
        if path == args.candidate:
            cand = (off, path, size, tname)
            break
    if cand is None:
        try:
            coff = int(args.candidate, 0)
        except ValueError:
            sys.exit("candidate %r is not a field name in the layout and "
                      "not a numeric offset -- run `tools/whichfield.py %s "
                      "--layout` to see the field names." % (args.candidate,
                                                              dwarf_type))
        for off, path, size, tname in fields:
            if off == coff:
                cand = (off, path, size, tname)
                break
        if cand is None:
            sys.exit("offset %d does not start a modelled field of %s "
                      "(tools/whichfield.py %s %d says where it lands)."
                      % (coff, dwarf_type, dwarf_type, coff))

    coff, cpath, csize, ctname = cand
    # Array ELEMENTS (`bits[3702]`) are excluded from the sibling pool: an
    # unmodelled buffer's individual bytes are not "already-named fields" in
    # CLAUDE.md's sense just because the array itself has a name, and
    # including them turned one real struct into thousands of
    # near-duplicate, information-free "siblings" the first time this ran.
    named = [(o, p, s, t) for (o, p, s, t) in fields
             if p != cpath and "[" not in p and not is_placeholder(p)]
    if not named:
        sys.exit("no already-named sibling fields in %s -- this technique "
                  "needs something to correlate against (per the task "
                  "brief: not a good candidate for value-correlation "
                  "mining, on its own merit)." % dwarf_type)

    samples = load_log(args.log, expect_n=total, test_filter=args.test_filter)
    print("log: %d checkpoints of matching size (%d bytes) out of the file"
          % (len(samples), total))
    if len(samples) < args.min_samples:
        sys.exit("only %d usable checkpoints (< --min-samples %d) -- this "
                  "is not enough to correlate anything; re-run `capture` "
                  "against more of the corpus, or widen --tests."
                  % (len(samples), args.min_samples))

    cvals = [field_value(raw, coff, csize) for (_, _, raw) in samples]
    tests_seen = sorted({t for (t, _, _) in samples})
    print("candidate %s (%s, +%d, %d bytes): %d distinct values across %d "
          "checkpoints from %d test binaries"
          % (cpath, ctname, coff, csize, len(set(cvals)), len(cvals),
             len(tests_seen)))

    sibling_vals = {path: [field_value(raw, off, size) for (_, _, raw) in samples]
                     for (off, path, size, tname) in named}

    results = []
    for off, path, size, tname in named:
        rel = relationship(cvals, sibling_vals[path])
        if rel is None:
            continue
        kind, detail, strength = rel
        results.append((strength, path, kind, detail,
                         len(set(sibling_vals[path]))))

    if args.joint and len(named) >= 2:
        names = [p for (_, p, _, _) in named]
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                p1, p2 = names[i], names[j]
                rel = joint_relationship(cvals, sibling_vals[p1],
                                          sibling_vals[p2])
                if rel is None:
                    continue
                kind, detail, strength = rel
                results.append((strength, "%s, %s" % (p1, p2), kind, detail,
                                 len(set(zip(sibling_vals[p1],
                                             sibling_vals[p2])))))

    results.sort(key=lambda r: -r[0])
    if not results:
        print("\nNO correlation found against any of the %d already-named "
              "sibling fields%s -- a genuine negative result, not a "
              "failure to look: every named field's paired values were "
              "checked for equality, a fixed offset, a fixed XOR mask, a "
              "deterministic function either direction, and |Pearson r| "
              ">= 0.9%s, over the %d captured checkpoints."
              % (len(named), " or pair of them" if args.joint else "",
                 " (plus every pair, jointly)" if args.joint else "",
                 len(cvals)))
        return

    print("\n%-40s %-22s distinct  detail" % (
        "sibling field(s)", "relationship"))
    for strength, path, kind, detail, ndistinct in results[:args.top]:
        print("%-40s %-22s %-9d %s" % (path, kind, ndistinct, detail))

    print("\nEVIDENCE STRENGTH: this is value-correlation, CLAUDE.md's "
          "evidence order rank ~2.5 -- stronger than usage inference, "
          "weaker than a typed caller or a format string, because a small "
          "state space can correlate for reasons unrelated to either "
          "field's meaning. %d distinct candidate values were observed "
          "over %d checkpoints; a correlation seen over few distinct "
          "values is weaker evidence than one seen over many independently "
          "varying ones." % (len(set(cvals)), len(cvals)))


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    pc = sub.add_parser("capture", help="run the test corpus and log a "
                         "type's field values at every diff_eq_obj checkpoint")
    pc.add_argument("--type", required=True,
                     help="exact string a diff_eq_obj call site passes")
    pc.add_argument("--out", required=True, help="log file (JSONL)")
    pc.add_argument("--build", default="build", help="build dir (default build)")
    pc.add_argument("--tests", nargs="*", help="specific test binaries; "
                     "default is every build/test/t_*")
    pc.add_argument("--append", action="store_true")
    pc.add_argument("--timeout", type=float, default=60.0)
    pc.set_defaults(func=cmd_capture)

    pa = sub.add_parser("analyze", help="correlate a captured log's "
                         "candidate field against its already-named siblings")
    pa.add_argument("--type", required=True,
                     help="exact string used when capturing")
    pa.add_argument("--dwarf-type", help="DWARF type name for "
                     "tools/whichfield.py, if it differs from --type "
                     "(default: same as --type)")
    pa.add_argument("--log", required=True)
    pa.add_argument("--test-filter", help="only checkpoints whose test "
                     "name (diff_begin's argument) contains this substring "
                     "-- e.g. restrict to one code path instead of blending "
                     "every checkpoint the type ever appears at")
    pa.add_argument("--candidate", required=True,
                     help="field name (see whichfield.py --layout) or offset")
    pa.add_argument("--min-samples", type=int, default=5)
    pa.add_argument("--top", type=int, default=15)
    pa.add_argument("--joint", action="store_true", default=True,
                     help="also test candidate against every PAIR of named "
                     "siblings jointly (default on; O(n^2) in sibling count)")
    pa.add_argument("--no-joint", dest="joint", action="store_false")
    pa.set_defaults(func=cmd_analyze)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    sys.exit(main())
