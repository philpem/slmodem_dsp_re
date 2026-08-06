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
OUT=${TC_OUT:-/tmp/tc_out}
FLAGS="-O2 -march=i386 -mfpmath=387 -fomit-frame-pointer -maccumulate-outgoing-args -Iinclude"

# MAKEFLAGS is cleared and the directory banner suppressed: run from inside a
# make recipe, both leak `make[1]: Entering directory ...` and a jobserver
# warning into the variable, and the container then tries to compile them.
SRC=$(MAKEFLAGS= make -s --no-print-directory print-SRC | sed 's/^SRC = //')
CXXSRC=$(MAKEFLAGS= make -s --no-print-directory print-CXXSRC | sed 's/^CXXSRC = //')

rm -rf "$OUT"; mkdir -p "$OUT"
docker run --rm --platform linux/386 \
  -v "$PWD:/src" -v "$OUT:/out" -w /src dsplibs-tc sh -c "
    fail=0
    for f in $SRC; do
      gcc -c $FLAGS -o /out/\$(echo \$f | tr / _).o \$f 2>/dev/null || { echo \"  FAIL \$f\"; fail=\$((fail+1)); }
    done
    for f in $CXXSRC; do
      g++ -c $FLAGS -fno-exceptions -fno-rtti -o /out/\$(echo \$f | tr / _).o \$f 2>/dev/null || { echo \"  FAIL \$f\"; fail=\$((fail+1)); }
    done
    echo \"period toolchain: \$(ls /out | wc -l) objects, \$fail failed\""
