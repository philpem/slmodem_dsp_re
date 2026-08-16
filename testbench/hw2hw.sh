#!/bin/bash
#
# CONTROL EXPERIMENT: one physical modem dials the other, across the same PBX
# and the same SIP/RTP path, with no slmodemd and no blob anywhere in it.
#
#   hw2hw.sh <logprefix> [originator-tty] [answer-tty] [number]
#
# This is the measurement that decides whose fault the V.8 failure is. If a
# USR Courier and a SupraExpress negotiate V.34 to each other over this path,
# the path carries V.8 and V.34 fine and the blob is the problem. If they fail
# each other the same way, the path is the problem and the blob is exonerated.
#
# *** IT DOES NOT MEASURE THE SIP PATH, AND THE COMMENT ABOVE IS WRONG ***
#
# Both modems hang off the SAME VG204.  Dialling 1902 matches `dial-peer voice
# 2 pots`, a local POTS peer with `destination-pattern 1902` and `port 0/1`,
# which is far more specific than the `.T` voip peer that reaches Asterisk.
# The Cisco therefore connects port 0/0 to port 0/1 INTERNALLY.  Nothing
# leaves the box: no RTP, no jitter buffer, no codec round trip through
# Asterisk, none of the transport that every other measurement on this bench
# includes.
#
# So a result here is about the VG204's own analogue and companding stages and
# says nothing about the path our calls actually take.  Comparing it against a
# slmodemd run is comparing two different channels.
#
# To make two modems talk over the REAL path the media has to be forced out of
# the gateway and back, which is #94: our own SIP endpoint answers 1901, dials
# 1902 and relays RTP between the two legs.  Until that exists this script is
# an ATA loopback test, which is a legitimate thing to want -- just not the
# control experiment its name suggests.
#
# `call.py` opens both ends with the same open_raw(), so its `--pty` argument
# takes a real serial port perfectly well; nothing else changes.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
. "$BENCH/modems.sh"
CAPTURES=$BENCH/captures
mkdir -p "$CAPTURES"

LOG=${1:-$CAPTURES/hw2hw}
FROM_ROLE=${2:-supra}		# originates
TO_ROLE=${3:-courier}		# answers
DIAL=${4:-1902}

# Same allow-list as row.sh.  Dialling here goes straight from an AT command to
# the PBX, so this script is the only guard on the path.
case " 1901 1902 4242 " in
	*" $DIAL "*) ;;
	*)
		echo "REFUSING to dial '$DIAL': not 1901, 1902 or 4242" >&2
		exit 2 ;;
esac

# BOTH ends pre-flight, and both before the dial.  This script has no
# slmodemd in it, so a missing modem here produces no log at all to diagnose
# from afterwards -- only the absence of a CONNECT.
echo "=== pre-flight"
FROM=$(modem_require "$FROM_ROLE") || exit 3
TO=$(modem_require "$TO_ROLE")     || exit 3

echo "=== $FROM dials $DIAL, answered by $TO -- no blob in this call"
python3 "$BENCH/call.py" --tty "$FROM" --pty "$TO" \
	--originator tty --dial "$DIAL" --log "$LOG.call.log" \
	--tty-extra "${TTY_EXTRA:-AT&F}" --pty-extra "${PTY_EXTRA:-AT&F}"
