#!/usr/bin/env python3
"""Enumerate a function's local-declaration order with the period compiler.

This is a code-generation measurement tool.  It never edits the source tree:
each candidate is a complete copied translation unit, compiled by GCC 3.4.2
with DSPLIB_REPRODUCE_BUGS enabled.  It is for a bounded domain only -- the
caller supplies the complete list of independent declaration names.
"""
import argparse
import hashlib
import itertools
import pathlib
import re
import shlex
import subprocess

from experiment_toolchain import (DEFAULT_IMAGE, compiler_path, docker_prefix,
                                  native_user, print_identity, compile_shell)

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_FLAGS = (
    "-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 "
    "-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args "
    "-I/src/include -D__SIZEOF_POINTER__=4 "
    "-include /src/tools/toolchain/period_compat.h "
).split()


def body_size(path, symbol):
    out = subprocess.check_output(["nm", "-S", "--defined-only", str(path)],
                                  text=True)
    for line in out.splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[3] == symbol:
            return int(fields[1], 16)
    raise RuntimeError("%s not defined in %s" % (symbol, path))


def instruction_count(path, symbol):
    out = subprocess.check_output(["objdump", "-dr", "-Mintel",
                                   "--disassemble=" + symbol, str(path)],
                                  text=True)
    return sum(1 for line in out.splitlines()
               if re.match(r"^\s*[0-9a-f]+:\s", line))


def declaration_block(text, names):
    lines = text.splitlines(True)
    wanted = set(names)
    for start in range(len(lines) - len(names) + 1):
        block = lines[start:start + len(names)]
        got = []
        for line in block:
            found = [n for n in names
                     if re.search(r"\b%s\b" % re.escape(n), line)]
            if len(found) != 1 or not line.rstrip().endswith(";"):
                break
            got.append(found[0])
        if set(got) == wanted and len(set(got)) == len(names):
            return start, block
    raise RuntimeError("could not find one contiguous declaration block for %s"
                       % ", ".join(names))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", required=True, type=pathlib.Path)
    ap.add_argument("--symbol", required=True)
    ap.add_argument("--names", required=True,
                    help="comma-separated independent local names")
    ap.add_argument("--blob", type=pathlib.Path,
                    default=ROOT / "ref/slmodemd/dsplibs.o")
    ap.add_argument("--work", type=pathlib.Path,
                    default=pathlib.Path("/tmp/declorder"))
    ap.add_argument("--extra", default="",
                    help="extra period GCC flags, e.g. '-O2'")
    ap.add_argument("--image", default=DEFAULT_IMAGE)
    ap.add_argument("--compiler-path", default=None,
                    help="directory prepended to PATH inside the image (defaults "
                         "to the selected image's compiler)")
    users = ap.add_mutually_exclusive_group()
    users.add_argument("--native-user", dest="native_user", action="store_true",
                       help="run as the image's native user")
    users.add_argument("--host-user", dest="native_user", action="store_false",
                       help="run as the host UID (for an alternate image)")
    ap.set_defaults(native_user=None)
    a = ap.parse_args()
    source = a.source.resolve()
    names = [x.strip() for x in a.names.split(",") if x.strip()]
    if len(names) < 2 or len(set(names)) != len(names):
        ap.error("--names needs at least two distinct names")
    text = source.read_text()
    start, block = declaration_block(text, names)
    a.work = a.work.resolve()
    out = a.work / "out"
    if a.work.exists():
        if not a.work.is_dir():
            ap.error("--work is not a directory: %s" % a.work)
        try:
            occupied = next(a.work.iterdir())
        except StopIteration:
            occupied = None
        if occupied is not None:
            ap.error("--work must be a new or empty directory: %s "
                     "(found %s); choose another --work" % (a.work, occupied))
    try:
        a.work.mkdir(parents=True, exist_ok=True)
        out.mkdir()
    except OSError as exc:
        ap.error("cannot prepare empty --work directory %s: %s" % (a.work, exc))
    image_path = compiler_path(a.image, a.compiler_path)
    run_native = native_user(a.image, a.native_user)
    print_identity(a.image, image_path, run_native)
    ref_size = body_size(a.blob, a.symbol)
    ref_insns = instruction_count(a.blob, a.symbol)
    rows = []
    for n, perm in enumerate(itertools.permutations(block)):
        candidate = a.work / source.name
        lines = text.splitlines(True)
        lines[start:start + len(block)] = perm
        candidate.write_text("".join(lines))
        obj = out / ("%03d.o" % n)
        flags = DEFAULT_FLAGS + shlex.split(a.extra)
        cmd = docker_prefix(a.image, ROOT, a.work, run_native, output=out,
                            work_target="/variant")
        cmd += ["/bin/sh", "-c", compile_shell(
            image_path, flags, "/out/%03d.o" % n, "/variant/" + source.name)]
        subprocess.check_call(cmd)
        rows.append((body_size(obj, a.symbol), instruction_count(obj, a.symbol),
                     hashlib.sha256(obj.read_bytes()).hexdigest(), n))
    groups = {}
    for size, insns, digest, n in rows:
        groups.setdefault((size, insns, digest), []).append(n)
    print("declaration order: %d candidates; blob %d bytes, %d instructions; "
          "%d distinct objects" % (len(rows), ref_size, ref_insns, len(groups)))
    for (size, insns, digest), indices in sorted(groups.items(),
                                                  key=lambda x: (abs(x[0][0] - ref_size),
                                                                 abs(x[0][1] - ref_insns), x[0])):
        print("  %3d candidate(s): %d bytes (%+d), %d instructions (%+d), %s"
              % (len(indices), size, size - ref_size, insns, insns - ref_insns,
                 ",".join(map(str, indices))))


if __name__ == "__main__":
    main()
