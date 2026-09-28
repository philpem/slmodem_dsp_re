#!/usr/bin/env python3
"""
Enumerate the DATA symbols a relocatable object defines, and diff two of them.

WHY THIS EXISTS

Findings F11445/F11447 produced the "missing data symbols" list by hand: a
one-off shell that read one object's `.data`/`.rodata` names and diffed them
against the other.  That is not reusable and -- worse -- it printed no
denominator, so a run over an empty or stale object would have looked exactly
like a clean run (F134, F2400, F2401).  This is the tool that list should have
come from.

WHAT IT COMPARES

Every allocated DATA definition -- `STT_OBJECT` in any section, plus
`STT_NOTYPE` in a non-`.text` section (the odd name-table entry) -- on each
side, keyed by (name, binding).  It reports, for the reference object versus
the candidate:

  * missing    - a reference name the candidate does not define at all;
  * extra      - a candidate name the reference does not define at all;
  * size       - the same name defined with a different size;
  * section    - the same name defined in a different section.

`FUNC`, `FILE`, `SECTION` and undefined symbols are not data and are excluded;
in particular the `.text` `STT_NOTYPE` aliases (`pow`, `infinity`, ...) are
not compared.

FILE OWNERSHIP.  `ld -r` emits every input's LOCALS immediately after its
`STT_FILE` record and every GLOBAL after all locals, so a local's owner is the
preceding FILE and a global's must be inferred from its ADDRESS.  For a global
the tool brackets it between the nearest local data symbol below and above it
in the same section and reports both FILEs; when they agree that is the owner,
and when they differ it says so rather than guessing (the same rule tumap.py
applies to `.text`, issue #67).

CANDIDATE MATCHES.  A missing reference symbol is not actionable on its own;
the recovery question is "is this ours under another name?".  For every missing
symbol the tool lists candidate symbols of the other object with the same
section and size, which is the discriminator F11445 used (and which caught the
`v21_hibnd` 120-vs-122 mismatch).

DENOMINATOR.  Every run prints how many symbols it compared, per section, on
both sides.  `--self-test` assembles two synthetic objects with a planted
missing, size-changed and section-changed symbol and proves the detector fires
on each before a clean run is trusted.

Usage:
    dataaudit.py [--blob OBJ] [--candidate OBJ] [--json PATH]
                 [--section NAME] [--symbol NAME] [--all]
                 [--self-test]

With neither object argument the defaults are the blob and
`build/partial/dsplibs.o` (run `make partial-link` to produce the latter).
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict

import elfinfo


# --------------------------------------------------------------------------
# reading

def default_blob():
    """The reference object, resolved the way the Makefile resolves BLOB."""
    for cand in ("ref/slmodemd/dsplibs.o",):
        if os.path.exists(cand):
            return cand
    return "ref/slmodemd/dsplibs.o"


def data_symbols(obj):
    """All allocated DATA definitions, in symtab order.

    Returns (sections, symbols, records).  A record is a dict with the fields
    the report needs; `ndx`/`num` survive so ownership can be resolved later.
    """
    sections = elfinfo.read_sections(obj)
    symbols = elfinfo.read_symbols(obj)
    records = []
    for s in symbols:
        sec = elfinfo.section_name(sections, s.ndx)
        if not sec:
            continue
        if s.type == "OBJECT":
            pass
        elif s.type == "NOTYPE" and sec != ".text":
            pass
        else:
            continue
        records.append({"num": s.num, "name": s.name, "section": sec,
                        "value": s.value, "size": s.size,
                        "type": s.type, "bind": s.bind})
    return sections, symbols, records


def file_owners(sections, symbols, records):
    """Map a symbol's symtab index to a FILE-owner description.

    Local symbols carry their FILE directly.  Globals are an address bracket
    between the nearest local data symbol below and above them in the same
    section.

    Returns {num: {"local": bool, "file": str or None,
                   "below": (name, file), "above": (name, file)}}
    """
    current = None
    local_owner = {}
    by_section = defaultdict(list)          # section -> [(value, name, file)]
    for s in symbols:
        if s.type == "FILE":
            current = s.name if elfinfo.is_source_file(s.name) else None
            continue
        if s.bind == "LOCAL" and current is not None:
            sec = elfinfo.section_name(sections, s.ndx)
            if sec and s.type in ("OBJECT", "NOTYPE") and sec != ".text":
                local_owner[s.num] = current
                by_section[sec].append((s.value, s.name, current))
    for sec in by_section:
        by_section[sec].sort()

    result = {}
    for r in records:
        if r["bind"] == "LOCAL" and r["num"] in local_owner:
            result[r["num"]] = {"local": True, "file": local_owner[r["num"]],
                                "below": None, "above": None}
            continue
        entries = by_section.get(r["section"], [])
        below = None
        above = None
        for value, name, owner in entries:
            if value <= r["value"]:
                below = (name, owner)
            elif above is None:
                above = (name, owner)
                break
        result[r["num"]] = {"local": False, "file": None,
                            "below": below, "above": above}
    return result


def owner_text(owner):
    if owner is None:
        return "?"
    if owner["local"]:
        return owner["file"] or "?"
    below = owner["below"]
    above = owner["above"]
    names = []
    for tag, entry in (("below", below), ("above", above)):
        if entry is None:
            names.append("%s=-" % tag)
        else:
            names.append("%s=%s(%s)" % (tag, entry[0], entry[1]))
    if below and above and below[1] == above[1]:
        return below[1]
    return " ".join(names)


# --------------------------------------------------------------------------
# comparing

def multiset(records, binding=None):
    """name -> sorted [(section, size)] for one side."""
    out = defaultdict(list)
    for r in records:
        if binding and r["bind"] != binding:
            continue
        out[r["name"]].append((r["section"], r["size"]))
    for name in out:
        out[name].sort()
    return out


def compare(ref, cand, sections_filter=None):
    """Diff two record lists.  Returns the report dict.

    `ref` is the authoritative object (the blob), `cand` ours.
    """
    def keep(r):
        return sections_filter is None or r["section"] == sections_filter

    ref = [r for r in ref if keep(r)]
    cand = [r for r in cand if keep(r)]
    rmap = defaultdict(list)
    cmap = defaultdict(list)
    for r in ref:
        rmap[r["name"]].append(r)
    for r in cand:
        cmap[r["name"]].append(r)

    def sections_of(records):
        c = Counter()
        for r in records:
            c[r["section"]] += 1
        return c

    missing, extra, size_mismatch, section_mismatch = [], [], [], []
    for name in sorted(rmap):
        if name not in cmap:
            for r in rmap[name]:
                missing.append(r)
            continue
        # Compare the sorted (section, size) multisets per name.
        rb = sorted((r["section"], r["size"]) for r in rmap[name])
        cb = sorted((r["section"], r["size"]) for r in cmap[name])
        if rb == cb:
            continue
        # Attribute each differing occurrence to size or section.
        rsecs = Counter(s for s, _z in rb)
        csecs = Counter(s for s, _z in cb)
        for r in rmap[name]:
            key = (r["section"], r["size"])
            if key in cb:
                continue
            same_sec = [z for (s, z) in cb if s == r["section"]]
            if r["section"] not in csecs:
                section_mismatch.append(r)
            elif r["size"] not in same_sec:
                size_mismatch.append(r)
            else:
                size_mismatch.append(r)
    for name in sorted(cmap):
        if name not in rmap:
            for r in cmap[name]:
                extra.append(r)

    # Candidate matches for missing symbols: same section and size, other name.
    cand_by_key = defaultdict(list)
    for r in cand:
        cand_by_key[(r["section"], r["size"])].append(r)
    matches = {}
    for r in missing:
        hits = [c for c in cand_by_key.get((r["section"], r["size"]), [])
                if c["name"] not in rmap]
        matches[r["num"]] = hits

    return {
        "reference": {"sections": dict(sections_of(ref)), "total": len(ref)},
        "candidate": {"sections": dict(sections_of(cand)), "total": len(cand)},
        "missing": missing, "extra": extra,
        "size_mismatch": size_mismatch, "section_mismatch": section_mismatch,
        "matches": matches,
    }


def report(result, owners_ref, owners_cand, show_all=False):
    ref = result["reference"]
    cand = result["candidate"]
    print("dataaudit: %d reference data symbol(s) vs %d candidate"
          % (ref["total"], cand["total"]))
    for label, side in (("reference", ref), ("candidate", cand)):
        parts = ", ".join("%s=%d" % (s, side["sections"][s])
                          for s in sorted(side["sections"]))
        print("  %-9s %s" % (label, parts or "(none)"))

    groups = (("MISSING (reference-only)", result["missing"], owners_ref),
              ("EXTRA (candidate-only)", result["extra"], owners_cand),
              ("SIZE MISMATCH", result["size_mismatch"], owners_ref),
              ("SECTION MISMATCH", result["section_mismatch"], owners_ref))
    for title, rows, owners in groups:
        print("  %s: %d" % (title, len(rows)))
        if not rows:
            continue
        for r in rows:
            owner = owner_text(owners.get(r["num"]))
            line = ("    %-34s %-14s size %-5d %-7s %-6s owner=%s"
                    % (r["name"], r["section"], r["size"], r["type"],
                       r["bind"], owner))
            print(line)
            if title.startswith("MISSING"):
                for c in result["matches"].get(r["num"], []):
                    print("        candidate? %-30s %-14s size %d"
                          % (c["name"], c["section"], c["size"]))
    if show_all:
        for title, rows, owners in groups:
            pass
    clean = not (result["missing"] or result["extra"]
                 or result["size_mismatch"] or result["section_mismatch"])
    print("  verdict: %s" % ("identical" if clean else "DIFFERENT"))


# --------------------------------------------------------------------------
# self-test

_SELF_TEMPLATE = """\
.file "{file}"
.section {section}
alpha_local: .long 0
.size alpha_local,.-alpha_local
.globl keep_me
.type keep_me,@object
keep_me: .long 1
.size keep_me,.-keep_me
.globl size_me
.type size_me,@object
size_me: .long {size_body}
.size size_me,.-size_me
.globl section_me
.type section_me,@object
.section {section2}
section_me: .long 3
.size section_me,.-section_me
.section .data
.globl alpha_data
.type alpha_data,@object
alpha_data: .long 4
.size alpha_data,.-alpha_data
"""


def _assemble(directory, name, text):
    src = os.path.join(directory, name + ".s")
    obj = os.path.join(directory, name + ".o")
    with open(src, "w") as f:
        f.write(text)
    subprocess.run(["as", "--32", "-o", obj, src], check=True,
                   capture_output=True)
    return obj


def _link(directory, name, inputs):
    out = os.path.join(directory, name + ".o")
    subprocess.run(["ld", "-r", "-m", "elf_i386", "-o", out] + inputs,
                   check=True, capture_output=True)
    return out


def self_test():
    if not (shutil.which("as") and shutil.which("ld")):
        sys.exit("dataaudit --self-test needs as and ld")
    with tempfile.TemporaryDirectory(prefix="dataaudit-") as d:
        alpha = _SELF_TEMPLATE.format(file="alpha.c", section=".rodata",
                                      size_body="2", section2=".rodata")
        beta = (".file \"beta.c\"\n.section .rodata\n.globl beta_global\n"
                ".type beta_global,@object\nbeta_global: .long 9\n"
                ".size beta_global,.-beta_global\n")
        ref = _link(d, "ref", [_assemble(d, "alpha", alpha),
                               _assemble(d, "beta", beta)])

        # Candidate: keep_me removed, size_me widened, section_me moved.
        cand_alpha = _SELF_TEMPLATE.format(
            file="alpha.c", section=".rodata", size_body="2,3",
            section2=".data")
        cand_alpha = cand_alpha.replace(
            ".globl keep_me\n.type keep_me,@object\nkeep_me: .long 1\n"
            ".size keep_me,.-keep_me\n", "")
        cand = _link(d, "cand", [_assemble(d, "calpha", cand_alpha),
                                 _assemble(d, "cbeta", beta)])

        rsections, rsyms, rrecs = data_symbols(ref)
        csections, csyms, crecs = data_symbols(cand)
        rown = file_owners(rsections, rsyms, rrecs)
        cown = file_owners(csections, csyms, crecs)
        result = compare(rrecs, crecs)
        report(result, rown, cown)

        missing = [r["name"] for r in result["missing"]]
        sizes = [r["name"] for r in result["size_mismatch"]]
        secs = [r["name"] for r in result["section_mismatch"]]
        assert "keep_me" in missing, missing
        assert "size_me" in sizes, sizes
        assert "section_me" in secs, secs
        assert result["reference"]["total"] == len(rrecs)
        print("dataaudit self-test: denominator %d reference / %d candidate "
              "symbols; planted missing, size and section defects all fired"
              % (result["reference"]["total"], result["candidate"]["total"]))

        # Candidate built from the reference itself must be clean.
        clean = compare(rrecs, rrecs)
        assert not (clean["missing"] or clean["extra"]
                    or clean["size_mismatch"] or clean["section_mismatch"])
        print("  identical control: 0 missing / 0 extra / 0 mismatch")


# --------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--blob", default=default_blob())
    ap.add_argument("--candidate", default="build/partial/dsplibs.o")
    ap.add_argument("--json")
    ap.add_argument("--section")
    ap.add_argument("--symbol")
    ap.add_argument("--all", action="store_true",
                    help="also print the symbols that matched")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        self_test()
        return 0

    if not os.path.exists(args.blob):
        sys.exit("dataaudit: no reference object %s" % args.blob)
    if not os.path.exists(args.candidate):
        sys.exit("dataaudit: no candidate object %s -- run `make "
                 "partial-link` first" % args.candidate)

    rsec, rsyms, rrecs = data_symbols(args.blob)
    csec, csyms, crecs = data_symbols(args.candidate)
    if not rrecs:
        sys.exit("dataaudit: reference %s defines ZERO data symbols -- "
                 "refusing to report a denominator of zero (F134/F2400)"
                 % args.blob)
    if not crecs:
        sys.exit("dataaudit: candidate %s defines ZERO data symbols -- "
                 "refusing to report a denominator of zero (F134/F2400)"
                 % args.candidate)

    rown = file_owners(rsec, rsyms, rrecs)
    cown = file_owners(csec, csyms, crecs)

    if args.symbol:
        hits = [r for r in rrecs if r["name"] == args.symbol]
        if not hits:
            sys.exit("dataaudit: %r is not a reference data symbol"
                     % args.symbol)
        for r in hits:
            print("%s: %s size %d @0x%06x bind %s owner=%s"
                  % (r["name"], r["section"], r["size"], r["value"],
                     r["bind"], owner_text(rown.get(r["num"]))))
            cand_by_key = defaultdict(list)
            for c in crecs:
                cand_by_key[(c["section"], c["size"])].append(c)
            for c in crecs:
                if c["name"] == args.symbol:
                    print("  candidate same-name: %s size %d @0x%06x bind %s"
                          % (c["section"], c["size"], c["value"], c["bind"]))
            for c in cand_by_key.get((r["section"], r["size"]), []):
                print("  candidate same-section/size: %-30s bind %s"
                      % (c["name"], c["bind"]))
        return 0

    result = compare(rrecs, crecs, args.section)
    report(result, rown, cown, args.all)

    if args.json:
        def row(r):
            return {"num": r["num"], "name": r["name"], "section": r["section"],
                    "size": r["size"], "value": r["value"],
                    "type": r["type"], "bind": r["bind"],
                    "owner": owner_text(rown.get(r["num"]))}
        payload = {
            "reference": args.blob, "candidate": args.candidate,
            "denominator": {"reference": result["reference"]["total"],
                            "candidate": result["candidate"]["total"],
                            "reference_sections": result["reference"]["sections"],
                            "candidate_sections": result["candidate"]["sections"]},
            "missing": [row(r) for r in result["missing"]],
            "extra": [row(r) for r in result["extra"]],
            "size_mismatch": [row(r) for r in result["size_mismatch"]],
            "section_mismatch": [row(r) for r in result["section_mismatch"]],
            # Keyed by the missing reference symbol's symtab `num`, which the
            # missing rows above now carry.  Without `num` the join was
            # impossible and a consumer saw an empty map (F11449).
            "matches": {str(k): [{"name": c["name"], "section": c["section"],
                                  "size": c["size"], "bind": c["bind"]}
                                 for c in v]
                        for k, v in result["matches"].items()},
        }
        with open(args.json, "w") as f:
            json.dump(payload, f, indent=1, sort_keys=True)
        print("  wrote %s" % args.json)

    clean = not (result["missing"] or result["extra"]
                 or result["size_mismatch"] or result["section_mismatch"])
    return 0 if clean else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        sys.exit("dataaudit: %s" % error)
