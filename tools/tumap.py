#!/usr/bin/env python3
"""
Reconstruct the translation-unit map of dsplibs.o.

Recovers, for each of the 283 original source files, the address range its
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

A TU whose bracket is shared with others is marked `bracket`; one with hard
anchors is marked `exact`.  Only `exact` extents should be relied on for
byte-level work.

Usage:
    tumap.py <dsplibs.o> [--json out.json] [--md out.md]
"""

import argparse
import json
import sys
from collections import OrderedDict

import elfinfo


def build_map(obj):
    sections = elfinfo.read_sections(obj)
    syms = elfinfo.read_symbols(obj)

    text_idx = next((i for i, s in sections.items() if s.name == ".text"), None)
    if text_idx is None:
        sys.exit("error: no .text section in %s" % obj)
    text_size = sections[text_idx].size

    # Pass 1 - walk the symtab in order, grouping locals under their FILE.
    # Duplicate filenames occur (voice.c appears twice), so key on position,
    # not on name, or the two copies collapse and break monotonicity.
    tus = []            # [{name, seq, anchors: [addr...]}]
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

    # Pass 2 - anchored TUs give hard points; everything else is bracketed
    # between the nearest anchored neighbours on either side.
    anchored = [t for t in tus if t["anchors"]]
    for t in anchored:
        t["lo"], t["hi"] = min(t["anchors"]), max(t["anchors"])
    anchored.sort(key=lambda t: t["lo"])

    # Sanity: anchor order must agree with FILE order, else our core
    # assumption about ld -r layout is wrong and the map is meaningless.
    seqs = [t["seq"] for t in anchored]
    monotonic = all(a < b for a, b in zip(seqs, seqs[1:]))

    for i, t in enumerate(anchored):
        t["end"] = anchored[i + 1]["lo"] if i + 1 < len(anchored) else text_size

    # Assign every TU a bracket: the span between the enclosing anchors.
    result = OrderedDict()
    for t in tus:
        prev = max((a for a in anchored if a["seq"] <= t["seq"]),
                   key=lambda a: a["seq"], default=None)
        nxt = min((a for a in anchored if a["seq"] > t["seq"]),
                  key=lambda a: a["seq"], default=None)
        if t["anchors"]:
            lo, hi, kind = t["lo"], t["end"], "exact"
        else:
            lo = prev["lo"] if prev else 0
            hi = nxt["lo"] if nxt else text_size
            kind = "bracket"
        result.setdefault(t["name"] + ("#%d" % t["seq"]
                                       if sum(1 for x in tus
                                              if x["name"] == t["name"]) > 1
                                       else ""),
                          {"file": t["name"], "seq": t["seq"], "lo": lo,
                           "hi": hi, "kind": kind,
                           "lang": t["name"].rsplit(".", 1)[-1],
                           "locals": t["locals"]})

    # Pass 3 - attribute globals by address to the exact TU that contains them.
    globals_by_tu = {}
    for s in syms:
        if (s.bind in ("GLOBAL", "WEAK")
                and elfinfo.section_name(sections, s.ndx) == ".text"
                and s.type in ("FUNC", "OBJECT", "NOTYPE")):
            owner = next((k for k, v in result.items()
                          if v["kind"] == "exact"
                          and v["lo"] <= s.value < v["hi"]), None)
            if owner:
                globals_by_tu.setdefault(owner, []).append(
                    (s.value, s.size, s.name))
    for k, g in globals_by_tu.items():
        result[k]["globals"] = sorted(g)

    return {"text_size": text_size, "monotonic": monotonic, "tus": result}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj")
    ap.add_argument("--json")
    ap.add_argument("--md")
    args = ap.parse_args()

    m = build_map(args.obj)
    tus = m["tus"]
    exact = sum(1 for v in tus.values() if v["kind"] == "exact")
    langs = {}
    for v in tus.values():
        langs[v["lang"]] = langs.get(v["lang"], 0) + 1

    print("translation units : %d" % len(tus))
    print("  by language     : %s" % ", ".join("%s=%d" % kv
                                               for kv in sorted(langs.items())))
    print("  exact extents   : %d  (rest are shared brackets)" % exact)
    print(".text size        : %d bytes" % m["text_size"])
    print("anchor order matches FILE order: %s" % m["monotonic"])
    if not m["monotonic"]:
        print("  WARNING: ld -r layout assumption violated - map is unreliable")

    if args.json:
        with open(args.json, "w") as f:
            json.dump(m, f, indent=1)
        print("wrote %s" % args.json)

    if args.md:
        with open(args.md, "w") as f:
            f.write("# dsplibs.o translation-unit map\n\n")
            f.write("Generated by `tools/tumap.py`. %d TUs, .text = %d bytes.\n\n"
                    % (len(tus), m["text_size"]))
            f.write("`exact` extents are anchored by local symbols and are "
                    "byte-accurate.\n`bracket` extents share a span between "
                    "two anchors; the split point within\nthe span is not "
                    "recoverable from the symbol table alone.\n\n")
            f.write("| # | source | lang | extent | bytes | kind |\n")
            f.write("|--:|---|---|---|--:|---|\n")
            for k, v in sorted(tus.items(), key=lambda kv: kv[1]["seq"]):
                f.write("| %d | `%s` | %s | `0x%06x-0x%06x` | %d | %s |\n"
                        % (v["seq"], v["file"], v["lang"], v["lo"], v["hi"],
                           v["hi"] - v["lo"], v["kind"]))
        print("wrote %s" % args.md)


if __name__ == "__main__":
    main()
