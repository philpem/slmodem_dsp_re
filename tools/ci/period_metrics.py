#!/usr/bin/env python3
"""Render and check a base-versus-head Gentoo period metric comparison."""

import argparse
import json
import sys


METRICS = ("exact", "regalloc", "unresolved", "reloc", "bytes", "size",
           "nodata", "compared", "reference_functions", "exact_bytes",
           "compared_bytes")
MARKER = "<!-- gentoo-period-metrics -->"


def load(path):
    try:
        with open(path) as f:
            metrics = json.load(f)
    except (OSError, ValueError) as exc:
        raise ValueError("cannot read %s: %s" % (path, exc))

    missing = [name for name in METRICS
               if not isinstance(metrics.get(name), int) or metrics[name] < 0]
    if missing:
        raise ValueError("%s lacks non-negative integer metric(s): %s" %
                         (path, ", ".join(missing)))
    symbols = metrics.get("exact_symbols")
    if (not isinstance(symbols, list) or
            any(not isinstance(symbol, str) for symbol in symbols) or
            len(set(symbols)) != len(symbols) or len(symbols) != metrics["exact"]):
        raise ValueError("%s has an invalid exact-symbol set" % path)
    if metrics["reference_functions"] == 0:
        raise ValueError("%s has no reference functions" % path)
    if metrics["compared"] > metrics["reference_functions"]:
        raise ValueError("%s compares more functions than the reference contains" % path)
    if metrics["compared_bytes"] == 0:
        raise ValueError("%s has no compared reference bytes" % path)
    if metrics["exact_bytes"] > metrics["compared_bytes"]:
        raise ValueError("%s has more exact bytes than compared reference bytes" %
                         path)
    buckets = ("exact", "regalloc", "unresolved", "reloc", "bytes", "size",
               "nodata")
    if sum(metrics[name] for name in buckets) != metrics["compared"]:
        raise ValueError("%s has a bucket total different from functions compared" %
                         path)
    return metrics


def delta(value):
    return "%+d" % value


def coverage(metrics):
    return "%d / %d (%.1f%%)" % (metrics["compared"],
                                  metrics["reference_functions"],
                                  100.0 * metrics["compared"] /
                                  metrics["reference_functions"])


def byte_coverage(metrics):
    return "%d / %d (%.1f%%)" % (metrics["exact_bytes"],
                                  metrics["compared_bytes"],
                                  100.0 * metrics["exact_bytes"] /
                                  metrics["compared_bytes"])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="base metrics JSON")
    ap.add_argument("--head", required=True, help="head metrics JSON")
    ap.add_argument("--check", action="store_true",
                    help="fail when any byte-identical function regresses")
    args = ap.parse_args()

    try:
        base = load(args.base)
        head = load(args.head)
    except ValueError as exc:
        ap.error(str(exc))

    exact_delta = head["exact"] - base["exact"]
    lost = sorted(set(base["exact_symbols"]) - set(head["exact_symbols"]))
    gained = sorted(set(head["exact_symbols"]) - set(base["exact_symbols"]))
    exact_status = (":red_circle: regression" if lost else
                    ":green_circle: pass")

    print(MARKER)
    print("## Reconstruction Metrics (GCC 3.4.2 Gentoo)")
    print()
    print("| Metric | Base | Head | Delta | Status |")
    print("| --- | ---: | ---: | ---: | --- |")
    print("| Byte-identical functions | %d | %d | %s | %s |" %
          (base["exact"], head["exact"], delta(exact_delta),
           exact_status))
    exact_byte_delta = head["exact_bytes"] - base["exact_bytes"]
    print("| Byte-identical reference code | %s | %s | %s bytes | informational |" %
          (byte_coverage(base), byte_coverage(head), delta(exact_byte_delta)))
    compared_delta = head["compared"] - base["compared"]
    print("| Functions compared | %s | %s | %s | informational |" %
          (coverage(base), coverage(head), delta(compared_delta)))
    print("| Not yet byte-identical | %d | %d | %s | informational |" %
          (base["compared"] - base["exact"],
           head["compared"] - head["exact"],
           delta((head["compared"] - head["exact"]) -
                 (base["compared"] - base["exact"]))))
    for name, label in (("regalloc", "Same code, different register allocation"),
                        ("reloc", "Bytes match, but call/data target differs"),
                        ("bytes", "Different bytes, same size"),
                        ("size", "Different size"),
                        ("unresolved", "Bytes match, target cannot be checked"),
                        ("nodata", "Could not be disassembled")):
        change = head[name] - base[name]
        print("| %s | %d | %d | %s | informational |" %
              (label, base[name], head[name], delta(change)))
    print()
    if lost:
        print("**Lost byte-identical functions:** " + ", ".join("`%s`" % symbol
                                                            for symbol in lost))
    elif gained:
        print("**New byte-identical functions:** " + ", ".join("`%s`" % symbol
                                                           for symbol in gained))
    print("Byte-identical functions are the gate. \"Same code\" permits only "
          "consistent register renaming; constants, memory operands, call/data "
          "targets, branches and operand order must still match. Other rows are "
          "informational.")

    return 1 if args.check and lost else 0


if __name__ == "__main__":
    sys.exit(main())
