#!/bin/sh
#
# Build every reconstructed translation unit with the period toolchain and
# leave the objects where compare.py looks for them.  See the Dockerfile for
# what the toolchain is and compare.py for where each flag came from.
#
# Nothing here touches build/ -- the project's own build is untouched and this
# output is for comparison only.
#
set -e
cd "$(dirname "$0")/../.."
#
# UNDER build/, NOT UNDER /tmp, so `make clean` reaches it.  This defaulted to
# /tmp/tc_out and wrote its manifest to /tmp/tc_manifest.txt -- 152 objects and
# an index that nothing in the tree ever removed, and that two concurrent
# worktrees would have written over each other.
#
OUT=${TC_OUT:-$PWD/build/tc_out}
# ONE DELIBERATE DIVERGENCE FROM `make period`, AND ONLY ONE: `-mno-ieee-fp`.
# The object's float compares are ordered -- 406 against four, and those four
# are inside libm -- so this build needs the flag to compare like for like.
# `make period` does NOT have it: with it, that tier goes 181 passed to 176,
# failing five suites on NaN and near-NaN inputs, because the flag also lets
# GCC invert a comparison and swap the branch.  Finding 1990 has the numbers
# and says what has to happen before the two can be reconciled.
#
# OTHERWISE THE SAME FLAGS `make period` USES, and they must stay the same.  The two
# diverged once and it cost real coverage: this script passed neither
# -D__SIZEOF_POINTER__=4 nor the compat header, so it compiled a smaller set
# than the period differential AND silently elided the 81 offset assertions
# guarded on that predefine -- V3 in docs/method/compilers.md, the variance
# that fails OPEN.
#
# -std=gnu99 is NOT here and is not an oversight: it is the C dialect the
# TEST HARNESS needs, and this script compiles only src/.
# ONE LINE, deliberately: $FLAGS is interpolated into the `docker ... sh -c`
# string below, where a newline ends the command rather than separating words.
FLAGS="-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387 -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h"

# TWO KNOBS, BOTH FOR A/B MEASUREMENT AND NEITHER A WAY TO CHANGE THE BUILD.
#
#   TC_IMAGE=dsplibs-tc      the OLD image, Debian sarge's GCC 3.4.4; the
#                            default is now `dsplibs-tc342`, GCC 3.4.2 itself
#                            (Dockerfile.exact).  Finding 2200
#   TC_EXTRA="-O2"           APPENDED after $FLAGS, so a repeat of an option
#                            overrides the one above -- `-O2` beats the `-O3`,
#                            `-mieee-fp` beats the `-mno-ieee-fp`.  That is how
#                            finding 2200's arms were taken without editing
#                            this line, which is what a flag conclusion has to
#                            be measured against.
#
# Set TC_OUT as well when you set either, or you will compare one arm against
# another arm's leftovers.  Findings 2155 and 1990 are the flag record; this
# is not a supported way to build the tree differently from what they say.
IMAGE=${TC_IMAGE:-dsplibs-tc342}
FLAGS="$FLAGS $TC_EXTRA"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "tools/toolchain: no docker image '$IMAGE'.  Build it with" >&2
    echo "  docker build --platform linux/386 \\" >&2
    echo "    -f tools/toolchain/Dockerfile.exact -t dsplibs-tc342 tools/toolchain" >&2
    echo "(the older 3.4.4 image is Dockerfile, -t dsplibs-tc.  Finding 2200.)" >&2
    exit 1
fi

# MAKEFLAGS is cleared and the directory banner suppressed: run from inside a
# make recipe, both leak `make[1]: Entering directory ...` and a jobserver
# warning into the variable, and the container then tries to compile them.
SRC=$(MAKEFLAGS= make -s --no-print-directory print-SRC | sed 's/^SRC = //')
CXXSRC=$(MAKEFLAGS= make -s --no-print-directory print-CXXSRC | sed 's/^CXXSRC = //')

rm -rf "$OUT"; mkdir -p "$OUT"

# Object name -> source path.  The names are the path with slashes turned into
# underscores, which is NOT reversible: `src/core/dp_wrapper.c` and a directory
# called `dp` produce the same string.  Record the mapping rather than guess it.
for f in $SRC $CXXSRC; do
    echo "$(echo "$f" | tr / _).o $f"
done > "$OUT/tc_manifest.txt"
# See tools/toolchain/period.sh for why --rm alone is not the whole of
# cleaning up: --name gives the trap a handle, and --user keeps root-owned
# objects out of the tree.
NAME="dsplibs-tcbuild-$$"
cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT INT TERM

docker run --rm --name "$NAME" --user "$(id -u):$(id -g)" --platform linux/386 \
  -v "$PWD:/src" -v "$OUT:/out" -w /src "$IMAGE" sh -c "
    fail=0
    for f in $SRC; do
      gcc -c $FLAGS -o /out/\$(echo \$f | tr / _).o \$f 2>/dev/null || { echo \"  FAIL \$f\"; fail=\$((fail+1)); }
    done
    for f in $CXXSRC; do
      g++ -c $FLAGS -fno-exceptions -fno-rtti -o /out/\$(echo \$f | tr / _).o \$f 2>/dev/null || { echo \"  FAIL \$f\"; fail=\$((fail+1)); }
    done
    echo \"period toolchain: \$(ls /out | wc -l) objects, \$fail failed, gcc \$(gcc -dumpversion)\""
