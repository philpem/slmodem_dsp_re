#!/bin/bash
#
# preemph_ab.sh -- does making pre-emphasis index 0 reachable change the link?
#
#   preemph_ab.sh [pairs]        # default 8 pairs = 16 calls
#
# THE QUESTION.  `probe_preemp` advances its counter before the comparison
# that leaves the loop, so it returns 6..10 and the author's own
# `if (i == 5) return 0` arm is dead (D53).  In V.34 the RECEIVER chooses the
# far transmitter's pre-emphasis filter and index 0 is flat, so this modem has
# never once asked for a flat line -- 463 index-6 and 40 index-7 decisions at
# 3429 baud across every call this bench has made, and no zeros (finding
# 1471).  A 1998 SupraExpress asks the same Courier for filter 0.
#
# The fix is one branch, behind DSPLIB_REPRODUCE_BUGS, and it is NOT applied by
# default.  This is the experiment that says whether it should be.
#
# THE DESIGN, and both halves matter:
#
#   INTERLEAVED, not blocked.  bug, fix, bug, fix.  Anything that drifts over
#   the run -- machine load, line conditions, the far end warming up -- then
#   hits both arms equally.  A block design would confound the branch with
#   whatever the afternoon did.
#
#   GATED ON LOAD.  slmodemd is real-time DSP; a loaded box degrades BOTH arms
#   and the degradation is larger than the effect being looked for, so
#   interleaving alone does not rescue it.  Sixteen calls were discarded for
#   exactly this (records/pab-DISCARDED.txt).  `waitquiet.sh` blocks before
#   every call until the load has stayed under half the cores for 30 s, and
#   the load is recorded again AFTER the call so one that started quiet and
#   finished loaded can be dropped at analysis time rather than believed.
#
# READ IT WITH abextract.py, NOT WITH GREP.  A call logs `V34EQU, equerr = N`
# about 57 times while training and `V34DATARATE, equerr = N,preerr=M` exactly
# once, when the rate is chosen.  Grepping the first match gets an early
# training sample -- 30431 on the first call of the discarded run -- which
# next to a 28800 connection reads as a contradiction of the object's own
# threshold table rather than as a measurement error.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
B=/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-vpcm
PAIRS=${1:-8}
CSV=$BENCH/captures/preemph-ab.csv

for arm in bug fix; do
	[ -x "$B/slmodemd-$arm" ] || { echo "missing $B/slmodemd-$arm" >&2; exit 2; }
done
cmp -s "$B/slmodemd-bug" "$B/slmodemd-fix" && {
	echo "the two arms are the SAME BINARY -- nothing to measure" >&2; exit 2; }

echo "call,arm,load_before,load_after,connect,our_tx,our_rx" > "$CSV"

for i in $(seq 1 "$PAIRS"); do
	for arm in bug fix; do
		L=$BENCH/captures/pab2-$arm-$i
		BEFORE=$(bash "$BENCH/waitquiet.sh") || {
			echo "ABORTING at $arm-$i: machine never went quiet" >&2; exit 3; }
		SLMODEMD=$B/slmodemd-$arm SLMODEMD_IODELAY=240 TTY=courier \
		TTY_EXTRA="AT&F;AT&A3;AT&B1" PTY_EXTRA="AT+MS=34,1;ATS70=7" \
			timeout 200 bash "$BENCH/row.sh" "$L" pty 1902 \
			> "$L.run.log" 2>&1
		AFTER=$(cut -d' ' -f1 /proc/loadavg)
		RX=$(grep -aoE 'pty +CONNECT [0-9]+' "$L.run.log" | head -1 | grep -oE '[0-9]+$')
		TX=$(grep -aoE 'TxRate: *[0-9]+' "$L.run.log" | head -1 | grep -oE '[0-9]+$')
		printf '%s,%s,%s,%s,%s,%s,%s\n' "pab2-$arm-$i" "$arm" "$BEFORE" "$AFTER" \
			"$([ -n "$RX" ] && echo 1 || echo 0)" "${TX:-}" "${RX:-}" >> "$CSV"
		printf '  %-3s %-2s  load %5s -> %-5s  rx=%s\n' "$arm" "$i" \
			"$BEFORE" "$AFTER" "${RX:-none}"
		sleep 6
	done
done
echo
echo "PREEMPH_AB done -- now: python3 $BENCH/abextract.py $BENCH/captures/pab2-*.run.log"
