#!/usr/bin/env python3
"""Which deviation-register entries have a test behind them?

`docs/deviations.md` records every place the reconstruction does not behave
identically to the original, and every defect found in the original along the
way. Each entry is a CLAIM about behaviour. This asks the obvious follow-up
question, which `docs/fastpass.md` deferred to task #47: is the claim measured,
or is it an assertion nobody has driven?

The test applied here is a NECESSARY condition, not a sufficient one: does any
compiled test object reference the `ref_` alias of the function the entry names?
If not, no differential test drives that function at all and the entry cannot
have been measured. If so, the function is under test -- which does not prove
the specific deviant path is, only that it could be.

So a "no" here is conclusive and a "yes" is an invitation to look. That
asymmetry is the point: it turns 59 entries into a short list worth reading.

TWO WAYS A "NO" IS STILL WRONG, both met in practice (finding 622):

  - A TABLE USED INTERNALLY by a tested function is exercised without any test
    naming it.  D42's `StateName` is read by `hs_setstate`, which the transcript
    sweep drives over all 87 states; no test mentions `ref_StateName` and the
    table is thoroughly driven.
  - AN AMBIGUOUS ALIAS.  `ref_AGC_DEF_ALPHA` exists at SIX addresses, because
    the name is file-static in six translation units and objcopy renamed them
    all alike.  A test declaring it binds to whichever the linker picks, so the
    absence of such a test is a feature, not a gap.

Read a "no" as "look at this entry", never as "write a test".

    tools/devaudit.py [--verbose]
"""

import glob
import os
import re
import subprocess
import sys

REG = "docs/deviations.md"


def tested_symbols(build="build"):
    """Every `ref_NAME` a compiled test object actually references."""
    out = set()
    objs = glob.glob(os.path.join(build, "test", "**", "*.o"), recursive=True)
    for o in objs:
        txt = subprocess.run(["nm", "--undefined-only", o],
                             capture_output=True, text=True).stdout
        for m in re.finditer(r"\bref_(\S+)", txt):
            out.add(m.group(1))
    return out


def entries(path=REG):
    """[(id, heading, [names it mentions in backticks])]"""
    rows = []
    for block in re.split(r"^## ", open(path).read(), flags=re.M)[1:]:
        head = block.split("\n", 1)[0]
        m = re.match(r"(D\d+)", head)
        if not m:
            continue
        # Names from the heading first -- that is the entry's subject -- then
        # from the Module line, which gives the file.
        names = re.findall(r"`([A-Za-z_][A-Za-z0-9_:<>~ ]*)`", head)
        body = block.split("\n", 1)[1] if "\n" in block else ""
        mod = re.search(r"\*\*Modules?\*\*(.*)", body)
        rows.append((m.group(1), head.strip(), names,
                     mod.group(1).strip() if mod else ""))
    return rows


def main():
    verbose = "--verbose" in sys.argv
    tested = tested_symbols()
    if not tested:
        sys.exit("no ref_ references found -- build the tests first")

    untested, covered, unnamed = [], [], []
    for did, head, names, mod in entries():
        # Strip C++ decorations; the alias is on the mangled name, and these
        # headings name classes as often as functions.
        cands = [n.strip() for n in names if n and " " not in n.strip()]

        #
        # THE MODULE LINE IS BETTER EVIDENCE THAN THE HEADING.  A heading names
        # whatever reads well -- a class, a table, a libc function -- and
        # matching those against the alias set produces confident nonsense:
        # D3's subject is `FixedRC`, whose symbols are all `RcFixed_*`, and
        # D62's heading happens to contain `sysdep_malloc`, which is the
        # harness's.  Both read as unmeasured and both are driven.
        #
        # So: if the entry names a source file, ask whether ANY symbol defined
        # in that file is under test.  That is the same necessary condition on
        # firmer ground.
        #
        for f in re.findall(r"`(src/[^`]+\.c(?:pp)?)`", mod):
            o = os.path.join("build", f[:-2] + ".o") if f.endswith(".c") \
                else os.path.join("build", f[:-4] + ".o")
            defined = subprocess.run(["nm", "--defined-only", o],
                                     capture_output=True, text=True).stdout
            cands += re.findall(r"\b[TtWw]\s+(\S+)", defined)

        if not cands:
            unnamed.append((did, head))
            continue
        hit = [c for c in cands if c in tested
               or any(c in t for t in tested)]
        (covered if hit else untested).append((did, head, cands, hit))

    print("Deviation register: %d entries, %d ref_ symbols referenced by tests\n"
          % (len(entries()), len(tested)))
    print("NO TEST DRIVES THE NAMED FUNCTION -- the claim is unmeasured:\n")
    for did, head, cands, _ in untested:
        print("  %-5s %s" % (did, head[:72]))
    print("\n  %d of %d entries name something no test references."
          % (len(untested), len(entries())))
    print("  %d name something under test (necessary, not sufficient)."
          % len(covered))
    if unnamed:
        print("  %d name nothing this tool can resolve." % len(unnamed))
    if verbose:
        print("\nUnder test:")
        for did, head, cands, hit in covered:
            print("  %-5s %-46s via %s" % (did, head[:46], ", ".join(hit[:2])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
