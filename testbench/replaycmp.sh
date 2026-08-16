#!/bin/bash
#
# replaycmp.sh -- run two datapumps over the SAME recorded audio and diff their
# equaliser trajectories.
#
#   replaycmp.sh captures/base-cx2-3.modem_rx.wav
#   replaycmp.sh captures/base-cx2-3.modem_rx.wav /path/to/other-slmodemd
#
# WHY THIS EXISTS.  See finding 1904: the two blob-versus-ours comparisons this
# project can make from bench captures point in OPPOSITE directions, because
# they come from different batches taken on different days and #129's
# between-batch floor has never been measured.  Feeding both binaries the same
# samples removes the confound entirely rather than averaging over it.
#
# IT DOES NOT NEED A QUIET MACHINE.  Unlike every other script in this
# directory, this one is not real time: `replay.py` writes exactly as many
# samples as slmodemd asks for, over a blocking socketpair, so the run is
# self-paced.  waitquiet.sh is deliberately NOT called here.  Run it during a
# build if you like -- that is the point of it.
#
# WHAT IT CANNOT TELL YOU.  A replayed far end does not react to what we
# transmit, so the run is open-loop and its CONNECT string is fiction.  The
# comparable quantity is the RECEIVER's equerr trajectory, which is driven by
# received audio and is identical input for both binaries.  Read the divergence
# point, not the rate.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
CR=/home/philpem/dev/sip-D-modem/claude_re

WAV=${1:?usage: replaycmp.sh <recording.wav> [ours-slmodemd] [blob-slmodemd]}
OURS=${2:-$CR/build/hybrid-fit/slmodemd-fit}
BLOB=${3:-/home/philpem/dev/sip-D-modem/d-modem/slmodemd/slmodemd}
OUT=${OUT:-$BENCH/captures/replay-$(basename "${WAV%%.*}")}

[ -r "$WAV" ] || { echo "replaycmp: cannot read $WAV" >&2; exit 2; }

run_one() {
	# $1 = label, $2 = binary, rest = extra environment for the datapump
	local label=$1 bin=$2; shift 2
	local log=$OUT.$label.log

	[ -x "$bin" ] || { echo "replaycmp: $bin is not executable" >&2; return 2; }
	#
	# Own process group and a hard timeout, for the reason row.sh documents
	# at length: an slmodemd that outlives its runner keeps a pty and a
	# registration and collides with the next run.  A replay cannot dial,
	# but it can certainly hang -- if the far end's audio runs out while
	# slmodemd is still waiting for a response that will never come, the
	# read blocks forever.  REPLAY_PAD feeds silence to make that a clean
	# ending rather than a hang, and the timeout catches the rest.
	#
	#
	# SOMETHING HAS TO ISSUE AN AT COMMAND.  slmodemd is a modem: it sits in
	# command mode and starts no datapump until a DTE tells it to.  A first
	# version of this script fed the audio and waited 180 s for an equaliser
	# that was never going to run.
	#
	# NO CALL IS PLACED BY THIS, and that is structural rather than a
	# promise.  On the bench it is d-modem, sitting on the far side of the
	# socket, that turns a dial string into SIP.  Here `replay.py` occupies
	# that slot and ignores the dial string completely -- slmodemd hands it
	# over as argv[1] and nothing reads it.  The number below is a token to
	# make the AT command well-formed; it reaches no network.
	#
	env "$@" REPLAY_WAV="$WAV" REPLAY_PAD=15 \
		setsid timeout 180 "$bin" -d9 -e "$BENCH/replay.py" \
		> "$log" 2>&1 &
	local pid=$!

	local pty="" i
	for i in $(seq 1 40); do
		pty=$(grep -a -oE '/dev/pts/[0-9]+' "$log" 2>/dev/null | head -1)
		[ -n "$pty" ] && break
		sleep 0.25
	done
	if [ -z "$pty" ]; then
		echo "  $label: slmodemd never announced a pty" >&2
		kill -TERM -"$pid" 2>/dev/null; wait $pid 2>/dev/null
		return 1
	fi

	python3 "$BENCH/replaydte.py" "$pty" "${REPLAY_HOLD:-120}"

	kill -TERM -"$pid" 2>/dev/null; wait $pid 2>/dev/null
	printf '  %-10s %6s equerr samples, %s rate decisions\n' "$label" \
		"$(grep -ac 'V34EQU' "$log" 2>/dev/null)" \
		"$(grep -ac 'V34DATARATE, equerr' "$log" 2>/dev/null)"
}

echo "replaycmp: $(basename "$WAV")"
run_one blob "$BLOB"
run_one ours "$OURS"

python3 - "$OUT" <<'PY'
import re, sys
base = sys.argv[1]

def traj(p):
    try:
        t = open(p, "rb").read().decode("latin-1")
    except OSError:
        return None
    return [int(v) for v in re.findall(r"V34EQU, equerr = (\d+)", t)]

a, b = traj(base + ".blob.log"), traj(base + ".ours.log")
if not a or not b:
    print("\n  one side produced no equaliser samples -- check the logs; an "
          "open-loop replay can legitimately fail to get that far.")
    raise SystemExit(1)

n = min(len(a), len(b))
same = 0
while same < n and a[same] == b[same]:
    same += 1

print("\n  blob %d blocks, ours %d blocks" % (len(a), len(b)))
if same == n and len(a) == len(b):
    print("  IDENTICAL for all %d blocks -- the receivers agree exactly on "
          "this input." % n)
else:
    print("  identical for the first %d block(s); they diverge at block %d:"
          % (same, same))
    lo, hi = max(0, same - 2), min(n, same + 4)
    print("      idx :  " + "  ".join("%7d" % i for i in range(lo, hi)))
    print("      blob:  " + "  ".join("%7d" % a[i] for i in range(lo, hi)))
    print("      ours:  " + "  ".join("%7d" % b[i] for i in range(lo, hi)))
    print("\n  A divergence is not automatically a defect: the run is "
          "open-loop,\n  so once our TRANSMIT differs the two are no longer "
          "solving the same\n  problem.  Block %d is where to look, not proof "
          "of anything by itself." % same)
PY
echo
echo "  logs: $OUT.{blob,ours}.log"
