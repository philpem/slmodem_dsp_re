#!/usr/bin/env python3
"""Parallel out-of-tree variant runner for period-compiler experiments.

Builds N source variants of ONE translation unit at once, grades each on the
symbols you name, and prints one comparison table.  Replaces the ad-hoc
per-target `tryXXX.py` harnesses, which patched the tree in place and compiled
serially.

WHY IT IS SAFE
  - It NEVER writes to the source tree.  The base source is read, the edits are
    applied in memory, and each variant is written to its own temporary
    directory which is bind-mounted into the container as /variant.
  - It does not guess the compiler flags.  It asks the build system for the real
    command (`make -n -f tools/toolchain/period.mk <abs obj>`), so per-TU flag
    sets (TC_DCR_FLAGS, TC_FAX_PROC_FLAGS, TC_V90P4_FLAGS, TC_V90DEMOD_FLAGS, and
    the C-vs-C++ split) come along automatically.
  - Every variant whose `find` string is absent is marked INVALID and NOT
    measured, so a bad patch can never be reported as a result.
  - The baseline variant is checked against the in-tree object by md5.  If the
    out-of-tree path does not reproduce it byte for byte, the whole run is a
    control failure and no verdict may be drawn from it.

USAGE
    python3 tools/toolchain/variant_run.py --spec SPEC.json [--jobs N] [--control-none]
    python3 tools/toolchain/variant_run.py --self-test

SPEC.json
    {
      "source":  "src/dsp/fpm_tone.c",          # repo-relative, required
      "symbols": ["FPM_TONE_detect"],           # required, one or more
      "base":    "/tmp/opencode/fpm_tone.base.c",   # optional alternative base
      "extra_flags": ["-fno-unswitch-loops"],       # optional, appended
      "variants": [
        {"name": "V0 baseline", "edits": []},
        {"name": "V1 nested",   "edits": [["if (a && b)", "if (a)\n\tif (b)"]]}
      ]
    }

An edit is [find, replace] (first occurrence) or [find, replace, "all"].
Each variant's source is left at <workdir>/<name>/<basename> for inspection.

VALIDATION
    `--self-test` asserts the three invariants a run depends on (baseline fidelity,
    absent-find refusal, flags reaching the compiler) against a known control
    (V90SignBitsExtractor::process).  Run it after touching this file.

Deliberately imports ONLY tools/toolchain: putting tools/ on sys.path shadows
the stdlib `dis` module and breaks pyelftools.
"""
import argparse
import collections
import concurrent.futures as cf
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools", "toolchain"))

import byteident as B          # noqa: E402
import fidelity                # noqa: E402

OBJROOT = os.path.join(REPO, "build", "tc_out")
SETCC = re.compile(r"set[a-z]+ +%[a-z0-9]+")


# ---------------------------------------------------------------- build plumbing

def objname(source):
    return source.replace("/", "_") + ".o"


def compile_template(source):
    """Ask the build system for the real docker compile line for `source`."""
    obj = os.path.join(OBJROOT, objname(source))
    r = subprocess.run(
        ["make", "-n", "-f", "tools/toolchain/period.mk", obj],
        cwd=REPO, capture_output=True, text=True, shell=False)
    lines = [l for l in r.stdout.splitlines() if l.startswith("docker run")]
    if not lines:
        raise SystemExit("variants.py: could not read a docker compile line "
                         "for %s from `make -n`" % source)
    argv = shlex.split(lines[-1])
    return argv


def variant_argv(template, source, variant_src, out_dir):
    """Retarget the template at an out-of-tree source and a private /out."""
    argv = list(template)
    # private output mount (the template has exactly one "<repo>/build/tc_out:/out")
    for i, tok in enumerate(argv):
        if tok.endswith("/build/tc_out:/out"):
            argv[i] = out_dir + ":/out"
    # extra read-only mount for the variant directory.  It MUST go among the
    # `docker run` options, i.e. BEFORE the image name -- anything after the
    # image is handed to the compiler.
    image = next(i for i, tok in enumerate(argv) if "gentoo" in tok or
                 (":" in tok and tok.endswith(":latest")))
    argv[image:image] = ["-v", os.path.dirname(variant_src) + ":/variant:ro"]
    # the source argument is the last token; the include root stays /src
    base = os.path.basename(source)
    if argv[-1] != source:
        raise SystemExit("variants.py: template source argument is %r, not %r"
                         % (argv[-1], source))
    argv[-1] = "/variant/" + base
    return argv


def build(template, source, variant_src, workdir, extra_flags, jobs_tag):
    out_dir = os.path.join(workdir, "out")
    os.makedirs(os.path.join(out_dir, ".deps"), exist_ok=True)
    argv = variant_argv(template, source, variant_src, out_dir)
    if extra_flags:
        # append AFTER the source so they cannot be reordered away
        argv.extend(extra_flags)
    r = subprocess.run(argv, capture_output=True, text=True, shell=False)
    obj = os.path.join(out_dir, objname(source))
    return (r.returncode == 0 and os.path.exists(obj)), obj, r.stderr.strip()


# -------------------------------------------------------------------- measuring

def rows(path, sym):
    try:
        return [fidelity.canonical((m, o))
                for m, o in B.insns(path, sym) if not B._padding(m, o)]
    except Exception:
        return None


def measure(obj, sym, blob):
    info = {}
    env = dict(os.environ, TC_OUT=os.path.dirname(obj))
    r = subprocess.run([sys.executable, "tools/toolchain/byteident.py",
                        "--why", sym],
                       cwd=REPO, capture_output=True, text=True, env=env)
    g = re.search(r"fidelity (\w+)", r.stdout)
    d = re.search(r"delta ([+-]?\d+)", r.stdout)
    run = re.search(r"run: (\d+) / (\d+)", r.stdout)
    cnt = re.search(r"candidate (\d+) instructions", r.stdout)
    info["grade"] = g.group(1) if g else "?"
    info["delta"] = int(d.group(1)) if d else None
    info["run"] = "%s/%s" % (run.group(1), run.group(2)) if run else "-"
    info["insns"] = int(cnt.group(1)) if cnt else None

    def cc(path):
        out = subprocess.run(["objdump", "-d", "--disassemble=" + sym, path],
                             capture_output=True, text=True, shell=False)
        c = collections.Counter(SETCC.findall(out.stdout))
        mn = collections.Counter(k.split()[0] for k in c.elements())
        return c, mn

    rc, rm = cc(blob)
    oc, om = cc(obj)
    info["setcc_ref"] = sum(rc.values())
    info["setcc_our"] = sum(oc.values())
    info["dset"] = sum((rc - oc).values()) + sum((oc - rc).values())
    info["dmn"] = sum((rm - om).values()) + sum((om - rm).values())

    br, bo = rows(blob, sym), rows(obj, sym)
    if br is None or bo is None:
        info["symdiff"] = None
        info["refn"] = info["ourn"] = None
    else:
        info["refn"], info["ourn"] = len(br), len(bo)
        cr, co = collections.Counter(br), collections.Counter(bo)
        info["symdiff"] = sum((cr - co).values()) + sum((co - cr).values())
    return info


# ------------------------------------------------------------------------- main

def apply_edits(text, edits):
    """Return (ok, text, notes).  ok is False if a find string was absent."""
    notes = []
    for e in edits:
        find, repl = e[0], e[1]
        mode = e[2] if len(e) > 2 else "first"
        n = text.count(find)
        if n == 0:
            return False, text, ["ABSENT: %r" % find[:60]]
        if mode == "all":
            text = text.replace(find, repl)
            notes.append("replaced all %d of %r" % (n, find[:40]))
        else:
            text = text.replace(find, repl, 1)
            notes.append("replaced 1 of %d of %r" % (n, find[:40]))
    return True, text, notes


def self_test():
    """Assert the three invariants a variant run depends on.

    1. the baseline variant reproduces the in-tree object byte for byte
       (the out-of-tree build path is faithful);
    2. an absent `find` string is REFUSED, not silently measured as the base;
    3. a compiler flag really reaches the compiler (a flagged variant's object
       differs from the baseline).
    """
    src = "src/pump/v90/V90SignBitsExtractor.cpp"
    spec = {
        "source": src,
        "symbols": ["_ZN20V90SignBitsExtractor7processEPhS0_"],
        "variants": [
            {"name": "st_baseline", "edits": []},
            {"name": "st_badfind",
             "edits": [["a string that cannot be in this file", "x"]]},
            {"name": "st_flagged", "edits": [], "extra_flags": ["-fno-peephole2"]},
        ],
    }
    d = tempfile.mkdtemp(prefix="variant-selftest-", dir="/tmp/opencode")
    p = os.path.join(d, "spec.json")
    json.dump(spec, open(p, "w"))
    r = subprocess.run([sys.executable, os.path.abspath(__file__), "--spec", p,
                        "--work", d], capture_output=True, text=True)
    out = r.stdout
    print(out)
    fails = []
    if "CONTROL: baseline reproduces the in-tree object: OK" not in out:
        fails.append("invariant 1: baseline did not reproduce the in-tree object")
    if "ABSENT:" not in out:
        fails.append("invariant 2: an absent find string was not refused")
    mds = dict(re.findall(r"^(st_\w+)\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+([0-9a-f]{8})",
                          out, re.M))
    if mds.get("st_baseline") is None or mds.get("st_flagged") is None:
        fails.append("invariant 3: could not read both object hashes")
    elif mds["st_baseline"] == mds["st_flagged"]:
        fails.append("invariant 3: extra_flags did not reach the compiler")
    print()
    if fails:
        print("SELF-TEST FAIL")
        for f in fails:
            print("  - " + f)
        return 1
    print("SELF-TEST PASS (3/3 invariants; baseline %s, flagged %s)"
          % (mds["st_baseline"], mds["st_flagged"]))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--work")
    ap.add_argument("--control-none", action="store_true",
                    help="skip the in-tree md5 control check")
    a = ap.parse_args()

    if a.self_test:
        sys.exit(self_test())
    if not a.spec:
        ap.error("--spec is required (or use --self-test)")

    spec = json.load(open(a.spec))
    source = spec["source"]
    symbols = spec["symbols"]
    variants = spec["variants"]
    extra = spec.get("extra_flags", [])
    blob = os.path.join(REPO, "ref", "slmodemd", "dsplibs.o")

    base_path = spec.get("base") or os.path.join(REPO, source)
    base_text = open(base_path).read()

    work = a.work or tempfile.mkdtemp(prefix="variants-", dir="/tmp/opencode")
    os.makedirs(work, exist_ok=True)
    template = compile_template(source)

    # --- in-tree control object (idempotent rebuild of the unchanged source)
    ctrl = os.path.join(OBJROOT, objname(source))
    if not a.control_none:
        subprocess.run(["make", "-f", "tools/toolchain/period.mk", ctrl],
                       cwd=REPO, capture_output=True, text=True)
    ctrl_md5 = (hashlib.md5(open(ctrl, "rb").read()).hexdigest()
                if os.path.exists(ctrl) else None)

    # --- materialise and build every variant
    jobs = []
    for v in variants:
        name = v["name"]
        ok, text, notes = apply_edits(base_text, v.get("edits", []))
        d = os.path.join(work, name)
        os.makedirs(d, exist_ok=True)
        src = os.path.join(d, os.path.basename(source))
        if not ok:
            jobs.append((v, None, None, notes, "INVALID"))
            continue
        open(src, "w").write(text)
        jobs.append((v, src, d, notes, "ok"))

    results = {}
    with cf.ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {}
        for v, src, d, notes, state in jobs:
            if state != "ok":
                continue
            flags = list(extra) + list(v.get("extra_flags", []))
            futs[ex.submit(build, template, source, src, d, flags, v["name"])] = v
        for f in cf.as_completed(futs):
            v = futs[f]
            results[v["name"]] = f.result()

    print("source   %s" % source)
    print("control  in-tree object %s" % (ctrl_md5 or "(absent)"))
    print("work     %s" % work)
    if extra:
        print("extra    %s" % " ".join(extra))
    print()

    hdr = ("%-26s %-9s %-6s %6s %8s %9s %5s %5s %7s  %s"
           % ("variant", "grade", "delta", "insns", "run", "setcc r/o",
              "dset", "dmn", "symdiff", "note"))
    print(hdr)
    print("-" * len(hdr))

    control_ok = None
    for v, src, d, notes, state in jobs:
        name = v["name"]
        if state != "ok":
            print("%-26s %s" % (name[:26], "; ".join(notes)))
            continue
        ok, obj, err = results[name]
        if not ok:
            print("%-26s BUILD FAILED: %s" % (name[:26], err[:60]))
            continue
        md5 = hashlib.md5(open(obj, "rb").read()).hexdigest()
        is_control = (not v.get("edits")) and (not v.get("extra_flags"))
        if is_control and ctrl_md5:
            control_ok = (md5 == ctrl_md5)
        if len(symbols) > 1:
            for s in symbols:
                m = measure(obj, s, blob)
                print("%-26s %-9s %-6s %6s %8s %4s/%-4s %5s %5s %7s  %s"
                      % (("%s [%s]" % (name, s))[:26], m["grade"], m["delta"],
                         m["insns"], m["run"], m["setcc_ref"], m["setcc_our"],
                         m["dset"], m["dmn"], m["symdiff"], md5[:8]))
        else:
            m = measure(obj, symbols[0], blob)
            note = md5[:8] + " " + "; ".join(notes)[:40]
            flag = ""
            if m["dmn"] == 0 and m["dset"] == 0:
                flag = " setcc=OK"
            elif m["dmn"] == 0:
                flag = " (lane only)"
            print("%-26s %-9s %-6s %6s %8s %4s/%-4s %5s %5s %7s  %s%s"
                  % (name[:26], m["grade"], m["delta"], m["insns"], m["run"],
                     m["setcc_ref"], m["setcc_our"], m["dset"], m["dmn"],
                     m["symdiff"], note, flag))

    if control_ok is not None:
        print()
        print("CONTROL: baseline reproduces the in-tree object: %s"
              % ("OK" if control_ok else "FAIL -- DO NOT TRUST THIS RUN"))
        if not control_ok:
            sys.exit(2)


if __name__ == "__main__":
    main()
