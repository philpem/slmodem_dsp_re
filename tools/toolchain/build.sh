#!/bin/sh
#
# A SHIM.  THE BUILD IS `tools/toolchain/period.mk`.
#
# This used to be the build itself: 106 lines that recompiled all 205 objects,
# serially, in one container, on every run, because `rm -rf $OUT` was the only
# dependency tracking it had.  It is a proper makefile now -- one rule per
# object, real `-MMD -MP` header dependencies, and `-j` -- so an edited source
# recompiles one object and an unchanged tree compiles nothing.
#
# THIS FILE STAYS BECAUSE NINE TOOLS NAME IT IN THEIR ERROR MESSAGES and tell
# the reader to run it, and because agents were running against that
# instruction when it changed.  Prefer `make tc` in anything new; every knob
# still works, because they are passed straight through:
#
#     TC_IMAGE=dsplibs-tc342-gentoo sh tools/toolchain/build.sh
#     TC_EXTRA=-O2 make tc
#
# `-D__SIZEOF_POINTER__=4` IS NOT IN THIS FILE ANY MORE, and that matters:
# `tools/assertlive.py` checks the flag is still passed by reading the build
# scripts, so its BUILDERS list names `period.mk` and `period_inner.sh`.  A
# shim is not a place to look for a flag.
#
cd "$(dirname "$0")/../.." || exit 1

# The same halving `make`'s own `J` does -- never `$(nproc)`, which leaves the
# machine unusable for the real-time DSP work this tree also does.  Under the
# script this build was serial; a shim that stayed serial would be SLOWER than
# what it replaced, because there is now one container per object.
J=${J:-$(( $(nproc 2>/dev/null || echo 4) / 2 ))}
[ "$J" -ge 1 ] 2>/dev/null || J=1

exec make -f tools/toolchain/period.mk -j"$J" "$@"
