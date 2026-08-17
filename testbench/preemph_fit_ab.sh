#!/bin/bash
#
# preemph_fit_ab.sh -- the fitted tilt estimator against the object's counter.
#
# Plan: captures/preemph-fit-ANALYSIS-PLAN.md, written before any call.
# PRIMARY is the steady-state received tilt (1908), not the rate: its spread at
# fixed index is 0.07 dB across five calls, so it resolves at n=12 what the
# rate cannot resolve at n=100.
#
# ONE BINARY, TWO ENVIRONMENTS.  Runs 1 and 2 used two separately-compiled
# binaries per experiment, leaving the compiler uncontrolled between arms
# (1901 could not rule it out).  Here the arms differ only in an environment
# variable read by tools/benchflags.c.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
FIT=/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit
N=${1:-12}
CSV=$BENCH/captures/${PREFIX:-preemph-fit}.csv
COURIER=$(. "$BENCH/modems.sh"; modem_resolve courier) || exit 3

echo "call,arm,load_before,load_after,connect,our_rx,index" > "$CSV"
for i in $(seq 1 "$N"); do
for arm in off fit; do
	L=$BENCH/captures/${PREFIX:-pfit}-$arm-$i
	LB=$(bash "$BENCH/waitquiet.sh" 8 30 1800) || { echo "never quiet" >&2; exit 3; }
	[ "$arm" = fit ] && F=1 || F=0

	DSPLIB_V34_FIT_PREEMP=$F DSPLIB_V34_DUMP_PROBE_BINS=1 \
	SLMODEMD=$FIT SLMODEMD_IODELAY=240 TTY=courier HOLD=45 \
	TTY_EXTRA="AT&F;AT&A3;AT&B1" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
		timeout 220 bash "$BENCH/row.sh" "$L" pty 1902 > "$L.run.log" 2>&1
	LA=$(cut -d' ' -f1 /proc/loadavg)

	# ATI11 on EVERY call: the pre-registered secondary is the far end's own
	# view, because our CONNECT string is emitted once and never revised
	# (1900).  1905's four calls lacked this and cannot be quoted.
	timeout 60 python3 "$BENCH/lastlink.py" "$COURIER" --label courier \
		--only ATI11,ATI6 > "$L.lastlink.log" 2>&1

	CR=$(grep -m1 -oE 'pty +CONNECT [0-9]+' "$L.run.log" 2>/dev/null | grep -oE '[0-9]+')
	IX=$(grep -m1 -oE 'FIT tilt [0-9.]+ dB over [0-9]+ bins -> index [0-9]+|index is [0-9]+' \
		"$L.slmodemd.log" 2>/dev/null | grep -oE '[0-9]+$')
	printf '%s,%s,%s,%s,%s,%s,%s\n' "${PREFIX:-pfit}-$arm-$i" "$arm" "$LB" "$LA" \
		"$([ -n "${CR:-}" ] && echo 1 || echo 0)" "${CR:-}" "${IX:-}" >> "$CSV"
	printf '  %-12s idx %-3s CONNECT %-7s load %s -> %s\n' \
		"${PREFIX:-pfit}-$arm-$i" "${IX:-?}" "${CR:-none}" "$LB" "$LA"
	sleep 6
done
done
echo "PFIT DONE -- $CSV"
