#!/bin/bash
#
# Does the echo, and the receive-rate deficit it explains, depend on WHICH
# MODEM is on the far end?
#
#   echo-compare.sh <role> [logprefix] [repeats]
#   echo-compare.sh supra   captures/ec-supra   5
#   echo-compare.sh courier captures/ec-courier 5
#
# THE CONTROL THIS IS.  #101/#104 measured, on the SupraExpress, that our own
# transmit returns at ~205 ms about 19 dB down, that the canceller's reach maxes
# at 31 ms, and that our receive rate sits at the SNR that echo implies.  Every
# one of those calls used the same modem, so "this is the SIP path" and "this is
# that particular modem" are not yet separated.
#
# The two hypotheses predict different things, which is what makes this worth
# the calls:
#
#   * The delay is the SIP round trip -- two traversals of the packet path
#     before the reflection reaches us.  Then the LAG should be the same for
#     both modems, because neither modem is in that path twice.
#
#   * The magnitude is the FXS port's hybrid meeting the modem's input
#     impedance.  Then the ERL may well DIFFER between modems, and the receive
#     rate should track it -- better match, less echo, higher rate.
#
# So: same lag + different ERL is the expected result and confirms the account.
# DIFFERENT LAG would refute it and mean the delay is not the packet path.
#
# The far end is deliberately given only commands both modems accept.  The
# SupraExpress is Rockwell (`AT+MS`, `ATS95`, `AT&V1`); the Courier is USR
# (`ATB`, `AT&A`, `ATI6`) and answers ERROR to all three of the Rockwell ones.
# Nothing here depends on the far end's own rate report: our RX and TX come from
# `slmodemd`, and the echo comes from our own recordings, so the measurement is
# identical for both modems.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
ROLE=${1:-supra}
PREFIX=${2:-$BENCH/captures/ec-$ROLE}
N=${3:-5}
IOD=${IOD:-240}

echo "=== pre-flight"
TTY=$(modem_require "$ROLE") || exit 3
EXT=$(modem_ext "$ROLE") || exit 3
echo
echo "MODEM UNDER TEST: $(modem_provenance "$ROLE")"
echo "IODELAY: $IOD   calls: $N   dialling: $EXT"
echo

# `AT&F` only -- the one command both command sets agree on.  V.34 is reached by
# automode on both, which is what we want anyway: forcing it with +MS would
# work on one modem and ERROR on the other.
printf '%-4s %-9s %-6s %-9s %-9s %-11s %-9s %s\n' \
	RUN MODEM CONN 'our TX' 'our RX' 'echo lag' 'ERL dB' 'sig/echo dB'
printf '%-4s %-9s %-6s %-9s %-9s %-11s %-9s %s\n' \
	---- ----- ---- ------ ------ -------- ------ -----------

for i in $(seq 1 "$N"); do
	L="$PREFIX-$i"
	SLMODEMD_IODELAY="$IOD" TTY="$TTY" \
	TTY_EXTRA="AT&F" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		bash "$BENCH/row.sh" "$L" pty "$EXT" > "$L.run.log" 2>&1

	rx=$(grep -a -oE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 | sed 's/.*CONNECT //')
	tx=$(grep -a -oE 'TxRate: *[0-9]+' "$L.run.log" | head -1 | grep -a -oE '[0-9]+$')

	if [ -n "$rx" ]; then
		read -r lag erl ser <<< "$(python3 "$BENCH/echofit.py" "$L" 2>/dev/null)"
		printf '%-4s %-9s %-6s %-9s %-9s %-11s %-9s %s\n' \
			"$i" "$ROLE" yes "${tx:-?}" "$rx" \
			"${lag:-?} ms" "${erl:-?}" "${ser:-?}"
	else
		printf '%-4s %-9s %-6s %-9s %-9s %-11s %-9s %s\n' \
			"$i" "$ROLE" NO - - - - -
	fi
done
