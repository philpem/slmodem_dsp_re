#!/bin/bash
#
# Task #104.  Does our RECEIVE rate move with IODELAY?
#
#   rxrate-sweep.sh [logprefix] [repeats] [values...]
#
# WHAT THIS IS FOR, and why it is a confirmation rather than a discovery.
#
# #101 established the asymmetry is real and one-sided: we transmit 33600 in
# five calls out of five and receive 12000 in four of them.  Symbol rate, level
# and receive-path starvation were all excluded by measurement, leaving a
# bits-per-symbol verdict -- our receiver judged an equally loud signal at the
# same symbol rate to be much noisier.
#
# `echoscan.py` then measured the reason directly, in the recordings that
# already existed: our own transmit comes back at **205.62 ms**, the same lag
# in 5 calls out of 5 to within one sample, about 19 dB down.  And the one call
# whose echo was 4.6 dB weaker is the one call that connected at 26400.
#
# The canceller cannot reach it.  `echo_delay = IODELAY + 60` samples at
# 9600 Hz, so the WHOLE adjustable range is:
#
#     IODELAY  88  ->  148 samples =  15.42 ms
#     IODELAY 240  ->  300 samples =  31.25 ms   <- the maximum
#
# 205.62 ms is 6.6x beyond the maximum.  So IODELAY cannot affect this echo at
# any setting, and the prediction is that our RX rate is FLAT across the band.
#
# THAT IS WHY THIS RUNS ANYWAY.  A knob claimed to be irrelevant should be
# shown to be irrelevant, on the same bench, rather than argued away -- the
# same discipline that says a detector which has never fired is not known to
# work.  A flat RX here confirms the echo account; an RX that MOVES refutes it
# and means something else is going on.
#
# Scored with REPEATS.  This bench connects 4 in 5 at a fixed configuration and
# RX varied 12000/12000/26400/12000/12000 at a FIXED setting, so a single call
# per point cannot distinguish a trend from that spread.  Three sweeps have
# already been retracted here for reading n=1 as signal.
#
# ATS70=7 is what makes the two directions separable: it turns on `TxRate:`, so
# `CONNECT` (our RX) and `TxRate` (our TX) can be read apart.  Every earlier
# IODELAY sweep had only one number and conflated them.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
. "$BENCH/modems.sh"
PREFIX=${1:-$BENCH/captures/rxrate}
N=${2:-3}
shift 2 2>/dev/null || true
VALUES=${*:-88 164 240}
MODEM=${MODEM:-supra}

echo "=== pre-flight"
TTY=$(modem_require "$MODEM") || exit 3
EXT=$(modem_ext "$MODEM")
echo

printf '%-8s %-11s %-9s %-14s %-22s %s\n' \
	IODELAY echo_reach CONNECTS 'our TX' 'our RX' 'echo lag (ms)'
printf '%-8s %-11s %-9s %-14s %-22s %s\n' \
	------- ---------- -------- ------ ------ -------------

for v in $VALUES; do
	conn=0; txs=""; rxs=""; lags=""
	for i in $(seq 1 "$N"); do
		L="$PREFIX-$v-$i"
		SLMODEMD_IODELAY="$v" TTY="$TTY" \
		TTY_EXTRA="AT+A8E=1,1;ATS95=47" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
			bash "$BENCH/row.sh" "$L" pty "$EXT" > "$L.run.log" 2>&1

		rx=$(grep -a -oE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 |
			sed 's/.*CONNECT //')
		tx=$(grep -a -oE 'TxRate: *[0-9]+' "$L.run.log" | head -1 |
			grep -a -oE '[0-9]+$')
		if [ -n "$rx" ]; then
			conn=$((conn + 1))
			rxs="$rxs $rx"; txs="$txs ${tx:-?}"
			# The echo is a property of the SIP path, not of IODELAY, so
			# this column should not move either.  If it does, the lag
			# measurement is picking up something else.
			lag=$(python3 "$BENCH/echoscan.py" "$L" --max-ms 300 2>/dev/null |
				awk '/strongest lag/{print $6}')
			lags="$lags ${lag:-?}"
		else
			rxs="$rxs -"; txs="$txs -"; lags="$lags -"
		fi
	done
	printf '%-8s %-11s %-9s %-14s %-22s %s\n' \
		"$v" "$(awk "BEGIN{printf \"%.1f ms\", ($v+60)*1000/9600}")" \
		"$conn/$N" "$txs" "$rxs" "$lags"
done

echo
echo "PREDICTION: our RX flat across the band, because 205.6 ms is 6.6x beyond"
echo "the canceller's reach even at IODELAY 240 (31.25 ms)."
