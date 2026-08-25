#!/bin/bash
#
# preemph_ab2.sh -- the pre-emphasis A/B, run 2.
#
#   preemph_ab2.sh [pairs]        # default 12 pairs = 24 calls
#
# THE PLAN IS captures/preemph-ab2-ANALYSIS-PLAN.md AND IT WAS COMMITTED
# BEFORE THE FIRST CALL.  Read it before reading any result; in particular it
# records that the "fix" arm is NOT a corrected version -- finding F1477 -- and
# that what is under test is whether one step less pre-emphasis suits a
# codec-limited path, not whether a bug fix helps.
#
# THREE THINGS RUN 1 GOT WRONG AND THIS FIXES:
#
#   * TWELVE per arm.  Eight cleared the floor of six only in one panel.
#   * ATI11 ON EVERY CALL, read from the far end after the drop and before
#     anything resets it.  It reports pre-emphasis, both directions' speed and
#     level, nonlinear encoding and round-trip delay -- the manipulated
#     variable measured by the other end's own instrument, for one AT command.
#   * LOAD RECORDED AS A RISE, not an absolute.  The after-reading includes
#     the call's own cost (~+0.8 median), so run 1's "< 6 after" rule threw
#     out 8 of 16 calls the machine never actually disturbed.
#
# The gate before each call is unchanged: waitquiet.sh, load under half the
# cores held 30 s.  A loaded box degrades BOTH arms, and interleaving cannot
# rescue that because the degradation is larger than the effect.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
B=/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-vpcm
PAIRS=${1:-12}
CSV=$BENCH/captures/preemph-ab2.csv

for arm in bug fix; do
	[ -x "$B/slmodemd-$arm" ] || { echo "missing $B/slmodemd-$arm" >&2; exit 2; }
done
cmp -s "$B/slmodemd-bug" "$B/slmodemd-fix" && {
	echo "the two arms are the SAME BINARY -- nothing to measure" >&2; exit 2; }

COURIER=$(. "$BENCH/modems.sh"; modem_resolve courier) || exit 3
echo "call,arm,load_before,load_after,load_rise,connect,our_tx,our_rx" > "$CSV"

for i in $(seq 1 "$PAIRS"); do
	for arm in bug fix; do
		L=$BENCH/captures/pab3-$arm-$i
		BEFORE=$(bash "$BENCH/waitquiet.sh") || {
			echo "ABORTING at $arm-$i: machine never went quiet" >&2; exit 3; }

		SLMODEMD=$B/slmodemd-$arm SLMODEMD_IODELAY=240 TTY=courier \
		TTY_EXTRA="AT&F;AT&A3;AT&B1" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
			timeout 200 bash "$BENCH/row.sh" "$L" pty 1902 \
			> "$L.run.log" 2>&1

		AFTER=$(cut -d' ' -f1 /proc/loadavg)

		# The far end's own account of the link, BEFORE anything resets it.
		# call.py tears down with `+++` and `ATH`, which leaves ATI11 intact;
		# an ATZ anywhere loses it.
		timeout 60 python3 "$BENCH/lastlink.py" "$COURIER" --label courier \
			--only ATI11,ATI6 > "$L.lastlink.log" 2>&1

		RX=$(grep -aoE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 | grep -oE '[0-9]+$')
		TX=$(grep -aoE 'TxRate: *[0-9]+' "$L.run.log" | head -1 | grep -oE '[0-9]+$')
		RISE=$(awk -v a="$BEFORE" -v b="$AFTER" 'BEGIN{printf "%.2f", b-a}')
		printf '%s,%s,%s,%s,%s,%s,%s,%s\n' "pab3-$arm-$i" "$arm" "$BEFORE" \
			"$AFTER" "$RISE" "$([ -n "$RX" ] && echo 1 || echo 0)" \
			"${TX:-}" "${RX:-}" >> "$CSV"
		printf '  %-3s %-2s  load %5s -> %-5s (%+5s)  rx=%-6s  preemph=%s\n' \
			"$arm" "$i" "$BEFORE" "$AFTER" "$RISE" "${RX:-none}" \
			"$(grep -aoE 'Preemphasis \(-dB\) +[0-9]+/[0-9]+' "$L.lastlink.log" |
			   grep -oE '[0-9]+/[0-9]+' || echo '?')"
		sleep 6
	done
done
echo
echo "PREEMPH_AB2 done -- python3 $BENCH/abcompare.py $BENCH/captures/pab3-*.run.log"
