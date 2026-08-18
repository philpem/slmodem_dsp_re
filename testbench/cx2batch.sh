#!/bin/bash
#
# cx2batch.sh -- the complex2 arm of the ATA impedance comparison.
#
#   cx2batch.sh [N]            # N calls, default 10
#
# THE ONE THING THAT CHANGED IS THE ATA.  The VG204's analogue ports used to
# carry no `impedance` line at all, so they ran at the platform default of
# 600 ohms resistive; they now carry `impedance complex2`.  Nothing else in
# the gateway config moved.  Terminating impedance sets the 2-to-4-wire
# hybrid match, so it moves trans-hybrid loss, the reflected frequency
# response and the level transfer -- and 2,037 of the 2,039 archived capture
# logs predate the change.
#
# SO EVERY OTHER VARIABLE IS PINNED TO WHAT THE ARCHIVE USED, deliberately and
# to the letter.  These are `preemph_fit_ab.sh`'s "off" arm settings, which is
# what produced `pfit-off-*` and `ts0-off-*` -- 24 calls, same far end, same
# binary, same flags, same HOLD.  A comparison whose arms differ in the
# binary, the IODELAY or the hold time is not a measurement of the impedance.
#
#     SLMODEMD              build/hybrid-fit/slmodemd-fit   (the same file:
#                           it has not been rebuilt since 16 Aug)
#     DSPLIB_V34_FIT_PREEMP 0     the object's own estimator, not #144's
#     DSPLIB_V34_DUMP_PROBE_BINS 1  without this there is no line probe to read
#     SLMODEMD_IODELAY      240
#     TTY                   courier (ext 1902)
#     HOLD                  45
#     TTY_EXTRA/PTY_EXTRA   as the archive arm
#
# WAITQUIET BEFORE EVERY CALL, not just the first.  Finding 1951: a call taken
# at load 4.28 returned CONNECT 4800 where a quiet machine returns 31200, and
# sixteen calls of an earlier A/B were discarded for exactly this.  The load is
# recorded before AND after each call so a run that was disturbed mid-call can
# be found afterwards rather than silently averaged in.
#
# THE DESTINATION IS 1902 AND IT IS NOT A PARAMETER.  row.sh holds the
# allow-list; this script does not get to widen it.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/.claude/worktrees/v34split/testbench
FIT=/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit
OUT=$BENCH/captures
N=${1:-10}
PREFIX=${PREFIX:-cx2-off}
CSV=$OUT/$PREFIX.csv

[ -x "$FIT" ] || { echo "no $FIT -- the archive arm's binary is missing" >&2; exit 3; }
mkdir -p "$OUT"
COURIER=$(. "$BENCH/modems.sh"; modem_resolve courier) || exit 3

echo "call,load_before,load_after,connect,our_rx" > "$CSV"
for i in $(seq 1 "$N"); do
	L=$OUT/$PREFIX-$i
	LB=$(bash "$BENCH/waitquiet.sh" 8 30 1800) || {
		echo "STOPPING: the machine would not go quiet before call $i" >&2
		exit 3; }

	DSPLIB_V34_FIT_PREEMP=0 DSPLIB_V34_DUMP_PROBE_BINS=1 \
	SLMODEMD=$FIT SLMODEMD_IODELAY=240 TTY=courier HOLD=45 \
	TTY_EXTRA="AT&F;AT&A3;AT&B1" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		timeout 220 bash "$BENCH/row.sh" "$L" pty 1902 > "$L.run.log" 2>&1
	LA=$(cut -d' ' -f1 /proc/loadavg)

	# The far end's own view of its own direction.  ATI11 gives the rates,
	# symbol rate, pre-emphasis and levels; ATI6 gives Retrains Granted,
	# which is the only external witness to finding 1917's count.
	timeout 60 python3 "$BENCH/lastlink.py" "$COURIER" --label courier \
		--only ATI11,ATI6 > "$L.lastlink.log" 2>&1

	CR=$(grep -m1 -oE 'pty +CONNECT [0-9]+' "$L.run.log" 2>/dev/null | grep -oE '[0-9]+')
	printf '%s,%s,%s,%s,%s\n' "$PREFIX-$i" "$LB" "$LA" \
		"$([ -n "${CR:-}" ] && echo 1 || echo 0)" "${CR:-}" >> "$CSV"
	printf '  %-12s CONNECT %-7s load %s -> %s\n' \
		"$PREFIX-$i" "${CR:-none}" "$LB" "$LA"
	sleep 6
done
echo "CX2 BATCH DONE -- $CSV"
