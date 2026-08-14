#!/bin/bash
#
# ab149.sh -- the #149 bench A/B, per records/ab149-PREREG.txt.
#
# 24 calls: 6 per arm per far end, 1901 and 1902, arms INTERLEAVED.  Reads its
# design from the pre-registration and does not deviate from it.
#
# IT WAITS FOR A QUIET MACHINE BEFORE EVERY CALL, not just at the start.
# A call taken at load 4.28 with a game pinning 123% of a core returned CONNECT
# 4800 against 31200 quiet on the same far end (1951).  Load average alone is
# the wrong instrument for that -- one busy process barely moves it -- so this
# gates on BOTH a load ceiling AND no single non-desktop process burning a core.
#
# THE CEILING IS 8, NOT 3.  The first version used 3.00 and stalled the batch
# dead at 18 of 48: this desktop idles at 3.0-3.4 with a browser open, so the
# threshold sat underneath the resting load and could never be satisfied.  A
# gate that cannot pass on an idle machine is worse than no gate, because it
# fails silently -- the batch just stops making calls.  8 on twelve cores
# leaves four cores of headroom, which is what a real-time datapump needs, and
# the busy-process check is what actually catches a game or a compiler.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/testbench
export SLMODEMD=${SLMODEMD:-/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit}
export DMODEM_VARIANT=jb
HOLD=${HOLD:-60}
# REPS per arm per far end, and the label stem.  Parameterised rather than
# forked so the replication runs the SAME harness as the batch it replicates --
# a second copy would drift and the comparison would quietly stop being one.
REPS=${REPS:-6}
TAG=${TAG:-ab149}
GIVEUP=${GIVEUP:-14400}          # 4 h to wait for the bench to come free
# Load ceiling x100.  See the note above on why this is not 300.
MAXLOAD=${MAXLOAD:-800}

# Processes that make a call unmeasurable.  Firefox and Xorg are the ordinary
# desktop and were present for every historical capture, so excluding them
# would mean never running; a game or a compiler is not.
BUSY_RE='MTGA|wine|steam|cc1|cc1plus|ld|make|cargo|rustc|node|ffmpeg|python3'

quiet_now() {
	local l hot
	l=$(awk '{print int($1*100)}' /proc/loadavg)
	[ "$l" -lt "$MAXLOAD" ] || return 1
	hot=$(ps -eo pcpu,comm --no-headers |
	      awk -v re="$BUSY_RE" '$1>50 && $2 ~ re {print $2}' | head -1)
	[ -z "$hot" ] || return 1
	return 0
}

wait_quiet() {
	local waited=0 held=0
	while [ "$waited" -lt "$GIVEUP" ]; do
		if quiet_now; then
			held=$((held+5))
			# Held for 30 s, because loadavg is a one-minute average and
			# falls slowly out of a spike as well as rising slowly into one.
			[ "$held" -ge 30 ] && return 0
		else
			[ "$held" -gt 0 ] && echo "  (machine busy again, resetting)" >&2
			held=0
		fi
		sleep 5; waited=$((waited+5))
	done
	echo "ab149: gave up waiting for a quiet machine after ${GIVEUP}s" >&2
	return 1
}

echo "$TAG: $((REPS*4)) calls, $REPS/arm/far-end, interleaved.  binary $(basename "$SLMODEMD")"
echo "$TAG: waiting for a quiet machine (load<$(($MAXLOAD/100)).00, no busy process)..."

n=0
for dest in 1901 1902; do
	case $dest in 1901) role=supra ;; 1902) role=courier ;; esac
	for i in $(seq 1 "$REPS"); do
		for arm in 0 1; do
			n=$((n+1))
			lbl="$TAG-$dest-a$arm-$i"
			#
			# RESUME.  A call that already produced a .run.txt is
			# done and is not repeated -- so a batch interrupted by
			# a bad gate, a reboot or an operator can be restarted
			# with the same TAG and picks up where it stopped,
			# without re-dialling calls whose data is already on
			# disk.  The interleaving is unaffected: the loop still
			# visits arms in the same order, it just skips the ones
			# it finds.
			#
			if [ -s "$BENCH/captures/$lbl.run.txt" ]; then
				n=$((n))
				echo "[$n/$((REPS*4))] $lbl  ALREADY DONE, skipping"
				continue
			fi
			wait_quiet || exit 1
			echo "[$n/$((REPS*4))] $lbl  (arm=$arm dest=$dest $(date +%H:%M:%S))"
			DSPLIB_V34_RRN_ON_BADBLOCK=$arm TTY=$role HOLD=$HOLD \
				timeout 260 "$BENCH/row.sh" "$BENCH/captures/$lbl" pty "$dest" \
				> "$BENCH/captures/$lbl.run.txt" 2>&1
			grep -aoE 'CONNECT [0-9]+' "$BENCH/captures/$lbl.run.txt" |
				head -2 | tr '\n' ' '; echo
			sleep 5
		done
	done
done
echo "$TAG: DONE $(date -Is)"
