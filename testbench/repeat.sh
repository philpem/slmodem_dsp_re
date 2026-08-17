#!/bin/bash
#
# Repeatability baseline: the SAME configuration, N times.
#
# Every delay-versus-behaviour claim on this bench so far rests on one call per
# data point, and the V.34 rate sweep produced a hole (136 fails, 152 passes)
# that no delay mechanism explains.  Before reading any more structure into
# those tables, measure how often a FIXED configuration fails.  If that rate is
# high, the tables are noise and the only fix is repeats.
#
#   repeat.sh <logprefix> <iodelay> <count>
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
PREFIX=${1:-$BENCH/captures/repeat}
IOD=${2:-120}
N=${3:-5}

echo "=== IODELAY $IOD, V.34, $N identical calls to 1901"
pass=0; conn=0
for i in $(seq 1 "$N"); do
	LOG="$PREFIX-$IOD-$i"
	SLMODEMD_IODELAY="$IOD" \
	TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1" \
		bash "$BENCH/row.sh" "$LOG" pty 1901 > "$LOG.run.log" 2>&1
	far=$(grep -a -oE 'tty +CONNECT [0-9]+[^ ]*' "$LOG.run.log" | head -1 | sed 's/.*CONNECT //')
	blob=$(grep -a -oE 'pty +CONNECT [0-9]+' "$LOG.run.log" | head -1 | sed 's/.*CONNECT //')
	d=$(grep -a -oE 'DATA BOTH WAYS: [A-Z]+' "$LOG.run.log" | head -1 | sed 's/.*: //')
	[ -n "$blob" ] && conn=$((conn + 1))
	[ "$d" = PASS ] && pass=$((pass + 1))
	printf '  %2d/%d  far=%-12s blob=%-12s data=%s\n' \
		"$i" "$N" "${far:-—}" "${blob:-no connect}" "${d:--}"
done
echo "  ---"
echo "  connected $conn/$N, data both ways $pass/$N"
