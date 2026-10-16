#!/usr/bin/env python3
"""Audit complete FixedRC cells and prove the packed-carrier detector fires."""
import argparse
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from recovery_sre_boundary_audit import nontext
from recovery_bitpacker_audit import nontext_relocations


def carrier(path):
    rows = d.b.insns(str(path), "RcFixed_Resample")
    return {"low_merge": sum(m == "and" and o.startswith("$0xffff0000,") for m, o in rows),
            "high_merge": sum(m == "and" and o.startswith("$0xfffe0000,") for m, o in rows),
            "high_extract": sum(m == "sar" and o.startswith("$0x10,") for m, o in rows)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--polyphase", action="store_true", help="also audit the three generic-loop/carried-index domains")
    args = parser.parse_args()
    original = carrier(d.b.BLOB)
    assert original["low_merge"] == original["high_merge"] == 1
    report = {"original_carrier": original, "runs": {}}
    cells = bodies = 0
    roots = ["recovery-fixedrc-pair", "recovery-fixedrc-wrap-owner", "recovery-fixedrc-pair-origins"]
    if args.polyphase:
        roots += ["recovery-fixedrc-polyphase-loop", "recovery-fixedrc-counter-epoch", "recovery-fixedrc-index-width"]
    for run in roots:
        root = d.ROOT / "build" / run
        record = json.loads((root / "results.json").read_text())["families"]["FixedRC"]
        baseline = root / "FixedRC/baseline/candidate.o"
        metadata = nontext(baseline)
        relocs = nontext_relocations(baseline)
        rows = {}
        for label, entry in record["cells"].items():
            path = root / "FixedRC" / label / "candidate.o"
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["object_hash"]
            assert nontext(path) == metadata, (run, label, "nontext/symbol metadata")
            assert nontext_relocations(path) == relocs, (run, label, "nontext relocations")
            grades = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))) for n in entry["verdicts"]}
            assert grades == entry["verdicts"]
            assert not entry.get("losses", [])
            assert set(entry.get("changed_bodies", [])) <= {"RcFixed_Resample"}
            observed = carrier(path)
            pair = run in ("recovery-fixedrc-polyphase-loop", "recovery-fixedrc-counter-epoch", "recovery-fixedrc-index-width") or (run == "recovery-fixedrc-pair" and label != "baseline") or label.startswith("pair-1-") or label.startswith("helper0-")
            assert observed["low_merge"] == observed["high_merge"] == int(pair), (run, label, observed)
            if pair:
                assert observed["high_extract"] >= 1
            rows[label] = {"verdicts": grades, "carrier": observed,
                           "metadata_nontext_relocations_unchanged": True,
                           "changed_bodies": entry.get("changed_bodies", [])}
            cells += 1
            bodies += len(grades)
        report["runs"][run] = rows
    report["denominator"] = {"compiled_cells": cells, "shared_body_verdicts": bodies}
    assert (cells, bodies) == ((24, 120) if args.polyphase else (12, 60))
    target = d.ROOT / ("build/recovery-packers-next/fixedrc-all-audit.json" if args.polyphase else "build/recovery-packers-next/fixedrc-audit.json")
    target.write_text(json.dumps(report, indent=2) + "\n")
    print("%d complete-TU cells / %d body verdicts: metadata, data, relocations and exact membership pass" % (cells, bodies))
    print("Packed carrier: original positive, scalar controls negative, aggregate controls positive")


if __name__ == "__main__":
    main()
