#!/usr/bin/env python3
"""
Attribute every .text function in dsplibs.o to its original translation unit.

Why this is needed
------------------
tumap.py recovers hard .text extents for only 19 of the 281 translation units;
the other 262 are known only as members of a shared address bracket.  That is
useless when you need to know which of the 73 V90*.cpp files a given function
came from, and per-TU attribution is the whole basis of a module-by-module
reconstruction.

What does NOT work
------------------
An earlier version of this tool tried to vote using relocations: a function
references its own TU's data, so follow the reference and read off the owner.
It scored 33% against ground truth.  The reason is instructive and worth
recording: the data sections have only 38/33/13 anchored TUs, so resolving a
data address yields the enclosing *bracket*, not the TU.  `ld -r` genuinely
discarded the per-input section boundaries - that information is gone, and no
amount of relocation chasing brings it back.

What does work
--------------
Smart Link's naming is highly systematic, and the TU filenames survived:

  * C++  - the mangled class name is the source filename.
           `_ZN12V90Equalizer5resetEv`  ->  class V90Equalizer  ->  V90Equalizer.cpp
  * C    - the symbol is prefixed by the filename stem.
           `fpm_sdm_init`  ->  fpm_sdm.c        `B103prc_run` -> B103prc.c

That alone attributes ~74% of functions.  Two further passes close most of the
gap:

  * Bracket filter - a name match is only accepted if the candidate TU's
    address bracket actually contains the function.  This kills coincidental
    prefix matches (e.g. `V32.c` matching a `V32bis`-era symbol in a distant
    bracket).
  * Contiguity fill - a TU's functions are contiguous in .text, so a run of
    unattributed functions bounded by two confident functions of the *same*
    TU belongs to that TU.  Runs bounded by two *different* TUs are left
    ambiguous and reported as `A|B` rather than guessed.

Evidence class is reported per function; name matches remain inferences.
Ground truth (the TUs with surviving local .text symbols) is held out and
checked; `--verify` prints the score and its coverage.  Duplicate symbol names
use occurrence keys with symtab index/type/section/size metadata in JSON.

Usage:
    tuattrib.py <dsplibs.o> [--verify] [--json out.json] [--md out.md]
    tuattrib.py --self-test
"""

import argparse
import collections
import json
import re
import subprocess
import sys

import elfinfo
import tumap


def demangle(names):
    """Batch-demangle through c++filt; plain C names pass through unchanged."""
    if not names:
        return []
    out = subprocess.run(["c++filt", "-p"], input="\n".join(names),
                         capture_output=True, text=True).stdout.splitlines()
    return out if len(out) == len(names) else list(names)


def cpp_class(demangled):
    """Leading `Class::` of a demangled C++ name, else None.

    Handles the common shapes: `V90Equalizer::reset()`, and constructors /
    destructors which demangle the same way.  Templates such as
    `GenericIIR<float, double>::process(...)` yield the bare template name,
    which is what the filename uses.
    """
    m = re.match(r"^(?:[\w\s:*&]+?\s)?([A-Za-z_]\w*)(?:<[^>]*>)?::", demangled)
    return m.group(1) if m else None


def build(obj):
    sections = elfinfo.read_sections(obj)
    syms = elfinfo.read_symbols(obj)
    text_size = next(s.size for s in sections.values() if s.name == ".text")

    tumeta = tumap.build_map(obj)["tus"]
    # stem -> [TU record]; repeated FILE spellings retain separate occurrences
    stems = collections.defaultdict(list)
    for key, v in tumeta.items():
        stems[v["file"].rsplit(".", 1)[0]].append(v)

    # Symbol names are not identities: separate FILE occurrences routinely
    # contain identically named static functions (even identical-sized ones).
    # Preserve each symtab occurrence and its type/section/size in the report.
    symbols = sorted((s for s in syms
                      if elfinfo.section_name(sections, s.ndx) == ".text"
                      and s.type == "FUNC" and s.size > 0),
                     key=lambda s: (s.value, s.num))
    counts = collections.Counter(s.name for s in symbols)
    reserved = {s.name for s in symbols}
    records, key_by_num, funcs = {}, {}, []
    for s in symbols:
        key = s.name
        if counts[s.name] > 1:
            key = "%s#sym%d" % (s.name, s.num)
            while key in reserved:
                key += "#"
        reserved.add(key)
        key_by_num[s.num] = key
        records[key] = dict(s._asdict(), section=elfinfo.section_name(sections, s.ndx))
        funcs.append((s.value, s.size, key))
    dem = demangle([s.name for s in symbols])

    # --- pass 1: name match, filtered by the address bracket -------------
    attrib = {}            # symbol occurrence key -> (TU key, confidence)
    for (addr, _size, key), d in zip(funcs, dem):
        raw = records[key]["name"]
        cands = []
        cls = cpp_class(d) if raw.startswith("_Z") else None
        if cls and cls in stems:
            cands = [(t, "class") for t in stems[cls]]
        if not cands:
            # Longest stem that prefixes the symbol wins: `V32mod` beats `V32`.
            # Case-sensitive first - Smart Link uses case to distinguish TUs
            # (`v32.c` is the datapump wrapper, `V32mod.c` is the modulator),
            # so a case-blind match silently assigns one to the other.
            matches = [s for s in stems if raw.startswith(s)]
            if not matches:
                matches = [s for s in stems if raw.lower().startswith(s.lower())]
            if matches:
                best = max(matches, key=len)
                cands = [(t, "prefix") for t in stems[best]]
        # Keep only candidates whose bracket actually contains this address.
        inside = [(t, how) for t, how in cands if t["lo"] <= addr < t["hi"]]
        if len(inside) == 1:
            attrib[key] = (inside[0][0]["key"], inside[0][1])
        elif inside:
            attrib[key] = ("|".join(t["key"] for t, _h in inside), "ambig-stem")
        elif len(cands) == 1 and cands[0][0]["kind"] != "exact":
            # Name is unambiguous but falls outside the interpolated bracket.
            # For an unanchored TU the bracket is only an approximation, so
            # trust the name and flag the weaker evidence.  When the TU has
            # *exact* extents the bracket is authoritative and a name match
            # outside it is simply wrong - e.g. `v32_data` prefix-matches the
            # wrapper `v32.c`, whose real extent is 0x4560-0x4c30, while the
            # function lives at 0x82xxx in `V32mod.c`.  Reject those.
            attrib[key] = (cands[0][0]["key"], "name-only")

    # --- pass 2: contiguity fill ------------------------------------------
    ordered = [(a, n) for a, _s, n in funcs]
    known = [(i, attrib[n][0]) for i, (_a, n) in enumerate(ordered)
             if n in attrib and attrib[n][1] in ("class", "prefix")]
    filled = 0
    for (i1, tu1), (i2, tu2) in zip(known, known[1:]):
        if i2 - i1 <= 1:
            continue
        gap = ordered[i1 + 1:i2]
        if tu1 == tu2:
            for _a, n in gap:
                if n in attrib:
                    continue
                attrib[n] = (tu1, "fill")
                filled += 1
        else:
            for _a, n in gap:
                if n in attrib:
                    continue
                attrib[n] = ("%s|%s" % (tu1, tu2), "ambiguous")

    # --- ground truth -----------------------------------------------------
    truth, cur, seq = {}, None, 0
    tu_by_seq = {v["seq"]: k for k, v in tumeta.items()}
    for s in syms:
        if s.type == "FILE":
            cur = None
            if elfinfo.is_source_file(s.name):
                cur = tu_by_seq[seq]
                seq += 1
        elif (cur and s.bind == "LOCAL" and s.type == "FUNC"
              and s.num in key_by_num):
            truth[key_by_num[s.num]] = cur

    return {"funcs": funcs, "attrib": attrib, "truth": truth,
            "text_size": text_size, "filled": filled, "symbols": records}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("obj", nargs="?")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--json")
    ap.add_argument("--md")
    args = ap.parse_args()

    if args.self_test:
        from tu_selftest import run
        run()
        return
    if not args.obj:
        ap.error("obj is required unless --self-test is given")

    b = build(args.obj)
    funcs, attrib, truth = b["funcs"], b["attrib"], b["truth"]

    conf = collections.Counter(c for _tu, c in attrib.values())
    resolved = {k: v for k, v in attrib.items()
                if v[1] not in ("ambiguous", "ambig-stem")}
    # Naming is evidence for a candidate, not proof of original ownership.
    firm = {k: v for k, v in attrib.items()
             if v[1] in ("class", "prefix")}

    print("functions with size : %d" % len(funcs))
    print("name-derived        : %d/%d  (%.1f%%)  - inferred candidates"
          % (len(firm), len(funcs), 100.0 * len(firm) / len(funcs) if funcs else 0))
    for how in ("class", "prefix"):
        if conf[how]:
            print("    %-12s %d" % (how, conf[how]))
    print("provisional         : %d  - inference only, do not rely on"
          % (conf["name-only"] + conf["fill"]))
    for how in ("name-only", "fill"):
        if conf[how]:
            print("    %-12s %d" % (how, conf[how]))
    print("ambiguous (A|B)     : %d" % (conf["ambiguous"] + conf["ambig-stem"]))
    print("unattributed        : %d" % (len(funcs) - len(attrib)))

    checked = {k: v for k, v in truth.items() if k in resolved}
    agree = sum(1 for k, v in checked.items() if resolved[k][0] == v)
    print("verification coverage: %d/%d local occurrences; %d/%d functions"
          % (len(checked), len(truth), len(checked), len(funcs)))
    if checked:
        print("verify vs ground truth: %d/%d agree (%.0f%%)"
              % (agree, len(checked), 100.0 * agree / len(checked)))
        # Agreement on the held-out locals does not prove global ownership.
        per = collections.defaultdict(lambda: [0, 0])
        for k, v in checked.items():
            how = resolved[k][1]
            per[how][1] += 1
            if resolved[k][0] == v:
                per[how][0] += 1
        for how in ("class", "prefix", "ambig-stem", "name-only", "fill"):
            if per[how][1]:
                ok, n = per[how]
                print("    %-12s %d/%d (%.0f%%)" % (how, ok, n, 100.0 * ok / n))
        for k, v in sorted(checked.items()):
            if resolved[k][0] != v:
                print("    MISMATCH %-30s truth=%-16s got=%s (%s)"
                      % (k, v, resolved[k][0], resolved[k][1]))

    tus = collections.Counter(tu for tu, c in resolved.values())
    print("distinct TUs identified: %d" % len(tus))

    if args.json:
        with open(args.json, "w") as f:
            json.dump({"attrib": {k: {"tu": v[0], "how": v[1]}
                                  for k, v in attrib.items()},
                       "symbols": b["symbols"],
                       "functions": len(funcs), "local_truth": len(truth),
                       "verified": len(checked), "agree": agree},
                      f, indent=1, sort_keys=True)
        print("wrote %s" % args.json)

    if args.md:
        addr_of = {n: a for a, _s, n in funcs}
        by_tu = collections.defaultdict(list)
        for fn, (tu, how) in attrib.items():
            by_tu[tu].append((addr_of[fn], fn, how))
        with open(args.md, "w") as f:
            f.write("# dsplibs.o function attribution\n\n")
            f.write("Generated by `tools/tuattrib.py`.\n\n")
            f.write("%d of %d sized functions attributed to %d translation "
                    "units; %d ambiguous, %d unattributed.\n"
                    % (len(resolved), len(funcs), len(tus),
                       conf["ambiguous"] + conf["ambig-stem"], len(funcs) - len(attrib)))
            f.write("Ground-truth agreement: %d/%d.\n\n" % (agree, len(checked)))
            f.write("Verification coverage: %d/%d local occurrences; %d/%d "
                    "sized functions.\n\n" %
                    (len(checked), len(truth), len(checked), len(funcs)))
            f.write("`how` values: `class` / `prefix` = symbol name matched "
                    "the TU filename (inference, not proof);\n"
                    "`ambig-stem` = multiple FILE occurrences match;\n"
                    "`fill` = inferred from contiguity "
                    "between two confident neighbours;\n`name-only` = name "
                    "matched but address fell outside the interpolated "
                    "bracket.\n\n")
            for tu in sorted(by_tu):
                fns = sorted(by_tu[tu])
                f.write("## `%s` (%d)\n\n`0x%06x` - `0x%06x`\n\n"
                        % (tu, len(fns), fns[0][0], fns[-1][0]))
                for a, n, how in fns:
                    f.write("- `0x%06x` %s _(%s)_\n" % (a, n, how))
                f.write("\n")
        print("wrote %s" % args.md)


if __name__ == "__main__":
    main()
