#!/bin/bash
#
# chancall.sh -- one emulated call: two datapumps, one channel, no hardware.
#
#   chancall.sh LABEL [seconds]
#   CHAN_DELAY_MS=70 CHAN_LOSS=0.001 chancall.sh noisy 90
#
# Spawns two slmodemd instances, each with the shared C channel shim in d-modem's place,
# joined over loopback with the bench path's measured characteristics applied.
# One answers, one originates; the pty side of each is driven by replaydte.py.
#
# NO PBX, NO ATA, NO SERIAL MODEM.  Nothing here can dial: the channel shim
# ignores the dial string entirely, exactly as `replay.py` does, so the four
# destination guards are not involved and cannot be involved.
#
# IT DOES NOT NEED A QUIET MACHINE.  Both ends are self-paced over blocking
# sockets, so the run cannot be perturbed by load -- unlike a bench call,
# where slmodemd works to a clock (waitquiet.sh).  That is most of the point:
# emulated calls can be run in bulk, in parallel, while the world compiles.
#
# WHAT IT CANNOT TELL YOU: Smart Link against Smart Link is not Smart Link
# against a Rockwell or a USR.  If the retrain thrashing of finding 1922 is an
# interop behaviour, this may reproduce none of it -- which localises the
# cause to the real path and is worth knowing.  Never quote an emulated rate
# as a bench rate.
#
set -u
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
# Sourced from THIS script's directory, not from $BENCH.  $BENCH is a
# hardcoded absolute path into the main tree, so a copy of this script
# running in a worktree would otherwise pull the MAIN tree's helpers --
# a different branch's idea of what these functions do.
. "$(dirname "$(readlink -f "$0")")/modems.sh"
SL=${SLMODEMD:-/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit}
VBT_ROOT=${VBT_ROOT:-/home/philpem/dev/sip-D-modem/claude_re/third_party/slopmodem-pstn-model}
SHIM=
L=${1:?usage: chancall.sh LABEL [seconds]}
SECS=${2:-60}
PORT=${CHAN_PORT:-$((45000 + RANDOM % 500))}
OUT=$BENCH/captures/$L

[ -x "$SL" ] || { echo "chancall: $SL not executable" >&2; exit 2; }
VBT_PROFILE=${VBT_PROFILE:-${CHAN_LINE_MODEL:-vg204-1907}}
export VBT_PROFILE
make -s -C "$VBT_ROOT" vbt-chanshim || exit 2
SHIM=$VBT_ROOT/build/vbt-chanshim

cleanup() {
	for p in ${PIDS:-}; do kill -TERM -"$p" 2>/dev/null; done
	sleep 0.3
	for p in ${PIDS:-}; do kill -KILL -"$p" 2>/dev/null; done
}
trap cleanup EXIT INT TERM
PIDS=""
START_PTY=""

start_side() {	# $1 = role (server|client), $2 = log suffix
	local pidf="$OUT.$2.pgid"
	rm -f "$pidf"
	# CHAN_SEED must reach BOTH sides and be the SAME on each: the two shims
	# model one channel, so different seeds would give the two directions
	# independent noise, which no real line does.
	#
	# THE PROBE DUMP IS DECIDED FROM THE BINARY, NOT FROM THE BRANCH.
	# `probe_flag_for` (modems.sh) emits DSPLIB_V34_DUMP_PROBE_BINS only if
	# this slmodemd actually contains V34PROBEBINS, and nothing otherwise.
	#
	# Both halves of that are the fix for a real regression.  The assignment
	# was once unconditional, which meant a binary without the flag got an
	# override that silently did nothing -- an A/B looking like two arms when
	# it ran one twice.  It was then deleted outright as "master does not read
	# this", which is true of master's SOURCE and false of the deployed
	# hybrid, built before the split and still carrying the dump: ten fresh
	# calls reduced to zero probes and bandshape.py had nothing to read.
	# Asking the binary is the only reading that is right in both cases.
	env $(probe_flag_for "$SL") \
	CHAN_ROLE=$1 CHAN_PORT=$PORT CHAN_SEED=${CHAN_SEED:-12345} \
	CHAN_LOSS=${CHAN_LOSS:-0} CHAN_BURST=${CHAN_BURST:-1} \
	CHAN_SLIP=${CHAN_SLIP:-0} CHAN_SLIP_MAX_MS=${CHAN_SLIP_MAX_MS:-500} \
	CHAN_TILT=${CHAN_TILT:-0} \
		setsid sh -c 'echo $$ > "$1"; exec "$2" -d9 -e "$3" > "$4" 2>&1' \
		_ "$pidf" "$SL" "$SHIM" "$OUT.$2.log" &
	sleep 2
	#
	# Return through a global rather than command substitution.  A shell waits
	# for asynchronous jobs when a command-substitution subshell exits, so
	# `PTY_A=$(start_side ...)` killed the just-launched modem before it could
	# create a PTY.  That made every self-call fail at setup while hsfcall.sh,
	# which launches directly from its parent shell, continued to work.
	#
	# PIDS is still reconstructed from the pid files below: it needs both
	# process groups, and the files are the authoritative values after setsid.
	START_PTY=$(grep -a -oE '/dev/pts/[0-9]+' "$OUT.$2.log" | head -1)
}

echo "chancall: $L, ${SECS}s, port $PORT, binary $(basename "$SL")"
start_side server answer
PTY_A=$START_PTY
start_side client origin
PTY_B=$START_PTY
PIDS="$(cat "$OUT.answer.pgid" 2>/dev/null) $(cat "$OUT.origin.pgid" 2>/dev/null)"
[ -n "$PTY_A" ] && [ -n "$PTY_B" ] || { echo "chancall: a side gave no pty" >&2; exit 1; }
echo "  answer pty $PTY_A   origin pty $PTY_B"

#
# THE ANSWERING SIDE TAKES `ATA`, NOT `ATS0=1`.  Auto-answer is gated on
# `sip_ringing` in modem_main.c, and `sip_ringing` is set by exactly one thing:
# an `SR` message arriving on the SIP socket from the `-e` child.  That child
# is the channel shim, which models a wire and knows nothing about call setup, so no
# ring is ever reported and `ATS0=1` waits for a bell that cannot ring.  `ATA`
# reaches modem_answer() directly and needs no ring; socket_dial() accepts it
# on the `strncasecmp(m->at_cmd,"ATA",3)` arm.
#
# ORDER AND `sleep 1` ARE COSMETIC.  Nothing flows until BOTH sides are
# started: each shim answers a frame from its own slmodemd with a frame from
# its peer, and an unstarted slmodemd transmits nothing, so the started side
# blocks after exactly one frame.  (That is precisely what the old logs show --
# one `call: process` line on the originator and then silence for the rest of
# the run.)  Both audio clocks therefore begin at the same instant, whatever
# the wall-clock ordering here.  Do not add sleeps to fix a sequencing problem;
# they cannot reach the datapumps.
#
DTE_ECHO=1 DTE_CMDS='ATZ;AT+MS=34,1;ATA' timeout $((SECS + 40)) \
	python3 "$BENCH/replaydte.py" "$PTY_A" "$SECS" >"$OUT.answer.dte" 2>&1 &
sleep 1
DTE_ECHO=1 DTE_CMDS='ATZ;AT+MS=34,1;ATDT1903' timeout $((SECS + 40)) python3 "$BENCH/replaydte.py" "$PTY_B" "$SECS" \
	>"$OUT.origin.dte" 2>&1 &
wait %2 2>/dev/null || true

#
# THE RATE IS ON THE PTY, NOT IN THE LOG.  slmodemd logs `modem report result:
# 1 (CONNECT)` and drops the speed; only the DTE sees `CONNECT 33600`, which is
# why replaydte.py now echoes the pty to its `.dte` file.  Fall back to the log
# so a run still reports "yes it connected" if the .dte is empty.
#
rate() {
	grep -a -oE 'CONNECT[ /]*[0-9]+' "$1.dte" 2>/dev/null | head -1 |
		grep -oE '[0-9]+' && return
	grep -aq 'result: 1 (CONNECT)' "$1.log" 2>/dev/null && echo yes || echo none
}
printf '  %-10s CONNECT %-7s (answer %-7s) V34 %-3s probes %-3s retrains %s\n' "$L" \
  "$(rate "$OUT.origin")" "$(rate "$OUT.answer")" \
  "$(grep -ac 'V34DATARATE' "$OUT.origin.log" 2>/dev/null)" \
  "$(grep -ac 'V34PROBEBINS' "$OUT.origin.log" 2>/dev/null)" \
  "$(grep -ac 'V34RTNCOUNT' "$OUT.origin.log" 2>/dev/null)"
echo "  logs: $OUT.{answer,origin}.log   dte: $OUT.{answer,origin}.dte"
