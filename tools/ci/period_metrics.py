#!/usr/bin/env python3
"""Render and check a base-versus-head Gentoo period metric comparison."""

import argparse
import json
import sys


METRICS = ("identical", "same_size", "compared")
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
    return metrics


def delta(value):
    return "%+d" % value


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", required=True, help="base metrics JSON")
    ap.add_argument("--head", required=True, help="head metrics JSON")
    ap.add_argument("--check", action="store_true",
                    help="fail when identical mnemonic sequences decrease")
    args = ap.parse_args()

    try:
        base = load(args.base)
        head = load(args.head)
    except ValueError as exc:
        ap.error(str(exc))

    exact_delta = head["identical"] - base["identical"]
    exact_status = ":green_circle: pass" if exact_delta >= 0 else ":red_circle: regression"

    print(MARKER)
    print("## Gentoo period metrics")
    print()
    print("| Metric | Base | Head | Delta | Status |")
    print("| --- | ---: | ---: | ---: | --- |")
    print("| Identical mnemonic sequences | %d | %d | %s | %s |" %
          (base["identical"], head["identical"], delta(exact_delta),
           exact_status))
    for name, label in (("same_size", "Same-size functions"),
                        ("compared", "Functions compared")):
        change = head[name] - base[name]
        print("| %s | %d | %d | %s | informational |" %
              (label, base[name], head[name], delta(change)))
    print()
    print("The ratchet gates only identical mnemonic sequences. Same-size functions "
          "can decrease when a function enters that category, and the "
          "comparison denominator can legitimately change with source coverage.")

    return 1 if args.check and exact_delta < 0 else 0


if __name__ == "__main__":
    sys.exit(main())
