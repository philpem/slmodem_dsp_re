#!/bin/bash
#
# Sweep MDMPRM_IODELAY across the usable band and score on CONNECT RATE, not
# on connect-or-fail.  IODELAY sets both the V.34 pipeline latency and the echo
# canceller's reach, and it already moved the rate once (14400 at 120 against
# 12000 at 240), so the operating point chosen for "does it connect" is not
# necessarily the one that connects FASTEST.
#
# The band: V.34 needs filtdelay >= 57, i.e. IODELAY >= 88, and V.32 breaks
# somewhere between 120 and 180.  These points span that.
#
#   filtdelay  = ((IODELAY + 6) >> 2) + 34
#   echo_delay = IODELAY + 60          [samples at 9600 Hz]
#
# THE FAR END IS MADE TO REPORT ITS LINE RATE.  A Rockwell reports the DTE rate
# in CONNECT by default (W0), which is why every earlier run showed 115200 and
# told us nothing.  S95 bit 0 switches CONNECT to the DCE (line) rate; 47 also
# adds /ARQ, CARRIER, PROTOCOL and COMPRESSION.  Note ATW and S95 are the same
# register on this firmware -- ATW2 returns OK and changes nothing visible,
# which is a good way to waste an afternoon.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
PREFIX=${1:-$BENCH/captures/v34rate}
shift || true
VALUES=${*:-88 104 120 136 152 168}

printf '%-8s %-9s %-10s %-14s %-14s %s\n' IODELAY filtdelay echo_delay 'FAR END' 'BLOB' DATA
printf '%-8s %-9s %-10s %-14s %-14s %s\n' ------- --------- ---------- ------- ---- ----
for v in $VALUES; do
	LOG="$PREFIX-$v"
	SLMODEMD_IODELAY="$v" \
	TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1" \
		bash "$BENCH/row.sh" "$LOG" pty 1901 > "$LOG.run.log" 2>&1

	far=$(grep -a -oE 'tty +CONNECT [0-9]+[^ ]*' "$LOG.run.log" | head -1 | sed 's/.*CONNECT //')
	blob=$(grep -a -oE 'pty +CONNECT [0-9]+' "$LOG.run.log" | head -1 | sed 's/.*CONNECT //')
	if grep -a -q 'DATA BOTH WAYS: PASS' "$LOG.run.log"; then d=PASS
	elif grep -a -q 'DATA BOTH WAYS: FAIL' "$LOG.run.log"; then d=fail
	else d='-'; fi
	fd=$(( ((v + 6) / 4) + 34 ))
	printf '%-8s %-9s %-10s %-14s %-14s %s\n' \
		"$v" "$fd" "$((v + 60))" "${far:-no connect}" "${blob:-no connect}" "$d"
done
echo
echo "=== recordings under $PREFIX-*"
