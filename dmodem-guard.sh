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
ALLOWED='1901'

DIAL="$1"
SOCK="$2"

# slmodemd may hand us the raw dial string with modifiers (T, P, commas, W).
CLEAN=$(printf '%s' "$DIAL" | tr -d 'TPWtpw ,;@!' )

case " $ALLOWED " in
	*" $CLEAN "*) ;;
	*)
		echo "GUARD: REFUSING '$DIAL' (cleaned '$CLEAN'); allowed: $ALLOWED" >&2
		exit 2 ;;
esac

SECRET=/home/philpem/dev/sip-D-modem/asterisk-login-4242.secret
SERVER=$(awk -F': *' '/^Server/{print $2}'    "$SECRET")
EXT=$(awk    -F': *' '/^Extension/{print $2}' "$SECRET")
PASS=$(awk   -F': *' '/^Secret/{print $2}'    "$SECRET")
SIP_LOGIN="${EXT}:${PASS}@${SERVER}"
export SIP_LOGIN

echo "GUARD: allowing $CLEAN as ${EXT}@${SERVER}" >&2
exec /home/philpem/dev/sip-D-modem/d-modem "$CLEAN" "$SOCK"
