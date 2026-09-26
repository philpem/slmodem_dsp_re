#!/usr/bin/env python3
"""Derive a stable partial-link input order from the reference object's TUs."""
import argparse
import collections
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import tumap

# These are naming heuristics, not proof of provenance.  In particular old
# attribution reports called an arbitrary same-stem choice "ambig-stem".
FIRM = set(("class", "prefix"))
# The blob contains two inputs named voice.c.  These reconstructed sources
# each contain symbols whose reference locations identify which occurrence
# they represent; a basename alone cannot do that.
SOURCE_FILE_OCCURRENCE = {
	"src/service/cid.c": ("cid.c", 0),
	"src/service/cidcore/cid.c": ("cid.c", 1),
	"src/service/voice.c": ("voice.c", 0),
    "src/voice/voice.c": ("voice.c", 1),
    "src/service/voicecmd.c": ("voice.c", 1),
    "src/service/voicedp.c": ("voice.c", 1),
	"src/service/voicesvc.c": ("voice.c", 1),
    # The reference's only assembly input.  Its basename appears TWICE in the
    # blob's FILE list (`pow.S` at 279 and, after `<command line>` and
    # `<built-in>`, again at 282), so the unique-basename arm cannot place it.
    # A single `.S` input emits that whole four-record run, and selecting the
    # FIRST occurrence orders the input so the run lands on 278..281 exactly.
    # F11407.
    "src/core/pow.S": ("pow.S", 0),
}


def defined_symbols(path):
    result = []
    for line in subprocess.check_output(["readelf", "-sW", str(path)],
                                        text=True).splitlines():
        field = line.split(None, 7)
        if (len(field) == 8 and field[0].endswith(":") and
                field[3] in ("FUNC", "OBJECT") and
                field[4] in ("GLOBAL", "WEAK") and
                field[6] not in ("UND", "ABS")):
            result.append(field[7])
    return result


def inputs(path):
    result = []
    for number, line in enumerate(Path(path).read_text().splitlines()):
        if not line.strip():
            continue
        field = line.split()
        if len(field) != 2:
            raise ValueError("%s:%d is not 'object source'" % (path, number + 1))
        result.append((number, field[0], field[1]))
    if not result or len({x[1] for x in result}) != len(result) or \
            len({x[2] for x in result}) != len(result):
        raise ValueError("%s is empty or repeats an input" % path)
    return result


def reference(blob):
    mapped = tumap.build_map(str(blob))["tus"]
    names = collections.defaultdict(list)
    for key, entry in mapped.items():
        names[entry["file"]].append((entry["seq"], key))
    return names


def recover(rows, directory, names, attribution):
    ordered = []
    for ordinal, obj, source in rows:
        path = directory / obj
        if not path.is_file():
            raise ValueError("missing %s for %s" % (path, source))
        evidence = []
        same_name = names.get(os.path.basename(source), [])
        filename_choice = None
        override = SOURCE_FILE_OCCURRENCE.get(source)
        if override:
            filename, occurrence = override
            choices = names.get(filename, [])
            if occurrence >= len(choices):
                raise ValueError("%s has no %dth %s input in the blob" %
                                 (source, occurrence, filename))
            seq, tu = choices[occurrence]
            filename_choice = (seq, "file-occurrence", tu, filename)
            evidence.append(filename_choice)
        elif len(same_name) == 1:
            seq, tu = same_name[0]
            filename_choice = (seq, "filename", tu, os.path.basename(source))
            evidence.append(filename_choice)
        for symbol in defined_symbols(path):
            item = attribution.get(symbol)
            if item and item.get("how") in FIRM and "|" not in item["tu"]:
                choices = names.get(item["tu"], [])
                if not choices:
                    choices = [(seq, tu) for group in names.values()
                               for seq, tu in group if tu == item["tu"]]
                # A legacy basename naming several FILE occurrences is not
                # an occurrence identity.  Never select its earliest copy.
                if len(choices) != 1:
                    continue
                for seq, tu in choices:
                    evidence.append((seq, "attributed-symbol", tu, symbol))
        # Filename correspondence supplies a stable ordering policy, not proof
        # that the reconstructed TU has the original contents.  Conflicting
        # symbol candidates alone do not identify an earliest constituent.
        candidates = {e[0] for e in evidence}
        chosen = filename_choice or (min(evidence) if len(candidates) == 1 else None)
        key = (0, chosen[0], ordinal) if chosen else (1, ordinal, ordinal)
        ordered.append({"object": obj, "source": source, "input_index": ordinal,
                        "chosen": chosen, "evidence": sorted(set(evidence)),
                        "classification": ("ordering-candidate" if chosen else
                                           "ambiguous" if evidence else "unknown"),
                        "key": key})
    return sorted(ordered, key=lambda row: row["key"])


def self_test():
    from tu_selftest import run
    run()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--blob", type=Path)
    ap.add_argument("--manifest", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--json", dest="report", type=Path)
    ap.add_argument("--attribution", type=Path, default=ROOT / "docs/attribution.json")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return
    if not all((args.blob, args.manifest, args.output, args.report)):
        ap.error("--blob, --manifest, --output and --json are required")
    attribution = json.loads(args.attribution.read_text())["attrib"]
    names = reference(args.blob)
    order = recover(inputs(args.manifest), args.manifest.parent, names,
                    attribution)
    args.output.write_text("".join("%s %s\n" % (x["object"], x["source"])
                                   for x in order))
    report = {"reference": str(args.blob), "input_manifest": str(args.manifest),
              "ordered_manifest": str(args.output), "inputs": len(order),
              "anchored": sum(x["chosen"] is not None for x in order),
              "unanchored": sum(x["chosen"] is None for x in order),
              "order": [{k: v for k, v in x.items() if k != "key"} for x in order]}
    args.report.write_text(json.dumps(report, indent=1, sort_keys=True) + "\n")
    print("partial-link order: %d inputs; %d ordering candidates, %d retained by source order" %
          (report["inputs"], report["anchored"], report["unanchored"]))
    print("  ordering candidates do not establish original TU provenance")
    print("  wrote %s and %s" % (args.output, args.report))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit("recoverorder: %s" % error)
