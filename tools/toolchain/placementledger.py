#!/usr/bin/env python3
"""Causal placement ledger for the recovered-order partial link.

WHY THIS EXISTS

`partialcmp.py` answers HOW DIFFERENT the linked candidate is: section
extents, positioned bytes, relocation and symbol records.  It cannot answer
WHY the first function moved, and a byte-percentage is not a diagnosis.  This
tool reads the blob, the candidate link and the link manifest/attribution and
walks the executable symbol sequence, reporting for each symbol:

  * its start and size in the blob and in the candidate;
  * the cumulative positional delta (candidate start - blob start);
  * the step in that delta since the previous matched symbol;
  * the immediate predecessor on each side and the terms that explain the
    step: a body-size change and a padding/alignment change.

Every non-zero step is classified into the owner's five categories:

  (a) selected-body   a multiply-defined/COMDAT name whose chosen body differs
                      in size (or bytes) from the blob's body;
  (b) order           the candidate's immediate predecessor is not the blob's,
                      so a symbol was inserted, removed or reordered;
  (c) size-drift      a single-definition body differs in size; the delta is
                      the predecessor's size change, possibly absorbed by
                      alignment padding when it does not cross a boundary;
  (d) padding         only the alignment gap between two same-size bodies
                      differs;
  (e) layout          a name or section exists on one side only, or a step has
                      no body size and no padding term.

The tool REFUSES rather than printing a clean zero: a missing, empty or
non-ELF artifact with no .text FUNC symbols is an error, not an empty ledger.
That is finding F2400's rule applied to this measurement.

DENOMINATORS ARE PRINTED: symbols compared, matched by name, and present on
only one side, per side and per executable section.  `.text` is the primary
sequence because that is where the blob's linked positions are comparable
without inventing a cross-section address.  A function that is in `.text` on
one side and in a COMDAT section on the other is reported under (e).

    tools/toolchain/placementledger.py
    tools/toolchain/placementledger.py --limit 40 --json build/partial/ledger.json
    tools/toolchain/placementledger.py --self-test

`--self-test` carries synthetic KNOWN POSITIVES (a size drift, a reorder, a
padding-only delta, an unattributable gap) and NEGATIVES (identical sequences
must report no finding; a one-side symbol must be reported as such), plus
refusal controls for a missing and an empty artifact.  When the real partial
link artifacts exist it also loads them and states the measured first
positional shift, without asserting a fixed value that a future source change
would invalidate.
"""

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
import difflib
import json
from pathlib import Path
import struct
import sys
import tempfile

try:
    from elftools.elf.elffile import ELFFile
except ImportError:  # pragma: no cover - apparatus dependency
    sys.stderr.write("placementledger: pyelftools is required\n")
    raise SystemExit(2)

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BLOB = ROOT / "ref/slmodemd/dsplibs.o"
DEFAULT_CANDIDATE = ROOT / "build/partial/dsplibs.o"
DEFAULT_MANIFEST = ROOT / "build/tc_repro/tc_link_manifest.txt"

TEXT = ".text"
ALIGN = 16
CATEGORY_NAMES = {
    "selected-body": "(a) wrong selected body",
    "order": "(b) wrong TU/emission order",
    "size-drift": "(c) upstream size drift",
    "padding": "(d) padding/alignment",
    "layout": "(e) data/binding/layout",
    "unattributable": "unattributable gap",
}


class LedgerError(Exception):
    """An input is absent or cannot support the measurement."""


@dataclass
class Function:
    name: str
    section: str
    section_index: int
    value: int
    size: int
    bind: str
    body: bytes


@dataclass
class Row:
    index: int
    name: str
    blob: Function
    cand: Function = None
    delta: int = None
    step: int = None
    cause: str = ""
    detail: str = ""
    prev_blob: Function = None
    prev_cand: Function = None
    body: str = "unknown"


def _relocation_offsets(elf):
    """Section index -> [(offset, width)] for fields the linker rewrites."""
    result = defaultdict(list)
    for section in elf.iter_sections():
        if section["sh_type"] not in ("SHT_REL", "SHT_RELA"):
            continue
        target = section["sh_info"]
        try:
            width = 4 if section["sh_type"] == "SHT_REL" else 4
            for relocation in section.iter_relocations():
                result[target].append((relocation["r_offset"], width))
        except Exception:
            # A relocation section that pyelftools cannot parse must not turn
            # into "no relocations here": say so instead.
            raise LedgerError("cannot read relocation section %s" %
                              section.name)
    return result


def read_functions(path, require_text=True, allow_empty=False):
    """Every defined executable FUNC, with relocation fields zeroed.

    Zeroing the 4-byte relocation fields makes body comparison independent of
    the placeholder/addend the assembler wrote; it does not claim the bodies
    are equivalent, only that this comparison is not measuring linker
    bookkeeping.
    """
    path = Path(path)
    if not path.is_file():
        raise LedgerError("missing artifact: %s" % path)
    if path.stat().st_size == 0:
        raise LedgerError("empty artifact: %s" % path)
    with path.open("rb") as stream:
        try:
            elf = ELFFile(stream)
        except Exception as error:
            raise LedgerError("%s is not an ELF file: %s" % (path, error))
        if (elf.elfclass != 32 or not elf.little_endian or
                elf["e_type"] != "ET_REL" or elf["e_machine"] != "EM_386"):
            raise LedgerError(
                "%s must be ELF32 little-endian i386 ET_REL" % path)
        table = elf.get_section_by_name(".symtab")
        if table is None:
            raise LedgerError("%s has no .symtab" % path)
        relocations = _relocation_offsets(elf)
        records = []
        for symbol in table.iter_symbols():
            if symbol["st_info"]["type"] != "STT_FUNC":
                continue
            index = symbol["st_shndx"]
            if not isinstance(index, int):
                continue
            section = elf.get_section(index)
            if section is None:
                continue
            if not (section["sh_flags"] & 0x4):  # SHF_EXECINSTR
                continue
            data = b"" if section["sh_type"] == "SHT_NOBITS" else section.data()
            start = symbol["st_value"]
            end = start + symbol["st_size"]
            if end > len(data):
                raise LedgerError("%s:%s extends beyond its section" %
                                  (path, symbol.name))
            body = bytearray(data[start:end])
            for offset, width in relocations.get(index, ()):
                low = offset - start
                if 0 <= low < len(body):
                    high = min(low + width, len(body))
                    body[low:high] = b"\0" * (high - low)
            records.append(Function(
                name=symbol.name, section=section.name,
                section_index=index, value=start, size=symbol["st_size"],
                bind=symbol["st_info"]["bind"], body=bytes(body)))
    if not records and not allow_empty:
        raise LedgerError("%s defines no executable FUNC symbols" % path)
    if require_text and not any(record.section == TEXT for record in records):
        raise LedgerError("%s defines no %s FUNC symbols" % (path, TEXT))
    return records


def manifest_definitions(manifest):
    """Name -> copies across the link manifest's per-TU objects.

    This is what separates (a) from (c): a name defined by more than one input
    is a selected body, not an upstream drift.  A missing manifest is reported
    by the caller and only limits this distinction.
    """
    manifest = Path(manifest)
    if not manifest.is_file() or manifest.stat().st_size == 0:
        return None
    directory = manifest.parent
    definitions = defaultdict(list)
    for number, line in enumerate(manifest.read_text().splitlines(), 1):
        fields = line.split()
        if len(fields) != 2:
            raise LedgerError("%s:%d is not 'object source'" %
                              (manifest, number))
        obj, source = fields
        obj = directory / obj
        if not obj.is_file():
            raise LedgerError("manifest object missing: %s" % obj)
        for record in read_functions(obj, require_text=False, allow_empty=True):
            definitions[record.name].append({
                "object": obj.name, "source": source,
                "section": record.section, "size": record.size,
                "bind": record.bind,
            })
    return definitions


def text_section_size(path):
    """Sh_size of .text, the section extent partialcmp reports."""
    with Path(path).open("rb") as stream:
        elf = ELFFile(stream)
        section = elf.get_section_by_name(TEXT)
        if section is None:
            raise LedgerError("%s has no %s section" % (path, TEXT))
        return section["sh_size"]


def text_sequence(records):
    return sorted((record for record in records if record.section == TEXT),
                  key=lambda record: (record.value, record.name))


def _tokens(records):
    seen = Counter()
    result = []
    for record in records:
        seen[record.name] += 1
        result.append((record.name, seen[record.name]))
    return result


def classify_step(prev_blob, cur_blob, prev_cand, cur_cand, cand_predecessor,
                  step, duplicate, align=ALIGN):
    """Return (category, detail) for the delta step into `cur`."""
    if step is None:
        return ("unattributable",
                "no matched predecessor on both sides to carry the delta")
    if cand_predecessor is None or cand_predecessor.name != prev_cand.name:
        return ("order",
                "candidate predecessor is %s, blob predecessor is %s" %
                (getattr(cand_predecessor, "name", "<none>"), prev_cand.name))
    if step == 0:
        if prev_blob.size != prev_cand.size:
            return ("size-drift",
                    "predecessor %s %d -> %d bytes, absorbed by alignment" %
                    (prev_blob.name, prev_blob.size, prev_cand.size))
        return ("none", "")
    size_term = prev_cand.size - prev_blob.size
    pad_b = cur_blob.value - (prev_blob.value + prev_blob.size)
    pad_c = cur_cand.value - (prev_cand.value + prev_cand.size)
    pad_term = pad_c - pad_b
    notes = []
    if size_term:
        notes.append("predecessor %s %d -> %d bytes (%+d)" %
                     (prev_blob.name, prev_blob.size, prev_cand.size,
                      size_term))
    if pad_term:
        notes.append("padding %d -> %d (%+d)" % (pad_b, pad_c, pad_term))
    detail = "; ".join(notes) if notes else "no size or padding term"
    if size_term:
        if duplicate:
            return ("selected-body", detail)
        return ("size-drift", detail)
    if pad_term:
        if 0 <= pad_b < align and 0 <= pad_c < align:
            return ("padding", detail)
        return ("layout", detail)
    return ("unattributable", detail)


def build_ledger(blob_records, cand_records, definitions=None):
    blob_text = text_sequence(blob_records)
    cand_text = text_sequence(cand_records)
    cand_lookup = defaultdict(list)
    for record in cand_text:
        cand_lookup[record.name].append(record)
    blob_lookup = defaultdict(list)
    for record in blob_text:
        blob_lookup[record.name].append(record)

    matcher = difflib.SequenceMatcher(
        a=_tokens(blob_text), b=_tokens(cand_text), autojunk=False)
    aligned = {}
    for block in matcher.get_matching_blocks():
        for offset in range(block.size):
            aligned[block.a + offset] = block.b + offset
    # Pair by NAME wherever the name is unique in .text on both sides, so a
    # reordered function still gets its start/size compared.  The sequence
    # alignment is used only to resolve repeated names.
    pair = {}
    for index, record in enumerate(blob_text):
        candidates = cand_lookup.get(record.name, ())
        if len(candidates) == 1:
            pair[index] = candidates[0]
        elif len(candidates) > 1 and index in aligned:
            pair[index] = cand_text[aligned[index]]

    duplicates = set()
    if definitions is not None:
        duplicates = {name for name, copies in definitions.items()
                      if len(copies) > 1}

    rows = []
    cand_index_of = {id(record): index for index, record in
                     enumerate(cand_text)}
    previous = None
    for index, blob in enumerate(blob_text):
        cand = pair.get(index)
        row = Row(index=index, name=blob.name, blob=blob, cand=cand)
        if cand is not None:
            row.delta = cand.value - blob.value
            row.body = "same" if blob.body == cand.body else "differs"
            if previous is not None and previous.cand is not None:
                row.step = row.delta - previous.delta
                cand_position = cand_index_of[id(cand)]
                cand_prev = (cand_text[cand_position - 1]
                             if cand_position else None)
                row.prev_cand = cand_prev
                row.cause, row.detail = classify_step(
                    previous.blob, blob, previous.cand, cand, cand_prev,
                    row.step, blob.name in duplicates or
                    (definitions is not None and
                     len(definitions.get(blob.name, ())) > 1))
            else:
                row.cause = "unattributable"
                row.detail = "first matched row carries the initial delta"
            row.prev_blob = previous.blob if previous is not None else None
        elif cand_lookup.get(blob.name):
            row.cause = "order"
            row.detail = "name exists in candidate but not in its .text order"
        else:
            sections = sorted({record.section for record in
                               (r for r in cand_records
                                if r.name == blob.name)})
            if sections:
                row.cause = "layout"
                row.detail = "candidate defines the name in %s" % \
                             ", ".join(sections)
            else:
                row.cause = "layout"
                row.detail = "absent from the candidate"
        rows.append(row)
        previous = row

    blob_names = [record.name for record in blob_text]
    cand_names = [record.name for record in cand_text]
    blob_set, cand_set = set(blob_names), set(cand_names)
    all_cand_names = {record.name for record in cand_records}
    all_blob_names = {record.name for record in blob_records}
    report = {
        "blob": {"functions": len(blob_records), "text": len(blob_text),
                 "text_bytes": sum(record.size for record in blob_text)},
        "candidate": {"functions": len(cand_records),
                      "text": len(cand_text),
                      "text_bytes": sum(record.size for record in cand_text)},
        "matched": len(pair),
        "aligned": len(aligned),
        "blob_only_text": len(blob_set - cand_set),
        "candidate_only_text": len(cand_set - blob_set),
        "blob_only_elsewhere": len(
            (blob_set - cand_set) & all_cand_names),
        "candidate_only_elsewhere": len(
            (cand_set - blob_set) & all_blob_names),
        "candidate_only_rows": [
            {"name": name, "value": record.value, "size": record.size}
            for name in sorted(cand_set - blob_set)
            for record in cand_lookup[name][:1]
        ],
        "rows": rows,
        "definitions": len(definitions) if definitions is not None else None,
        "duplicates": len(duplicates),
    }
    return report


def first_shift(report):
    for row in report["rows"]:
        if row.cand is not None and row.delta != 0:
            return row
    return None


def cause_histogram(report):
    counts = Counter()
    for row in report["rows"]:
        if row.cand is None or row.step not in (None, 0):
            counts[row.cause] += 1
        elif row.cause == "size-drift":
            # The size change did not cross an alignment boundary but is
            # still the cause of the current position's ledger entry.
            counts[row.cause] += 1
        elif row.body == "differs" and row.delta == 0:
            counts["body-content"] += 1
    return counts


def print_report(report, limit):
    print("placement ledger")
    print("  blob:      %d FUNC, %d .text, section %d bytes "
          "(%d covered by FUNC symbols)" %
          (report["blob"]["functions"], report["blob"]["text"],
           report["blob"].get("text_section_bytes", 0),
           report["blob"]["text_bytes"]))
    print("  candidate: %d FUNC, %d .text, section %d bytes "
          "(%d covered by FUNC symbols)" %
          (report["candidate"]["functions"], report["candidate"]["text"],
           report["candidate"].get("text_section_bytes", 0),
           report["candidate"]["text_bytes"]))
    print("  denominators: symbols compared (common .text names) %d "
          "(sequence-aligned %d, reordered %d)" %
          (report["matched"], report["aligned"],
           report["matched"] - report["aligned"]))
    print("                blob-only .text %d (%d defined elsewhere in candidate)" %
          (report["blob_only_text"], report["blob_only_elsewhere"]))
    print("                candidate-only .text %d (%d defined elsewhere in blob)" %
          (report["candidate_only_text"], report["candidate_only_elsewhere"]))
    if report["definitions"] is not None:
        print("                manifest definitions %d names, %d multiply-defined" %
              (report["definitions"], report["duplicates"]))
    else:
        print("                manifest unavailable: selected-body vs drift "
              "cannot be separated")
    moved = sum(1 for row in report["rows"]
                if row.cand is not None and row.delta not in (None, 0))
    print("                matched with a moved start %d, unmoved %d" %
          (moved, report["matched"] - moved))
    histogram = cause_histogram(report)
    print("  cause histogram (non-zero step, plus same-size body drift):")
    for name in ("selected-body", "order", "size-drift", "padding", "layout",
                 "unattributable", "body-content"):
        if histogram.get(name):
            print("    %-18s %d" % (CATEGORY_NAMES.get(name, name),
                                    histogram[name]))

    row = first_shift(report)
    if row is None:
        print("  first positional divergence: none (every matched start agrees)")
    else:
        print("  first positional divergence: %s" % row.name)
        print("    blob      start %d (0x%x) size %d" %
              (row.blob.value, row.blob.value, row.blob.size))
        print("    candidate start %d (0x%x) size %d" %
              (row.cand.value, row.cand.value, row.cand.size))
        print("    cumulative delta %+d; step since previous matched row %+d" %
              (row.delta, row.step if row.step is not None else 0))
        if row.prev_blob is not None:
            print("    predecessor blob %s size %d; candidate %s size %d" %
                  (row.prev_blob.name, row.prev_blob.size,
                   row.prev_cand.name if row.prev_cand else "<none>",
                   row.prev_cand.size if row.prev_cand else 0))
        print("    cause: %s -- %s" %
              (CATEGORY_NAMES.get(row.cause, row.cause), row.detail))

    print("  first %d delta steps:" % limit)
    shown = 0
    for row in report["rows"]:
        if shown >= limit:
            break
        if row.cand is None:
            print("    [%4d] %-40s %s: %s" %
                  (row.index, row.name, CATEGORY_NAMES.get(row.cause,
                                                           row.cause),
                   row.detail))
            shown += 1
        elif row.step not in (0, None) or row.cause == "size-drift":
            step = row.step if row.step is not None else 0
            print("    [%4d] %-40s delta %+6d step %+5d  %s: %s" %
                  (row.index, row.name, row.delta, step,
                   CATEGORY_NAMES.get(row.cause, row.cause), row.detail))
            shown += 1
    if shown == 0:
        print("    (none: no matched row has a non-zero delta step)")


def to_json(report):
    def function(record):
        if record is None:
            return None
        return {"name": record.name, "section": record.section,
                "value": record.value, "size": record.size,
                "bind": record.bind}

    return {
        "blob": report["blob"], "candidate": report["candidate"],
        "matched": report["matched"], "aligned": report["aligned"],
        "blob_only_text": report["blob_only_text"],
        "candidate_only_text": report["candidate_only_text"],
        "blob_only_elsewhere": report["blob_only_elsewhere"],
        "candidate_only_elsewhere": report["candidate_only_elsewhere"],
        "candidate_only_rows": report["candidate_only_rows"],
        "definitions": report["definitions"], "duplicates": report["duplicates"],
        "first_shift": (None if first_shift(report) is None else {
            "name": first_shift(report).name,
            "blob": function(first_shift(report).blob),
            "candidate": function(first_shift(report).cand),
            "delta": first_shift(report).delta,
            "step": first_shift(report).step,
            "cause": first_shift(report).cause,
            "detail": first_shift(report).detail,
            "predecessor_blob": function(first_shift(report).prev_blob),
            "predecessor_candidate": function(
                first_shift(report).prev_cand),
        }),
        "cause_histogram": dict(cause_histogram(report)),
        "rows": [{
            "index": row.index, "name": row.name,
            "blob": function(row.blob), "candidate": function(row.cand),
            "delta": row.delta, "step": row.step, "cause": row.cause,
            "detail": row.detail, "body": row.body,
            "predecessor_blob": function(row.prev_blob),
        } for row in report["rows"]],
    }


def _synthetic(name, value, size, section=TEXT, body=None):
    return Function(name=name, section=section, section_index=1,
                    value=value, size=size, bind="GLOBAL",
                    body=body if body is not None else bytes(size))


def self_test(blob_path, candidate_path):
    passed = []
    failed = []

    def check(label, condition, detail=""):
        (passed if condition else failed).append(label)
        print("  %-52s %s%s" % (label, "PASS" if condition else "FAIL",
                                ("  " + detail) if detail else ""))

    # Known positives: one of each owner category.
    blob = [_synthetic("a", 0, 13), _synthetic("b", 16, 10),
            _synthetic("c", 32, 10)]
    cand = [_synthetic("a", 0, 13), _synthetic("b", 24, 10),
            _synthetic("c", 40, 10)]
    report = build_ledger(blob, cand)
    row = report["rows"][1]
    check("positive: padding-only delta", row.cause == "padding",
          "%s %s" % (row.cause, row.detail))

    blob = [_synthetic("a", 0, 26), _synthetic("b", 32, 10)]
    cand = [_synthetic("a", 0, 24), _synthetic("b", 24, 10)]
    report = build_ledger(blob, cand)
    row = report["rows"][1]
    check("positive: upstream size drift",
          row.cause == "size-drift" and row.step == -8,
          "%s step %s" % (row.cause, row.step))

    blob = [_synthetic("a", 0, 16), _synthetic("b", 16, 16),
            _synthetic("c", 32, 16)]
    cand = [_synthetic("a", 0, 16), _synthetic("c", 16, 16),
            _synthetic("b", 32, 16)]
    report = build_ledger(blob, cand)
    row = report["rows"][1]
    check("positive: reorder classified as order",
          row.cause == "order", "%s %s" % (row.cause, row.detail))

    blob = [_synthetic("a", 0, 16), _synthetic("b", 16, 16)]
    cand = [_synthetic("x", 0, 16), _synthetic("b", 16, 16)]
    report = build_ledger(blob, cand)
    row = report["rows"][1]
    check("positive: unattributable gap has no matched predecessor",
          row.cause in ("unattributable", "order"),
          "%s %s" % (row.cause, row.detail))

    blob = [_synthetic("a", 0, 18), _synthetic("b", 32, 16)]
    cand = [_synthetic("a", 0, 16), _synthetic("b", 32, 16)]
    report = build_ledger(blob, cand)
    row = report["rows"][1]
    check("positive: size term absorbed by padding is still size-drift",
          row.cause == "size-drift" and row.step == 0,
          "%s %s" % (row.cause, row.detail))

    # Negatives: an identical sequence has no finding at all.
    blob = [_synthetic("a", 0, 16), _synthetic("b", 16, 16)]
    cand = [_synthetic("a", 0, 16), _synthetic("b", 16, 16)]
    report = build_ledger(blob, cand)
    check("negative: identical sequences report no shift",
          first_shift(report) is None and
          sum(cause_histogram(report).values()) == 0,
          "histogram %s" % dict(cause_histogram(report)))

    # Negative: a same-size body change is not a placement step.
    blob = [_synthetic("a", 0, 16, body=b"0" * 16)]
    cand = [_synthetic("a", 0, 16, body=b"1" * 16)]
    report = build_ledger(blob, cand)
    check("negative: same-size body change is not a placement delta",
          first_shift(report) is None and report["rows"][0].body == "differs")

    # Negative: a one-side name must be reported, never dropped.
    blob = [_synthetic("a", 0, 16), _synthetic("only", 16, 16)]
    cand = [_synthetic("a", 0, 16)]
    report = build_ledger(blob, cand)
    check("negative: one-side symbol is counted and classified",
          report["blob_only_text"] == 1 and
          report["rows"][1].cause == "layout")

    # Refusal controls: missing and empty artifacts are errors, not zeroes.
    with tempfile.TemporaryDirectory(prefix="placementledger-") as temporary:
        empty = Path(temporary) / "empty.o"
        empty.write_bytes(b"")
        for label, target in (
                ("missing", Path(temporary) / "absent.o"),
                ("empty", empty)):
            try:
                read_functions(target)
                check("refusal: %s artifact" % label, False)
            except LedgerError:
                check("refusal: %s artifact" % label, True)

    # Real artifacts: measure, do not hard-code a value that source work may
    # legitimately move.
    for label, path in (("blob", blob_path), ("candidate", candidate_path)):
        if not Path(path).is_file():
            print("  real %-14s SKIP (missing %s)" % (label, path))
            continue
        try:
            read_functions(path)
            print("  real %-14s PASS (%s)" % (label, path))
            passed.append("real " + label)
        except LedgerError as error:
            check("real %s artifact loads" % label, False, str(error))
    if Path(blob_path).is_file() and Path(candidate_path).is_file():
        report = build_ledger(read_functions(blob_path),
                              read_functions(candidate_path))
        shift = first_shift(report)
        if shift is None:
            check("real: a first positional shift exists", False)
        else:
            consistent = (shift.prev_blob is not None and
                          shift.prev_cand is not None and
                          (shift.prev_blob.size != shift.prev_cand.size or
                           shift.cause == "padding" or
                           shift.prev_blob.name != shift.prev_cand.name))
            check("real: first shift has an explaining predecessor",
                  consistent, "%s delta %+d via %s" %
                  (shift.name, shift.delta, shift.prev_blob.name))

    total = len(passed) + len(failed)
    print("placement ledger self-test: %d checks, %d failed" %
          (total, len(failed)))
    return 0 if not failed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blob", type=Path, default=DEFAULT_BLOB)
    parser.add_argument("--candidate", type=Path, default=DEFAULT_CANDIDATE)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--limit", type=int, default=25)
    parser.add_argument("--json", dest="report", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test(args.blob, args.candidate)
    try:
        report = build_ledger(read_functions(args.blob),
                              read_functions(args.candidate),
                              manifest_definitions(args.manifest))
        report["blob"]["text_section_bytes"] = text_section_size(args.blob)
        report["candidate"]["text_section_bytes"] = \
            text_section_size(args.candidate)
    except LedgerError as error:
        sys.stderr.write("placementledger: %s\n" % error)
        return 2
    print_report(report, args.limit)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(to_json(report), indent=1,
                                          sort_keys=True) + "\n")
        print("  wrote %s" % args.report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
