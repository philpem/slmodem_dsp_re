#!/bin/sh
#
# period.sh -- the differential suite, built and run by the ORIGINAL compiler.
#
#   make period                 everything
#   make period T=t_resampler   one binary, or a list
#
# WHY THIS IS THE GOLD STANDARD AND `make phase` IS NOT.
#
# `make phase` builds with GCC 13.  The object was built with GCC 3.4.2.  So
# every place the two compilers disagree, the disagreement had to be absorbed
# SOMEWHERE, and the only place available was the reconstruction's own source:
# `round32`'s `volatile` in Resampler.cpp forces GCC 13 to emit a store the
# object has and modern GCC otherwise optimises away (finding 1352).  The
# tests then pass and the source has drifted away from what the author wrote,
# with every gate green.  That is the failure mode this target closes.
#
# Here, our source and the object are compiled by the SAME compiler and
# compared at runtime.  A difference is then a difference in the code, not in
# the toolchain, which is the only reading that supports the project's claim.
#
# The modern build stays, and stays required to compile and pass -- it is a
# portability check and a much faster inner loop.  It is no longer the thing
# that decides.
#
# THREE JOINS HAD TO BE PROVEN BEFORE ANY OF THIS WAS WORTH WRITING, and the
# third is the one that looked least likely:
#
#   1. GCC 3.4.2 compiles src/ -- 152 of 152, once the C++11 opaque enums
#      became C++98 definitions.  See docs/method/compilers.md.
#   2. It compiles test/harness and test/unit, needing only -std=gnu99: GCC
#      3.4 defaults to gnu89, where `for (int i = 0; ...)` is an error.
#   3. binutils 2.15 links the blob -- which modern objcopy has rewritten to
#      produce build/dsplibs_ref.o -- against period-compiled objects, and the
#      result RUNS.  It does, and the blob's own alaw2linear(0x55) returns -8
#      from a binary built entirely by the 2005 toolchain.
#
# -static, and it is not a preference.  A dynamic link fails with
#
#	/usr/bin/ld: Not enough room for program headers (allocated 8, need 9)
#
# because the blob carries more sections than ld 2.15's default layout leaves
# segments for.  Static linking sidesteps the program-header table entirely.
#
# FLAGS COME FROM build.sh, NOT FROM THE MAKEFILE, because build.sh's set was
# derived from the object (findings 606, 607, 612, 616) and the Makefile's was
# not.  Three of the Makefile's must NOT appear here:
#
#   -fno-lifetime-dse    postdates 3.4.2 (finding 32634's date argument); its
#                        ABSENCE is the period semantic, which is exactly why
#                        the modern build has to ask for it (finding 1272).
#   -fno-pie             no PIE to disable; the Dockerfile establishes the
#   -fno-stack-protector Gentoo ssp/pie patches were off in the object anyway.
#
set -e
cd "$(dirname "$0")/../.."

# THE IMAGE IS GCC 3.4.2 ITSELF SINCE FINDING 2200 -- this used to default to
# `dsplibs-tc`, which is Debian sarge's 3.4.4 prerelease and not the compiler
# every comment in this directory claims.  `PERIOD_IMG=dsplibs-tc` still
# selects the old one, which is how the two were compared; both are green here
# at 183 passed / 0 failed, so this tier does not decide between them.
IMG=${PERIOD_IMG:-dsplibs-tc342}

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "tools/toolchain: no docker image '$IMG'.  Build it with" >&2
    echo "  docker build --platform linux/386 \\" >&2
    echo "    -f tools/toolchain/Dockerfile.exact -t dsplibs-tc342 tools/toolchain" >&2
    echo "(the older 3.4.4 image is Dockerfile, -t dsplibs-tc.  Finding 2200.)" >&2
    exit 1
fi
REF=${REF:-build/dsplibs_ref.o}
#
# HALF THE CORES, because this NESTS.  `make phase` already runs at -j$(nproc)
# and one of its recipes is this script, which then asks the container for
# another J compilers -- so a naive `nproc` here is up to twice the machine's
# worth of concurrent GCC, and the box stops being usable for anything else
# for the duration.
#
# `J=12 make period` if you want the machine to yourself.
#
J=${J:-$(( $(nproc 2>/dev/null || echo 4) / 2 ))}
[ "$J" -ge 1 ] 2>/dev/null || J=1

#
# INCREMENTAL BY DEFAULT, and it is sound rather than merely fast.  An object
# is reused only if it is newer than its source AND newer than the newest
# header in the tree, so a header edit rebuilds everything -- see
# period_inner.sh.  That is what lets this be a GATE: `make phase` runs it on
# every invocation, and on an unchanged tree it relinks rather than
# recompiling 156 translation units.
#
# `KEEP= make period` for a full rebuild, if ever a doubt needs settling.
#
KEEP=${KEEP-1}

[ -f "$REF" ] || { echo "period: $REF is missing -- run 'make $REF' first" >&2; exit 1; }

# MAKEFLAGS is cleared and the banner suppressed: run from inside a make
# recipe, both leak "Entering directory" and a jobserver warning into the
# variable, and the container then tries to compile them.  build.sh's note.
SRC=$(MAKEFLAGS= make -s --no-print-directory print-SRC | sed 's/^SRC = //')
CXXSRC=$(MAKEFLAGS= make -s --no-print-directory print-CXXSRC | sed 's/^CXXSRC = //')

# `make period T=...` narrows to named binaries.  The whole suite is ~90
# links against a 1.2 MB object, so the narrow form is the inner loop.
if [ -n "${T:-}" ]; then
	TESTS=$T
else
	TESTS=$(ls test/unit/t_*.c test/unit/t_*.cpp 2>/dev/null |
	        sed 's|.*/||; s|\.cpp$||; s|\.c$||' | sort -u)
fi

# The object directory is MOUNTED rather than left inside the container, so a
# second run relinks instead of recompiling 156 translation units.  `make
# period` on an unchanged tree is then seconds rather than minutes, which is
# what makes it usable as a gate instead of a ceremony.
OUT=${PERIOD_OUT:-build/period}
mkdir -p "$OUT"

#
# CLEANING UP AFTER ITSELF, three ways, because `--rm` alone covers only one.
#
#   --rm      the container goes when it exits.  Necessary, not sufficient:
#             it is the DAEMON that honours it on exit, so a run killed
#             before the container is up -- Ctrl-C, a `timeout`, an agent
#             giving up -- can still leave one behind.
#   --name    so the trap below has something to name.  $$ is in it because
#             sibling worktrees run this concurrently and a fixed name would
#             have them killing each other's container.
#   --user    so nothing ROOT-OWNED is left in the tree.  The container is
#             root by default and $OUT is bind-mounted, so every object it
#             wrote landed owned by root inside build/.  `rm -rf` still works
#             (deletion needs write on the DIRECTORY, which is ours), so
#             nothing was broken -- but a build tree salted with root-owned
#             files is a thing to hand someone else, and it only takes a flag.
#
NAME="dsplibs-period-$$"
cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT INT TERM

docker run --rm --name "$NAME" --user "$(id -u):$(id -g)" \
    --platform linux/386 -v "$PWD:/src" -v "$PWD/$OUT:/out" -w /src \
    -e "SRC=$SRC" -e "CXXSRC=$CXXSRC" -e "TESTS=$TESTS" -e "J=$J" -e "REF=$REF" \
    -e "KEEP=${KEEP:-}" \
    "$IMG" sh /src/tools/toolchain/period_inner.sh
