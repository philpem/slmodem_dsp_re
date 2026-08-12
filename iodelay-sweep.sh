#!/bin/bash
#
# Sweep MDMPRM_IODELAY against a fixed modulation, to find the operating point
# for a SIP path.  One number sets both the V.34 pipeline latency and the echo
# canceller's reach, and the echo canceller's tap count moves one for one with
# it, so this is the knob that decides whether the canceller can see a SIP
# leg's echo at all.
#
#   iodelay-sweep.sh <logprefix> <ttymod> <ptymod> [values...]
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
PREFIX=${1:-$BENCH/captures/iod}
TMOD=${2:-9}          # Rockwell code for the hardware modem
PMOD=${3:-32}         # slmodemd's datapump id
shift 3 || true
VALUES=${*:-48 120 180 240}

printf '%-8s %-22s %s\n' IODELAY RESULT DETAIL
printf '%-8s %-22s %s\n' ------- ------ ------
for v in $VALUES; do
	LOG="$PREFIX-$v"
	SLMODEMD_IODELAY="$v" TTY_EXTRA="AT+MS=$TMOD,0" PTY_EXTRA="AT+MS=$PMOD,0" \
		bash "$BENCH/row.sh" "$LOG" pty 1901 > "$LOG.run.log" 2>&1
	conn=$(grep -a -oE 'CONNECT [0-9]+' "$LOG.run.log" | tail -1)
	if grep -a -q 'DATA BOTH WAYS: PASS' "$LOG.run.log"; then r='PASS, data both ways'
	elif [ -n "$conn" ]; then r='connects, no data'
	else r='NO CONNECT'; fi
	# what the modem was actually told, straight from its own trace
	told=$(grep -a -oE 'MDMCTL_IODELAY -> [0-9]+ \([^)]*\)' "$LOG.slmodemd.log" 2>/dev/null | head -1)
	printf '%-8s %-22s %s | %s\n' "$v" "$r" "${conn:-}" "${told:-no ioctl seen}"
done
