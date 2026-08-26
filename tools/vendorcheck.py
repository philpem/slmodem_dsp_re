#!/usr/bin/env python3
"""Are the vendored slmodem headers still verbatim, and still upstream's?

WHY THIS EXISTS

`third_party/slmodem/*.h` are slmodemd's own headers, copied byte for byte.
They matter because they are the OTHER SIDE of the ABI `dsplibs.o` was
compiled against: `struct dp`, `struct dp_operations`, `enum DP_ID` and the
`DPSTAT_*` codes are not inferred from the disassembly here, they are stated
by the project the blob links into.  Before they were vendored this tree
carried a hand copy, and `include/dsplib/dp.h` said so in its own header
comment -- "the originals are in slmodemd/modem_dp.h; they are duplicated here
so this tree builds standalone".  A hand copy of a layout is a divergence
waiting to happen and no test can see it: both halves compile, and every
offset in the loser is quietly wrong.  That is `onedef.py`'s argument, one
repository out.

**VERBATIM MEANS THE VENDORED FILE IS NEVER EDITED.**  Every accommodation
lands on our side -- a wrapper header, a `#define` around a C++ keyword, an
extra `-I`.  That rule is the whole value: it is what makes a drift check mean
something, and the moment one local "small fix" goes in, the copies stop being
evidence and become a second opinion.

TWO DIFFERENT FAILURES, AND ONE OF THEM CANNOT ALWAYS BE CHECKED

1. **The copies against `tools/vendor.json`.**  Catches a local edit, an added
   file nobody recorded, a deleted one.  Always checkable, because both sides
   are in git.
2. **Upstream against the copies.**  Catches upstream moving underneath us.
   Only checkable when upstream is THERE -- and in a fresh clone or an agent
   worktree it is not, which is exactly how `third_party/spandsp` used to fail
   at the top of a 1,573-line log everybody read the tail of.

So an absent upstream SKIPS check 2 and says so, with the file count, and does
not report a clean run.  A skip that renders as a pass is finding F2400's
shape and this tree has been bitten by it four times.

THE DECOYS ARE REAL.  Three same-named directories exist under `d-modem/`.
`tools/vendor.json` records the upstream PATH as well as the hashes, so
"checked against upstream" names which upstream.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

VENDOR = "third_party/slmodem"
MANIFEST = "tools/vendor.json"


def sha256(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def load():
    if not os.path.exists(MANIFEST):
        sys.exit("vendorcheck.py: %s is missing -- there is nothing to check "
                 "against, which is not the same as everything being fine."
                 % MANIFEST)
    m = json.load(open(MANIFEST))
    if not m.get("files"):
        sys.exit("vendorcheck.py: %s records NO files.  Every check below "
                 "would run over an empty set and exit 0.  Finding F2400."
                 % MANIFEST)
    return m


def check_copies(man, vendor=VENDOR):
    """The copies against the manifest.  Returns (problems, checked)."""
    bad = []
    recorded = man["files"]
    present = sorted(f for f in os.listdir(vendor)) if os.path.isdir(vendor) else []
    present_h = [f for f in present if f.endswith(".h")]

    for name, want in sorted(recorded.items()):
        p = os.path.join(vendor, name)
        if not os.path.exists(p):
            bad.append((name, "RECORDED BUT MISSING", ""))
            continue
        got = sha256(p)
        if got != want:
            bad.append((name, "EDITED", "%s -> %s" % (want[:16], got[:16])))
    for name in present_h:
        if name not in recorded:
            bad.append((name, "PRESENT BUT NOT RECORDED", ""))
    return bad, len(recorded)


def check_upstream(man, vendor=VENDOR, upstream=None):
    """Upstream against the copies.  Returns (problems, checked) or None."""
    up = upstream or man.get("upstream")
    if not up or not os.path.isdir(up):
        return None, up
    bad = []
    for name in sorted(man["files"]):
        u = os.path.join(up, name)
        v = os.path.join(vendor, name)
        if not os.path.exists(u):
            bad.append((name, "GONE UPSTREAM", ""))
            continue
        if not os.path.exists(v):
            continue                      # check 1 already reported it
        a, b = sha256(u), sha256(v)
        if a != b:
            bad.append((name, "UPSTREAM DRIFTED", "%s -> %s" % (b[:16], a[:16])))
    return bad, up


#
# A CHECKER THAT HAS NEVER BEEN SEEN TO FAIL IS NOT KNOWN TO WORK.  `extcheck`
# printed "(none)" through four broken versions.  This one builds a throwaway
# tree, breaks it three ways, and requires each break to be reported.
#
def self_test():
    man = load()
    names = sorted(man["files"])
    if len(names) < 2:
        sys.exit("vendorcheck.py --self-test needs at least two recorded files")
    tmp = tempfile.mkdtemp(prefix="vendorcheck-")
    try:
        v = os.path.join(tmp, "vendor")
        shutil.copytree(VENDOR, v)
        cases = []

        # clean
        bad, n = check_copies(man, v)
        cases.append(("an untouched copy is clean", not bad))

        # edited: one byte appended
        with open(os.path.join(v, names[0]), "ab") as f:
            f.write(b"\n")
        bad, n = check_copies(man, v)
        cases.append(("an EDITED copy is caught",
                      any(b[1] == "EDITED" for b in bad)))
        shutil.rmtree(v); shutil.copytree(VENDOR, v)

        # deleted
        os.remove(os.path.join(v, names[1]))
        bad, n = check_copies(man, v)
        cases.append(("a DELETED copy is caught",
                      any(b[1] == "RECORDED BUT MISSING" for b in bad)))
        shutil.rmtree(v); shutil.copytree(VENDOR, v)

        # added but unrecorded
        open(os.path.join(v, "sneaked_in.h"), "w").write("/* not recorded */\n")
        bad, n = check_copies(man, v)
        cases.append(("an UNRECORDED addition is caught",
                      any(b[1] == "PRESENT BUT NOT RECORDED" for b in bad)))
        shutil.rmtree(v); shutil.copytree(VENDOR, v)

        # upstream drift, simulated by pointing --upstream at a mangled tree
        u = os.path.join(tmp, "upstream")
        shutil.copytree(VENDOR, u)
        with open(os.path.join(u, names[0]), "ab") as f:
            f.write(b"/* upstream moved */\n")
        bad, up = check_upstream(man, v, u)
        cases.append(("UPSTREAM DRIFT is caught",
                      bad is not None and any(b[1] == "UPSTREAM DRIFTED"
                                              for b in bad)))

        # absent upstream must SKIP, not pass
        bad, up = check_upstream(man, v, os.path.join(tmp, "nowhere"))
        cases.append(("an ABSENT upstream skips rather than passing",
                      bad is None))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    fails = 0
    for what, ok in cases:
        fails += not ok
        print("  %-5s %s" % ("ok" if ok else "FAIL", what))
    print("\n  %d case(s), %d must-CATCH, %d failure(s)"
          % (len(cases), 4, fails))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--upstream", metavar="DIR",
                    help="override the recorded upstream path")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--bless", action="store_true",
                    help="rewrite the manifest from the current copies -- ONLY "
                         "after a deliberate re-vendoring, and say why in the "
                         "commit")
    a = ap.parse_args()

    if a.self_test:
        return self_test()

    man = load()

    if a.bless:
        files = sorted(f for f in os.listdir(VENDOR) if f.endswith(".h"))
        man["files"] = {f: sha256(os.path.join(VENDOR, f)) for f in files}
        json.dump(man, open(MANIFEST, "w"), indent=2, sort_keys=False)
        open(MANIFEST, "a").write("\n")
        print("blessed %d file(s) into %s" % (len(files), MANIFEST))
        return 0

    bad, n = check_copies(man)
    print("vendored headers, %s:" % VENDOR)
    if bad:
        for name, why, detail in bad:
            print("  %-22s %-26s %s" % (name, why, detail))
        sys.exit(
            "\nvendorcheck.py: %d of %d vendored file(s) are NOT VERBATIM.\n"
            "  These are slmodemd's headers and they are never edited here --\n"
            "  every accommodation belongs on our side, in include/dsplib or\n"
            "  in a wrapper.  A hand-modified vendored header is a hand copy\n"
            "  again, and a hand copy is what vendoring them removed.\n"
            "  If the change is a deliberate RE-VENDORING from a newer\n"
            "  upstream, re-copy all of them and run --bless in that commit."
            % (len(bad), n))
    print("  %d file(s) match tools/vendor.json byte for byte  OK" % n)

    bad, up = check_upstream(man, upstream=a.upstream)
    if bad is None:
        print("  upstream %s is NOT PRESENT -- the %d file(s) above were checked\n"
              "  against the manifest only, and upstream drift was NOT checked.\n"
              "  That is expected in a fresh clone or an agent worktree; it is\n"
              "  reported rather than passed over, because a skip that renders\n"
              "  as a pass is finding F2400." % (up, n))
        return 0
    if bad:
        for name, why, detail in bad:
            print("  %-22s %-26s %s" % (name, why, detail))
        sys.exit(
            "\nvendorcheck.py: upstream at %s has MOVED under %d vendored\n"
            "  file(s).  Decide deliberately: re-vendor (copy, --bless, say in\n"
            "  the commit what changed and whether any boundary type moved), or\n"
            "  record why this tree stays on the older text.  What must not\n"
            "  happen is the two drifting quietly, which is the whole reason\n"
            "  they are vendored." % (up, len(bad)))
    print("  and match upstream %s byte for byte  OK" % up)
    return 0


if __name__ == "__main__":
    sys.exit(main())
