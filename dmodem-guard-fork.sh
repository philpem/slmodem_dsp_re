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
# The authorised destinations, each one because Phil named it.  Identified by
# what the modem says it is, NOT by ttyUSBn -- that is handed out in
# enumeration order and has already renumbered under this bench once:
#
#   1901  SupraExpress 56e PRO   Rockwell RCV56DPF-PLL L8571A
#   1902  USR Courier            USR's own DSP, rev 11.3
#   1903  Oli'Net V92 Ready      Conexant CX06827-11        added 2026-08-13
#
# Space-separated for the `case` below; converted to commas before it reaches
# d-modem, which splits on ',' -- a space-separated list would arrive as one
# entry, match nothing, and silently block every call.
#
# THIS IS THE THIRD OF FOUR PLACES A NUMBER IS CHECKED, and the one that
# actually stops a call: row.sh, this script, then `dmodem_dest_allowed()`
# inside the binary (which this exports DMODEM_ALLOWED_DEST to), plus
# relay.py/rtprelay.py for the relay path.  Adding a destination in fewer than
# all of them fails in a way that looks like a modem fault: four calls to 1903
# were spent before anyone noticed the number never reached pjsua at all.
ALLOWED='1901 1902 1903'

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
# The enforceable block for the fork is dmodem_dest_allowed() in d-modem.c,
# at the pjsua_call_make_call site.  This guard still covers upstream
# slmodemd, which does pass the number in argv[1].
if [ -z "$CLEAN" ]; then
	echo "GUARD: startup spawn, no destination in argv; d-modem's own allow-list is the block" >&2
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

# Second layer, inside the binary: dmodem_dest_allowed() in d-modem.c checks
# the number that arrives over the socket, which is the only one that exists
# in this configuration.
DMODEM_ALLOWED_DEST=$(printf '%s' "$ALLOWED" | tr ' ' ',')
export DMODEM_ALLOWED_DEST

# WHICH BINARY.  A/B work needs more than one build, but an arbitrary path from
# the environment would let a binary WITHOUT dmodem_dest_allowed() be exec'd
# here, and that in-binary check is the only block that sees the number in this
# configuration (see the note at line 41).  So the variant selects from a FIXED
# list of names in a fixed directory -- never a caller-supplied path -- and the
# chosen file is then checked for the guard before it is exec'd.  That is
# stricter than the unconditional exec this replaced.
#
#   default   the production build
#   jb        + jitter-buffer stats, DMODEM_JB_DISCARD selects the algorithm
#   noaudio   built without -DWITH_AUDIO: no monitor speaker, no splitcomb
DMODEM_DIR=/home/philpem/dev/D-Modem-fork
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

# The destination allow-list must be compiled into whatever we are about to
# run.  A build with that check removed would take DMODEM_ALLOWED_DEST from the
# environment and ignore it, leaving no block at all.
if ! grep -qa 'DMODEM_ALLOWED_DEST' "$DMODEM_BIN"; then
	echo "GUARD: REFUSING $DMODEM_BIN -- no in-binary destination guard" >&2
	exit 2
fi

echo "GUARD: launching ${DMODEM_VARIANT:-default} as ${EXT}@${SERVER}, destinations limited to $ALLOWED" >&2
exec "$DMODEM_BIN" "$CLEAN" "$@"
