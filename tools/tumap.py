#!/usr/bin/env python3
"""
Reconstruct the translation-unit map of dsplibs.o.

Recovers, for each of the 281 original source files, the address range its
code occupies in .text and the symbols defined there.  This is what lets the
reconstruction mirror the original module layout instead of inventing one.

How it works
------------
`ld -r` concatenates each input object's .text in link order, and the symbol
table records that order as a sequence of STT_FILE entries.  Only ~19 TUs
still have local .text symbols to anchor them directly, so extents are
recovered in three passes:

  1. Anchor pass   - each TU with local .text symbols gets a known [min, max].
  2. Interpolate   - unanchored TUs lie between their neighbouring anchors,
                     in FILE order.  Their exact split point is unknown, so
                     the span is reported as a shared bracket.
  3. Attribute     - global symbols (emitted after all locals, so carrying no
                     FILE association) are assigned by address.

An `exact` extent is the bytes a TU is PROVEN to own: the envelope of its own
local symbols, `[min anchor, max(anchor + size)]`.  It is NOT extended to the
next anchor.  The gap between one TU's last local and the next anchor's first
local belongs to nobody in particular -- the first TU's trailing globals and
any number of unanchored TUs can sit in it -- and is reported as a shared
`bracket` span with its candidate TUs named.  A global that lands in such a
gap is reported as ambiguous ("1 of N TUs") rather than handed to the
predecessor.  This is issue #67: the earlier rule extended the extent to the
next anchor and reassigned `fax.c`/`rd.c`/`ringDetector.c`'s globals to
`voice.c` (finding F11351).

Usage:
    tumap.py <dsplibs.o> [--json out.json] [--md out.md]
    tumap.py --self-test

`--self-test` builds a synthetic two-anchor-one-unanchored object and proves
the invariant checks fire on the pre-#67 extent rule and stay silent on the
current one.  It prints its denominator (F134: a detector that prints nothing
is indistinguishable from a broken one).
"""

import argparse
import json
import sys
from collections import OrderedDict

import elfinfo


def _tu_key(t, tus):
    """Stable key for a TU.  Duplicate filenames occur (voice.c twice), so a
    repeated name gets its FILE sequence appended."""
    if sum(1 for x in tus if x["name"] == t["name"]) > 1:
        return t["name"] + "#%d" % t["seq"]
    return t["name"]


def _hard_points(tus):
    """TUs with local .text symbols, as [{tu, lo, own_hi}] sorted by lo.

    `own_hi` is the end of the last local symbol -- the end of the bytes the
    TU is PROVEN to own.  It is not the next anchor.
    """
    hard = []
    for t in tus:
        if not t["anchors"]:
            continue
        lo = min(t["anchors"])
        hi = max(a + sz for a, sz, _ in t["locals"])
        if hi > lo:
            hard.append({"tu": t, "lo": lo, "own_hi": hi})
    hard.sort(key=lambda h: h["lo"])
    return hard


def _gap_members(tus, exact, lo, hi):
    """Candidate TU keys for an unattributed span [lo, hi).

    The trailing anchored TU (its globals may run past its last local), every
    unanchored TU before the next anchor in FILE order, and the leading
    anchored TU (a TU's globals may precede its first local -- finding F78).
    """
    pre = [v for v in exact if v["hi"] <= lo]
    fol = [v for v in exact if v["lo"] >= hi]
    pre = max(pre, key=lambda v: v["hi"]) if pre else None
    fol = min(fol, key=lambda v: v["lo"]) if fol else None
    lo_seq = pre["seq"] if pre else -1
    hi_seq = fol["seq"] if fol else 10 ** 9
    members = []
    for t in tus:
        if pre is not None and t["seq"] == pre["seq"]:
            members.append(t)
        elif fol is not None and t["seq"] == fol["seq"]:
            members.append(t)
        elif lo_seq < t["seq"] < hi_seq and not t["anchors"]:
            members.append(t)
    if not members:
        members = [x for x in tus if not x["anchors"]] or list(tus)
    # The TUs whose code sits in the gap come first (they are the span's
    # content); the boundary anchors follow as the remaining candidates for
    # its globals.  The first member is the span label.
    members.sort(key=lambda t: (bool(t["anchors"]), t["seq"]))
    return [_tu_key(t, tus) for t in members]


def assign_extents(tus, text_size, globals_=(), legacy=False):
    """Attach `lo`/`hi`/`kind` to every TU and attribute .text globals.

    legacy=True reproduces the pre-#67 rule (an anchored TU runs to the next
    anchor) so the self-test can show the validator firing on it.  It is never
    used to produce the shipped map.
    """
    hard = _hard_points(tus)
    for i, h in enumerate(hard):
        if legacy:
            h["end"] = hard[i + 1]["lo"] if i + 1 < len(hard) else text_size
        else:
            h["end"] = h["own_hi"]
    by_seq = {h["tu"]["seq"]: h for h in hard}

    result = OrderedDict()
    for t in tus:
        key = _tu_key(t, tus)
        if t["seq"] in by_seq:
            h = by_seq[t["seq"]]
            lo, hi, kind = h["lo"], h["end"], "exact"
        else:
            prev = max((h for h in hard if h["tu"]["seq"] <= t["seq"]),
                       key=lambda h: h["tu"]["seq"], default=None)
            nxt = min((h for h in hard if h["tu"]["seq"] > t["seq"]),
                      key=lambda h: h["tu"]["seq"], default=None)
            if legacy:
                lo = prev["lo"] if prev else 0
            else:
                lo = prev["own_hi"] if prev else 0
            hi = nxt["lo"] if nxt else text_size
            kind = "bracket"
        result[key] = {"key": key, "file": t["name"], "seq": t["seq"],
                       "lo": lo, "hi": hi, "kind": kind,
                       "lang": t["name"].rsplit(".", 1)[-1],
                       "locals": t["locals"]}

    # Attribute globals to the exact extent that PROVES ownership.  Anything
    # else lands in a shared gap and is reported ambiguous.
    exact = sorted((v for v in result.values() if v["kind"] == "exact"),
                   key=lambda v: v["lo"])
    spans = []
    cursor = 0
    for v in exact:
        if v["lo"] > cursor:
            spans.append(_make_span(tus, exact, cursor, v["lo"]))
        spans.append({"lo": v["lo"], "hi": v["hi"], "kind": "exact",
                      "members": [v["key"]], "globals": []})
        cursor = v["hi"]
    if cursor < text_size:
        spans.append(_make_span(tus, exact, cursor, text_size))

    unattributed = []
    for value, size, name in globals_:
        owner = next((v for v in exact if v["lo"] <= value < v["hi"]), None)
        if owner is not None:
            owner.setdefault("globals", []).append((value, size, name))
            continue
        for sp in spans:
            if sp["kind"] == "bracket" and sp["lo"] <= value < sp["hi"]:
                sp["globals"].append((value, size, name))
                break
        else:
            unattributed.append((value, size, name))

    # Sort each exact TU's globals for stable output.
    for v in exact:
        if v.get("globals"):
            v["globals"].sort()

    return {"tus": result, "spans": spans, "monotonic": _monotonic(hard),
            "unattributed": unattributed}


def _make_span(tus, exact, lo, hi):
    return {"lo": lo, "hi": hi, "kind": "bracket",
            "members": _gap_members(tus, exact, lo, hi), "globals": []}


def _monotonic(hard):
    seqs = [h["tu"]["seq"] for h in hard]
    return all(a < b for a, b in zip(seqs, seqs[1:]))


def read_tus(obj):
    """Parse an object's symtab into (tus, globals_, text_size)."""
    sections = elfinfo.read_sections(obj)
    syms = elfinfo.read_symbols(obj)

    text_idx = next((i for i, s in sections.items() if s.name == ".text"), None)
    if text_idx is None:
        sys.exit("error: no .text section in %s" % obj)
    text_size = sections[text_idx].size

    # Pass 1 - walk the symtab in order, grouping locals under their FILE.
    # Duplicate filenames occur (voice.c appears twice), so key on position,
    # not on name, or the two copies collapse and break monotonicity.
    tus = []            # [{name, seq, anchors: [addr...], locals: [...]}]
    cur = None
    for s in syms:
        if s.type == "FILE":
            if elfinfo.is_source_file(s.name):
                cur = {"name": s.name, "seq": len(tus), "anchors": [],
                       "locals": []}
                tus.append(cur)
            else:
                cur = None          # <built-in> / <command line>
        elif (cur is not None and s.bind == "LOCAL"
              and elfinfo.section_name(sections, s.ndx) == ".text"
              and s.type in ("FUNC", "OBJECT", "NOTYPE")):
            cur["anchors"].append(s.value)
            cur["locals"].append((s.value, s.size, s.name))

    # Pass 3 input - globals carry no FILE association; collect them by
    # address for assign_extents.
    globals_ = [(s.value, s.size, s.name) for s in syms
                if s.bind in ("GLOBAL", "WEAK")
                and elfinfo.section_name(sections, s.ndx) == ".text"
                and s.type in ("FUNC", "OBJECT", "NOTYPE")]
    return tus, globals_, text_size


def build_map(obj):
    tus, globals_, text_size = read_tus(obj)
    m = assign_extents(tus, text_size, globals_)
    m["text_size"] = text_size
    return m


def validate(m):
    """Return (problems, stats).  Zero problems is the only clean result.

    Invariants (issue #67):
      * an `exact` extent is exactly one TU's local-symbol envelope -- it
        contains no bytes of any other FILE, and is not extended over a gap;
      * `exact` extents do not overlap;
      * every attributed global lies inside its owner's exact extent;
      * exact extents and bracket spans tile [0, text_size) with no gap and
        no overlap;
      * TUs are non-decreasing in `lo` in FILE order.
    """
    tus = m["tus"]
    spans = m["spans"]
    text_size = m["text_size"]
    problems = []

    exact = [(k, v) for k, v in tus.items() if v["kind"] == "exact"]

    # 1. exact extent == own local envelope.
    for k, v in exact:
        env_lo = min(a for a, _s, _n in v["locals"])
        env_hi = max(a + s for a, s, _n in v["locals"])
        if v["lo"] != env_lo or v["hi"] != env_hi:
            problems.append(
                "%s: exact extent 0x%06x-0x%06x is not its local envelope "
                "0x%06x-0x%06x" % (k, v["lo"], v["hi"], env_lo, env_hi))

    # 2. no exact extent overlaps another.
    ex = sorted((v["lo"], v["hi"], k) for k, v in exact)
    for (l1, h1, k1), (l2, _h2, k2) in zip(ex, ex[1:]):
        if l2 < h1:
            problems.append("exact extents overlap: %s 0x%06x-0x%06x and "
                            "%s 0x%06x" % (k1, l1, h1, k2, l2))

    # 3. no exact extent contains another FILE's local symbol.
    for k, v in exact:
        for k2, v2 in tus.items():
            if k2 == k:
                continue
            for a, _s, n in v2["locals"]:
                if v["lo"] <= a < v["hi"]:
                    problems.append(
                        "%s's exact extent 0x%06x-0x%06x contains %s's local "
                        "%s at 0x%06x" % (k, v["lo"], v["hi"], k2, n, a))

    # 4. every attributed global is inside its owner.
    for k, v in exact:
        for a, _s, n in v.get("globals", []):
            if not (v["lo"] <= a < v["hi"]):
                problems.append(
                    "%s is attributed global %s at 0x%06x outside its exact "
                    "extent 0x%06x-0x%06x" % (k, n, a, v["lo"], v["hi"]))

    # 5. tiling (spans is the full partition, exact extents included).
    cov = sorted([(sp["lo"], sp["hi"], "span") for sp in spans])
    if not cov:
        problems.append("no extents cover .text")
    else:
        if cov[0][0] != 0:
            problems.append(".text 0x0 is not covered (first extent 0x%06x)"
                            % cov[0][0])
        for (l1, h1, k1), (l2, _h2, k2) in zip(cov, cov[1:]):
            if l2 < h1:
                problems.append("coverage overlaps: %s 0x%06x-0x%06x and %s "
                                "at 0x%06x" % (k1, l1, h1, k2, l2))
            elif l2 > h1:
                problems.append("coverage gap 0x%06x-0x%06x between %s and %s"
                                % (h1, l2, k1, k2))
        if cov[-1][1] != text_size:
            problems.append(".text 0x%06x end is not covered (last extent "
                            "0x%06x)" % (text_size, cov[-1][1]))

    # 6. FILE-order lo is non-decreasing.
    ordered = sorted(tus.values(), key=lambda v: v["seq"])
    for a, b in zip(ordered, ordered[1:]):
        if b["lo"] < a["lo"]:
            problems.append(
                "FILE order regresses in address: %s (seq %d) lo 0x%06x then "
                "%s (seq %d) lo 0x%06x" % (a["file"], a["seq"], a["lo"],
                                           b["file"], b["seq"], b["lo"]))

    attributed = sum(len(v.get("globals", [])) for k, v in exact)
    ambiguous = sum(len(sp["globals"]) for sp in spans)
    for value, _s, name in m.get("unattributed", []):
        problems.append("global %s at 0x%06x falls in no span" % (name, value))
    stats = {"tus": len(tus), "exact": len(exact),
             "locals": sum(len(v["locals"]) for v in tus.values()),
             "globals": attributed + ambiguous, "attributed": attributed,
             "ambiguous": ambiguous, "spans": len(spans),
             "unattributed": len(m.get("unattributed", []))}
    return problems, stats


def self_test(obj=None):
    """Two anchors, one unanchored TU, one global in the shared gap.

    Shows the checks fire on the pre-#67 extent rule and are clean on the
    current one, with the denominator printed.  With `obj`, the pre-#67 rule
    is also run on the real object to show the detector firing there.
    """
    def sym(name, value, size=0):
        return (value, size, name)

    tus = [
        {"name": "a.c", "seq": 0,
         "anchors": [0x100], "locals": [sym("a_f", 0x100, 0x20)]},
        {"name": "b.c", "seq": 1, "anchors": [], "locals": []},
        {"name": "c.c", "seq": 2,
         "anchors": [0x200], "locals": [sym("c_f", 0x200, 0x20)]},
    ]
    text_size = 0x300
    globals_ = [(0x140, 4, "b_data")]

    fixed = assign_extents(tus, text_size, globals_)
    fixed["text_size"] = text_size
    bad = assign_extents(tus, text_size, globals_, legacy=True)
    bad["text_size"] = text_size

    problems, stats = validate(fixed)
    legacy_problems, _ = validate(bad)

    assert not problems, problems
    # The legacy rule must trip at least the envelope check (a.c swallowed
    # b.c's gap), and must not have attributed b_data to the right place.
    assert legacy_problems, "validator did not fire on the pre-#67 map"

    # The unanchored TU is a bracket shared by a.c/b.c/c.c, and its global is
    # reported ambiguous rather than handed to a.c.
    b = fixed["tus"]["b.c"]
    assert b["kind"] == "bracket" and b["lo"] == 0x120 and b["hi"] == 0x200, b
    assert fixed["tus"]["a.c"]["hi"] == 0x120, fixed["tus"]["a.c"]
    assert "globals" not in fixed["tus"].get("a.c", {}), fixed["tus"]["a.c"]
    gap = [s for s in fixed["spans"] if s["kind"] == "bracket"
           and s["lo"] == 0x120]
    assert len(gap) == 1, gap
    assert gap[0]["members"] == ["b.c", "a.c", "c.c"], gap
    assert gap[0]["globals"] == [(0x140, 4, "b_data")], gap

    print("tumap self-test: %d TUs, %d exact, %d locals, %d globals checked"
          % (stats["tus"], stats["exact"], stats["locals"], stats["globals"]))
    print("  fixed map : %d invariant problems (expected 0)" % len(problems))
    print("  pre-#67   : %d invariant problems (expected >0) -- detector fires"
          % len(legacy_problems))

    if obj:
        rtus, rglobals, rtext = read_tus(obj)
        rfix = assign_extents(rtus, rtext, rglobals)
        rfix["text_size"] = rtext
        rbad = assign_extents(rtus, rtext, rglobals, legacy=True)
        rbad["text_size"] = rtext
        rfix_problems, rstats = validate(rfix)
        rbad_problems, _ = validate(rbad)
        assert not rfix_problems, rfix_problems
        assert rbad_problems, "validator did not fire on %s" % obj
        # voice.c is the issue's own case: the pre-#67 rule hands it the
        # whole span and the globals that belong to fax.c/rd.c/ringDetector.c.
        voc = [v for v in rbad["tus"].values()
               if v["file"] == "voice.c" and v["kind"] == "exact"]
        assert voc and max(v["hi"] - v["lo"] for v in voc) > 0x100, voc
        print("  %s: %d TUs, %d locals, %d globals -- fixed %d problems, "
              "pre-#67 %d" % (obj, rstats["tus"], rstats["locals"],
                              rstats["globals"], len(rfix_problems),
                              len(rbad_problems)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("obj", nargs="?", help="the ld -r object (dsplibs.o)")
    ap.add_argument("--json")
    ap.add_argument("--md")
    ap.add_argument("--check", action="store_true",
                    help="validate the invariants and exit non-zero on failure")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        self_test(args.obj)
        return
    if not args.obj:
        ap.error("obj is required unless --self-test is given")

    m = build_map(args.obj)
    tus = m["tus"]
    exact = sum(1 for v in tus.values() if v["kind"] == "exact")
    langs = {}
    for v in tus.values():
        langs[v["lang"]] = langs.get(v["lang"], 0) + 1

    problems, stats = validate(m)

    print("translation units : %d" % len(tus))
    print("  by language     : %s" % ", ".join("%s=%d" % kv
                                               for kv in sorted(langs.items())))
    print("  exact extents   : %d  (rest are shared brackets)" % exact)
    print(".text size        : %d bytes" % m["text_size"])
    print("shared spans      : %d  (globals ambiguous in them: %d)"
          % (stats["spans"] - exact, stats["ambiguous"]))
    print("globals attributed: %d of %d  (exact owners); %d ambiguous"
          % (stats["attributed"], stats["globals"], stats["ambiguous"]))
    print("anchor order matches FILE order: %s" % m["monotonic"])
    if not m["monotonic"]:
        print("  WARNING: ld -r layout assumption violated - map is unreliable")
    print("invariant check   : %d problems (over %d TUs, %d locals, %d globals)"
          % (len(problems), stats["tus"], stats["locals"], stats["globals"]))
    for p in problems:
        print("  VIOLATION: %s" % p)

    if args.json:
        with open(args.json, "w") as f:
            json.dump(m, f, indent=1)
        print("wrote %s" % args.json)

    if args.md:
        with open(args.md, "w") as f:
            f.write("# dsplibs.o translation-unit map\n\n")
            f.write("Generated by `tools/tumap.py`. %d TUs, .text = %d bytes.\n\n"
                    % (len(tus), m["text_size"]))
            f.write("`exact` is the envelope of a TU's own local symbols and "
                    "is byte-accurate\nfor those bytes.  It is not extended "
                    "over the gap to the next anchor: that\ngap is a `bracket` "
                    "shared by the candidate TUs named below, and globals in "
                    "it\nare ambiguous rather than assigned.  See issue #67.\n\n")
            f.write("| # | source | lang | extent | bytes | kind |\n")
            f.write("|--:|---|---|---|--:|---|\n")
            for v in sorted(tus.values(), key=lambda v: v["seq"]):
                f.write("| %d | `%s` | %s | `0x%06x-0x%06x` | %d | %s |\n"
                        % (v["seq"], v["file"], v["lang"], v["lo"], v["hi"],
                           v["hi"] - v["lo"], v["kind"]))
            brackets = [s for s in m["spans"] if s["kind"] == "bracket"
                        and s["globals"]]
            if brackets:
                f.write("\n## Globals in shared spans (ambiguous)\n\n")
                f.write("| span | candidates | globals |\n")
                f.write("|---|--:|--:|\n")
                for s in brackets:
                    f.write("| `0x%06x-0x%06x` | %s | %d |\n"
                            % (s["lo"], s["hi"], ", ".join(s["members"]),
                               len(s["globals"])))
        print("wrote %s" % args.md)

    if args.check and problems:
        sys.exit(1)


if __name__ == "__main__":
    main()
