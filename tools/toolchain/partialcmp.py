#!/usr/bin/env python3
"""Compare link-relevant structure of two relocatable ELF objects.

The semantic comparison covers ELF ABI/flags, allocated sections and their
order/metadata, positioned file-backed contents, NOBITS sizes, normalized
relocation records/section metadata, and defined/undefined symbol records/order.
REL addends remain visible in section contents. Debug data, .comment, string
packing and file offsets may differ under this comparison.

The default is a non-failing census because the reconstruction is not exact
yet. Use --require-exact for semantic equality, --require-raw-exact for literal
file identity, and --json to retain a machine-readable census. Raw identity
requires equal bytes, including headers, metadata and non-allocated sections.
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
import struct
import subprocess
import sys
import tempfile


SECTION_RE = re.compile(
    r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+"
    r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
    r"(\S*)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
RELOC_SECTION_RE = re.compile(r"Relocation section '([^']+)'.*contains\s+(\d+)\s+entr")
RELOC_RE = re.compile(r"\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+(R_386_\S+)\s+"
                      r"[0-9a-fA-F]+(?:\s+(.+?))?\s*$")


def run(*args):
    return subprocess.run(args, check=True, capture_output=True,
                          text=True, env=dict(os.environ, LC_ALL="C")).stdout


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
    raw = Path(path).read_bytes()
    if len(raw) < 52:
        raise ValueError("truncated ELF32 header in %s" % path)
    return (raw[6], raw[7], raw[8], struct.unpack_from("<I", raw, 20)[0],
            struct.unpack_from("<I", raw, 36)[0])


def sections(path, include_nonallocated=False):
    result = []
    for line in run("readelf", "-SW", path).splitlines():
        match = SECTION_RE.match(line)
        if not match:
            continue
        (index, name, kind, address, offset, size, entsize, flags, link,
         info, align) = match.groups()
        # SHF_ALLOC is the semantic payload.  The non-allocated GNU-stack note
        # is retained separately because its X flag changes link policy.
        if not include_nonallocated and "A" not in flags and name != ".note.GNU-stack":
            continue
        result.append({
            "index": int(index), "name": name, "type": kind,
            "address": int(address, 16), "offset": int(offset, 16),
            "size": int(size, 16), "entsize": int(entsize, 16),
            "flags": flags, "link": int(link), "info": int(info),
            "align": int(align),
        })
    return result


def contents(path, entries):
    raw = Path(path).read_bytes()
    result = {}
    for entry in entries:
        if entry["type"] == "NOBITS":
            continue
        lo = entry["offset"]
        hi = lo + entry["size"]
        if hi > len(raw):
            raise ValueError("section %s extends beyond EOF" % entry["name"])
        result[entry["name"]] = raw[lo:hi]
    return result


def relocations(path, targets):
    result = []
    current = None
    expected = observed = 0
    for line in run("readelf", "-rW", path).splitlines():
        match = RELOC_SECTION_RE.search(line)
        if match:
            if observed != expected:
                raise ValueError("parsed %d/%d relocation records in %s:%s" %
                                 (observed, expected, path, current))
            current = match.group(1)
            expected, observed = int(match.group(2)), 0
            continue
        match = RELOC_RE.match(line)
        if current and match:
            offset, info, kind, target = match.groups()
            index = int(info, 16) >> 8  # ELF32_R_SYM
            if index not in targets:
                raise ValueError("relocation references missing symbol %d in %s" %
                                 (index, path))
            result.append((current, int(offset, 16), kind,
                           (target or "").strip(), targets[index]))
            observed += 1
    if observed != expected:
        raise ValueError("parsed %d/%d relocation records in %s:%s" %
                         (observed, expected, path, current))
    return result


def symbols(path, entries):
    owners = {str(entry["index"]): entry["name"] for entry in entries}
    result = []
    targets = {}
    for line in run("readelf", "-sW", path).splitlines():
        fields = line.split(None, 7)
        if len(fields) < 7 or not fields[0].endswith(":"):
            continue
        try:
            index = int(fields[0][:-1])
            value = int(fields[1], 16)
            size = int(fields[2])
        except ValueError:
            continue
        kind, bind, visibility, ndx = fields[3:7]
        name = fields[7] if len(fields) == 8 else ""
        record = (kind, bind, visibility, owners.get(ndx, ndx), value, size, name)
        targets[index] = record
        # Section records identify relocation destinations but their table
        # indices/order can change with non-allocated debug sections. Null is
        # bookkeeping; every other symbol kind, including TLS and UND, counts.
        if index == 0 or kind == "SECTION":
            continue
        result.append(record)
    return result, targets


def section_metadata(entries, owners):
    """Retain link-relevant fields, naming section references across packing."""
    result = []
    for entry in entries:
        record = dict(entry)
        for field in ("link", "info"):
            is_section = field == "link" or entry["type"] in ("REL", "RELA") or \
                "I" in entry["flags"]
            if is_section and entry[field]:
                if entry[field] not in owners:
                    raise ValueError("section %s has invalid sh_%s %d" %
                                     (entry["name"], field, entry[field]))
                record[field] = owners[entry[field]]
        result.append(record)
    return result


def section_groups(path, entries, owners, targets):
    """Normalize COMDAT/group ownership without hiding its signature or flags."""
    raw = Path(path).read_bytes()
    result = []
    for entry in entries:
        if entry["type"] != "GROUP":
            continue
        start, size = entry["offset"], entry["size"]
        if entry["entsize"] != 4 or size < 4 or size % 4 or start + size > len(raw):
            raise ValueError("invalid ELF32 section group in %s" % path)
        words = struct.unpack_from("<%dI" % (size // 4), raw, start)
        if entry["info"] not in targets or any(i not in owners for i in words[1:]):
            raise ValueError("section group has invalid signature/member in %s" % path)
        result.append((entry["name"], entry["flags"], entry["address"], size,
                       entry["align"], entry["entsize"], owners.get(entry["link"]),
                       targets[entry["info"]], words[0],
                       tuple(owners[i] for i in words[1:])))
    return result


def load(path):
    abi = validate_elf(path)
    all_entries = sections(path, include_nonallocated=True)
    if sum(entry["type"] in ("SYMTAB", "DYNSYM") for entry in all_entries) > 1:
        raise ValueError("multiple symbol tables are not supported in %s" % path)
    owners = {entry["index"]: entry["name"] for entry in all_entries}
    entries = section_metadata([entry for entry in all_entries
                                if "A" in entry["flags"] or
                                entry["name"] == ".note.GNU-stack"], owners)
    if not entries:
        raise ValueError("%s has zero compared sections" % path)
    names = [entry["name"] for entry in entries]
    duplicate = [name for name, count in Counter(names).items() if count > 1]
    if duplicate:
        raise ValueError("duplicate compared section name(s): %s" %
                         ", ".join(sorted(duplicate)))
    symbol_records, targets = symbols(path, all_entries)
    relocation_sections = section_metadata([entry for entry in all_entries
                                           if entry["type"] in ("REL", "RELA")], owners)
    return {"path": str(path), "sha256": digest(path), "raw_size":
            os.path.getsize(path), "sections": entries,
            "abi": abi, "contents": contents(path, entries),
            "groups": section_groups(path, all_entries, owners, targets),
            "relocation_sections": relocation_sections,
            "relocations": relocations(path, targets), "symbols": symbol_records}


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
    fields = ("name", "type", "flags", "address", "size", "align",
              "entsize", "link", "info")
    ref_section_records = [tuple(entry[field] for field in fields)
                           for entry in ref_entries]
    cand_section_records = [tuple(entry[field] for field in fields)
                            for entry in cand_entries]

    reference_bytes = candidate_bytes = equal_bytes = exact_sections = 0
    contents_by_section = {}
    for name, entry in ref_by_name.items():
        if entry["type"] == "NOBITS":
            continue
        left = reference["contents"][name]
        right = candidate["contents"].get(name, b"")
        reference_bytes += len(left)
        equal_bytes += sum(a == b for a, b in zip(left, right))
        exact_sections += int(name in cand_by_name and left == right)
        contents_by_section[name] = {
            "reference": len(left), "candidate": len(right),
            "equal": sum(a == b for a, b in zip(left, right)),
        }
    # A file-backed section the candidate has and the reference does not is
    # extra apparatus and worth naming in the per-section census.
    for name, entry in cand_by_name.items():
        if entry["type"] != "NOBITS" and name not in contents_by_section:
            right = candidate["contents"][name]
            contents_by_section[name] = {"reference": 0, "candidate":
                                         len(right), "equal": 0}
    candidate_bytes = sum(entry["size"] for entry in cand_entries
                          if entry["type"] != "NOBITS")
    ref_nobits = sum(entry["size"] for entry in ref_entries
                     if entry["type"] == "NOBITS")
    cand_nobits = sum(entry["size"] for entry in cand_entries
                      if entry["type"] == "NOBITS")

    ref_reloc = reference["relocations"]
    cand_reloc = candidate["relocations"]
    ref_symbols = reference["symbols"]
    cand_symbols = candidate["symbols"]
    ref_reloc_sections = [tuple(entry[field] for field in fields)
                          for entry in reference["relocation_sections"]]
    cand_reloc_sections = [tuple(entry[field] for field in fields)
                           for entry in candidate["relocation_sections"]]
    result = {
        "comparison_schema": 2,
        "reference": {key: reference[key] for key in
                      ("path", "sha256", "raw_size")},
        "candidate": {key: candidate[key] for key in
                      ("path", "sha256", "raw_size")},
        "elf_abi": {"reference": reference["abi"], "candidate": candidate["abi"],
                    "exact": reference["abi"] == candidate["abi"]},
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
            "by_section": contents_by_section,
        },
        "relocations": {
            "reference": len(ref_reloc), "candidate": len(cand_reloc),
            "exact_records": common(ref_reloc, cand_reloc),
            "ordered_matches": ordered(ref_reloc, cand_reloc),
            "first_difference": first_difference(ref_reloc, cand_reloc),
        },
        "relocation_sections": {
            "reference": len(ref_reloc_sections), "candidate": len(cand_reloc_sections),
            "exact_records": common(ref_reloc_sections, cand_reloc_sections),
            "first_difference": first_difference(ref_reloc_sections, cand_reloc_sections),
        },
        "groups": {
            "reference": len(reference["groups"]), "candidate": len(candidate["groups"]),
            "exact_records": common(reference["groups"], candidate["groups"]),
            "first_difference": first_difference(reference["groups"], candidate["groups"]),
        },
        "symbols": {
            "reference": len(ref_symbols), "candidate": len(cand_symbols),
            "exact_records": common(ref_symbols, cand_symbols),
            "ordered_matches": ordered(ref_symbols, cand_symbols),
            "first_difference": first_difference(ref_symbols, cand_symbols),
        },
    }
    result["raw_exact"] = (reference["sha256"] == candidate["sha256"] and
                           Path(reference["path"]).read_bytes() ==
                           Path(candidate["path"]).read_bytes())
    result["exact"] = (reference["abi"] == candidate["abi"] and
                       ref_section_records == cand_section_records and
                       ref_reloc_sections == cand_reloc_sections and
                       reference["groups"] == candidate["groups"] and
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


def report(result, per_section=False):
    sec = result["sections"]
    data = result["contents"]
    rel = result["relocations"]
    relsec = result["relocation_sections"]
    groups = result["groups"]
    sym = result["symbols"]
    print("partial-link comparison")
    print("  ELF ABI       %s (ident version, OS ABI, ABI version, version, flags)" %
          ("EXACT" if result["elf_abi"]["exact"] else "DIFFERENT"))
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
    print("  reloc sections %d/%d exact; %d candidate" %
          (relsec["exact_records"], relsec["reference"], relsec["candidate"]))
    print("  groups        %d/%d exact; %d candidate" %
          (groups["exact_records"], groups["reference"], groups["candidate"]))
    print("  symbols       %d/%d exact (%s); %d ordered matches; %d candidate" %
          (sym["exact_records"], sym["reference"], percent(sym["exact_records"],
          sym["reference"]), sym["ordered_matches"], sym["candidate"]))
    if not result["exact"]:
        print("  first section %s" % shorten(sec["first_difference"]))
        print("  first reloc   %s" % shorten(rel["first_difference"]))
        print("  first relsec  %s" % shorten(relsec["first_difference"]))
        print("  first group   %s" % shorten(groups["first_difference"]))
        print("  first symbol  %s" % shorten(sym["first_difference"]))
    if per_section:
        # The aggregate "contents" line cannot say WHICH section is short;
        # recovering missing data is a per-section question, so name the
        # sections whose reference/candidate sizes disagree, largest first.
        rows = sorted(data["by_section"].items(),
                      key=lambda kv: abs(kv[1]["reference"] - kv[1]["candidate"]),
                      reverse=True)
        print("  per-section   %d file-backed sections; size-delta and positioned match"
              % len(rows))
        for name, entry in rows:
            delta = entry["candidate"] - entry["reference"]
            if delta == 0 and entry["equal"] == entry["reference"]:
                continue
            print("    %-22s ref %-7d cand %-7d delta %+d  equal %d/%d"
                  % (name, entry["reference"], entry["candidate"], delta,
                     entry["equal"], entry["reference"]))
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


def completion_self_test():
    """Exercise the public gates, including metadata omitted by the old census."""
    template = '''.file "completion.s"
.text
.globl entry
.type entry,@function
entry: mov $1,%eax
ret
.size entry,.-entry
.data
.long outside_a
.section .mergevals,"aM",@progbits,4
.long 1,2
.section .comment
.string "compiler original"
.section .note.GNU-stack,"",@progbits
'''
    cases = []
    with tempfile.TemporaryDirectory(prefix="partialcmp-completion-") as directory:
        base = assemble(directory, "base", template)
        def add(name, source, exact=False, reference=base):
            path = assemble(directory, name, source)
            cases.append((name, reference, path, exact))
            return path

        cases.append(("exact copy", base, base, True))
        add("changed data", template.replace(".long 1,2", ".long 9,2"))
        add("comment only", template.replace("compiler original", "compiler changed"),
            exact=True)
        add("merge entry size", template.replace("@progbits,4", "@progbits,8"))
        array = template + '\n.section .init_array,"aw",@init_array\n.long 1\n'
        array_base = add("added allocated INIT_ARRAY", array)
        add("changed INIT_ARRAY data", array.replace(".long 1\n", ".long 2\n"),
            reference=array_base)
        add("added undefined symbol", template + "\n.globl unused_external\n")
        add("undefined binding", template + "\n.weak outside_a\n")
        add("undefined type", template + "\n.type outside_a,@function\n")
        add("undefined visibility", template + "\n.hidden outside_a\n")
        linked = template + '\n.section .ordered,"ao",@progbits,.text\n.long 7\n'
        linked_base = assemble(directory, "linked_base", linked)
        add("section link target", linked.replace("@progbits,.text", "@progbits,.data"),
            reference=linked_base)

        # ELF32 header e_flags is independent of contents and symbols.
        flags = Path(directory) / "flags.o"
        raw = bytearray(base.read_bytes())
        struct.pack_into("<I", raw, 36, 1)
        flags.write_bytes(raw)
        cases.append(("ELF flags", base, flags, False))

        # sh_info chooses the relocated section, irrespective of its name.
        target = Path(directory) / "relocation_target.o"
        raw = bytearray(base.read_bytes())
        shoff = struct.unpack_from("<I", raw, 32)[0]
        shentsize = struct.unpack_from("<H", raw, 46)[0]
        shnum = struct.unpack_from("<H", raw, 48)[0]
        for i in range(shnum):
            at = shoff + i * shentsize
            if struct.unpack_from("<I", raw, at + 4)[0] == 9:  # SHT_REL
                struct.pack_into("<I", raw, at + 28, 1)  # .text
                break
        else:
            raise AssertionError("fixture has no SHT_REL")
        target.write_bytes(raw)
        cases.append(("relocation section target", base, target, False))

        # Keep both duplicate local symbol records unchanged; change only the
        # relocation's symbol index. A printed name cannot identify its target.
        duplicate = '''.file "duplicate.s"
.data
.globl slot1,slot2,pointer
.type slot1,@object
slot1: .long 1
.size slot1,.-slot1
.type slot2,@object
slot2: .long 2
.size slot2,.-slot2
pointer: .long TARGET
'''
        objects = []
        for name, destination in (("duplicate_left", "slot1"),
                                  ("duplicate_right", "slot2")):
            obj = assemble(directory, name, duplicate.replace("TARGET", destination))
            raw = bytearray(obj.read_bytes())
            shoff = struct.unpack_from("<I", raw, 32)[0]
            shentsize = struct.unpack_from("<H", raw, 46)[0]
            shnum = struct.unpack_from("<H", raw, 48)[0]
            for i in range(shnum):
                at = shoff + i * shentsize
                if struct.unpack_from("<I", raw, at + 4)[0] != 2:  # SHT_SYMTAB
                    continue
                start, size, strings = struct.unpack_from("<III", raw, at + 16)
                string_at = struct.unpack_from("<I", raw,
                                               shoff + strings * shentsize + 16)[0]
                last_local = 0
                for symbol in range(size // 16):
                    pos = start + symbol * 16
                    label_at = string_at + struct.unpack_from("<I", raw, pos)[0]
                    end = raw.index(0, label_at)
                    if bytes(raw[label_at:end]) in (b"slot1", b"slot2"):
                        raw[end - 1] = 0
                        raw[pos + 12] &= 15  # retain type, set STB_LOCAL
                        last_local = symbol
                struct.pack_into("<I", raw, at + 28, last_local + 1)
            obj.write_bytes(raw)
            objects.append(obj)
        cases.append(("duplicate-name relocation", *objects, False))

        grouped = template + '''
.section .group_one,"aG",@progbits,signature_one,comdat
.long 11
.section .group_two,"aG",@progbits,signature_two,comdat
.long 22
'''
        group_base = assemble(directory, "group_base", grouped)
        cases.append(("multiple section groups", group_base, group_base, True))
        group_flags = Path(directory) / "group_flags.o"
        raw = bytearray(group_base.read_bytes())
        shoff = struct.unpack_from("<I", raw, 32)[0]
        shentsize = struct.unpack_from("<H", raw, 46)[0]
        shnum = struct.unpack_from("<H", raw, 48)[0]
        for i in range(shnum):
            at = shoff + i * shentsize
            if struct.unpack_from("<I", raw, at + 4)[0] == 17:  # SHT_GROUP
                content = struct.unpack_from("<I", raw, at + 16)[0]
                struct.pack_into("<I", raw, content, 0)  # remove GRP_COMDAT
                break
        else:
            raise AssertionError("fixture has no SHT_GROUP")
        group_flags.write_bytes(raw)
        cases.append(("COMDAT group flags", group_base, group_flags, False))

        failures = []
        checks = 0
        for name, left, right, semantic_exact in cases:
            if name in ("duplicate-name relocation", "relocation section target",
                        "COMDAT group flags"):
                measured = compare(load(left), load(right))
                # These controls change metadata only. A detector failing on
                # incidental symbol or allocated-byte drift proves nothing.
                assert measured["symbols"]["exact_records"] == \
                    measured["symbols"]["reference"] == measured["symbols"]["candidate"]
                assert measured["contents"]["equal_positioned_bytes"] == \
                    measured["contents"]["reference_bytes"] == \
                    measured["contents"]["candidate_bytes"]
            for gate, expected in (("--require-exact", int(not semantic_exact)),
                                   ("--require-raw-exact", int(left != right))):
                command = [sys.executable, str(Path(__file__).resolve()),
                           str(left), str(right), gate]
                result = subprocess.run(command, capture_output=True, text=True)
                checks += 1
                passed = result.returncode == expected
                print("  %s %-31s %-19s exit %d expected %d" %
                      ("ok" if passed else "FAIL", name, gate,
                       result.returncode, expected))
                if not passed:
                    failures.append((name, gate, result.stderr))
        print("partialcmp completion controls: %d pairs, %d gate decisions, "
              "%d failed" % (len(cases), checks, len(failures)))
        if failures:
            raise AssertionError("completion controls failed: %r" % failures)


def relocation_parsing_self_test():
    """An unsupported or dropped record must refuse, rather than disappear."""
    from unittest.mock import patch
    header = "Relocation section '.rel.data' at offset 0x40 contains %d entries:\n"
    record = "00000000 00000101 R_386_32 00000000 outside_a\n"
    target = ("NOTYPE", "GLOBAL", "DEFAULT", "UND", 0, 0, "outside_a")
    cases = [("supported record", header % 1 + record, False),
             ("empty section", header % 0, False),
             ("unsupported record", header % 1 + "00000000 000001ff unrecognized: ff\n", True),
             ("missing record", header % 2 + record, True)]
    for name, output, refused in cases:
        with patch.object(sys.modules[__name__], "run", return_value=output):
            try:
                result = relocations("fixture.o", {1: target})
            except ValueError:
                assert refused, name
            else:
                assert not refused, name
                assert len(result) == (0 if name == "empty section" else 1), name
    print("partialcmp relocation parser: 4 controls, 2 accepted, 2 refused")


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
        # The per-section census must reconcile with the aggregate it is a
        # breakdown of, or the new view is measuring something else.
        by_section = data["contents"]["by_section"]
        assert by_section
        assert sum(e["reference"] for e in by_section.values()) == \
            data["contents"]["reference_bytes"]
        assert sum(e["equal"] for e in by_section.values()) == \
            data["contents"]["equal_positioned_bytes"]
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
    completion_self_test()
    relocation_parsing_self_test()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", nargs="?")
    parser.add_argument("candidate", nargs="?")
    parser.add_argument("--json", metavar="PATH")
    parser.add_argument("--per-section", action="store_true",
                        help="print the positioned-byte census per section")
    parser.add_argument("--require-exact", action="store_true",
                        help="fail unless the link-relevant comparison is exact")
    parser.add_argument("--require-raw-exact", action="store_true",
                        help="fail unless every file byte is identical")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.reference or not args.candidate:
        parser.error("reference and candidate are required")
    result = compare(load(args.reference), load(args.candidate))
    report(result, args.per_section)
    if args.require_raw_exact:
        print("  raw gate      %s" % ("PASS" if result["raw_exact"] else "FAIL"))
    if args.json:
        rendered = json.dumps(result, indent=1, sort_keys=True) + "\n"
        if args.json == "-":
            print(rendered, end="")
        else:
            Path(args.json).write_text(rendered)
            print("  wrote         %s" % args.json)
    return int((args.require_exact and not result["exact"]) or
               (args.require_raw_exact and not result["raw_exact"]))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        sys.exit("partialcmp: %s" % error)
