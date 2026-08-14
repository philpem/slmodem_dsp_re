#!/bin/bash
#
# waitquiet.sh -- block until the machine is quiet enough to measure on.
#
#   waitquiet.sh                 # default: load < half the cores, held 30 s
#   waitquiet.sh 6 30 1800       # threshold, hold seconds, give-up seconds
#
# WHY A BENCH CALL NEEDS THIS.  `slmodemd` is a soft modem: it produces and
# consumes 9600 samples a second against a live RTP stream, on schedule.
# Compilers competing for the cores add scheduling jitter, and jitter in a
# sample stream is exactly what stopped a pjsua conference bridge from
# carrying a V.34 handshake at all (finding 1468).  A call taken on a loaded
# box either fails for a reason that has nothing to do with the code, or --
# worse -- connects at a degraded rate and quietly poisons a measurement that
# looks fine.
#
# Sixteen calls of a pre-emphasis A/B were discarded for precisely this:
# taken at load 10-11 while three reconstruction agents compiled, on a machine
# that reached 42.  See records/pab-DISCARDED.txt.
#
# WHY "HELD FOR 30 SECONDS" AND NOT A SINGLE READING.  /proc/loadavg's first
# field is a one-minute exponential average, so it FALLS SLOWLY after a spike
# and RISES SLOWLY into one.  A single sample below the threshold can be the
# tail of a build that has just finished -- fine -- or the leading edge of one
# that is just starting, which is not.  Requiring the reading to stay down
# across a window costs half a minute and removes both cases.  It is a lagging
# indicator either way, which is the conservative direction here.
#
# WHAT IT DOES NOT DO.  Stop a build that starts DURING a call.  Nothing here
# can, short of taking the same lock the builders take -- and a 90-second call
# holding a build lock is its own kind of rude.  Check the load again AFTER a
# run and discard the call if it moved; `row.sh` records nothing about load,
# so the runner has to do it.
#
set -u

CORES=$(nproc)
#
# EIGHT ON A TWELVE-CORE BOX, not six.  Raised from half the cores at Phil's
# request after watching a run spend more time waiting for the gate than
# placing calls: with a browser running, the one-minute average sits around
# 5-6 by itself, so a threshold of 6 was gating on the desktop rather than on
# anything that would disturb a call.  Two thirds still leaves four cores
# clear, which is more than slmodemd needs.
#
# Override per-run with the first argument or WAITQUIET_THRESH; the point of
# the default moving is that the common case should not need either.
#
THRESH=${1:-${WAITQUIET_THRESH:-$(( CORES * 2 / 3 ))}}
HOLD=${2:-30}
GIVEUP=${3:-1800}

start=$(date +%s)
ok=0
printf 'waitquiet: need 1-min load < %s held for %ss (%s cores)\n' "$THRESH" "$HOLD" "$CORES" >&2

while :; do
	load=$(cut -d' ' -f1 /proc/loadavg)
	if awk -v l="$load" -v t="$THRESH" 'BEGIN{exit !(l < t)}'; then
		ok=$(( ok + 2 ))
		if [ "$ok" -ge "$HOLD" ]; then
			printf 'waitquiet: quiet (load %s) after %ss\n' \
				"$load" "$(( $(date +%s) - start ))" >&2
			echo "$load"
			exit 0
		fi
	elif [ "$ok" -gt 0 ]; then
		# Reset, and say so: a run that keeps resetting is a machine that
		# is never going to be quiet, and the operator should know that
		# rather than watch a silent spinner.
		printf 'waitquiet: load %s >= %s, resetting the window\n' "$load" "$THRESH" >&2
		ok=0
	fi
	if [ $(( $(date +%s) - start )) -ge "$GIVEUP" ]; then
		printf 'waitquiet: GAVE UP after %ss, load is still %s\n' "$GIVEUP" "$load" >&2
		exit 1
	fi
	sleep 2
done
