#!/bin/sh
#
# slmodemd's `-e` target.  slmodemd execs this as:  <this> <dialstring> <socket>
#
# HARD DESTINATION GUARD.  The PBX this registers to can reach the PSTN, so
# this is the one place where a number becomes a call, and the allow-list is
# checked here rather than anywhere more convenient.  Anything not listed is
# refused and the call never reaches pjsua.  Do not turn ALLOWED into a
# variable, an argument, or an environment lookup.
#
# The authorised destinations, each one because Phil named it.  Hardware modems
# are identified by what the modem SAYS it is, NOT by ttyUSBn -- that is handed
# out in enumeration order and has already renumbered under this bench once.
# Sourced from asterisk-info.secret, which is deliberately not in git:
#
#   1901         SupraExpress 56e PRO   Rockwell RCV56DPF-PLL L8571A
#   1902         USR Courier            USR's own DSP, rev 11.3
#   1903         Oli'Net V92 Ready      Conexant CX06827-11    added 2026-08-13
#   4242         SIP extension, this bench's own registration
#   4343         SIP extension, the second registration        added 2026-08-17
#   01138773693  A&A DID -> rings 4242.  OUT AND BACK IN       added 2026-08-17
#   01133501928  Sipgate DID -> rings 4242.  OUT AND BACK IN   added 2026-08-17
#
# THE LAST TWO LEAVE THE BUILDING AND COME BACK, which is the point of them: the
# call egresses via the A&A outgoing trunk, reaches the operator, and is routed
# back in to 4242.  A&A returns through A&A's own infrastructure at low latency;
# A&A <-> Sipgate has to traverse the PSTN, and Sipgate may additionally impose
# G.711A/G.711U transforms that have not been characterised yet.  So these two
# are NOT interchangeable as a "loopback" -- they are two different channels and
# a result from one says nothing about the other.
#
# The A&A outgoing trunk's own number, 01132608958, is deliberately NOT here.
# It is the identity calls egress WITH, not a destination to dial.
#
# Space-separated for the `case` below; converted to commas before it reaches
# d-modem, which splits on ',' -- a space-separated list would arrive as one
# entry, match nothing, and silently block every call.
#
# MATCHING IS EXACT, and the external numbers are stored in the form the notes
# give: full national, leading 0, no dial-out prefix.  If this PBX ever needs a
# prefix for an outside line, "9W01138773693" cleans to "901138773693" and is
# REFUSED rather than prefix-matched -- which is the safe direction, but it will
# look like the trunk is down.  Add the prefixed form explicitly if that day
# comes; do not soften the match.
#
# THIS IS NOW THE LAST PLACE A NUMBER IS CHECKED, and since 2026-08-17 it only
# sees the number when argv carries one.  The places were: row.sh, this script,
# `dmodem_dest_allowed()` inside the binary, and relay.py/rtprelay.py for the
# relay path.  THE THIRD IS GONE -- see the note at the argv check below -- so
# the fork's socket dials are unchecked and this list is three, not four.
# Adding a destination in fewer than all of them still fails in a way that looks
# like a modem fault: four calls to 1903 were spent before anyone noticed the
# number never reached pjsua at all.
ALLOWED='1901 1902 1903 4242 4343 01138773693 01133501928'

# The fork's d-modem takes THREE positional arguments:
#   dialstr audio_sock sip_sock
# so everything after the dial string is forwarded verbatim.  Dropping the
# third silently produced its usage message and an immediately dead child.
DIAL="$1"
shift

# slmodemd may hand us the raw dial string with modifiers (T, P, commas, W).
CLEAN=$(printf '%s' "$DIAL" | tr -d 'TPWtpw ,;@!' )

# The fork starts d-modem ONCE at init with an EMPTY dial string and sends the
# number over the socket afterwards (modem_main.c socket_dial -> "D" packet),
# so argv never carries a destination in this configuration.  Refusing the
# empty spawn kills the child and every later dial fails with ECONNREFUSED.
#
# THERE USED TO BE A SECOND BLOCK BELOW THIS ONE AND THERE IS NOT ANY MORE.
# dmodem_dest_allowed() in d-modem.c was the only check that ever saw a
# socket-dialled number; it was removed from the fork on 2026-08-17 because the
# vendored tree had to go back to being cryan209's, and Phil accepted unchecked
# dialling on the condition that we only dial numbers he has authorised and are
# careful with any change to dialling.  So: this argv check still covers
# upstream slmodemd, which passes the number in argv[1], and NOTHING covers the
# fork's socket dials.  Do not let the launch line pretend otherwise.
if [ -z "$CLEAN" ]; then
	echo "GUARD: startup spawn, no destination in argv; nothing here can check it -- see the launch line for whether the binary can" >&2
else
	case " $ALLOWED " in
		*" $CLEAN "*) ;;
		*)
			echo "GUARD: REFUSING '$DIAL' (cleaned '$CLEAN'); allowed: $ALLOWED" >&2
			exit 2 ;;
	esac
fi

SECRET=/home/philpem/dev/sip-D-modem/asterisk-login-4242.secret
SERVER=$(awk -F': *' '/^Server/{print $2}'    "$SECRET")
EXT=$(awk    -F': *' '/^Extension/{print $2}' "$SECRET")
PASS=$(awk   -F': *' '/^Secret/{print $2}'    "$SECRET")
# Passed by environment, never on the command line: --sip-password would put
# the secret in argv where `ps` exposes it for the life of the call.
SIP_SERVER="$SERVER"
SIP_USER="$EXT"
SIP_PASSWORD="$PASS"
export SIP_SERVER SIP_USER SIP_PASSWORD

# STILL EXPORTED, AND CURRENTLY READ BY NOTHING.  dmodem_dest_allowed() used to
# consume this; the default and noaudio builds no longer have it, so this is
# inert for them.  It is kept because d-modem-jb is an OLDER binary that DOES
# still contain the check, and because a future build that restores it picks the
# list up again with no change here.  Exporting an unread variable costs
# nothing; silently dropping it would break the jb variant.
DMODEM_ALLOWED_DEST=$(printf '%s' "$ALLOWED" | tr ' ' ',')
export DMODEM_ALLOWED_DEST

# WHICH BINARY.  A/B work needs more than one build, but an arbitrary path from
# the environment would let anything at all be exec'd from inside the guard --
# which is the one place on this bench where a number becomes a call.  So the
# variant selects from a FIXED list of names in a fixed directory, never a
# caller-supplied path.  That is stricter than the unconditional exec this
# replaced, and it is the part that still matters now that the in-binary
# allow-list is gone: the directory and the three names are the whole set of
# things this can ever run.
#
# (No line numbers in these cross-references on purpose.  This file has been
# renumbered twice today and a comment pointing at "line 41" was already wrong.)
#
#   default   the production build
#   jb        + jitter-buffer stats, DMODEM_JB_DISCARD selects the algorithm
#   noaudio   built without -DWITH_AUDIO: no monitor speaker, no splitcomb
DMODEM_DIR=/home/philpem/dev/sip-D-modem/d-modem
case "${DMODEM_VARIANT:-default}" in
	default) DMODEM_BIN=$DMODEM_DIR/d-modem ;;
	jb)      DMODEM_BIN=$DMODEM_DIR/d-modem-jb ;;
	noaudio) DMODEM_BIN=$DMODEM_DIR/d-modem-noaudio ;;
	*)
		echo "GUARD: REFUSING unknown DMODEM_VARIANT '$DMODEM_VARIANT'" >&2
		exit 2 ;;
esac

if [ ! -x "$DMODEM_BIN" ]; then
	echo "GUARD: REFUSING, $DMODEM_BIN is not executable" >&2
	exit 2
fi

# WHETHER THE BINARY CARRIES ITS OWN CHECK -- REPORTED, NOT REQUIRED.
#
# This used to `exit 2` on a binary with no in-binary allow-list, and that was
# right while one existed: with the fork's empty-argv startup it was the only
# thing that ever saw a socket-dialled number, so a build without it left no
# block at all.  Since 2026-08-17 the default and noaudio builds do not have it
# and refusing them would stop every bench call, so the check now DESCRIBES the
# binary instead of vetoing it.
#
# It is deliberately still a `grep` of the binary and not a hardcoded string:
# d-modem-jb is an older build that DOES still contain the check, so the answer
# genuinely differs per variant, and it will start saying PRESENT again by
# itself if the check is ever restored.  Probing the artefact means this cannot
# drift out of date the way a comment can.
if grep -qa 'DMODEM_ALLOWED_DEST' "$DMODEM_BIN"; then
	INBIN='in-binary allow-list PRESENT (socket dials checked)'
else
	INBIN='in-binary allow-list ABSENT (socket dials NOT checked)'
fi

# THE LAUNCH LINE IS EVIDENCE, so it must not claim a limit that is not
# enforced.  It used to print "destinations limited to $ALLOWED" for every
# launch; with no in-binary check that is false for the fork's socket dials, and
# this line is exactly what gets read out of a log when a call turns up
# somewhere unexpected.  A guard that lies in its own log is worse than one that
# says nothing.  So say which of the two cases this launch actually is.
if [ -n "$CLEAN" ]; then
	echo "GUARD: launching ${DMODEM_VARIANT:-default} as ${EXT}@${SERVER}; argv destination '$CLEAN' checked against $ALLOWED; $INBIN" >&2
else
	echo "GUARD: launching ${DMODEM_VARIANT:-default} as ${EXT}@${SERVER}; startup spawn, no argv destination to check; $INBIN" >&2
fi
exec "$DMODEM_BIN" "$CLEAN" "$@"
