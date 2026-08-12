#!/bin/bash
#
# Walk the modulation ladder, one call per rung, and print a summary.
#
#   ladder.sh [logprefix] [tty] [extension]
#
# Each rung forces ONE modulation on both ends with AT+MS=<mod>,0 (automode
# off), so nothing negotiates and each result is attributable to that
# modulation alone.  V.34 is the exception -- it needs V.8, so it runs in
# automode.
#
# Runs are SEQUENTIAL.  There is one modem and one slmodemd; two calls at once
# would contend for both.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
. "$BENCH/modems.sh"
PREFIX=${1:-$BENCH/captures/ladder}
ROLE=${2:-supra}
EXT=${3:-$(modem_ext "$ROLE" 2>/dev/null || echo 1901)}

# Five sequential calls.  Pre-flight once here so a missing modem costs one
# line rather than five rungs of NO CONNECT that look like a modulation result.
echo "=== pre-flight"
TTY=$(modem_require "$ROLE") || exit 3

# name          rockwell(tty)  slmodemd(pty)   automode
RUNGS='
v22            1              22              0
v22bis         2              122             0
v32            9              32              0
v32bis         10             132             0
v34            11             34              1
'

echo "=== ladder, T.38 off, fixed playout"
echo "MODEM UNDER TEST: $(modem_provenance "$ROLE")"
echo

printf '%-10s %-9s %-22s %s\n' MODULATION MODEM RESULT DETAIL
printf '%-10s %-9s %-22s %s\n' ---------- ----- ------ ------

echo "$RUNGS" | while read -r name tmod pmod auto; do
	[ -z "$name" ] && continue
	LOG="$PREFIX-$name"
	TTY="$TTY" TTY_EXTRA="AT+MS=$tmod,$auto" PTY_EXTRA="AT+MS=$pmod,$auto" \
		bash "$BENCH/row.sh" "$LOG" pty "$EXT" > "$LOG.run.log" 2>&1

	conn=$(grep -a -oE 'CONNECT [0-9]+' "$LOG.run.log" | head -1)
	data=$(grep -a -oE 'DATA BOTH WAYS: (PASS|FAIL)' "$LOG.run.log" | head -1)
	if grep -a -q 'DATA BOTH WAYS: PASS' "$LOG.run.log"; then
		verdict='PASS, data both ways'
	elif [ -n "$conn" ]; then
		verdict='connects, no data'
	else
		verdict='NO CONNECT'
	fi
	printf '%-10s %-9s %-22s %s\n' "$name" "$ROLE" "$verdict" "${conn:-} ${data:-}"
done

echo
echo "=== logs and recordings under $PREFIX-*"
