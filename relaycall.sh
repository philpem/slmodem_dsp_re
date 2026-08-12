#!/bin/bash
#
# THE CONTROL EXPERIMENT FOR FINDING 1466: two hardware modems, over the path
# our calls actually take, with no slmodemd and no blob anywhere in the call.
#
#   relaycall.sh [logprefix] [originator-role] [answer-role]
#   relaycall.sh captures/relay-1 supra courier
#
# WHY NOT hw2hw.sh.  It dials 1902 from 1901 and both modems hang off the same
# VG204, so the Cisco matches `dial-peer voice 2 pots` and bridges port 0/0 to
# port 0/1 inside itself.  No RTP, no jitter buffer, no codec round trip
# (finding 1467).  Here the originating modem dials 4242 instead, which matches
# only the `.T` voip peer, so it goes out to Asterisk and comes back to
# `relay.py` -- and the relay then places the second leg to the other modem.
# The audio traverses Asterisk twice and RTP four times, which is a superset of
# the path an slmodemd call uses.
#
# WHICH MAKES IT ONE-SIDED, and the result has to be read that way:
#
#   both modems reach their ceiling  ->  the path is clean.  Conclusive: it
#                                        carries V.34 at full rate, so the
#                                        deficit in 1466 is our receiver.
#   the same one-sided deficit       ->  AMBIGUOUS.  This path has one more
#                                        RTP hop and one more jitter buffer
#                                        than the one under investigation.
#
# Only the first outcome settles anything.  Do not report the second as
# "the path is at fault".
#
# WHAT TO READ OUT OF IT.  Each modem's own view of the link, which is the
# whole measurement:
#
#   CARRIER nnnnn   what that modem's receiver locked onto -- the FAR end's
#                   transmit rate.  Needs S95/W on the Rockwell.
#   CONNECT nnnnn   with &B1 on the USR so it reports the link and not the DTE
#   PROTOCOL:       whether V.42 agreed, which 1466 also touches
#
# Two CARRIERs, one per modem, is the two-directional measurement slmodemd
# cannot give us because it is one of the two ends.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
. "$BENCH/modems.sh"

LOG=${1:-$BENCH/captures/relay}
FROM_ROLE=${2:-supra}		# originates -- dials 4242
TO_ROLE=${3:-courier}		# answers the relay's outbound leg
HOLD=${HOLD:-60}

echo "=== pre-flight"
FROM=$(modem_require "$FROM_ROLE") || exit 3
TO=$(modem_require "$TO_ROLE")     || exit 3
TO_EXT=$(modem_ext "$TO_ROLE")     || exit 3
echo
echo "ORIGINATOR: $(modem_provenance "$FROM_ROLE")"
echo "ANSWERER:   $(modem_provenance "$TO_ROLE")"
echo

# The relay must own the 4242 registration for the whole call.  If a d-modem
# is registered too, Asterisk has two contacts for one AOR and the INVITE goes
# to whichever it likes -- which is a coin toss that looks like a routing bug.
if pgrep -x d-modem >/dev/null 2>&1 || pgrep -x slmodemd >/dev/null 2>&1; then
	echo "REFUSING: d-modem or slmodemd is running and will fight us for the" >&2
	echo "          4242 registration.  Stop the batch first." >&2
	exit 4
fi

echo "=== starting the relay (registers 4242, bridges inbound -> $TO_EXT)"
# `timeout` because relay.py's timers live inside a blocking read on pjsua's
# stdout: once both legs are up pjsua can go quiet for a minute, and if
# call.py dies early nothing ever unblocks the loop.
timeout $((HOLD + 120)) python3 "$BENCH/relay.py" --dial "$TO_EXT" --hold "$HOLD" \
	--rec "$LOG.conf.wav" \
	--log "$LOG.relay.log" > "$LOG.relay.out" 2>&1 &
RELAY=$!
trap 'kill $RELAY 2>/dev/null' EXIT

# Wait for REGISTERED before the modem dials, or the INVITE arrives at an
# Asterisk that has nowhere to send it and the modem hears fast busy.
for i in $(seq 1 40); do
	grep -q REGISTERED "$LOG.relay.out" 2>/dev/null && break
	kill -0 $RELAY 2>/dev/null || { echo "relay died:"; cat "$LOG.relay.out"; exit 5; }
	sleep 0.5
done
if ! grep -q REGISTERED "$LOG.relay.out" 2>/dev/null; then
	echo "relay never registered:" >&2; cat "$LOG.relay.out" >&2; exit 5
fi
echo "    relay registered"

# S95=47 on the Rockwell so it reports CARRIER (its RECEIVE rate) as well as
# CONNECT; &A3 and &B1 on the USR so its CONNECT carries the link rate and
# protocol rather than the DTE rate.  Semicolon-separated so an ERROR from any
# one of them is visible instead of hidden among the OKs -- which is how
# `AT+MS=34,1` was caught: it ERRORs on this Supra, so the first relay call ran
# with an unset carrier list and nobody would have known.  V.34 automode is the
# default anyway, and it is what the whole baseline was measured with.
echo "=== $FROM_ROLE dials 4242; relay bridges to $TO_ROLE on $TO_EXT"
python3 "$BENCH/call.py" --tty "$FROM" --pty "$TO" \
	--originator tty --dial 4242 --log "$LOG.call.log" \
	--hold "$HOLD" \
	--tty-extra "${TTY_EXTRA:-AT&F;ATS95=47}" \
	--pty-extra "${PTY_EXTRA:-AT&F;AT&A3;AT&B1}"
rc=$?

wait $RELAY 2>/dev/null
trap - EXIT

echo
echo "=== relay"
sed -n 's/^relay: /    /p' "$LOG.relay.out"
echo "=== media statistics (did audio actually flow?)"
grep -aE 'RX pt=|TX pt=|Call time|pkt|loss|jitter|discard' "$LOG.relay.log" 2>/dev/null |
	tail -30 | sed 's/^/    /'
echo "=== what each modem said during the call"
grep -aE 'CARRIER|CONNECT|PROTOCOL|NO CARRIER|BUSY|NO ANSWER|ERROR' "$LOG.call.log" |
	sed 's/^/    /'

# THE MEASUREMENT.  call.py tears down with `+++` and `ATH`, not `ATZ`, so the
# last-connection registers survive -- and this is the only window in which
# they do.  The Courier's ATI6 lists RECEIVE and TRANSMIT rates separately,
# which is the second direction the result codes cannot give us; the Supra's
# S86 says why the call ended.
echo
echo "=== last-connection report, read before anything resets them"
{
	python3 "$BENCH/lastlink.py" "$FROM" --label "$FROM_ROLE"
	python3 "$BENCH/lastlink.py" "$TO"   --label "$TO_ROLE"
} 2>&1 | tee "$LOG.lastlink.log"

echo
echo "=== HOW TO READ THIS"
echo "    The threshold is 14400, NOT 33600.  Our slmodemd calls sit at 14400"
echo "    median (17 of 27 exactly there), the Courier's own ceiling as a far"
echo "    end has been 28800 rather than 33600, and this path adds a full"
echo "    a-law encode/decode round trip per direction that the path under"
echo "    investigation does not have -- pjsua's bridge decodes both legs to"
echo "    linear, mixes, and re-encodes.  Some loss here is EXPECTED."
echo
echo "    both directions well above 14400  -> the path carries V.34.  The"
echo "                                         deficit in 1466 is our receiver."
echo "    the same one-sided deficit        -> AMBIGUOUS.  One more RTP hop and"
echo "                                         one more transcode than the path"
echo "                                         being investigated.  Do not"
echo "                                         report it as 'the path is bad'."
echo
echo "    No data was written (--hold), so this is a RATE measurement only."
exit $rc
