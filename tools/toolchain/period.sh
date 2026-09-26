#!/bin/sh
#
# period.sh -- the differential suite, built and run by the ORIGINAL compiler.
#
#   make period                 everything
#   make period T=t_resampler   one binary, or a list
#
# WHY THIS IS THE GOLD STANDARD.
#
# When the default gate built with modern GCC, every place the two compilers
# disagreed had to be absorbed SOMEWHERE, and the only place available was the
# reconstruction's own source: `round32`'s `volatile` in Resampler.cpp forces
# GCC 13 to emit a store the object has and modern GCC otherwise optimises away
# (finding F1352).  The tests then passed and the source drifted away from what
# the author wrote, with every gate green.  That is the failure mode this target
# closes.
#
# Here, our source and the object are compiled by the SAME compiler and
# compared at runtime.  A difference is then a difference in the code, not in
# the toolchain, which is the only reading that supports the project's claim.
#
# The modern build stays as an explicit opt-in portability check and a much
# faster inner loop.  It is not the thing that decides.
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
# FLAGS COME FROM tools/toolchain/period.mk, NOT FROM THE TOP-LEVEL MAKEFILE,
# because period.mk's set was
# derived from the object (findings F606, F607, F612, F616) and the Makefile's was
# not.  Three of the Makefile's must NOT appear here:
#
#   -fno-lifetime-dse    postdates 3.4.2 (finding 32634's date argument); its
#                        ABSENCE is the period semantic, which is exactly why
#                        the modern build has to ask for it (finding F1272).
#   -fno-pie             no PIE to disable; the Dockerfile establishes the
#   -fno-stack-protector Gentoo ssp/pie patches were off in the object anyway.
#
set -e
cd "$(dirname "$0")/../.."

# THE DEFAULT IMAGE IS THE PUBLISHED GENTOO-PATCHED GCC 3.4.2-r2 NAMED BY THE
# BLOB.  Its compiler and binutils are the reconstruction authority.
# `PERIOD_IMG=dsplibs-tc342` and
# `PERIOD_IMG=dsplibs-tc` select the stock 3.4.2 and Debian 3.4.4 arms only
# for explicit A/B measurements.
# Set PERIOD_OUT with it: build/period is incremental on SOURCE mtime and
# does not notice that the compiler changed.
GENTOO_IMG=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest
IMG=${PERIOD_IMG:-$GENTOO_IMG}

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    if [ "$IMG" = "$GENTOO_IMG" ]; then
        echo "tools/toolchain: pulling recovered Gentoo image '$IMG'" >&2
        docker pull --platform linux/386 "$IMG"
    else
        echo "tools/toolchain: no docker image '$IMG'." >&2
        echo "The recovered Gentoo image is published at '$GENTOO_IMG'." >&2
        exit 1
    fi
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
# variable, and the container then tries to compile them.  period.mk's note.
SRC=$(MAKEFLAGS= make -s --no-print-directory print-SRC | sed 's/^SRC = //')
CXXSRC=$(MAKEFLAGS= make -s --no-print-directory print-CXXSRC | sed 's/^CXXSRC = //')
ASRC=$(MAKEFLAGS= make -s --no-print-directory print-ASRC | sed 's/^ASRC = //')

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

# GCC's 2005 Gentoo driver invokes `whoami`; an arbitrary host UID is absent
# from that stage3's passwd file and makes the driver select its bootstrap
# compiler.  Run that one image as root, then restore the bind mount's
# ownership before returning.  The stock images keep the ordinary host UID.
run_stage() {
    stage=$1
    if [ "$IMG" = "$GENTOO_IMG" ]; then
        docker run --rm --name "$NAME" --platform linux/386 \
            -v "$PWD:/src" -v "$PWD/$OUT:/out" -w /src \
            -e "SRC=$SRC" -e "CXXSRC=$CXXSRC" -e "ASRC=$ASRC" -e "TESTS=$TESTS" -e "J=$J" -e "REF=$REF" \
            -e "KEEP=${KEEP:-}" -e "OUT_UID=$(id -u)" -e "OUT_GID=$(id -g)" \
            -e "VISIBLE=${VISIBLE:-}" -e "STAGE=$stage" \
            "$IMG" sh -c '
                sh /src/tools/toolchain/period_inner.sh
                status=$?
                chown -R "$OUT_UID:$OUT_GID" /out
                exit "$status"
            '
    else
        docker run --rm --name "$NAME" --user "$(id -u):$(id -g)" \
            --platform linux/386 -v "$PWD:/src" -v "$PWD/$OUT:/out" -w /src \
            -e "SRC=$SRC" -e "CXXSRC=$CXXSRC" -e "ASRC=$ASRC" -e "TESTS=$TESTS" -e "J=$J" -e "REF=$REF" \
            -e "KEEP=${KEEP:-}" -e "VISIBLE=${VISIBLE:-}" -e "STAGE=$stage" \
            "$IMG" sh /src/tools/toolchain/period_inner.sh
    fi
}

# COMPILE, then globalize on the HOST, then LINK.  Two container passes because
# binutils 2.15 -- the period linker -- predates `objcopy --globalize-symbols`;
# the modern host objcopy performs that step on the compiled objects, in a copy
# under $OUT/testhost.  The mtime is preserved so the link stage's incremental
# check (`KEEP`) still knows what moved.
#
# THE NAME LIST COMES FROM THE PERIOD OBJECTS, NOT THE HOST TREE.  `make period`
# must not build $(OBJ_REPRO) with the host compiler -- the deciding build is
# the container's, and the CI period job has no host 32-bit headers.  The
# compile stage writes srcobjs.list; testvisible.py reads those objects.
run_stage compile

if [ -s "$OUT/srcobjs.list" ]; then
    VISIBLE="$OUT/test_visible.txt"
    # The compile stage recorded container paths (/out/...); translate them to
    # this host's view before the tool opens them.
    : > "$OUT/srcobjs_host.list"
    while read -r o; do
        [ -n "$o" ] || continue
        echo "$OUT/${o#/out/}" >> "$OUT/srcobjs_host.list"
    done < "$OUT/srcobjs.list"
    python3 tools/testvisible.py --from-list "$OUT/srcobjs_host.list" \
        --tests test -o "$VISIBLE" --redefine "$OUT/test_redefine.txt"
    mkdir -p "$OUT/testhost"
    while read -r o; do
        [ -n "$o" ] || continue
        o="${o#/out/}"			# the container sees /out, we see $OUT
        g="$OUT/testhost/$(basename "$o")"
        # GCC 4+ renames an internal-linkage C++ variable; GCC 3.4.2, the
        # period compiler, already emits the plain name so the redefine list
        # is empty here and this pass is a no-op.
        objcopy --redefine-syms="$OUT/test_redefine.txt" "$OUT/$o" "$g.redef"
        objcopy --globalize-symbols="$VISIBLE" "$g.redef" "$g"
        rm -f "$g.redef"
        touch -r "$OUT/$o" "$g"
    done < "$OUT/srcobjs.list"
fi

run_stage link
