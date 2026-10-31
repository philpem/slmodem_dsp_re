#!/usr/bin/env python3
"""Check the pinned COMDAT link-selected report from byteident.

`tools/toolchain/byteident.py --link-selected` writes a DIAGNOSTIC view to
stderr: for every multiply-defined name it prints the body the recovered-order
partial link selects beside the worst-copy verdict, together with the
denominator.  This checker pins the expectation measured on master so a CI run
is reproducible evidence rather than a report somebody has to eyeball.

WHY THE EXPECTATION IS PINNED

A new multiply-defined name, a changed definition-record count, a name leaving
the link-resolved set, or a different selected grade is a change to the
COMDAT/selection ledger.  It must be a deliberate update to this file in the
same change that causes it, not silent drift.  The census, buckets, ratchet and
exit status of byteident are NOT graded here: this tool reads the diagnostic
report and the `--summary` block, and never runs the ratchet or changes a
bucket.  The negative control (`_Z7hanningIfEvPT_j`) is a disagreement whose
selected copy is not EXACT; if it ever appears in the report the pin no longer
describes master.

The summary cross-check is what ties phase 2 to phase 0: link-selected EXACT
must be at least per-TU EXACT, its excess over per-TU EXACT must equal the
number of names whose SELECTED body is EXACT while the worst per-TU copy is
not (the "selected-EXACT-only" disagreements), grade 0-or-1 must be at least
EXACT, and the end-target line must be present.  A genuine change to any of
those is a deliberate update to this file, not drift.

    tools/ci/link_selected_check.py REPORT --summary SUMMARY
    tools/ci/link_selected_check.py --self-test
"""

import argparse
import re
import sys

EXPECTED = {
    "names": 32,
    "records": 66,
    "resolved": 32,
    "unresolved": 0,
    "disagreements": 3,
}

EXACT_SELECTED = frozenset({
    "_ZN9ScramblerIhhE5resetEh",
    "_ZN9ScramblerIhhEC1Ejjj",
    "_ZN9ScramblerIhiE5resetEh",
})

NEGATIVE_CONTROL = "_Z7hanningIfEvPT_j"

# Phase 2's correction: names whose SELECTED body is EXACT while the worst
# per-TU copy is not.  Each one is +1 to link-selected EXACT over per-TU
# EXACT, so the pinned difference and the report must agree.
EXPECTED_SELECTED_ONLY = 3

DENOMINATOR = re.compile(
    r"denominator\s*:\s*(\d+)\s+multiply-defined name\(s\),\s*"
    r"(\d+)\s+definition record\(s\);")
RESOLVED = re.compile(r"(\d+)\s+link-resolved,\s*(\d+)\s+unresolved")
DISAGREEMENTS = re.compile(
    r"disagreements:\s*(\d+)\s+name\(s\) whose copies get different verdicts")
ROW = re.compile(r"^\s*(EXACT|UNRESOLVED|REGALLOC|RELOC|BYTES|SIZE|NODATA)"
                 r"\s+(EXACT|UNRESOLVED|REGALLOC|RELOC|BYTES|SIZE|NODATA)"
                 r"\s+(\S+)")

SUMMARY_HEADING = "phase summary (diagnostic)"
SUMMARY_GRADE01 = re.compile(
    r"(?:legacy protected|functions right)\s*:\s*per-TU grade 0 or 1\s+(\d+)\s*/\s*(\d+)")
SUMMARY_SELECTED = re.compile(
    r"right place\s*:\s*link-selected EXACT\s+(\d+)\s*$", re.MULTILINE)
SUMMARY_EXACT = re.compile(r"per-TU bytes\s*:\s*EXACT\s+(\d+)\s*$",
                           re.MULTILINE)
SUMMARY_DISPLACED = re.compile(
    r"displaced\s*:\s*(\d+)\s+reviewed in tools/ci/displaced\.json")
SUMMARY_END = "end target        : make partial-compare-exact"


class ReportError(ValueError):
    pass


def parse(text):
    if not text.strip():
        raise ReportError("report is empty")
    if "link-selected view" not in text:
        raise ReportError("report has no link-selected view heading")
    if "selected linked bodies matching an input copy:" not in text:
        raise ReportError("report is truncated (no final body-match line)")

    counts = {}
    match = DENOMINATOR.search(text)
    if match is None:
        raise ReportError("report has no denominator line")
    counts["names"], counts["records"] = map(int, match.groups())

    match = RESOLVED.search(text)
    if match is None:
        raise ReportError("report has no link-resolved/unresolved line")
    counts["resolved"], counts["unresolved"] = map(int, match.groups())

    match = DISAGREEMENTS.search(text)
    if match is None:
        raise ReportError("report has no disagreements line")
    counts["disagreements"] = int(match.group(1))

    rows = {}
    for line in text.splitlines():
        match = ROW.match(line)
        if match is not None:
            worst, selected, symbol = match.groups()
            rows[symbol] = (worst, selected)
    return counts, rows


def parse_summary(text):
    """Read the phase summary.  Every line must be present and numeric."""
    if text is None or not text.strip():
        raise ReportError("summary is empty")
    if SUMMARY_HEADING not in text:
        raise ReportError("summary has no phase-summary heading")
    if "link-selected EXACT : unavailable" in text:
        raise ReportError(
            "summary reports the link-selected metric as unavailable "
            "(run make partial-link)")
    if SUMMARY_END not in text:
        raise ReportError("summary has no end-target line")
    summary = {}
    match = SUMMARY_GRADE01.search(text)
    if match is None:
        raise ReportError("summary has no grade-0-or-1 line")
    summary["grade01"], summary["compared"] = map(int, match.groups())
    if summary["compared"] == 0:
        raise ReportError("summary grade-0-or-1 denominator is zero")
    match = SUMMARY_SELECTED.search(text)
    if match is None:
        raise ReportError("summary has no link-selected EXACT count")
    summary["selected"] = int(match.group(1))
    match = SUMMARY_EXACT.search(text)
    if match is None:
        raise ReportError("summary has no per-TU EXACT count")
    summary["exact"] = int(match.group(1))
    match = SUMMARY_DISPLACED.search(text)
    if match is None:
        raise ReportError("summary has no displaced-ledger line")
    summary["displaced"] = int(match.group(1))
    return summary


def check(text, summary_text=None):
    """Return the list of pinned-expectation failures; empty means pass."""
    failures = []
    try:
        counts, rows = parse(text)
    except ReportError as error:
        return ["%s" % error]

    for field in ("names", "records", "resolved", "unresolved",
                  "disagreements"):
        if counts[field] != EXPECTED[field]:
            failures.append(
                "%s: expected %d, found %d"
                % (field, EXPECTED[field], counts[field]))

    if counts["resolved"] != counts["names"]:
        failures.append(
            "denominator is short: %d of %d names link-resolved"
            % (counts["resolved"], counts["names"]))

    if len(rows) != counts["disagreements"]:
        failures.append(
            "printed rows: expected %d disagreement row(s), found %d"
            % (counts["disagreements"], len(rows)))

    for symbol in sorted(EXACT_SELECTED):
        if symbol not in rows:
            failures.append("%s: expected disagreement row is missing" % symbol)
            continue
        selected = rows[symbol][1]
        if selected != "EXACT":
            failures.append(
                "%s: expected selected EXACT, found %s"
                % (symbol, selected))

    unexpected = set(rows) - EXACT_SELECTED - {NEGATIVE_CONTROL}
    for symbol in sorted(unexpected):
        failures.append("%s: unexpected disagreement row" % symbol)

    if NEGATIVE_CONTROL in rows:
        failures.append(
            "%s: negative control must NOT be printed" % NEGATIVE_CONTROL)

    # --- the phase summary -------------------------------------------------
    try:
        summary = parse_summary(summary_text)
    except ReportError as error:
        failures.append("summary: %s" % error)
        return failures

    if summary["grade01"] < summary["exact"]:
        failures.append(
            "grade-0-or-1 %d is below per-TU EXACT %d"
            % (summary["grade01"], summary["exact"]))
    if summary["selected"] < summary["exact"]:
        failures.append(
            "link-selected EXACT %d is below per-TU EXACT %d; the selected "
            "body can add a name, never lose a whole phase-0 symbol"
            % (summary["selected"], summary["exact"]))

    selected_only = sum(
        1 for worst, selected in rows.values()
        if selected == "EXACT" and worst != "EXACT")
    selected_losses = sum(
        1 for worst, selected in rows.values()
        if selected != "EXACT" and worst == "EXACT")
    difference = summary["selected"] - summary["exact"]
    if difference != selected_only - selected_losses:
        failures.append(
            "link-selected EXACT minus per-TU EXACT is %+d, but the report "
            "adds %d and removes %d selected-only name(s)"
            % (difference, selected_only, selected_losses))
    if selected_only != EXPECTED_SELECTED_ONLY:
        failures.append(
            "selected-EXACT-only disagreements: expected %d, found %d"
            % (EXPECTED_SELECTED_ONLY, selected_only))

    return failures


FIXTURE = """\
  link-selected view -- recovered-order partial link (diagnostic only;
  census, buckets and exit status are unchanged)
    order       : build/tc_repro/tc_link_manifest.txt (300 input(s); 279 ordering candidate(s), 21 retained by source order)
    linked      : build/partial/dsplibs.o  sha256 0000000000000000000000000000000000000000000000000000000000000000
    denominator : 32 multiply-defined name(s), 66 definition record(s);
                  32 link-resolved, 0 unresolved
    disagreements: 3 name(s) whose copies get different verdicts
    worst-copy  selected    symbol
    REGALLOC    EXACT       _ZN9ScramblerIhhE5resetEh
    BYTES       EXACT       _ZN9ScramblerIhhEC1Ejjj
    RELOC       EXACT       _ZN9ScramblerIhiE5resetEh
    selected linked bodies matching an input copy: 30/32
"""

SUMMARY_FIXTURE = """\
phase summary (diagnostic)
  functions right   : per-TU grade 0 or 1   1182 / 1852
  right place       : link-selected EXACT   1149
  per-TU bytes      : EXACT                 1146
  displaced         : 0 reviewed in tools/ci/displaced.json
  end target        : make partial-compare-exact (not yet reached)
"""


def self_test():
    checks = []

    def expect(label, text, should_pass, summary=SUMMARY_FIXTURE):
        problems = check(text, summary)
        ok = (not problems) if should_pass else bool(problems)
        checks.append((label, ok))

    expect("known-good fixture passes", FIXTURE, True)
    new_summary = SUMMARY_FIXTURE.replace(
        "functions right   : per-TU grade 0 or 1   1182 / 1852",
        "functions right   : EXACT + REGALLOC   1181 / 1852\n"
        "  legacy protected  : per-TU grade 0 or 1   1182 / 1852")
    expect("new reporting format retains the legacy floor", FIXTURE, True, new_summary)
    checks.append(("new matched total does not replace legacy protected membership",
                   parse_summary(new_summary)["grade01"] == 1182))
    expect("matched headline alone cannot stand in for the protected floor", FIXTURE, False,
           "\n".join(line for line in new_summary.splitlines()
                     if "legacy protected" not in line))
    expect("empty report rejected", "", False)
    expect("heading missing rejected", FIXTURE.replace("link-selected view",
                                                       "something else"), False)
    expect("truncated report rejected",
           FIXTURE.split("selected linked bodies")[0], False)

    expect("changed name count rejected",
           FIXTURE.replace("32 multiply-defined name(s)",
                           "33 multiply-defined name(s)"), False)
    expect("changed record count rejected",
           FIXTURE.replace("66 definition record(s)",
                           "65 definition record(s)"), False)
    expect("changed resolved count rejected",
           FIXTURE.replace("32 link-resolved, 0 unresolved",
                           "31 link-resolved, 1 unresolved"), False)
    expect("changed disagreement count rejected",
           FIXTURE.replace("3 name(s) whose copies get different verdicts",
                           "2 name(s) whose copies get different verdicts"),
           False)

    missing = "\n".join(line for line in FIXTURE.splitlines()
                        if "_ZN9ScramblerIhiE5resetEh" not in line) + "\n"
    expect("missing exact row rejected", missing, False)

    expect("wrong selected grade rejected",
           FIXTURE.replace(
               "RELOC       EXACT       _ZN9ScramblerIhiE5resetEh",
               "RELOC       REGALLOC    _ZN9ScramblerIhiE5resetEh"), False)

    control = FIXTURE.replace(
        "    selected linked bodies",
        "    SIZE        EXACT       " + NEGATIVE_CONTROL +
        "\n    selected linked bodies")
    expect("negative control present rejected", control, False)

    expect("unexpected disagreement row rejected",
           FIXTURE.replace(
               "    selected linked bodies",
               "    SIZE        EXACT       _Z7unexpected\n"
               "    selected linked bodies"), False)

    # --- the phase summary and its cross-checks ----------------------------
    expect("summary missing rejected", FIXTURE, False, None)
    expect("summary heading missing rejected", FIXTURE, False,
           SUMMARY_FIXTURE.replace(SUMMARY_HEADING, "something else"))
    expect("end-target line missing rejected", FIXTURE, False,
           "\n".join(line for line in SUMMARY_FIXTURE.splitlines()
                     if "end target" not in line) + "\n")
    expect("unavailable link-selected metric rejected", FIXTURE, False,
           SUMMARY_FIXTURE.replace(
               "link-selected EXACT   1149",
               "link-selected EXACT : unavailable (run make partial-link)"))
    expect("grade-0-or-1 below EXACT rejected", FIXTURE, False,
           SUMMARY_FIXTURE.replace("1182 / 1852", "1145 / 1852"))
    expect("link-selected EXACT below EXACT rejected", FIXTURE, False,
           SUMMARY_FIXTURE.replace("link-selected EXACT   1149",
                                   "link-selected EXACT   1145"))
    expect("selected/exact difference mismatch rejected", FIXTURE, False,
           SUMMARY_FIXTURE.replace("link-selected EXACT   1149",
                                   "link-selected EXACT   1150"))
    two = FIXTURE.replace(
        "    BYTES       EXACT       _ZN9ScramblerIhhEC1Ejjj",
        "    EXACT       EXACT       _ZN9ScramblerIhhEC1Ejjj")
    expect("selected-only disagreement pin rejected", two, False,
           SUMMARY_FIXTURE.replace("link-selected EXACT   1149",
                                   "link-selected EXACT   1148"))
    expect("displaced ledger line missing rejected", FIXTURE, False,
           "\n".join(line for line in SUMMARY_FIXTURE.splitlines()
                     if "displaced" not in line) + "\n")

    bad = sum(not ok for _, ok in checks)
    for label, ok in checks:
        print("  %s  %s" % ("ok  " if ok else "FAIL", label))
    print("\n  %d case(s), %d failure(s)" % (len(checks), bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("report", nargs="?",
                    help="stderr report from byteident.py --link-selected")
    ap.add_argument("--summary", metavar="PATH",
                    help="stdout file from byteident.py --summary (required "
                         "outside --self-test)")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.report:
        ap.error("REPORT is required unless --self-test is given")
    if not args.summary:
        ap.error("--summary is required unless --self-test is given")

    try:
        with open(args.report) as stream:
            text = stream.read()
        with open(args.summary) as stream:
            summary_text = stream.read()
    except OSError as error:
        print("link_selected_check: cannot read input: %s" % error,
              file=sys.stderr)
        return 1

    failures = check(text, summary_text)
    if failures:
        print("link_selected_check: %d pinned expectation(s) FAILED:"
              % len(failures), file=sys.stderr)
        for failure in failures:
            print("  FAIL  %s" % failure, file=sys.stderr)
        return 1

    print("link_selected_check: %d name(s), %d record(s), %d resolved, "
          "%d disagreement(s); %d exact-selected name(s) as pinned; "
          "legacy protected %d/%d, link-selected EXACT %d vs per-TU EXACT %d"
          % (EXPECTED["names"], EXPECTED["records"], EXPECTED["resolved"],
             EXPECTED["disagreements"], len(EXACT_SELECTED),
             parse_summary(summary_text)["grade01"],
             parse_summary(summary_text)["compared"],
             parse_summary(summary_text)["selected"],
             parse_summary(summary_text)["exact"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
