#!/bin/bash
#
# modems.sh -- resolve a modem ROLE to a stable device path, and refuse to
# dial into a port with no modem behind it.
#
# Source it:                . "$BENCH/modems.sh"
#                           TTY=$(modem_require supra) || exit 3
#
# Or run it:                modems.sh list          what is attached, probed
#                           modems.sh resolve supra just the path
#                           modems.sh check supra   resolve + pre-flight
#                           modems.sh pin           write modems.conf from
#                                                   what is attached right now
#
# THE TWO PROBLEMS THIS SOLVES, both of which have already cost a run.
#
# 1. `ttyUSBn` IS NOT A STABLE NAME.  It is handed out in enumeration order.
#    The hub dropped every adapter at once on 10 August; when it came back the
#    SAME two modems were on ttyUSB0 and ttyUSB1, having been ttyUSB1 and
#    ttyUSB2 before.  A harness hard-coding /dev/ttyUSB1 would have dialled
#    1901 with the USR Courier attached and reported it as the SupraExpress.
#    Nothing would have looked wrong.  /dev/serial/by-id/ carries the adapter's
#    serial number and does not move.
#
# 2. A DEVICE NODE IS NOT A MODEM.  Five calls went out into an empty line and
#    printed five rows of `no connect`, because the only check was an open()
#    inside each call, after the dial.  `atprobe.py` asks the modem instead,
#    and the pre-flight runs BEFORE anything is dialled.
#
# Resolution order, first hit wins:
#
#   1. $SUPRA_TTY / $COURIER_TTY        -- explicit override, always obeyed
#   2. testbench/modems.conf            -- pinned by serial number (preferred)
#   3. a glob over /dev/serial/by-id/   -- fallback; AMBIGUITY IS AN ERROR
#
# Step 3 refuses rather than guesses.  Two FTDI adapters were on this hub at
# one point, and "pick the first match" is how you get a confident report about
# the wrong modem.
#
BENCH=${BENCH:-/home/philpem/dev/sip-D-modem/claude_re/testbench}
MODEMS_CONF=${MODEMS_CONF:-$BENCH/modems.conf}
BYID=/dev/serial/by-id

# role      ext   by-id glob                      identity substring
# The globs are the LAST resort; modems.conf pins these by serial number.
#
# THESE GLOBS NAME THE USB-RS232 ADAPTER, NOT THE MODEM.  The modems are
# ordinary serial modems sitting behind USB adapters, so `FTDI` and `Prolific`
# are adapter makes and the serial number in the by-id string is the ADAPTER's.
# Move a modem to a different adapter and it silently inherits that adapter's
# role -- which is exactly what `_modem_expect_*` below exists to catch, since
# only the modem's own ATI response identifies the modem.
#
# THE SERIAL NUMBERS ARE NOT FUSSINESS.  `supra` was '*FTDI*' while only one
# FTDI adapter was attached.  A third modem arrived on 2026-08-13 behind a
# second FTDI adapter, and '*FTDI*' immediately matched two devices, which
# `modem_resolve` refuses as AMBIGUOUS -- breaking every supra run until the
# glob was narrowed.  Any new device behind an FTDI adapter must be added WITH
# its adapter serial, and the existing ones left specific.
#
_modem_glob_supra='*FTDI*BG00D17G*'
_modem_glob_courier='*Prolific*'
_modem_glob_olinet='*FTDI*B002XHV6*'
_modem_ext_supra=1901
_modem_ext_courier=1902
_modem_ext_olinet=1903
# What the modem itself must say it is.  These are STATED, not derived from
# whatever happened to be plugged in when `pin` last ran -- `pin` checks the
# attached modem against them and refuses on a mismatch, so swapping the two
# modems between adapters is caught at pin time rather than becoming the new
# definition of the role.
_modem_expect_supra='SupraExpress'
_modem_expect_courier='Courier'
#
# The Oli'Net answers ATI3/ATI7 with "Oli'Net V92 Ready" and ATI0/ATI with
# "56000 V5.015DS".  The apostrophe is real and would need quoting, so the
# identity substring is the half without it.
#
_modem_expect_olinet='V92 Ready'
#
# WHAT THE OLI'NET ACTUALLY IS, because a third far end is only useful if it
# is independent of the other two.  Conexant CX06827-11 with an SST 39SF020
# flash (256 KB) and an IDT 71024 SRAM beside it -- a CONTROLLER-BASED modem
# running its own firmware out of flash, not a host-driven softmodem.
# Conexant is the Rockwell lineage, which makes it a different DSP heritage
# from both the USR Courier and from Smart Link.  So a behaviour seen against
# the Courier AND the Oli'Net is a property of our end; one seen against only
# one of them is an interop quirk.
#

# THE INIT STRING IS PER MODEM, because AT commands are not portable.
# `AT&A3` (extended result codes) and `AT&B1` (fixed DTE rate) are USR
# commands; the Oli'Net answers ERROR to both, and a run that ignores that
# proceeds with a modem in an unknown state.  Seen on 2026-08-13.
#
# ORDER MATTERS AND `AT&F` MUST COME FIRST.  The harness sets ATS0=1 for
# autoanswer BEFORE sending this string, so an `AT&F` anywhere but the front
# resets S0 back to its factory value and the modem never picks up.  That is
# exactly what happened to four Oli'Net calls.
_modem_init_supra='AT&F'
_modem_init_courier='AT&F;AT&A3;AT&B1'
_modem_init_olinet='AT&F'

# NOT ATTACHED, but available if a question needs another far end.  Chipsets
# unknown -- run modemid.py once each and record what it says rather than
# guessing from the badge.  The SupraExpress is the worked example: nothing
# about the name says Rockwell, and ATI7 reporting RCV56DPF-PLL is the only
# reason anyone here knows it is one.  Chipsets below are UNVERIFIED:
#
#   Hayes Accura 56K model 15400        Hayes liquidated Jan 1999, so late
#                                       production; plausibly Rockwell
#   US Robotics 56K Faxmodem            64-245630-04R, possibly model 5630D;
#                                       USR badge does not guarantee USR silicon
#   Diamond (Phil's)                    Rockwell
#
# NO LUCENT/AGERE PART IS AVAILABLE, so any claim of the form "every modem
# does X" is really "every Rockwell-family modem and one USR does X".  Lucent
# was one of the three big V.34/V.90 chipset families and this bench cannot
# see it.

# ASK THE MODEM FIRST, fall back to the table.  The by-id path names the
# ADAPTER, so the table above is keyed on the wrong thing: move a modem to
# another adapter and it inherits a dialect it does not speak.  modemid.py
# sends ATI0/ATI3/ATI7 and classifies from the reply, which is keyed on the
# device that has to run the commands.  The table survives as the fallback for
# a modem that will not answer a probe -- better a safe `AT&F` than no run.
_modem_dialect() {
	local role=$1 field=$2 path out
	path=$(modem_resolve "$role" 2>/dev/null) || return 1
	out=$(timeout 20 python3 "$BENCH/modemid.py" "$path" --field "$field" \
	      2>/dev/null) || return 1
	[ -n "$out" ] || return 1
	echo "$out"
}

modem_init() {
	local t
	if t=$(_modem_dialect "$1" init); then echo "$t"; return 0; fi
	case "$1" in
	supra)   echo "$_modem_init_supra" ;;
	courier) echo "$_modem_init_courier" ;;
	olinet)  echo "$_modem_init_olinet" ;;
	*) echo "modem_init: unknown role '$1'" >&2; return 1 ;;
	esac
}

# The link-diagnostic command, which is NOT portable and fails SILENTLY when
# it is wrong: ATI11 on a USR prints a full link report, and on the Conexant it
# prints the product name and OK.  Four Oli'Net calls were recorded with an
# empty far-end rate before anyone noticed the command was the problem rather
# than the modem.
modem_diag() {
	local t
	if t=$(_modem_dialect "$1" diag); then echo "$t"; return 0; fi
	case "$1" in
	courier) echo "ATI11" ;;
	*)       echo "AT&V1" ;;
	esac
}

# The extension each role answers on.  Kept here so a script that needs both
# cannot pair the right modem with the wrong number.
modem_ext() {
	case "$1" in
	supra)   echo "$_modem_ext_supra" ;;
	courier) echo "$_modem_ext_courier" ;;
	olinet)  echo "$_modem_ext_olinet" ;;
	*) echo "modem_ext: unknown role '$1'" >&2; return 1 ;;
	esac
}

# Field $2 (path) or $3 (expected identity) for a role in modems.conf.
_modem_conf_field() {
	[ -f "$MODEMS_CONF" ] || return 1
	awk -v role="$1" -v n="$2" '
		/^[[:space:]]*(#|$)/ { next }
		$1 == role { print $n; found = 1; exit }
		END { exit !found }' "$MODEMS_CONF"
}

# Path only.  No probing, no I/O -- so a caller that just wants to print the
# configuration does not have to touch the hardware.
modem_resolve() {
	local role=$1 env_override path matches n

	# An explicit device path passes straight through: ad-hoc runs against a
	# third modem should not need a config entry.
	case "$role" in
	/dev/*) echo "$role"; return 0 ;;
	esac

	case "$role" in
	supra)   env_override=${SUPRA_TTY:-} ;;
	courier) env_override=${COURIER_TTY:-} ;;
	olinet)  env_override=${OLINET_TTY:-} ;;
	*) echo "modem_resolve: unknown role '$role' (supra|courier|olinet|/dev/...)" >&2
	   return 1 ;;
	esac

	if [ -n "$env_override" ]; then
		echo "$env_override"
		return 0
	fi

	if path=$(_modem_conf_field "$role" 2); then
		echo "$path"
		return 0
	fi

	# Fallback: glob by-id.  Refuse on 0 or >1.
	local glob
	eval "glob=\$_modem_glob_$role"
	matches=$(ls -1 "$BYID"/$glob 2>/dev/null)
	n=$(printf '%s' "$matches" | grep -c . 2>/dev/null || true)
	if [ "${n:-0}" -eq 1 ]; then
		echo "$BYID/$(basename "$matches")"
		return 0
	fi
	if [ "${n:-0}" -eq 0 ]; then
		echo "modem_resolve: no device in $BYID matches '$glob' for role '$role'" >&2
		echo "  attached right now:" >&2
		ls -1 "$BYID" 2>/dev/null | sed 's/^/    /' >&2 ||
			echo "    (nothing -- $BYID does not exist)" >&2
	else
		echo "modem_resolve: '$glob' is AMBIGUOUS for role '$role' -- $n matches:" >&2
		printf '%s\n' "$matches" | sed 's/^/    /' >&2
		echo "  pin it: $BENCH/modems.sh pin  (writes $MODEMS_CONF)" >&2
	fi
	return 1
}

# Is there a modem there, and is it the RIGHT one?
modem_preflight() {
	local path=$1 label=${2:-modem} expect=${3:-} out
	if [ ! -e "$path" ]; then
		echo "PRE-FLIGHT FAIL: $label: $path does not exist" >&2
		return 1
	fi
	# RETRY, because "busy right now" and "gone" are different faults.
	# A 30-call batch aborted at call 2 when the modem had not finished the
	# previous call's `+++ATH` guard time before the next pre-flight probed
	# it: present, powered, working seconds later, and reported as absent.
	# A guard that cannot survive that is a guard that stops good runs.
	# Three attempts, 2 s apart -- still fails hard if the modem is really
	# away, which is the case it exists for.
	local try
	for try in 1 2 3; do
		if out=$(python3 "$BENCH/atprobe.py" "$path" \
				${expect:+--expect "$expect"} 2>&1); then
			break
		fi
		[ "$try" -eq 3 ] && { echo "PRE-FLIGHT FAIL: $label: $out" >&2
			echo "                 (3 attempts, 2 s apart)" >&2
			return 1; }
		sleep 2
	done
	echo "  pre-flight OK: $label ${out#OK|}"
	echo "                 $path"
	return 0
}

# The one callers should use: resolve, pre-flight, echo the path.
#
# The path goes to stdout and everything else to stderr, so
# `TTY=$(modem_require supra) || exit 3` captures a clean path while the
# operator still sees the diagnosis.
modem_require() {
	local role=$1 path expect
	path=$(modem_resolve "$role") || return 1
	expect=$(_modem_conf_field "$role" 3 2>/dev/null || true)
	modem_preflight "$path" "$role" "$expect" >&2 || return 1
	echo "$path"
}

# Provenance for a report: which modem, which adapter, which extension.
#
# A result table that does not name its modem is not reproducible -- this bench
# has two, they answer on different extensions, and `ttyUSBn` renumbering has
# already swapped them once.  Every summary script prints this block, and
# `row.sh` writes a greppable MODEM: line into each run log.
modem_provenance() {
	local role=$1 path ident
	path=$(modem_resolve "$role" 2>/dev/null) || { echo "$role (UNRESOLVED)"; return 1; }
	ident=$(python3 "$BENCH/atprobe.py" "$path" 2>/dev/null) || ident="OK|(no reply)"
	printf '%s | ext %s | %s | %s\n' \
		"$role" "$(modem_ext "$role" 2>/dev/null || echo '?')" \
		"${ident#OK|}" "$path"
}

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

_modem_cli_list() {
	if [ ! -d "$BYID" ]; then
		echo "$BYID does not exist -- no USB serial adapter is attached."
		echo "Check the hub: replug it, power-cycle it, or check the upstream port."
		return 1
	fi
	printf '%-70s %-10s %s\n' DEVICE NODE 'ANSWERS AT'
	printf '%-70s %-10s %s\n' ------ ---- ----------
	local d node out
	for d in "$BYID"/*; do
		[ -e "$d" ] || continue
		node=$(readlink -f "$d")
		if out=$(python3 "$BENCH/atprobe.py" "$d" 2>&1); then
			out="yes -- ${out#OK|}"
		else
			out="no (${out#atprobe: *: })"
		fi
		printf '%-70s %-10s %s\n' "$(basename "$d")" "$node" "$out"
	done
	echo
	local role
	for role in supra courier; do
		printf '  %-8s (ext %s) -> %s\n' "$role" "$(modem_ext $role)" \
			"$(modem_resolve $role 2>/dev/null || echo UNRESOLVED)"
	done
}

# Write modems.conf from what is attached NOW, with each role's identity as
# read from the modem itself.  Run this after a replug; it is the one command
# that turns a fresh enumeration into a pinned configuration.
_modem_cli_pin() {
	local role path ident tmp
	tmp=$(mktemp)
	{
		echo "# testbench/modems.conf -- generated by \`modems.sh pin\`"
		echo "#"
		echo "# role      device (by-id, carries the adapter's serial number)      expected identity"
		echo "#"
		echo "# The third field is checked against what the modem reports to ATI3."
		echo "# A mismatch FAILS pre-flight, because it means the modem behind this"
		echo "# adapter has been swapped and the run would be mislabelled."
	} > "$tmp"
	local expect bad=0
	for role in supra courier; do
		path=$(modem_resolve "$role" 2>/dev/null) || {
			echo "pin: $role unresolved, skipping" >&2
			bad=1; continue
		}
		ident=$(python3 "$BENCH/atprobe.py" "$path" 2>/dev/null) || {
			echo "pin: $role at $path did not answer AT, skipping" >&2
			bad=1; continue
		}
		ident=${ident#OK|}
		eval "expect=\$_modem_expect_$role"
		# REFUSE to pin a mismatch.  Writing whatever answered as the role's
		# definition would turn a swapped pair into a configuration that
		# pre-flights green for ever after, which is worse than no config.
		case "$ident" in
		*"$expect"*) ;;
		*)
			echo "pin: REFUSING $role -> $path: identifies as '$ident'," >&2
			echo "     which does not contain '$expect'.  Are the two modems" >&2
			echo "     swapped between adapters?" >&2
			bad=1; continue ;;
		esac
		printf '%-10s %-58s %s\n' "$role" "$path" "$expect" >> "$tmp"
		echo "pin: $role -> $path  ($ident)" >&2
	done
	if [ "$bad" -ne 0 ]; then
		rm -f "$tmp"
		echo "pin: nothing written -- fix the above first" >&2
		return 1
	fi
	mv "$tmp" "$MODEMS_CONF"
	echo "wrote $MODEMS_CONF" >&2
	cat "$MODEMS_CONF"
}

# Only run the CLI when executed, not when sourced.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
	case "${1:-list}" in
	list)    _modem_cli_list ;;
	pin)     _modem_cli_pin ;;
	resolve) modem_resolve "${2:?role}" ;;
	prov)    modem_provenance "${2:?role}" ;;
	check)   modem_require "${2:?role}" ;;
	ext)     modem_ext "${2:?role}" ;;
	*) echo "usage: modems.sh list|pin|resolve <role>|check <role>|ext <role>|prov <role>" >&2
	   exit 2 ;;
	esac
fi
