#!/usr/bin/env python3
"""Compare link-relevant structure of two relocatable ELF objects.

Raw ELF bytes are reported but do not decide the semantic result: debug data,
string-table packing and file offsets may differ without changing code or data.
The semantic comparison covers allocated sections and their order/metadata,
positioned PROGBITS contents, NOBITS sizes, normalized relocation records, and
defined symbol records/order.  REL addends remain visible in section contents.

The default is a non-failing census because the reconstruction is not exact
yet.  Use --require-exact for a strict final-object gate and --json to retain a
machine-readable before/after measurement for an ordering experiment.
"""

import argparse
from collections import Counter
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


SECTION_RE = re.compile(
    r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+"
    r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
    r"(\S*)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
RELOC_SECTION_RE = re.compile(r"Relocation section '([^']+)'.*contains")
RELOC_RE = re.compile(r"\s*([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+(\S+)\s+"
                      r"[0-9a-fA-F]+\s+(.+?)\s*$")


def run(*args):
    return subprocess.run(args, check=True, capture_output=True,
                          text=True).stdout


def digest(path):
    value = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def validate_elf(path):
    header = run("readelf", "-h", path)
    required = {
        "Class:": "ELF32",
        "Data:": "2's complement, little endian",
        "Type:": "REL (Relocatable file)",
        "Machine:": "Intel 80386",
    }
    observed = {}
    for line in header.splitlines():
        stripped = line.strip()
        for label in required:
            if stripped.startswith(label):
                observed[label] = stripped[len(label):].strip()
    wrong = ["%s expected %r, got %r" %
             (label, wanted, observed.get(label))
             for label, wanted in required.items()
             if observed.get(label) != wanted]
    if wrong:
        raise ValueError("%s is not an ELF32 little-endian i386 ET_REL: %s" %
                         (path, "; ".join(wrong)))


def sections(path):
    result = []
    for line in run("readelf", "-SW", path).splitlines():
        match = SECTION_RE.match(line)
        if not match:
            continue
        (index, name, kind, address, offset, size, entsize, flags, link,
         info, align) = match.groups()
        # SHF_ALLOC is the semantic payload.  The non-allocated GNU-stack note
        # is retained separately because its X flag changes link policy.
        if not ("A" in flags and kind in ("PROGBITS", "NOBITS")) and \
                name != ".note.GNU-stack":
            continue
        result.append({
            "index": int(index), "name": name, "type": kind,
            "address": int(address, 16), "offset": int(offset, 16),
            "size": int(size, 16), "entsize": int(entsize, 16),
            "flags": flags, "link": int(link), "info": int(info),
            "align": int(align),
        })
    names = [entry["name"] for entry in result]
    duplicate = [name for name, count in Counter(names).items() if count > 1]
    if duplicate:
        raise ValueError("duplicate compared section name(s): %s" %
                         ", ".join(sorted(duplicate)))
    return result


def contents(path, entries):
    raw = Path(path).read_bytes()
    result = {}
    for entry in entries:
        if entry["type"] != "PROGBITS":
            continue
        lo = entry["offset"]
        hi = lo + entry["size"]
        if hi > len(raw):
            raise ValueError("section %s extends beyond EOF" % entry["name"])
        result[entry["name"]] = raw[lo:hi]
    return result


def relocations(path):
    result = []
    current = None
    for line in run("readelf", "-rW", path).splitlines():
        match = RELOC_SECTION_RE.search(line)
        if match:
            current = match.group(1)
            continue
        match = RELOC_RE.match(line)
        if current and match:
            offset, kind, target = match.groups()
            result.append((current, int(offset, 16), kind, target.strip()))
    return result


def symbols(path, entries):
    owners = {str(entry["index"]): entry["name"] for entry in entries}
    result = []
    for line in run("readelf", "-sW", path).splitlines():
        fields = line.split(None, 7)
        if len(fields) < 7 or not fields[0].endswith(":"):
            continue
        try:
            int(fields[0][:-1])
            value = int(fields[1], 16)
            size = int(fields[2])
        except ValueError:
            continue
        kind, bind, visibility, ndx = fields[3:7]
        name = fields[7] if len(fields) == 8 else ""
        if ndx == "UND" or kind not in ("FILE", "FUNC", "OBJECT", "NOTYPE"):
            continue
        result.append((kind, bind, visibility, owners.get(ndx, ndx),
                       value, size, name))
    return result


def load(path):
    validate_elf(path)
    entries = sections(path)
    if not entries:
        raise ValueError("%s has zero compared sections" % path)
    return {"path": str(path), "sha256": digest(path), "raw_size":
            os.path.getsize(path), "sections": entries,
            "contents": contents(path, entries),
            "relocations": relocations(path), "symbols": symbols(path, entries)}


def common(left, right):
    return sum((Counter(left) & Counter(right)).values())


def ordered(left, right):
    matcher = difflib.SequenceMatcher(a=left, b=right, autojunk=False)
    return sum(block.size for block in matcher.get_matching_blocks())


def first_difference(left, right):
    for index, (a, b) in enumerate(zip(left, right)):
        if a != b:
            return [index, a, b]
    if len(left) != len(right):
        index = min(len(left), len(right))
        return [index, left[index] if index < len(left) else None,
                right[index] if index < len(right) else None]
    return None


def compare(reference, candidate):
    ref_entries = reference["sections"]
    cand_entries = candidate["sections"]
    ref_by_name = {entry["name"]: entry for entry in ref_entries}
    cand_by_name = {entry["name"]: entry for entry in cand_entries}
    fields = ("name", "type", "flags", "address", "size", "align")
    ref_section_records = [tuple(entry[field] for field in fields)
                           for entry in ref_entries]
    cand_section_records = [tuple(entry[field] for field in fields)
                            for entry in cand_entries]

    reference_bytes = candidate_bytes = equal_bytes = exact_sections = 0
    for name, entry in ref_by_name.items():
        if entry["type"] != "PROGBITS":
            continue
        left = reference["contents"][name]
        right = candidate["contents"].get(name, b"")
        reference_bytes += len(left)
        equal_bytes += sum(a == b for a, b in zip(left, right))
        exact_sections += int(name in cand_by_name and left == right)
    candidate_bytes = sum(entry["size"] for entry in cand_entries
                          if entry["type"] == "PROGBITS")
    ref_nobits = sum(entry["size"] for entry in ref_entries
                     if entry["type"] == "NOBITS")
    cand_nobits = sum(entry["size"] for entry in cand_entries
                      if entry["type"] == "NOBITS")

    ref_reloc = reference["relocations"]
    cand_reloc = candidate["relocations"]
    ref_symbols = reference["symbols"]
    cand_symbols = candidate["symbols"]
    result = {
        "reference": {key: reference[key] for key in
                      ("path", "sha256", "raw_size")},
        "candidate": {key: candidate[key] for key in
                      ("path", "sha256", "raw_size")},
        "sections": {
            "reference": len(ref_entries), "candidate": len(cand_entries),
            "common_names": len(set(ref_by_name) & set(cand_by_name)),
            "exact_records": common(ref_section_records, cand_section_records),
            "ordered_matches": ordered(ref_section_records,
                                       cand_section_records),
            "exact_contents": exact_sections,
            "first_difference": first_difference(ref_section_records,
                                                   cand_section_records),
        },
        "contents": {
            "reference_bytes": reference_bytes,
            "candidate_bytes": candidate_bytes,
            "equal_positioned_bytes": equal_bytes,
            "different_or_missing_reference_bytes": reference_bytes-equal_bytes,
            "candidate_size_delta": candidate_bytes-reference_bytes,
            "reference_nobits_bytes": ref_nobits,
            "candidate_nobits_bytes": cand_nobits,
        },
        "relocations": {
            "reference": len(ref_reloc), "candidate": len(cand_reloc),
            "exact_records": common(ref_reloc, cand_reloc),
            "ordered_matches": ordered(ref_reloc, cand_reloc),
            "first_difference": first_difference(ref_reloc, cand_reloc),
        },
        "symbols": {
            "reference": len(ref_symbols), "candidate": len(cand_symbols),
            "exact_records": common(ref_symbols, cand_symbols),
            "ordered_matches": ordered(ref_symbols, cand_symbols),
            "first_difference": first_difference(ref_symbols, cand_symbols),
        },
    }
    result["raw_exact"] = reference["sha256"] == candidate["sha256"]
    result["exact"] = (ref_section_records == cand_section_records and
                       reference_bytes == candidate_bytes and
                       equal_bytes == reference_bytes and
                       ref_nobits == cand_nobits and
                       ref_reloc == cand_reloc and ref_symbols == cand_symbols)
    return result


def percent(value, total):
    return "%.1f%%" % (100.0 * value / total) if total else "n/a"


def shorten(value):
    value = repr(value)
    return value if len(value) < 150 else value[:147] + "..."


def report(result):
    sec = result["sections"]
    data = result["contents"]
    rel = result["relocations"]
    sym = result["symbols"]
    print("partial-link comparison")
    print("  sections      %d ref / %d candidate; %d shared names; %d exact; "
          "%d ordered matches" % (sec["reference"], sec["candidate"],
          sec["common_names"], sec["exact_records"], sec["ordered_matches"]))
    print("  contents      %d/%d positioned reference bytes (%s); candidate "
          "delta %+d; %d exact sections" % (data["equal_positioned_bytes"],
          data["reference_bytes"], percent(data["equal_positioned_bytes"],
          data["reference_bytes"]), data["candidate_size_delta"],
          sec["exact_contents"]))
    print("  NOBITS        %d ref bytes / %d candidate bytes" %
          (data["reference_nobits_bytes"], data["candidate_nobits_bytes"]))
    print("  relocations   %d/%d exact (%s); %d ordered matches; %d candidate" %
          (rel["exact_records"], rel["reference"], percent(rel["exact_records"],
          rel["reference"]), rel["ordered_matches"], rel["candidate"]))
    print("  symbols       %d/%d exact (%s); %d ordered matches; %d candidate" %
          (sym["exact_records"], sym["reference"], percent(sym["exact_records"],
          sym["reference"]), sym["ordered_matches"], sym["candidate"]))
    if not result["exact"]:
        print("  first section %s" % shorten(sec["first_difference"]))
        print("  first reloc   %s" % shorten(rel["first_difference"]))
        print("  first symbol  %s" % shorten(sym["first_difference"]))
    print("  verdict       %s (raw file %s)" %
          ("EXACT" if result["exact"] else "DIFFERENT",
           "EXACT" if result["raw_exact"] else "different"))


def assemble(directory, name, source):
    source_path = Path(directory) / (name + ".s")
    object_path = Path(directory) / (name + ".o")
    source_path.write_text(source)
    subprocess.run(["as", "--32", "-o", str(object_path), str(source_path)],
                   check=True, capture_output=True)
    return object_path


def self_test():
    if not shutil.which("as") or not shutil.which("readelf"):
        raise RuntimeError("self-test needs as and readelf")
    template = """.file \"fixture.s\"
.text
.globl first
.type first,@function
first: mov $CODE,%eax
ret
.size first,.-first
.globl second
.type second,@function
second: xor %eax,%eax
ret
.size second,.-second
.section .rodata
.globl constant
.type constant,@object
constant: .long DATA
.size constant,.-constant
.data
.globl pointer
.type pointer,@object
pointer: .long TARGET
.size pointer,.-pointer
.bss
.globl scratch
.type scratch,@object
scratch: .zero 4
.size scratch,.-scratch
.section .note.GNU-stack,"",@progbits
"""
    def variant(code="1", data="0x1234", target="outside_a"):
        return template.replace("CODE", code).replace("DATA", data).replace(
            "TARGET", target)
    with tempfile.TemporaryDirectory(prefix="partialcmp-") as directory:
        base = assemble(directory, "base", variant())
        same = Path(directory) / "same.o"
        shutil.copyfile(base, same)
        changed_code = assemble(directory, "code", variant(code="2"))
        changed_data = assemble(directory, "data", variant(data="0x5678"))
        changed_reloc = assemble(directory, "reloc",
                                 variant(target="outside_b"))
        changed_addend = assemble(directory, "addend",
                                  variant(target="outside_a+4"))
        changed_bss = assemble(directory, "bss",
                               variant().replace("scratch: .zero 4",
                                                 "scratch: .zero 8")
                               .replace(".size scratch,.-scratch",
                                        ".size scratch,.-scratch"))
        changed_stack = assemble(directory, "stack",
                                 variant().replace(
                                     '.section .note.GNU-stack,"",@progbits',
                                     '.section .note.GNU-stack,"x",@progbits'))
        swapped = variant().replace(
            ".globl first\n.type first,@function\nfirst: mov $1,%eax\nret\n"
            ".size first,.-first\n.globl second\n.type second,@function\n"
            "second: xor %eax,%eax\nret\n.size second,.-second",
            ".globl second\n.type second,@function\nsecond: xor %eax,%eax\n"
            "ret\n.size second,.-second\n.globl first\n.type first,@function\n"
            "first: mov $1,%eax\nret\n.size first,.-first")
        changed_order = assemble(directory, "order", swapped)
        original = load(base)
        exact = compare(original, load(same))
        code = compare(original, load(changed_code))
        data = compare(original, load(changed_data))
        reloc = compare(original, load(changed_reloc))
        addend = compare(original, load(changed_addend))
        bss = compare(original, load(changed_bss))
        stack = compare(original, load(changed_stack))
        order = compare(original, load(changed_order))
        assert exact["exact"]
        assert code["contents"]["equal_positioned_bytes"] < \
            code["contents"]["reference_bytes"]
        assert data["contents"]["equal_positioned_bytes"] < \
            data["contents"]["reference_bytes"]
        assert reloc["relocations"]["exact_records"] < \
            reloc["relocations"]["reference"]
        assert addend["contents"]["equal_positioned_bytes"] < \
            addend["contents"]["reference_bytes"]
        assert bss["sections"]["exact_records"] < \
            bss["sections"]["reference"]
        assert stack["sections"]["exact_records"] < \
            stack["sections"]["reference"]
        assert order["symbols"]["ordered_matches"] < \
            order["symbols"]["reference"]
        print("partialcmp self-test: 8 comparisons, 8 passed")
        print("  accepted exact copy; detected planted code, data, NOBITS, "
              "stack-policy, relocation target/addend and symbol-order changes")
        print("  denominator: %d allocated bytes, %d relocations, %d symbols" %
              (exact["contents"]["reference_bytes"],
               exact["relocations"]["reference"],
               exact["symbols"]["reference"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", nargs="?")
    parser.add_argument("candidate", nargs="?")
    parser.add_argument("--json", metavar="PATH")
    parser.add_argument("--require-exact", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.reference or not args.candidate:
        parser.error("reference and candidate are required")
    result = compare(load(args.reference), load(args.candidate))
    report(result)
    if args.json:
        rendered = json.dumps(result, indent=1, sort_keys=True) + "\n"
        if args.json == "-":
            print(rendered, end="")
        else:
            Path(args.json).write_text(rendered)
            print("  wrote         %s" % args.json)
    return int(args.require_exact and not result["exact"])


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        sys.exit("partialcmp: %s" % error)
