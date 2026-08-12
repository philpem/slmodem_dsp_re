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
# 1901 = SupraExpress 56e PRO (ttyUSB1), 1902 = second modem (ttyUSB2).
# Space-separated for the `case` below; converted to commas before it reaches
# d-modem, which splits on ',' -- a space-separated list would arrive as one
# entry, match nothing, and silently block every call.
ALLOWED='1901 1902'

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

echo "GUARD: launching as ${EXT}@${SERVER}, destinations limited to $ALLOWED" >&2
exec /home/philpem/dev/D-Modem-fork/d-modem "$CLEAN" "$@"
