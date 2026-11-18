#!/usr/bin/env python3
"""Verify a delegated batch from the workers' JSON reports, in ONE call.

The failure mode this closes: a subagent's result message is lost or blank while
its edits are already in the tree.  Rather than trust prose (or its absence),
each worker writes a small JSON report; this tool re-derives every numeric claim
from the repo -- rebuild, grade, setcc multiset, canonical symmetric difference
-- and prints a PASS/FAIL table, then lists `git status` so an edit with no
report is visible.

Report schema (a list of objects, one per claim; unknown keys ignored):

    [{"symbol": "<demangled>", "object": "src_x_Y.c.o",
      "source": "src/x/Y.c", "md5": "<object md5>",
      "delta": <int|null>, "candidate_bytes": <int|null>, "insns": <int|null>,
      "run": "N/M"|null, "symdiff": <int|null>,
      "setcc_ref": <int|null>, "setcc_our": <int|null>,
      "retained": true|false, "fixtures": ["t_one", ...],
      "note": "short"}]

A null field is simply not checked.  `--json-out FILE` writes the derived table.

    python3 scratch/cd/verify_report.py REPORT.json [REPORT2.json ...] [--json-out F]

Exit status: 0 when every non-null claim matches, 1 otherwise (including a
report that cannot be read or a file that does not rebuild).
"""
import hashlib
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools", "toolchain"))

OBJROOT = os.path.join(REPO, "build", "tc_out")
BLOB = os.path.join(REPO, "ref", "slmodemd", "dsplibs.o")
SETCC = re.compile(r"set[a-z]+ +%[a-z0-9]+")


def rebuild(obj):
    path = os.path.join(OBJROOT, obj)
    if os.path.exists(path):
        os.remove(path)
    r = subprocess.run(
        ["make", "-f", "tools/toolchain/period.mk", path],
        cwd=REPO, capture_output=True, text=True)
    return os.path.exists(path), (r.stdout + r.stderr)[-400:]


def setcc_count(objpath, sym):
    """setcc mnemonics inside the symbol's own disassembly block only.

    `objdump -d --disassemble=SYM` prints EVERY section header when the symbol
    is missing, so scope to the block that starts at `^<addr> <sym>:$`.
    """
    out = subprocess.run(["objdump", "-d", "--disassemble=" + sym, objpath],
                         capture_output=True, text=True).stdout
    lines = out.splitlines()
    start = None
    for i, l in enumerate(lines):
        if re.match(r"^[0-9a-f]+ <" + re.escape(sym) + r">:$", l):
            start = i
            break
    if start is None:
        return None
    block = []
    for l in lines[start + 1:]:
        if re.match(r"^[0-9a-f]+ <", l):
            break
        block.append(l)
    return sorted(SETCC.findall("\n".join(block)))


def grade(obj, sym):
    r = subprocess.run(["python3", "tools/toolchain/byteident.py", "--why", sym],
                       cwd=REPO, capture_output=True, text=True)
    txt = r.stdout + r.stderr
    g = re.search(r"fidelity (\w+)", txt)
    d = re.search(r"delta ([+-]?\d+)", txt)
    c = re.search(r"candidate (\d+) instructions", txt)
    run = re.search(r"contiguous instruction run: (\d+) / (\d+)", txt)
    return {
        "fidelity": g.group(1) if g else None,
        "delta": int(d.group(1)) if d else None,
        "insns": int(c.group(1)) if c else None,
        "run": f"{run.group(1)}/{run.group(2)}" if run else None,
    }


def symdiff(obj, sym):
    """Total canonical-multiset symmetric difference, MULTIPLICITY form.

    This is the definition `tools/toolchain/variant_run.py` prints (the sum of
    |ref - ours| over the multiset), NOT the count of distinct differing keys
    that the older /tmp/opencode/symdiff.py reports.  Two different metrics were
    in use for a while; standardise on this one, because a metric that ignores
    multiplicity understates a site repeated many times (F12184).
    """
    import collections
    import byteident as B
    import fidelity
    def rows(path):
        return [fidelity.canonical((m, o)) for m, o in B.insns(path, sym)
                if not B._padding(m, o)]
    r = collections.Counter(rows(BLOB))
    o = collections.Counter(rows(os.path.join(OBJROOT, obj)))
    return sum((r - o).values()) + sum((o - r).values())


def main(argv):
    reports, json_out = [], None
    i = 0
    while i < len(argv):
        if argv[i] == "--json-out":
            i += 1
            json_out = argv[i]
        else:
            reports.append(argv[i])
        i += 1
    if not reports:
        print(__doc__)
        return 2

    claims = []
    for path in reports:
        try:
            with open(path) as f:
                data = json.load(f)
        except Exception as e:                                    # noqa: BLE001
            print(f"FAIL  cannot read report {path}: {e}")
            return 1
        if isinstance(data, dict):
            data = data.get("claims", [data])
        claims.extend(data)

    bad = 0
    print(f"{'sym':34} {'obj md5':>10} {'delta':>12} {'insns':>12} "
          f"{'run':>12} {'symdiff':>10} {'setcc':>12}  verdict")
    rows = []
    for c in claims:
        sym, obj = c.get("symbol"), c.get("object")
        if not sym or not obj:
            print("FAIL  claim without symbol/object:", c)
            bad += 1
            continue
        objpath = os.path.join(OBJROOT, obj)
        ok, tail = rebuild(obj)
        if not ok:
            print(f"FAIL  {sym}: does not rebuild\n{tail}")
            bad += 1
            continue
        md5 = hashlib.md5(open(objpath, "rb").read()).hexdigest()
        g = grade(obj, sym)
        sd = symdiff(obj, sym)
        our = setcc_count(objpath, sym)
        ref = setcc_count(BLOB, sym)
        got = {
            "delta": g["delta"], "candidate_bytes": None, "insns": g["insns"],
            "run": g["run"], "symdiff": sd,
            "setcc_ref": len(ref) if ref is not None else None,
            "setcc_our": len(our) if our is not None else None,
            "md5": md5,
        }
        # candidate_bytes comes from the diagnostic text, not the grade line
        r = subprocess.run(["python3", "tools/toolchain/byteident.py", "--why", sym],
                           cwd=REPO, capture_output=True, text=True)
        m = re.search(r"candidate (\d+),", r.stdout + r.stderr)
        if m:
            got["candidate_bytes"] = int(m.group(1))

        checks = [(k, c.get(k), got.get(k)) for k in
                  ("md5", "delta", "candidate_bytes", "insns", "run",
                   "symdiff", "setcc_ref", "setcc_our") if c.get(k) is not None]
        bad_here = [f"{k}: claimed {a!r}, actual {b!r}" for k, a, b in checks if a != b]
        verdict = "PASS" if not bad_here else "FAIL " + "; ".join(bad_here)
        if bad_here:
            bad += 1
        rows.append({"symbol": sym, "object": obj, "claimed": c, "actual": got,
                     "verdict": verdict, "note": c.get("note", "")})
        print(f"{sym[:34]:34} {md5[:8]:>10} {str(g['delta']):>12} "
              f"{str(g['insns']):>12} {str(g['run']):>12} {str(sd):>10} "
              f"{got['setcc_our']!s}/{got['setcc_ref']!s:<6} {verdict}")

    print("\n--- git status (an edit with no claim above is UNREPORTED) ---")
    print(subprocess.run(["git", "status", "--short", "src/", "include/"],
                         cwd=REPO, capture_output=True, text=True).stdout.rstrip()
          or "(clean)")
    if json_out:
        with open(json_out, "w") as f:
            json.dump(rows, f, indent=1)
    print(f"\n{len(claims) - bad} / {len(claims)} claims verified")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
