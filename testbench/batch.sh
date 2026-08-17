#!/bin/bash
#
# Task #108.  N calls at ONE fixed configuration, one CSV row each.
#
#   batch.sh [logprefix] [count] [role]
#   batch.sh captures/batch 30 supra
#
# NOT A SWEEP.  Nothing is varied.  The point is a sample large enough to
# reject a covariate, which every measurement on this bench so far has not
# been: four candidates for the receive-rate deficit have been proposed and
# refuted -- the jitter buffer, per-call ERL, between-modem ERL, and the
# equaliser error -- and each looked convincing at n=5-9 before the sample
# grew.  In the equerr case one call moved r from -0.787 to -0.263.
#
# What makes this worth ~50 minutes of bench time: finding 1207 showed ONE call
# reached 28800 on this path, with the same echo at the same lag as every other
# call.  So the transport is capable and something intermittent costs the rest
# two to three rate steps.  At n=30 the high-rate outcome should recur several
# times, and "what distinguishes those calls" becomes answerable rather than
# resting on a single point.
#
# Every column already exists at `-d9`; only the CSV emission is new.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
PREFIX=${1:-$BENCH/captures/batch}
N=${2:-30}
ROLE=${3:-supra}
IOD=${IOD:-240}
CSV="$PREFIX.csv"

echo "=== pre-flight"
TTY=$(modem_require "$ROLE") || exit 3
EXT=$(modem_ext "$ROLE") || exit 3
echo
echo "MODEM UNDER TEST: $(modem_provenance "$ROLE")"
echo "FIXED: IODELAY=$IOD, V.34 automode, ext $EXT, $N calls"
echo "CSV:   $CSV"
echo

python3 "$BENCH/callstats.py" --header > "$CSV"

ok=0
for i in $(seq 1 "$N"); do
	L="$PREFIX-$i"
	# Let the far end finish the previous call's hangup.  The escape
	# sequence has a guard time and `ATH` is not instant; probing straight
	# after teardown reads as "no modem".  The older scripts got this for
	# free because they read AT&V1 between calls.
	[ "$i" -gt 1 ] && sleep 6
	# Pass the ROLE, not the resolved path: row.sh then pre-flights on every
	# call, so a mid-batch unplug stops the run instead of filling the CSV
	# with `no connect` rows, and the MODEM: line names the role.
	SLMODEMD_IODELAY="$IOD" TTY="$ROLE" \
	TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		bash "$BENCH/row.sh" "$L" pty "$EXT" > "$L.run.log" 2>&1
	rc=$?
	if [ $rc -eq 3 ]; then
		echo "  ABORTING at call $i: pre-flight failed -- the modem went away." >&2
		break
	fi

	python3 "$BENCH/callstats.py" "$L" >> "$CSV"
	line=$(tail -1 "$CSV")
	rx=$(echo "$line" | cut -d, -f6)
	[ -n "$rx" ] && ok=$((ok + 1))
	printf '  %2d/%-3d rx=%-8s tx=%-8s equerr_pre=%-7s erl=%-7s  (%d connected)\n' \
		"$i" "$N" "${rx:-none}" \
		"$(echo "$line" | cut -d, -f5)" \
		"$(echo "$line" | cut -d, -f10)" \
		"$(echo "$line" | cut -d, -f8)" "$ok"
done

echo
echo "=== $ok/$N connected.  Rows in $CSV"
