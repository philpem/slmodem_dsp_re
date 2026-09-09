#!/usr/bin/env python3
"""Compile one translation unit under a declared period-GCC flag matrix.

This is a code-generation experiment, not a build mechanism.  Each named
variant gets a fresh object, compiled with the supplied period image and with
DSPLIB_REPRODUCE_BUGS forced on.  The output reports the reference symbol's
size and instruction-count deltas; callers must still use byteident.py before
claiming a recovery.

The matrix is a text file: one ``name: extra compiler flags`` entry per line.
It is deliberately explicit: a result can say exactly which compiler domain
was enumerated, and the same file can be rerun against another period image.
"""
import argparse
import os
import pathlib
import re
import shlex
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE_FLAGS = (
    "-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 "
    "-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args "
    "-I/src/include -D__SIZEOF_POINTER__=4 "
    "-include /src/tools/toolchain/period_compat.h "
    "-DDSPLIB_REPRODUCE_BUGS"
)


def symbol_metrics(path, symbol):
    symbols = subprocess.check_output(["nm", "-S", "--defined-only", str(path)],
                                      text=True)
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[3] == symbol:
            start = int(fields[0], 16)
            size = int(fields[1], 16)
            break
    else:
        raise RuntimeError("%s is not defined by %s" % (symbol, path))
    dis = subprocess.check_output(["objdump", "-d", "-Mintel", "--no-show-raw-insn",
                                   "--disassemble=" + symbol, str(path)],
                                  text=True)
    # `-r` prints relocation records as ``address: R_386_*`` and their
    # address prefix resembles an instruction.  Count disassembly without
    # relocations; byteident.py remains the acceptance test.
    instructions = 0
    for line in dis.splitlines():
        match = re.match(r"^\s*([0-9a-f]+):\s+[a-z]", line)
        if match and start <= int(match.group(1), 16) < start + size:
            instructions += 1
    return size, instructions


def variants(path):
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            raise RuntimeError("%s:%d: expected 'name: flags'" % (path, lineno))
        name, flags = line.split(":", 1)
        if not re.match(r"^[a-zA-Z0-9_.-]+$", name.strip()):
            raise RuntimeError("%s:%d: invalid variant name" % (path, lineno))
        yield name.strip(), shlex.split(flags)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", required=True, type=pathlib.Path)
    ap.add_argument("--symbol", required=True)
    ap.add_argument("--matrix", required=True, type=pathlib.Path)
    ap.add_argument("--work", required=True, type=pathlib.Path)
    ap.add_argument("--image", default="dsplibs-tc342")
    ap.add_argument("--compiler-path", default="/opt/gcc342/bin",
                    help="directory prepended to PATH inside the image")
    ap.add_argument("--base-flags", default=BASE_FLAGS)
    ap.add_argument("--blob", type=pathlib.Path,
                    default=ROOT / "ref/slmodemd/dsplibs.o")
    a = ap.parse_args()
    source = a.source.resolve()
    a.work = a.work.resolve()
    a.work.mkdir(parents=True, exist_ok=True)
    ref_size, ref_insns = symbol_metrics(a.blob, a.symbol)
    rows = list(variants(a.matrix.resolve()))
    if not rows:
        ap.error("the matrix has no variants")
    print("flag matrix: %d variants; blob %d bytes, %d instructions; image %s"
          % (len(rows), ref_size, ref_insns, a.image))
    for name, extra in rows:
        output = a.work / (name + ".o")
        cmd = ["docker", "run", "--rm", "--user", "%d:%d" %
               (os.getuid(), os.getgid()), "--platform", "linux/386",
               "-v", "%s:/src" % ROOT, "-v", "%s:/out" % a.work,
               "-w", "/src", a.image, "/bin/sh", "-lc",
               "export PATH=%s:$PATH; exec gcc -c %s %s -o /out/%s %s" %
               (a.compiler_path, shlex.join(shlex.split(a.base_flags)),
                shlex.join(extra), output.name, "/src/" + str(source.relative_to(ROOT)))]
        subprocess.check_call(cmd)
        size, insns = symbol_metrics(output, a.symbol)
        print("  %-20s %4d bytes (%+d), %3d instructions (%+d)  %s" %
              (name, size, size - ref_size, insns, insns - ref_insns,
               shlex.join(extra) or "(base)"))


if __name__ == "__main__":
    main()
