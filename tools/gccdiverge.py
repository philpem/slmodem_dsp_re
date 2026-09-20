#!/usr/bin/env python3
"""gccdiverge.py -- run one test, and excuse ONLY the checks a modern compiler
provably cannot get right.

    tools/gccdiverge.py t_resampler build/test/t_resampler
    tools/gccdiverge.py --list

WHAT THIS IS FOR, and it is a narrow thing.

The reconstruction is compared against an object built by GCC 3.4.2.  At a few
sites the modern compiler CANNOT reproduce that object from correct source --
not because our source is wrong, but because the two compilers keep different
intermediate precision.  `four1` in src/dsp/fft.cpp is the worked example
(finding F1354): the object narrows its butterfly temporaries to `float`
mid-loop because GCC 3.4.2 runs out of x87 registers, GCC 13 does not run out
and keeps 80 bits, and NOTHING in standard C moves it -- not a plain
assignment, not an explicit `(float)` cast, not a named `double` temporary.
Only `volatile`, which is a shim for GCC 13 alone.

AND MOST SITES DO NOT NEED AN ENTRY, which is the point of trying first.
src/pump/v90/Resampler.cpp looked identical and was not: accumulating in a
`double` and narrowing with an explicit conversion satisfies BOTH compilers,
because a double-to-float conversion is one the compiler must really perform.
Reach for this register only after that kind of spelling has failed.

Before `make period` existed, the only way to keep the gate green was to force
the store from the SOURCE -- a `volatile` that exists for the compiler and not
for the object.  That is backwards: it makes the reconstruction less like what
the author wrote in order to satisfy a compiler the author never used.

So the source gets corrected, `make period` proves it against the object with
the period compiler, and the modern build declares the site here.

THE DISCIPLINE, because an allow-list is otherwise a place where failures go
to be forgotten:

  * ALL ENTRIES ARE BLOCKED PENDING MIGRATION. Legacy `checks` arrays name
    GROUPS, not assertions. Literal-output v1 is diagnostic-only: a Boolean
    strcmp or abbreviated object failure can hide different underlying defects
    at the same site/input. It CANNOT authorize an exemption, even if copied
    output matches. Lossless evidence and assertion/build identity are required.
  * A STALE ENTRY IS AN ERROR.  If an allow-listed test passes, this exits
    non-zero and says so.  Otherwise the register silently accumulates
    excuses for problems that fixed themselves, and the next real regression
    hides behind one.
  * IT APPLIES TO THE MODERN BUILD ONLY.  tools/toolchain/period_inner.sh
    never consults this file.  `make period` has no allow-list and is not
    getting one: the period compiler has no excuse, because it is the
    compiler the object was built with.
  * EVERY ENTRY CITES A FINDING.  "GCC 13 differs here" is not a reason; the
    finding is where the disassembly, the instruction sequence and the
    measurement live.

An entry records a historical divergence claim, not established compiler
causation or approval. See docs/tolerance-split-register-review.md for all 14
blocked entries and their missing evidence. No migration is enabled here.
"""

import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REGISTER = os.path.join(HERE, "gccdiverge.json")

def literal_output_matches(entry, returncode, stdout, stderr):
    """Diagnostic syntax/equality check ONLY; this cannot authorize an exemption.

    Boolean strcmp failures at the same site/input can serialize unrelated
    underlying defects identically. Object diagnostics can abbreviate bytes.
    Even a literal match is NOT lossless assertion evidence and cannot establish
    compiler/build identity. Migration needs a separate, reviewed structured
    evidence protocol; copying stdout into v1 is explicitly insufficient.
    """
    if returncode != 1:
        return False, "abnormal exit (only ordinary harness exit 1 is eligible)"
    contract = entry.get("exact_transcript_v1")
    if not isinstance(contract, dict):
        return False, "BLOCKED: legacy group-only entry needs assertion-level review"
    if stdout != contract.get("stdout") or stderr != contract.get("stderr"):
        return False, "output differs from reviewed assertion/input transcript"
    if not stdout.endswith("\n") or not stderr.endswith("\n"):
        return False, "incomplete output (missing final newline)"
    lines = stdout.splitlines()
    summary = re.compile(r"^(PASS|FAIL)\s+(.+?)\s+(?:(\d+)/)?(\d+) checks"
                         r"( failed)?(?: \(\d+ within modern tolerance\))?$")
    total = failures = groups = 0
    for line in lines:
        match = summary.fullmatch(line)
        if not match:
            return False, "unparsed output (transcript v1 accepts summaries only)"
        status, name, bad, count, suffix = match.groups()
        count, bad = int(count), int(bad or 0)
        if count <= 0 or bad > count or ((status == "FAIL") != (bad > 0)):
            return False, "invalid or zero denominator"
        if (status == "FAIL") != bool(suffix):
            return False, "malformed summary"
        total += count
        failures += bad
        groups += 1
    diagnostics = stderr.splitlines()
    if not groups or not failures or len(diagnostics) != failures:
        return False, "missing/truncated failure diagnostics or summaries"
    if any(not re.fullmatch(r"[^\n]+:\d+: .+  got .+, reference .+", line)
           for line in diagnostics):
        return False, "unparsed assertion diagnostic"
    return True, "%d failures / %d checks in %d groups; exact transcript" % (
        failures, total, groups)


def declared_failure(entry, returncode, stdout, stderr):
    matched, reason = literal_output_matches(entry, returncode, stdout, stderr)
    if matched:
        return False, ("BLOCKED: literal-output v1 is insufficient; missing lossless "
                       "underlying evidence and assertion/build identity")
    return False, reason


def load():
    with open(REGISTER) as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("test", nargs="?")
    ap.add_argument("binary", nargs="?")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    reg = load()

    if args.list:
        if not reg:
            print("gccdiverge: no entries -- the modern build matches the "
                  "object everywhere it is tested")
            return 0
        for name, e in sorted(reg.items()):
            print("%s  (finding %s)" % (name, e.get("finding", "?")))
            print("    BLOCKED: migration requires lossless structured evidence; "
                  "literal-output v1 cannot authorize exemptions")
            for c in e.get("checks", []):
                print("    %s" % c)
            print("    %s" % e.get("why", "").strip())
        return 0

    if not args.test or not args.binary:
        ap.error("need a test name and a binary")

    r = subprocess.run([args.binary], capture_output=True, text=True)
    out = r.stdout + r.stderr
    entry = reg.get(args.test)

    if r.returncode == 0:
        sys.stdout.write(out)
        if entry:
            # Good news the gate must not swallow.  Either the compiler
            # improved or the reason expired; either way the register is now
            # lying, and a lying register is how the next real failure hides.
            print("  STALE  %s is allow-listed in tools/gccdiverge.json and "
                  "now PASSES." % args.test)
            print("         Delete the entry.  If the reason expired, say so "
                  "in finding %s." % entry.get("finding", "?"))
            return 1
        return 0

    sys.stdout.write(out)
    if not entry:
        return r.returncode

    allowed, reason = declared_failure(entry, r.returncode, r.stdout, r.stderr)
    if not allowed:
        print("  REJECTED %s: %s" % (args.test, reason))
        return 1

    # No approval protocol is implemented. Fail closed even if a future edit
    # accidentally lets the diagnostic primitive return True here.
    print("  BLOCKED: no lossless evidence migration protocol is enabled")
    return 1


if __name__ == "__main__":
    sys.exit(main())
