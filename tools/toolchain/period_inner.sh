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

# Source C keeps the same default GNU89 dialect as period.mk. Fixture and
# harness C need GNU99 for declarations in `for` initialisers.
SOURCE_CFLAGS="$FLAGS"
CFLAGS="$FLAGS -std=gnu99"
CXXFLAGS="$FLAGS -fno-exceptions -fno-rtti -fno-math-errno -Wno-invalid-offsetof"
SOURCE_CXXFLAGS="$CXXFLAGS -ffast-math"

# ASSEMBLY INPUTS carry the arch/codegen flags only: -include period_compat.h
# is a C header and cannot be fed to the assembler, and -std is irrelevant.
# Their FILE records come from `cd`-ing to the source's own directory and
# passing the BASENAME, because `as` records the path it is handed.  F11407.
ASFLAGS="-O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
         -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args"

# Keep the provisional DCR candidate local to its translation unit.
# See TC_DCR_FLAGS in period.mk and the corrected finding F10269: the earlier
# stack-only equivalence claim was not supported by the complete comparison.
DCR_FLAGS="-O2 -fno-rerun-cse-after-loop"

# Same original processor TUs and PRE profile as TC_FAX_PROC_FLAGS in period.mk.
FAX_PROC_FLAGS="-fno-gcse"

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
NEWEST_HDR=$(ls -t $(find include src test/harness -name '*.h') test/unit/*.cxxflags 2>/dev/null | head -1)

# Several transcript fixtures include another test/unit .cpp. Its mtime is
# not covered by NEWEST_HDR. Keep compiler-produced dependencies for fixtures
# only; reconstruction and harness compilation retain their existing flags.
fixture_receipt() {
	f=$1; o=$2
	cksum "$o" "$o.d" || return 1
	case $f in
	*.cpp)	if [ -f "${f%.*}.cxxflags" ]; then
			cksum "${f%.*}.cxxflags" || return 1
		else
			printf '%s\n' no-cxxflags
		fi ;;
	esac
}

fixture_current() {
	f=$1; o=$2
	[ -s "$o.d" ] && [ -s "$o.inputs" ] || return 1
	actual=$(fixture_receipt "$f" "$o") || return 1
	[ "$actual" = "$(cat "$o.inputs")" ] || return 1
	# No -MP rules or make evaluation. Fold only backslash-newline; reject
	# unsupported escaping and extra rules rather than accepting partial input.
	dependencies=$(awk -v target="$o" '
		{
			if (done) bad = 1
			part = $0
			continued = sub(/\\$/, "", part)
			text = text part " "
			if (!continued) done = 1
		}
		END {
			colon = index(text, ":")
			if (bad || !done || continued || !colon ||
			    substr(text, 1, colon - 1) != target) exit 1
			deps = substr(text, colon + 1)
			if (deps ~ /[\\:$#*?]/ || deps ~ /\[/ || deps ~ /\]/ ||
			    deps !~ /[^[:space:]]/) exit 1
			print deps
		}' "$o.d") || return 1
	found_source=0
	for dependency in $dependencies; do
		[ "$dependency" != "$f" ] || found_source=1
		[ -f "$dependency" ] && [ "$o" -nt "$dependency" ] || return 1
	done
	[ "$found_source" -eq 1 ]
}

# COMPILE AND LINK ARE SPLIT, because binutils 2.15 (the period linker) has no
# `objcopy --globalize-symbols`.  The host does that step between the two
# stages.  `STAGE=all` (the default) keeps the old one-pass behaviour for a
# hand-run of this script; period.sh drives compile then link.
STAGE=${STAGE:-all}

# --- compile ---------------------------------------------------------------
: > "$OUT/failed"
compile_one() {
	f=$1; o=$(obj "$f")
	fixture=0
	case $f in test/unit/*) fixture=1 ;; esac
	# Flag/dispatch changes in this script invalidate every compiled object.
	[ -n "$KEEP" ] && [ -f "$o" ] && [ "$o" -nt "$f" ] && [ "$o" -nt "$0" ] \
		&& { [ -z "$NEWEST_HDR" ] || [ "$o" -nt "$NEWEST_HDR" ]; } \
		&& { [ "$fixture" -eq 0 ] || fixture_current "$f" "$o"; } \
		&& return 0
	depflags=""
	if [ "$fixture" -eq 1 ]; then
		# Remove the old receipt BEFORE compilation. A failed/interrupted
		# compiler must never pair a newer partial object with old dependencies.
		rm -f "$o.d" "$o.inputs" "$o.d.tmp" "$o.inputs.tmp"
		# This driver already derives the target from -o. An explicit -MT
		# makes GCC 3.4.2 repeat the target in the generated dependency rule.
		depflags="-MMD -MF $o.d.tmp"
	fi
	case $f in
	src/*.cpp) g++ -c $SOURCE_CXXFLAGS -o "$o" "$f" 2>"$o.log" ;;
	*.cpp)	extra=""; [ -f "${f%.*}.cxxflags" ] && extra=$(cat "${f%.*}.cxxflags")
		g++ -c $CXXFLAGS $extra $depflags -o "$o" "$f" 2>"$o.log" ;;
	src/*.S) (cd "$(dirname "$f")" && gcc -c $ASFLAGS \
		-o "$o" "$(basename "$f")") 2>"$o.log" ;;
	*)	case $f in
		src/service/dcr.c) cflags="$SOURCE_CFLAGS $DCR_FLAGS" ;;
		src/fax/V17t_prc.c|src/fax/V29t_prc.c) cflags="$SOURCE_CFLAGS $FAX_PROC_FLAGS" ;;
		src/*) cflags="$SOURCE_CFLAGS" ;;
		*) cflags="$CFLAGS" ;;
		esac
		gcc -c $cflags $depflags -o "$o" "$f" 2>"$o.log" ;;
	esac || { echo "$f" >> "$OUT/failed"; sed -n '1,4p' "$o.log" >&2; return 0; }
	if [ "$fixture" -eq 1 ]; then
		if [ -s "$o.d.tmp" ] && mv "$o.d.tmp" "$o.d" \
		    && fixture_receipt "$f" "$o" > "$o.inputs.tmp" \
		    && mv "$o.inputs.tmp" "$o.inputs"; then
			:
		else
			echo "$f" >> "$OUT/failed"
			echo "period: missing fixture dependency receipt: $f" >&2
		fi
	fi
}

if [ "$STAGE" != "link" ]; then
echo "period: compiling $(echo $SRC $CXXSRC $ASRC $HARNESS | wc -w) objects with $(gcc -dumpversion)"
echo "period: flags $(echo $FLAGS)"
echo "period: C source flags $(echo $SOURCE_CFLAGS)"
echo "period: C fixture flags $(echo $CFLAGS)"
echo "period: DCR source flags $(echo $SOURCE_CFLAGS $DCR_FLAGS)"
echo "period: V17t_prc.c/V29t_prc.c source flags $(echo $SOURCE_CFLAGS $FAX_PROC_FLAGS)"
echo "period: C++ source flags $(echo $SOURCE_CXXFLAGS)"
echo "period: C++ fixture flags $(echo $CXXFLAGS)"
echo "period: fixture dependency apparatus -MMD -MF <object>.d.tmp"
for f in $SRC $CXXSRC $ASRC $HARNESS; do
	# A serial gate must not depend on jobs output from a command
	# substitution (the period shell can report an empty job table there).
	if [ "$J" -eq 1 ]; then
		compile_one "$f"
		continue
	fi
	compile_one "$f" &
	while [ "$(jobs -p | wc -l)" -ge "$J" ]; do wait -n 2>/dev/null || wait; done
done
wait

if [ -s "$OUT/failed" ]; then
	echo "period: $(wc -l < "$OUT/failed") translation units FAILED TO COMPILE" >&2
	echo "  a rejection here is a finding: the author's compiler was this one." >&2
	exit 1
fi
fi

OBJS=""
for f in $SRC $CXXSRC $ASRC $HARNESS; do OBJS="$OBJS $(obj "$f")"; done

# COMPILE-ONLY stage: record the object list so the HOST can run the
# `objcopy --globalize-symbols` the period binutils 2.15 does not have, then
# re-enter for the link stage.
if [ "$STAGE" = "compile" ]; then
	for o in $OBJS; do echo "$o"; done > "$OUT/objs.list"
	# The reconstructed objects only -- the harness is not ours to globalize,
	# and a harness name must not make a reconstruction name look ambiguous.
	: > "$OUT/srcobjs.list"
	: > "$OUT/srcmanifest.list"
	for f in $SRC $CXXSRC $ASRC; do
		echo "$(obj "$f")" >> "$OUT/srcobjs.list"
		echo "$(basename "$(obj "$f")") $f" >> "$OUT/srcmanifest.list"
	done
	exit 0
fi

#
# THE FILE-LOCAL SYMBOLS A TEST NAMES, globalized by the host between the two
# stages.  The reference keeps many tables and datapump entry points LOCAL, so
# `src/` defines them `static`; a test that names one would otherwise have no
# symbol to link against.  The copies in $OUT/testhost are linked into the test
# binaries only -- the partial-link candidate is built elsewhere and keeps the
# LOCAL binding.
#
if [ -n "${VISIBLE:-}" ] && [ -s "$VISIBLE" ] && [ -d "$OUT/testhost" ]; then
	GLOB=""
	for o in $OBJS; do
		g="$OUT/testhost/$(basename "$o")"
		# Only the reconstructed objects were globalized; the harness keeps
		# its own copy.
		if [ -f "$g" ]; then GLOB="$GLOB $g"; else GLOB="$GLOB $o"; fi
	done
	OBJS="$GLOB"
fi

# Only reconstructed source is aggregated: fixture and harness references stay
# unresolved until the final link. Wrapping must happen while the source TUs
# are still separate inputs; a final-only --wrap cannot intercept bound calls.
aggregate_for() {
	wrap_flags=$1
	mkdir -p "$OUT/aggregate"
	ld_program=$(gcc -print-prog-name=ld)
	{
		cat "$OUT/aggregate-inputs.key"
		cksum tools/toolchain/period_inner.sh
		printf '%s\n' "$wrap_flags" "$ld_program"
		"$ld_program" --version
	} > "$OUT/aggregate/request.key"
	key=$(cksum "$OUT/aggregate/request.key" | cut -d ' ' -f 1)
	aggregate="$OUT/aggregate/$key.o"
	if [ ! -s "$aggregate" ] || ! cmp -s "$OUT/aggregate/request.key" "$aggregate.key"; then
		set -- $(cat "$OUT/aggregate-order.list")
		if ! (set -x; gcc -nostdlib -r -o "$aggregate.tmp" "$@" $wrap_flags) > "$aggregate.log" 2>&1; then
			cat "$aggregate.log" >&2; return 1
		fi
		mv "$aggregate.tmp" "$aggregate"
		cp "$OUT/aggregate/request.key" "$aggregate.key"
	fi
	echo "$aggregate"
}

HARNESS_OBJS=""
for f in $HARNESS; do HARNESS_OBJS="$HARNESS_OBJS $(obj "$f")"; done

# Each executable's cache records its resolved mode, ordered object inputs,
# final flags and this apparatus revision. Runs are never cached. Aggregate
# object caching additionally uses source content and selected linker identity.

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

	# Fixtures opt in with .ldmode. The environment override is diagnostic:
	# PERIOD_LINK_MODE=aggregate validates every fixture; individual is the
	# negative control for an aggregate-boundary fixture.
	LINK_MODE=${PERIOD_LINK_MODE:-fixture}
	if [ "$LINK_MODE" = fixture ]; then
		LINK_MODE=individual
		[ ! -f "test/unit/$t.ldmode" ] || LINK_MODE=$(cat "test/unit/$t.ldmode")
	fi
	LINK_OBJS=$OBJS
	case "$LINK_MODE" in
	individual) ;;
	aggregate)
		WRAP_LD=""
		for flag in $EXTRA_LD; do
			case "$flag" in
			-Wl,--wrap=*) WRAP_LD="$WRAP_LD $flag" ;;
			*--wrap*) echo "unsupported aggregate wrap syntax: $flag" >&2; exit 1 ;;
			esac
		done
		AGGREGATE=$(aggregate_for "$WRAP_LD")
		LINK_OBJS="$AGGREGATE $HARNESS_OBJS"
		;;
	*) echo "invalid period link mode: $LINK_MODE" >&2; exit 1 ;;
	esac
	LINK_NEWEST=$(ls -t $LINK_OBJS "$REF" 2>/dev/null | head -1)
	{
		printf '%s\n' "$LINK_MODE" "$LINK_OBJS" "$REF" "$EXTRA_LD"
		cksum tools/toolchain/period_inner.sh
	} > "$OUT/$t.link-config.tmp"

	if [ -n "$KEEP" ] && cmp -s "$OUT/$t.link-config.tmp" "$OUT/$t.link-config" && [ -x "$OUT/$t" ] && [ "$OUT/$t" -nt "$(obj "$src")" ] \
	   && { [ -z "$LINK_NEWEST" ] || [ "$OUT/$t" -nt "$LINK_NEWEST" ]; }; then
		:					# inputs unmoved; keep the binary
	elif ! gcc -static -Wl,-N -o "$OUT/$t" "$(obj "$src")" $LINK_OBJS "$REF" \
	     $EXTRA_LD -lm \
	     > "$OUT/$t.link.log" 2>&1; then
		echo "  LINK-FAIL    $t"; sed -n '1,3p' "$OUT/$t.link.log"
		fail=$((fail + 1)); failed="$failed $t"; continue
	fi

	mv "$OUT/$t.link-config.tmp" "$OUT/$t.link-config"

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
