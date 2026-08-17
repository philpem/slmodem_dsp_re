#!/bin/sh
#
# test/blobfix/run.sh -- the acceptance test for tools/blobfix.py.
#
# WHAT IT HAS TO PROVE, and why it is built the way it is
#
# D1 is the rehearsal precisely because fixing it changes NOTHING observable:
# the blob's out-of-bounds read lands on `FPM_div_table[0]`, which already
# holds the value the missing entry should have.  So "the answer is right"
# is not evidence of anything, and a test that only compares answers would
# pass identically on a fix that was never applied -- which is exactly the
# failure the fork shipped at `rebuildJMSequence+0x136` (`docs/forkblob.md`).
#
# Every check below therefore observes the ACCESS.  Three independent ways,
# because each has a blind spot the others do not:
#
#   STATIC     the linked binary is read back: the displacement the linker
#              actually wrote against the table's address and size.  Catches
#              a fix that did not take.  Cannot see what happens at run time.
#
#   SENTINEL   the replacement table's new last entry is poisoned, and the
#              inputs whose result changes are exactly the inputs that read
#              it.  Names the out-of-bounds population -- 85 for D1, 255 for
#              D4 -- without the test knowing the index expression.  Needs no
#              debugger, so this is the check that must never be skipped.
#
#   WATCHPOINT a hardware read watchpoint on the byte the blob overruns INTO.
#              The only check that observes the original defect directly
#              rather than by substitution.  Needs ptrace and a debug
#              register, so it degrades to SKIP rather than to a false pass.
#
# Run from the tree root:   sh test/blobfix/run.sh
#
# `make phase` does not run this and must pass with neither fix applied.
#

BLOB=${BLOB:-ref/slmodemd/dsplibs.o}
OUT=build/blobfix
CC=${CC:-gcc}
CFLAGS="-m32 -fno-pie -O2 -Wall"
LDFLAGS="-m32 -fno-pie -no-pie -Wl,-z,noexecstack,-z,notext"
PYTHON=${PYTHON:-python3}

fails=0
skips=0

ok()   { echo "  PASS  $*"; }
bad()  { echo "  FAIL  $*"; fails=$((fails + 1)); }
skip() { echo "  SKIP  $*"; skips=$((skips + 1)); }

# Expect an exact number, and say both sides when it is not that.
want() {
	if [ "$2" = "$3" ]; then ok "$1: $3"; else bad "$1: got $2, want $3"; fi
}

test -f "$BLOB" || { echo "no blob at $BLOB"; exit 1; }
mkdir -p $OUT

# --- build one probe per configuration ------------------------------------
#
# `plain` links the blob untouched: it is the control, and every check that
# is supposed to fail without the fix is run against it.

build() {                       # build <tag> <blobfix.py generate args...>
	tag=$1
	shift
	d=$OUT/$tag
	mkdir -p $d
	if [ $# -gt 0 ]; then
		$PYTHON tools/blobfix.py --blob "$BLOB" generate \
			--outdir $d "$@" > $d/generate.log 2>&1 || {
			cat $d/generate.log
			return 1
		}
		obj=$d/dsplibs_fixed.o
		extra=$d/blobfix.c
	else
		obj=$BLOB
		extra=
	fi
	$CC $LDFLAGS -o $d/probe test/blobfix/probe.c test/blobfix/stubs.c \
		$extra $obj -lm || return 1
	$d/probe sqrt  > $d/sqrt.txt  &&
	$d/probe div   > $d/div.txt   &&
	$d/probe div32 > $d/div32.txt &&
	$d/probe addr  > $d/addr.txt
}

echo "building probes"
build plain            || { echo "control build failed"; exit 1; }
build d1  --fix D1     || { echo "D1 build failed"; exit 1; }
build d1s --fix D1 --sentinel D1=0x1234 || { echo "D1 sentinel failed"; exit 1; }
build d4  --fix D4     || { echo "D4 build failed"; exit 1; }
build d4s --fix D4 --sentinel D4=0x1234 || { echo "D4 sentinel failed"; exit 1; }
build both --fix D1 --fix D4 || { echo "combined build failed"; exit 1; }

# --- the defect's camouflage, measured ------------------------------------

echo
echo "adjacency (this is what makes D1 harmless and D4 harmful)"
sq=$(awk '/FPM_sqrt_table/{print strtonum($2)}' $OUT/plain/addr.txt)
dv=$(awk '/FPM_div_table/{print strtonum($2)}'  $OUT/plain/addr.txt)
xo=$(awk '/FPM_xor_table/{print strtonum($2)}'  $OUT/plain/addr.txt)
want "unpatched: div_table - sqrt_table" $((dv - sq)) 384
want "unpatched: xor_table - div_table"  $((xo - dv)) 256
sq1=$(awk '/FPM_sqrt_table/{print strtonum($2)}' $OUT/d1/addr.txt)
dv1=$(awk '/FPM_div_table/{print strtonum($2)}'  $OUT/d1/addr.txt)
if [ $((dv1 - sq1)) -eq 384 ]; then
	bad "D1: replacement table is still adjacent to FPM_div_table"
else
	ok "D1: replacement table no longer abuts FPM_div_table"
fi

# --- STATIC: read the link back -------------------------------------------

echo
echo "static: what the linker wrote, against the table it wrote it to"
check() {                       # check <expect pass|fail> <binary> <args...>
	exp=$1; shift
	bin=$1; shift
	if $PYTHON tools/blobfix.py --blob "$BLOB" checklink "$bin" "$@" \
			> $OUT/checklink.log 2>&1; then
		got=pass
	else
		got=fail
	fi
	if [ "$got" = "$exp" ]; then
		ok "checklink $(basename $(dirname $bin))/$(basename $bin) $*: $got as expected"
	else
		bad "checklink $(basename $(dirname $bin))/$(basename $bin) $*: $got, want $exp"
		sed 's/^/        /' $OUT/checklink.log
	fi
}
check fail $OUT/plain/probe --fix D1
check pass $OUT/d1/probe    --fix D1
check fail $OUT/plain/probe --fix D4
check pass $OUT/d4/probe    --fix D4
check pass $OUT/both/probe  --fix D1 --fix D4

#
# AND THE CHECKER IS SHOWN TO FAIL.  A gate nobody has watched reject
# something is a gate that may be passing because it is broken -- CLAUDE.md's
# rule, and `extcheck.py` printing "(none)" through four dead versions is why
# it is a rule.  Both arms of `checklink` are exercised against deliberate
# damage: a table reference off by one entry, and an instruction byte changed
# outside the relocated field.
#
for mode in displacement opcode; do
	$PYTHON tools/blobfix.py --blob "$BLOB" tamper $OUT/d1/probe \
		--fix D1 --mode $mode -o $OUT/d1/tampered-$mode \
		> /dev/null || bad "tamper $mode did not run"
	check fail $OUT/d1/tampered-$mode --fix D1
done

# --- SENTINEL: which inputs read the new entry ----------------------------

echo
echo "sentinel: the inputs that read one past the end, named by poisoning it"

#
# `FPM_sqrt` returns `table[index] >> (exponent >> 1)`, so an input that
# reads the poisoned entry does NOT return the sentinel -- it returns the
# sentinel shifted by however far normalisation went.  Checking for the bare
# value finds 64 of the 85 and calls the other 21 a failure.  The invariant
# that actually holds is that ONE shift explains both sides at once:
#
#     unpatched value = 32768 >> k     and     patched value = 0x1234 >> k
#
# which is a far stronger statement than either half, because it says the
# only thing that changed about the read is the datum it landed on.
#
read n bad <<EOF
$(paste $OUT/plain/sqrt.txt $OUT/d1s/sqrt.txt | awk '
	$1 != $3 { print "DESYNC"; exit 1 }
	$2 != $4 {
		n++
		p = strtonum("0x" $2); s = strtonum("0x" $4); ok = 0
		for (k = 0; k < 16; k++)
			if (p == int(32768 / 2 ^ k) && s == int(4660 / 2 ^ k))
				ok = 1
		if (!ok) bad++
	}
	END { printf "%d %d\n", n + 0, bad + 0 }')
EOF
want "D1: inputs whose result moves when entry 192 is poisoned" "$n" 85
want "D1:   ... all explained by 32768>>k becoming 0x1234>>k" "$bad" 0

# The fix itself must be invisible.  This is the check that says the
# rehearsal is a rehearsal.
if cmp -s $OUT/plain/sqrt.txt $OUT/d1/sqrt.txt; then
	ok "D1: all 32768 Q15 results bit-identical to the unpatched blob"
else
	bad "D1: patched object returns different values -- fix is not faithful"
fi

#
# `FPM_div` hands the table entry back unshifted -- the normalisation count
# is returned separately -- so here the raw values are the invariant, and the
# reciprocal really is zero on the unpatched object.
#
read n bad <<EOF
$(paste $OUT/plain/div.txt $OUT/d4s/div.txt | awk '
	$1 != $5 { print "DESYNC"; exit 1 }
	$3 != $7 { n++; if ($7 != "1234") bad++ }
	END { printf "%d %d\n", n + 0, bad + 0 }')
EOF
want "D4: denominators whose result moves when entry 128 is poisoned" "$n" 255
want "D4:   ... every one of them returning the sentinel" "$bad" 0

read n bad <<EOF
$(paste $OUT/plain/div.txt $OUT/d4/div.txt | awk '
	$3 != $7 {
		n++
		if ($3 != "0000" || $7 != "4000") bad++
		if ($4 != $8) shift++
	}
	END { printf "%d %d\n", n + 0, bad + shift + 0 }')
EOF
want "D4: denominators the generated entry changes" "$n" 255
want "D4:   ... 0 becoming 16384 and the shift untouched, every time" "$bad" 0

#
# THE SECOND CONSUMER.  `FPM_div_32` also indexes this table, has no
# reconstruction and therefore no differential test, and does NOT clamp.
# Swept exhaustively over the mantissa, so this is a statement about the
# index domain and not about a sample: exactly 128 of the 32768 possible
# mantissas -- 0xff80..0xffff -- form index 128.
#
read n bad <<EOF
$(paste $OUT/plain/div32.txt $OUT/d4/div32.txt | awk '
	$3 != $7 {
		n++
		if ($3 != "0000" || $7 != "4000" || $4 != $8) bad++
		if (strtonum("0x" $1) < 65408) bad++
	}
	END { printf "%d %d\n", n + 0, bad + 0 }')
EOF
want "D4: FPM_div_32 mantissas the generated entry changes" "$n" 128
want "D4:   ... all in 0xff80..0xffff, 0 -> 16384, shift untouched" "$bad" 0

# The two fixes must not interfere: the combined build has to reproduce both.
if cmp -s $OUT/plain/sqrt.txt $OUT/both/sqrt.txt &&
		cmp -s $OUT/d4/div.txt $OUT/both/div.txt; then
	ok "combined build: D1 still invisible, D4 still fixed"
else
	bad "combined build: the two fixes interfere"
fi

# --- WATCHPOINT: watch the byte the blob overruns into --------------------
#
# Deliberately run against the SINGLE-fix builds.  In the combined build the
# name `FPM_div_table` resolves to the replacement, so a watchpoint set on it
# would watch the wrong address and report zero hits for the wrong reason.

echo
echo "watchpoint: does the blob still touch the byte past the table?"
rwatch() {                      # rwatch <binary> <mode> <symbol>
	cat > $OUT/watch.gdb <<-EOF
	set confirm off
	set pagination off
	set height 0
	break main
	run $2 > /dev/null
	rwatch *(unsigned short *)&$3
	commands
	silent
	printf "HIT\n"
	continue
	end
	continue
	quit
	EOF
	timeout 300 gdb -q -batch -x $OUT/watch.gdb "$1" 2>&1
}

if ! command -v gdb > /dev/null; then
	skip "no gdb: the direct observation of the overrun is not available"
else
	out=$(rwatch $OUT/plain/probe sqrt FPM_div_table)
	base=$(echo "$out" | grep -c '^HIT$')
	if echo "$out" | grep -q 'Hardware read watchpoint'; then
		want "D1: unpatched blob reads FPM_div_table[0]" "$base" 85
		n=$(rwatch $OUT/d1/probe sqrt FPM_div_table | grep -c '^HIT$')
		want "D1: patched object reads it" "$n" 0

		n=$(rwatch $OUT/plain/probe div FPM_xor_table |
			grep -c '^HIT$')
		want "D4: unpatched blob reads FPM_xor_table[0]" "$n" 255
		n=$(rwatch $OUT/d4/probe div FPM_xor_table | grep -c '^HIT$')
		want "D4: patched object reads it" "$n" 0

		n=$(rwatch $OUT/plain/probe div32 FPM_xor_table |
			grep -c '^HIT$')
		want "D4: unpatched FPM_div_32 reads it" "$n" 128
		n=$(rwatch $OUT/d4/probe div32 FPM_xor_table | grep -c '^HIT$')
		want "D4: patched FPM_div_32 reads it" "$n" 0
	else
		skip "no hardware read watchpoint available in this environment"
	fi
fi

echo
if [ $fails -eq 0 ]; then
	if [ $skips -gt 0 ]; then
		echo "blobfix: OK, $skips check(s) skipped"
	else
		echo "blobfix: OK"
	fi
	exit 0
fi
echo "blobfix: $fails FAILED"
exit 1
