#!/usr/bin/env python3
"""Enumerate selected top-level function-definition orders with period GCC.

The compiler's register-allocation state can carry across a translation unit.
This tool measures that finite source-order domain without editing the source
tree.  It moves complete definitions selected by ``--names``; comments and
other text retain their original positions because they cannot affect codegen.
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
FLAGS = ("-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 "
         "-mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args "
         "-I/src/include -D__SIZEOF_POINTER__=4 "
         "-include /src/tools/toolchain/period_compat.h").split()


def definitions(text, names):
    """Return complete definition spans, using blank lines as signature starts."""
    spans = []
    for name in names:
        match = re.search(r"(?m)^%s\s*\(" % re.escape(name), text)
        if not match:
            raise RuntimeError("no top-level definition for %s" % name)
        start = text.rfind("\n\n", 0, match.start()) + 2
        brace = text.find("{", match.end())
        semi = text.find(";", match.end(), brace if brace >= 0 else len(text))
        if brace < 0 or semi >= 0:
            raise RuntimeError("%s is not a definition" % name)
        depth = 0
        end = None
        for pos in range(brace, len(text)):
            if text[pos] == "{":
                depth += 1
            elif text[pos] == "}" and not (pos and text[pos - 1] == "\\"):
                depth -= 1
                if depth == 0:
                    end = pos + 1
                    break
        if end is None:
            raise RuntimeError("unclosed definition for %s" % name)
        spans.append((start, end, text[start:end], name))
    if len({(x[0], x[1]) for x in spans}) != len(spans):
        raise RuntimeError("definition spans overlap")
    return sorted(spans)


def substitute(text, spans, permutation):
    pieces = []
    cursor = 0
    for (start, end, _, _), replacement in zip(spans, permutation):
        pieces.append(text[cursor:start])
        pieces.append(replacement)
        cursor = end
    pieces.append(text[cursor:])
    return "".join(pieces)


def metrics(path, symbol):
    for line in subprocess.check_output(["nm", "-S", "--defined-only", str(path)],
                                        text=True).splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[3] == symbol:
            return int(fields[1], 16)
    raise RuntimeError("%s is not defined" % symbol)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", required=True, type=pathlib.Path)
    ap.add_argument("--symbol", required=True, help="symbol to measure")
    ap.add_argument("--names", required=True, help="comma-separated definitions")
    ap.add_argument("--work", required=True, type=pathlib.Path)
    ap.add_argument("--extra", default="", help="extra period GCC flags")
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
    if len(names) < 2 or len(names) > 7 or len(names) != len(set(names)):
        ap.error("--names needs 2..7 distinct definitions")
    text = source.read_text()
    spans = definitions(text, names)
    a.work = a.work.resolve()
    a.work.mkdir(parents=True, exist_ok=True)
    image_path = compiler_path(a.image, a.compiler_path)
    run_native = native_user(a.image, a.native_user)
    print_identity(a.image, image_path, run_native)
    rows = []
    for n, perm in enumerate(itertools.permutations(spans)):
        candidate = a.work / source.name
        candidate.write_text(substitute(text, spans, [x[2] for x in perm]))
        output = a.work / ("%03d.o" % n)
        flags = FLAGS + shlex.split(a.extra)
        cmd = docker_prefix(a.image, ROOT, a.work, run_native, output=a.work,
                            work_target="/variant")
        cmd += ["/bin/sh", "-c", compile_shell(
            image_path, flags, "/out/%03d.o" % n, "/variant/" + source.name)]
        subprocess.check_call(cmd)
        rows.append((metrics(output, a.symbol), hashlib.sha256(output.read_bytes()).hexdigest(),
                     n, tuple(x[3] for x in perm)))
    groups = {}
    for size, digest, n, order in rows:
        groups.setdefault((size, digest), []).append((n, order))
    print("function order: %d candidates; %d distinct objects" % (len(rows), len(groups)))
    for (size, _), candidates in sorted(groups.items()):
        rendered = "; ".join("%03d=%s" % (n, ",".join(order))
                             for n, order in candidates)
        print("  %3d candidate(s): %d bytes, %s" %
              (len(candidates), size, rendered))


if __name__ == "__main__":
    main()
