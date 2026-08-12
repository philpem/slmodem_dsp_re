#!/bin/sh
#
# One D-Modem call to the lab modem, with a HARD DESTINATION GUARD.
#
# The PBX this registers to can reach the PSTN.  Nothing here may dial
# anything except the extensions explicitly authorised for this bench, so the
# allow-list is checked before pjsua is started and the script exits non-zero
# on anything else.  Do not "improve" this into a variable.
#
ALLOWED='1901'

DEST="$1"; shift
case " $ALLOWED " in
	*" $DEST "*) ;;
	*)
		echo "dial.sh: REFUSING destination '$DEST' -- allow-list is: $ALLOWED" >&2
		exit 2 ;;
esac

SECRET=/home/philpem/dev/sip-D-modem/asterisk-login-4242.secret
SERVER=$(awk -F': *' '/^Server/{print $2}'   "$SECRET")
EXT=$(awk    -F': *' '/^Extension/{print $2}' "$SECRET")
PASS=$(awk   -F': *' '/^Secret/{print $2}'    "$SECRET")

# Never echoed, never in argv -- d-modem reads it from the environment.
SIP_LOGIN="${EXT}:${PASS}@${SERVER}"
export SIP_LOGIN

echo "dial.sh: ${EXT}@${SERVER} -> ${DEST}"
exec "$@"
