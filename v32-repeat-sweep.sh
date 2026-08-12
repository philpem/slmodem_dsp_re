#!/bin/bash
#
# The V.32 IODELAY sweep, REDONE with repeats.
#
# The original was one call per point and concluded a cliff between 120 and
# 180.  A repeatability baseline then measured 4/5 connects at a FIXED setting,
# so a six-point single-sample sweep has roughly a 74% chance of at least one
# spurious failure.  Scored as fractions this time.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
PREFIX=${1:-$BENCH/captures/v32rep}
N=${2:-5}
shift 2 || true
VALUES=${*:-48 120 180 240}

printf '%-8s %-10s %-8s %s\n' IODELAY echo_delay CONNECTS RATES
printf '%-8s %-10s %-8s %s\n' ------- ---------- -------- -----
for v in $VALUES; do
	conn=0; rates=""
	for i in $(seq 1 "$N"); do
		LOG="$PREFIX-$v-$i"
		SLMODEMD_IODELAY="$v" \
		TTY_EXTRA="AT+MS=9,0;ATS95=47" PTY_EXTRA="AT+MS=32,0" \
			bash "$BENCH/row.sh" "$LOG" pty 1901 > "$LOG.run.log" 2>&1
		r=$(grep -a -oE 'pty +CONNECT [0-9]+' "$LOG.run.log" | head -1 | sed 's/.*CONNECT //')
		if [ -n "$r" ]; then conn=$((conn+1)); rates="$rates $r"; else rates="$rates -"; fi
	done
	printf '%-8s %-10s %-8s %s\n' "$v" "$((v+60))" "$conn/$N" "$rates"
done
