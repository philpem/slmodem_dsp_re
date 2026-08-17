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
ALLOWED='1901 1902 1903 4242 4343 01138773693 01133501928'

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
# WAS the old strozfriedberg build at sip-D-modem/d-modem, which was a FILE at
# that path.  The two D-Modem forks were consolidated and that path is now the
# directory holding the surviving one, so this runs the fork's binary -- the
# one carrying d-modem.c's own destination allow-list.  That makes row2.sh,
# rtprelay.py and relay.py strictly safer than they were, and it is a change
# in what they execute: flagged rather than silent.
exec /home/philpem/dev/sip-D-modem/d-modem/d-modem "$CLEAN" "$SOCK"
