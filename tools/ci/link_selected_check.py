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
exit status of byteident are NOT checked here: this tool reads only the
diagnostic report and never grades a reconstruction.  The negative control
(`_Z7hanningIfEvPT_j`) is a disagreement whose selected copy is not EXACT; if it
ever appears in the report the pin no longer describes master.

    tools/ci/link_selected_check.py REPORT
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

DENOMINATOR = re.compile(
    r"denominator\s*:\s*(\d+)\s+multiply-defined name\(s\),\s*"
    r"(\d+)\s+definition record\(s\);")
RESOLVED = re.compile(r"(\d+)\s+link-resolved,\s*(\d+)\s+unresolved")
DISAGREEMENTS = re.compile(
    r"disagreements:\s*(\d+)\s+name\(s\) whose copies get different verdicts")
ROW = re.compile(r"^\s*(EXACT|UNRESOLVED|REGALLOC|RELOC|BYTES|SIZE|NODATA)"
                 r"\s+(EXACT|UNRESOLVED|REGALLOC|RELOC|BYTES|SIZE|NODATA)"
                 r"\s+(\S+)")


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


def check(text):
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


def self_test():
    checks = []

    def expect(label, text, should_pass):
        problems = check(text)
        ok = (not problems) if should_pass else bool(problems)
        checks.append((label, ok))

    expect("known-good fixture passes", FIXTURE, True)
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
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if not args.report:
        ap.error("REPORT is required unless --self-test is given")

    try:
        with open(args.report) as stream:
            text = stream.read()
    except OSError as error:
        print("link_selected_check: cannot read report: %s" % error,
              file=sys.stderr)
        return 1

    failures = check(text)
    if failures:
        print("link_selected_check: %d pinned expectation(s) FAILED:"
              % len(failures), file=sys.stderr)
        for failure in failures:
            print("  FAIL  %s" % failure, file=sys.stderr)
        return 1

    print("link_selected_check: %d name(s), %d record(s), %d resolved, "
          "%d disagreement(s); %d exact-selected name(s) as pinned"
          % (EXPECTED["names"], EXPECTED["records"], EXPECTED["resolved"],
             EXPECTED["disagreements"], len(EXACT_SELECTED)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
