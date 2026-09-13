#!/usr/bin/env python3
"""Render and check a base-versus-head partial-link metric comparison.

The codegen ratchet (`period_metrics.py`) measures FUNCTION identity.  The
binding and translation-unit work toward the final object moves a different
axis, which that table cannot see: the ordered section descriptors, positioned
bytes, NOBITS size, relocation records and defined-symbol records that
`tools/toolchain/partialcmp.py` compares.  This tool renders that census and
ratchets the dimensions that must not go backwards.

WHAT IS RATCHETED, AND WHAT IS NOT

Exact section descriptors, exact relocations and exact defined symbols must not
decrease.  Those are structural; a decrease is a regression no excuse covers.

Positioned bytes and NOBITS are reported but NOT ratcheted.  They move by tens
of bytes whenever a layout shifts, in either direction, on sections that were
already inexact -- measured across the binding pass, where every commit moved
positioned bytes by +30/-20/-14/-4 with no section crossing the exact boundary.
A byte ratchet would fail on noise and teach everyone to ignore it.
"""

import argparse
import json
import sys


MARKER = "<!-- gentoo-partial-metrics -->"


def number(table, *path):
    node = table
    for key in path:
        node = node[key]
    return node


def load(path):
    try:
        with open(path) as f:
            census = json.load(f)
    except (OSError, ValueError) as exc:
        raise ValueError("cannot read %s: %s" % (path, exc))

    for section in ("sections", "contents", "relocations", "symbols"):
        if not isinstance(census.get(section), dict):
            raise ValueError("%s lacks a %s object" % (path, section))

    for section, field in (("relocations", "reference"),
                           ("relocations", "exact_records"),
                           ("symbols", "reference"),
                           ("symbols", "exact_records"),
                           ("sections", "reference"),
                           ("sections", "exact_records"),
                           ("contents", "reference_bytes"),
                           ("contents", "equal_positioned_bytes"),
                           ("contents", "reference_nobits_bytes"),
                           ("contents", "candidate_nobits_bytes"),
                           ("contents", "candidate_size_delta")):
        value = number(census, section, field)
        if not isinstance(value, int):
            raise ValueError("%s.%s is not an integer" % (section, field))

    if number(census, "relocations", "reference") == 0:
        raise ValueError("%s has no reference relocations" % path)
    if number(census, "symbols", "reference") == 0:
        raise ValueError("%s has no reference symbols" % path)
    if number(census, "sections", "reference") == 0:
        raise ValueError("%s has no reference sections" % path)
    if number(census, "contents", "reference_bytes") == 0:
        raise ValueError("%s has no reference bytes" % path)
    for section in ("relocations", "symbols"):
        if number(census, section, "exact_records") > \
                number(census, section, "reference"):
            raise ValueError("%s has more exact %s than reference" %
                             (path, section))
    return census


def delta(value):
    return "%+d" % value


def status_cell(change, better):
    """Directional status for an informational row (better = +1 up, -1 down)."""
    if change == 0:
        return ":white_circle: no change"
    improved = change > 0 if better > 0 else change < 0
    return ":green_circle: better" if improved else ":red_circle: worse"


def ratio(exact, total):
    return "%d / %d (%.1f%%)" % (exact, total, 100.0 * exact / total)


def rows(census):
    bytes_total = number(census, "contents", "reference_bytes")
    nobits_delta = (number(census, "contents", "candidate_nobits_bytes") -
                    number(census, "contents", "reference_nobits_bytes"))
    return {
        "sections": (number(census, "sections", "exact_records"),
                     number(census, "sections", "reference")),
        "bytes": (number(census, "contents", "equal_positioned_bytes"),
                  bytes_total),
        "nobits_delta": nobits_delta,
        "size_delta": number(census, "contents", "candidate_size_delta"),
        "relocations": (number(census, "relocations", "exact_records"),
                        number(census, "relocations", "reference")),
        "symbols": (number(census, "symbols", "exact_records"),
                    number(census, "symbols", "reference")),
    }


def render(base, head, base_bindings=None, head_bindings=None):
    """Render the table and return (lines, regressions).

    The GATE is the binding-mismatch count (`tools/bindcmp.py`): it is the one
    dimension here that is monotonic in fidelity and is exactly what the
    binding and translation-unit work moves.  The positional counts are
    reported with their direction but never regress the run -- moving a
    function between translation units shifts every later offset, so a
    positionally-exact relocation or symbol record can fall while the object
    gets closer to the reference.  That was measured the first time it
    happened.
    """
    b = rows(base)
    h = rows(head)
    regressions = []
    if (base_bindings is not None and head_bindings is not None
            and head_bindings > base_bindings):
        regressions.append("Binding mismatches vs reference: %d -> %d"
                           % (base_bindings, head_bindings))

    def counted(name, label):
        change = h[name][0] - b[name][0]
        return ("| %s | %s | %s | %s | %s |" %
                (label, ratio(*b[name]), ratio(*h[name]), delta(change),
                 status_cell(change, 1)))

    lines = [MARKER, "## Partial-link Metrics (GCC 3.4.2, binutils 2.15)", "",
             "| Metric | Base | Head | Delta | Status |",
             "| --- | ---: | ---: | ---: | --- |"]
    if base_bindings is not None and head_bindings is not None:
        change = head_bindings - base_bindings
        status = ("**:red_circle: regression**" if change > 0
                  else status_cell(-change, 1))
        lines.append("| Binding mismatches vs reference | %d | %d | %s | %s |" %
                     (base_bindings, head_bindings, delta(change), status))
    lines.append(counted("sections", "Exact section descriptors"))
    byte_change = h["bytes"][0] - b["bytes"][0]
    lines.append("| Positioned reference bytes | %s | %s | %s | %s |" %
                 (ratio(*b["bytes"]), ratio(*h["bytes"]), delta(byte_change),
                  status_cell(byte_change, 1)))
    nobits_change = abs(h["nobits_delta"]) - abs(b["nobits_delta"])
    lines.append("| NOBITS shortfall (candidate - reference) | %s | %s | %s | %s |" %
                 (delta(b["nobits_delta"]), delta(h["nobits_delta"]),
                  delta(h["nobits_delta"] - b["nobits_delta"]),
                  status_cell(nobits_change, -1)))
    size_change = abs(h["size_delta"]) - abs(b["size_delta"])
    lines.append("| Candidate size delta | %s | %s | %s | %s |" %
                 (delta(b["size_delta"]), delta(h["size_delta"]),
                  delta(h["size_delta"] - b["size_delta"]),
                  status_cell(size_change, -1)))
    lines.append(counted("relocations", "Exact relocation records"))
    lines.append(counted("symbols", "Exact defined-symbol records"))
    lines.append("")
    if regressions:
        lines.append("**:red_circle: Partial-link regression:** " +
                     "; ".join(regressions))
    else:
        lines.append("**:green_circle: pass** -- the binding-mismatch count "
                     "did not increase.")
    lines.append("The binding-mismatch count is the gate. Positioned bytes, "
                 "NOBITS and the positional exact counts move whenever a "
                 "translation unit or a layout shifts and are informational; "
                 "the completion gate remains `partialcmp.py --require-exact`.")
    return lines, regressions


def metrics(sections_exact, sections_ref, bytes_exact, bytes_ref,
            nobits_ref, nobits_cand, size_delta, reloc_exact, reloc_ref,
            sym_exact, sym_ref):
    """A complete, valid census for the self-test."""
    return {
        "sections": {"reference": sections_ref, "exact_records": sections_exact},
        "contents": {"reference_bytes": bytes_ref,
                     "equal_positioned_bytes": bytes_exact,
                     "reference_nobits_bytes": nobits_ref,
                     "candidate_nobits_bytes": nobits_cand,
                     "candidate_size_delta": size_delta},
        "relocations": {"reference": reloc_ref, "exact_records": reloc_exact},
        "symbols": {"reference": sym_ref, "exact_records": sym_exact},
    }


def self_test():
    """Prove the direction logic and the ratchet accept AND reject."""
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
    check("zero is no change", status_cell(0, 1) == ":white_circle: no change")

    base = metrics(50, 92, 50000, 900000, 2836, 2400, -44000, 900, 18000, 220, 2900)
    improved = metrics(52, 92, 50100, 900000, 2836, 2760, -44000, 915, 18000, 226, 2900)
    positional = metrics(49, 92, 49900, 900000, 2836, 2760, -44000, 890, 18000, 219, 2900)

    lines, regressions = render(base, improved, 14, 12)
    check("binding improvement passes", regressions == [])
    check("improvement is green", "**:green_circle: pass**" in lines[-2])
    check("fewer binding mismatches is better",
          ":green_circle: better" in
          row(lines, "Binding mismatches vs reference"))
    check("more exact bytes is better",
          ":green_circle: better" in
          row(lines, "Positioned reference bytes"))

    # A POSITIONAL decrease must NOT fail the run: moving a function between
    # translation units shifts every later offset.
    lines, regressions = render(base, positional, 12, 12)
    check("positional decrease alone does not fail", regressions == [])
    check("positional decrease is still shown as worse",
          ":red_circle: worse" in row(lines, "Exact relocation records"))

    # An INCREASE in binding mismatches is the failure condition.
    lines, regressions = render(base, improved, 12, 14)
    check("binding regression fails", len(regressions) == 1)
    check("binding regression is named",
          "Binding mismatches" in regressions[0])
    check("binding regression is red",
          "**:red_circle: Partial-link regression:**" in "".join(lines))
    check("binding regression row is bold red",
          "**:red_circle: regression**" in
          row(lines, "Binding mismatches vs reference"))

    total = checks[0]
    print("partial_metrics self-test: %d checks, %d failed" % (total, bad[0]))
    return 1 if bad[0] else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base", help="base partialcmp.py --json output")
    ap.add_argument("--head", help="head partialcmp.py --json output")
    ap.add_argument("--base-bindings", type=int,
                    help="base binding-mismatch count (tools/bindcmp.py)")
    ap.add_argument("--head-bindings", type=int,
                    help="head binding-mismatch count (tools/bindcmp.py)")
    ap.add_argument("--check", action="store_true",
                    help="fail when the binding-mismatch count increases")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the status and ratchet logic, then exit")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    if not args.base or not args.head:
        ap.error("--base and --head are required")
    if ((args.base_bindings is None) != (args.head_bindings is None)):
        ap.error("--base-bindings and --head-bindings must be given together "
                 "or not at all")

    try:
        base = load(args.base)
        head = load(args.head)
    except ValueError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 1

    lines, regressions = render(base, head, args.base_bindings,
                                args.head_bindings)
    print("\n".join(lines))
    if args.check and regressions:
        print("\npartial-link ratchet FAILED: %d dimension(s) regressed"
              % len(regressions), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
