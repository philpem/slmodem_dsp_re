#!/bin/sh
#
# The half of period.sh that runs INSIDE the container.  Kept in its own file
# rather than as a `sh -c` string because quoting a hundred lines through two
# shells is how the earlier attempt lost its error messages: `2>/dev/null` on
# a compile whose failure was the thing being measured.
#
# Reads SRC, CXXSRC, TESTS, J and REF from the environment.  See period.sh.
#
set -e

OUT=/out
mkdir -p "$OUT"

# The object's flags (findings F606, F607, F612, F616), plus three of ours:
#   -Itest/harness            the tests' own headers
#   -D__SIZEOF_POINTER__=4    a GCC 4.6+ predefine.  Without it the 81 offset
#                             assertions guarded on it read `#if 0` and VANISH
#                             -- the TU compiles clean with its structural
#                             checks deleted, which is worse than failing.
#   -include period_compat.h  __builtin_offsetof, GCC 4.0+.  Apparatus, not
#                             reconstruction; see that file for the line.
FLAGS="-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
       -mno-ieee-fp
       -fomit-frame-pointer -maccumulate-outgoing-args
       -Iinclude -Itest/harness -DDSPLIB_REPRODUCE_BUGS
       -D__SIZEOF_POINTER__=4 -include tools/toolchain/period_compat.h"

# GCC 3.4 defaults to gnu89, where a declaration in a `for` initialiser is an
# error.  Nothing else in the C half needs a newer dialect.
CFLAGS="$FLAGS -std=gnu99"
CXXFLAGS="$FLAGS -fno-exceptions -fno-rtti"

# Keep the provisional DCR candidate local to its translation unit.
# See TC_DCR_FLAGS in period.mk and the corrected finding F10269: the earlier
# stack-only equivalence claim was not supported by the complete comparison.
DCR_FLAGS="-O2 -fno-rerun-cse-after-loop"

HARNESS="test/harness/harness.c test/harness/runtime.c
         test/harness/fakedp.c test/harness/v34hsstep.c
         test/harness/unwritten.c"

obj() { echo "$OUT/$(echo "$1" | tr / _ | sed 's/\.[^.]*$//').o"; }

#
# THE NEWEST HEADER, so that KEEP is safe to use as a gate.
#
# There is no -MMD dependency tracking here, so "is the object newer than its
# .c" would happily keep an object built against a header that has since
# changed -- which is exactly how a gate goes green on stale results.  The
# cheap sound alternative is to treat ANY header edit as invalidating
# everything: one `find`, and a rebuild that is no less correct than a clean
# one.  Header edits are rare next to source edits, so the incremental case
# still pays for itself.
#
# The newest header by mtime.  NOT `find -newer "$OUT"`: $OUT is written on
# every build, so its own mtime is always fresher than any header and the test
# would never fire.  An object is reusable only if it is newer than BOTH its
# source and this.
NEWEST_HDR=$(ls -t $(find include src test/harness -name '*.h') 2>/dev/null | head -1)

# --- compile ---------------------------------------------------------------
: > "$OUT/failed"
compile_one() {
	f=$1; o=$(obj "$f")
	[ -n "$KEEP" ] && [ -f "$o" ] && [ "$o" -nt "$f" ] \
		&& { [ -z "$NEWEST_HDR" ] || [ "$o" -nt "$NEWEST_HDR" ]; } \
		&& return 0
	case $f in
	*.cpp)	g++ -c $CXXFLAGS -o "$o" "$f" 2>"$o.log" ;;
	*)	case $f in src/service/dcr.c) cflags="$CFLAGS $DCR_FLAGS" ;; *) cflags="$CFLAGS" ;; esac
		gcc -c $cflags -o "$o" "$f" 2>"$o.log" ;;
	esac || { echo "$f" >> "$OUT/failed"; sed -n '1,4p' "$o.log" >&2; }
}

echo "period: compiling $(echo $SRC $CXXSRC $HARNESS | wc -w) objects with $(gcc -dumpversion)"
echo "period: flags $(echo $FLAGS)"
for f in $SRC $CXXSRC $HARNESS; do
	compile_one "$f" &
	while [ "$(jobs -p | wc -l)" -ge "$J" ]; do wait -n 2>/dev/null || wait; done
done
wait

if [ -s "$OUT/failed" ]; then
	echo "period: $(wc -l < "$OUT/failed") translation units FAILED TO COMPILE" >&2
	echo "  a rejection here is a finding: the author's compiler was this one." >&2
	exit 1
fi

OBJS=""
for f in $SRC $CXXSRC $HARNESS; do OBJS="$OBJS $(obj "$f")"; done

# The newest input any test binary has, so a relink can be skipped when
# nothing it depends on moved.  The LINK is the expensive half here -- 155
# static binaries against a 1.2 MB object -- and skipping it is sound because
# the inputs are identical.  The RUN is never skipped: a cached pass is not a
# pass, and running the binaries is the cheap part.
NEWEST_IN=$(ls -t $OBJS "$REF" 2>/dev/null | head -1)

# --- link and run ----------------------------------------------------------
#
# -static because ld 2.15 cannot fit the program headers a dynamic link of the
# blob needs.  Linked with gcc, not g++: our C++ is -fno-exceptions -fno-rtti
# with no new/delete, so nothing wants libstdc++ -- the Makefile's argument,
# and it holds here for the same reason.
pass=0; fail=0; failed=""
for t in $TESTS; do
	src=test/unit/$t.c
	[ -f "$src" ] || src=test/unit/$t.cpp
	[ -f "$src" ] || { echo "  ?? no source for $t"; continue; }

	compile_one "$src"
	if [ -s "$OUT/failed" ]; then
		echo "  COMPILE-FAIL $t"; fail=$((fail + 1)); failed="$failed $t"
		: > "$OUT/failed"; continue
	fi

	# -static -N, and both are forced.  A dynamic link fails outright
	# ("Not enough room for program headers (allocated 8, need 9)"), and a
	# plain static one fails the same way one segment lower.  -N (OMAGIC)
	# puts text and data in a single writable, non-page-aligned segment, so
	# ld 2.15 needs no extra program header and the question does not
	# arise.  It costs a writable text section in a test binary, which is
	# nothing.  The alternative -- a hand-written linker script with a
	# PHDRS block -- buys the same result and one more thing to maintain.
	# Per-binary link flags, from the SAME file the Makefile's TESTLDFLAGS
	# reads -- one test needs `--wrap` on a cross-TU call, and two link
	# lines that can disagree about that is a defect waiting to happen.
	# A binary with no such file gets an empty string and links as before.
	EXTRA_LD=""
	[ -f "test/unit/$t.ldflags" ] && EXTRA_LD=$(cat "test/unit/$t.ldflags")

	if [ -n "$KEEP" ] && [ -x "$OUT/$t" ] && [ "$OUT/$t" -nt "$(obj "$src")" ] \
	   && { [ -z "$NEWEST_IN" ] || [ "$OUT/$t" -nt "$NEWEST_IN" ]; }; then
		:					# inputs unmoved; keep the binary
	elif ! gcc -static -Wl,-N -o "$OUT/$t" "$(obj "$src")" $OBJS "$REF" \
	     $EXTRA_LD -lm \
	     > "$OUT/$t.link.log" 2>&1; then
		echo "  LINK-FAIL    $t"; sed -n '1,3p' "$OUT/$t.link.log"
		fail=$((fail + 1)); failed="$failed $t"; continue
	fi

	if "$OUT/$t" > "$OUT/$t.run.log" 2>&1; then
		pass=$((pass + 1))
	else
		echo "  RUN-FAIL     $t (exit $?)"
		tail -6 "$OUT/$t.run.log" | sed 's/^/      /'
		fail=$((fail + 1)); failed="$failed $t"
	fi
done

echo
echo "period differential: $pass passed, $fail failed"
[ "$fail" -eq 0 ] || { echo "  failed:$failed"; exit 1; }
