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


def status_cell(change, better):
    """Directional status for an informational row.

    `better` is the sign of change that improves the metric: +1 when more is
    better, -1 when less is better. A zero change is neither, and is reported
    as such rather than borrowed into one direction.
    """
    if change == 0:
        return ":white_circle: no change"
    improved = change > 0 if better > 0 else change < 0
    return ":green_circle: better" if improved else ":red_circle: worse"


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


def render(base, head):
    """Render the base-versus-head markdown table and return (lines, lost).

    `lost` is the byte-identical symbols present in base but not head, which is
    the ratchet's failure condition; the caller exits non-zero on it when
    `--check` is set.
    """
    exact_delta = head["exact"] - base["exact"]
    lost = sorted(set(base["exact_symbols"]) - set(head["exact_symbols"]))
    gained = sorted(set(head["exact_symbols"]) - set(base["exact_symbols"]))
    exact_status = ("**:red_circle: regression**" if lost else
                    "**:green_circle: pass**")

    lines = [MARKER, "## Reconstruction Metrics (GCC 3.4.2 Gentoo)", "",
             "| Metric | Base | Head | Delta | Status |",
             "| --- | ---: | ---: | ---: | --- |"]
    lines.append("| Byte-identical functions | %d | %d | %s | %s |" %
                 (base["exact"], head["exact"], delta(exact_delta),
                  exact_status))
    exact_byte_delta = head["exact_bytes"] - base["exact_bytes"]
    lines.append("| Byte-identical reference code | %s | %s | %s bytes | %s |" %
                 (byte_coverage(base), byte_coverage(head),
                  delta(exact_byte_delta), status_cell(exact_byte_delta, 1)))
    compared_delta = head["compared"] - base["compared"]
    lines.append("| Functions compared | %s | %s | %s | %s |" %
                 (coverage(base), coverage(head), delta(compared_delta),
                  status_cell(compared_delta, 1)))
    not_yet_delta = ((head["compared"] - head["exact"]) -
                     (base["compared"] - base["exact"]))
    lines.append("| Not yet byte-identical | %d | %d | %s | %s |" %
                 (base["compared"] - base["exact"],
                  head["compared"] - head["exact"],
                  delta(not_yet_delta), status_cell(not_yet_delta, -1)))
    for name, label in (("regalloc", "Same code, different register allocation"),
                        ("reloc", "Bytes match, but call/data target differs"),
                        ("bytes", "Different bytes, same size"),
                        ("size", "Different size"),
                        ("unresolved", "Bytes match, target cannot be checked"),
                        ("nodata", "Could not be disassembled")):
        change = head[name] - base[name]
        lines.append("| %s | %d | %d | %s | %s |" %
                     (label, base[name], head[name], delta(change),
                      status_cell(change, -1)))
    lines.append("")
    if lost:
        lines.append("**Lost byte-identical functions:** " +
                     ", ".join("`%s`" % symbol for symbol in lost))
    elif gained:
        lines.append("**New byte-identical functions:** " +
                     ", ".join("`%s`" % symbol for symbol in gained))
    lines.append("Byte-identical functions are the gate. \"Same code\" permits "
                 "only consistent register renaming; constants, memory "
                 "operands, call/data targets, branches and operand order must "
                 "still match. Other rows are informational, with their status "
                 "showing the direction that improves the metric.")
    return lines, lost


def metrics(exact, symbols, regalloc, reloc, unresolved, same_bytes, size,
            nodata, compared, exact_bytes):
    """A complete, valid metric dict for the self-test."""
    return dict(exact=exact, exact_symbols=symbols, regalloc=regalloc,
                reloc=reloc, unresolved=unresolved, bytes=same_bytes,
                size=size, nodata=nodata, compared=compared,
                reference_functions=10, exact_bytes=exact_bytes,
                compared_bytes=1000)


def self_test():
    """Prove the direction logic accepts AND rejects before trusting a run."""
    bad = [0]
    checks = [0]

    def check(label, condition):
        checks[0] += 1
        if not condition:
            bad[0] += 1
            print("FAIL: %s" % label)

    def row(lines, label):
        for line in lines:
            if line.startswith("| %s |" % label):
                return line
        return None

    check("up-is-better: increase", status_cell(1, 1) == ":green_circle: better")
    check("up-is-better: decrease", status_cell(-1, 1) == ":red_circle: worse")
    check("down-is-better: decrease", status_cell(-1, -1) == ":green_circle: better")
    check("down-is-better: increase", status_cell(1, -1) == ":red_circle: worse")
    check("zero is no change", status_cell(0, 1) == ":white_circle: no change")
    check("zero is no change, down", status_cell(0, -1) == ":white_circle: no change")

    base = metrics(2, ["a", "b"], 1, 0, 0, 1, 1, 0, 5, 100)
    improved = metrics(3, ["a", "b", "c"], 0, 0, 0, 1, 1, 0, 5, 200)
    regressed = metrics(1, ["a"], 2, 1, 0, 1, 2, 0, 7, 50)

    lines, lost = render(base, improved)
    check("gain reports pass, bold", row(lines, "Byte-identical functions") and
          "**:green_circle: pass**" in row(lines, "Byte-identical functions"))
    check("gain loses nothing", lost == [])
    check("gain lists the new symbol",
          any("`c`" in line for line in lines))
    check("more exact bytes is better",
          ":green_circle: better" in row(lines, "Byte-identical reference code"))
    check("fewer non-exact is better",
          ":green_circle: better" in row(lines, "Not yet byte-identical"))
    check("fewer regalloc is better",
          ":green_circle: better" in
          row(lines, "Same code, different register allocation"))

    lines, lost = render(base, regressed)
    check("loss reports regression, bold",
          "**:red_circle: regression**" in
          row(lines, "Byte-identical functions"))
    check("loss names the lost symbol", lost == ["b"] and
          any("**Lost byte-identical functions:**" in line for line in lines))
    check("fewer exact bytes is worse",
          ":red_circle: worse" in row(lines, "Byte-identical reference code"))
    check("more compared is better",
          ":green_circle: better" in row(lines, "Functions compared"))
    check("more different size is worse",
          ":red_circle: worse" in row(lines, "Different size"))
    check("unchanged size is no change",
          ":white_circle: no change" in row(lines, "Could not be disassembled"))

    total = checks[0]
    print("period_metrics self-test: %d checks, %d failed" %
          (total, bad[0]))
    return 1 if bad[0] else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", help="base metrics JSON")
    ap.add_argument("--head", help="head metrics JSON")
    ap.add_argument("--check", action="store_true",
                    help="fail when any byte-identical function regresses")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the status logic accepts and rejects, then exit")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.base or not args.head:
        ap.error("--base and --head are required unless --self-test is given")

    try:
        base = load(args.base)
        head = load(args.head)
    except ValueError as exc:
        ap.error(str(exc))

    lines, lost = render(base, head)
    print("\n".join(lines))
    return 1 if args.check and lost else 0


if __name__ == "__main__":
    sys.exit(main())
