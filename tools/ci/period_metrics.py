#!/usr/bin/env python3
"""Render and check a base-versus-head Gentoo period metric comparison."""

import argparse
import json
import sys


METRICS = ("exact", "regalloc", "compared", "reference_functions")
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
    return metrics


def delta(value):
    return "%+d" % value


def coverage(metrics):
    return "%d / %d (%.1f%%)" % (metrics["compared"],
                                  metrics["reference_functions"],
                                  100.0 * metrics["compared"] /
                                  metrics["reference_functions"])


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
    print("## Gentoo period metrics")
    print()
    print("| Metric | Base | Head | Delta | Status |")
    print("| --- | ---: | ---: | ---: | --- |")
    print("| Byte-identical functions | %d | %d | %s | %s |" %
          (base["exact"], head["exact"], delta(exact_delta),
           exact_status))
    for name, label in (("regalloc", "Same code, different register allocation"),):
        change = head[name] - base[name]
        print("| %s | %d | %d | %s | informational |" %
              (label, base[name], head[name], delta(change)))
    compared_delta = head["compared"] - base["compared"]
    print("| Functions compared | %s | %s | %s | informational |" %
          (coverage(base), coverage(head), delta(compared_delta)))
    print()
    if lost:
        print("**Lost byte-identical functions:** " + ", ".join("`%s`" % symbol
                                                            for symbol in lost))
    elif gained:
        print("**New byte-identical functions:** " + ", ".join("`%s`" % symbol
                                                           for symbol in gained))
    print("The ratchet gates the byte-identical symbol set, not only its count. "
          "The register-allocation "
          "category permits only a consistent register rename: immediates, memory "
          "displacements/scales, relocation targets, branch targets and operand "
          "order must already match. It can decrease when a function becomes "
          "byte-identical, and the comparison denominator can legitimately change "
          "with source coverage. Functions compared are reference functions that "
          "the reconstructed source also emits.")

    return 1 if args.check and lost else 0


if __name__ == "__main__":
    sys.exit(main())
