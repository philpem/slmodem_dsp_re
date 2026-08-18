#!/bin/bash
#
# hsfcall.sh -- one emulated call: OUR datapump against the Conexant HSF
#               datapump, over one channel model.  No PBX, no hardware, no
#               dialling, no d-modem.
#
#   hsfcall.sh LABEL [seconds]
#   HSF_ROLE=originate CHAN_DELAY_MS=0 hsfcall.sh clean0 60
#
#     slmodemd -e chanshim.py                chanshim.py            hsfuser
#       (CHAN_ROLE=server)     <-loopback->  (CHAN_ROLE=client) <->  modem dmframe
#            OUR datapump                                          CONEXANT HSF
#
# WHY, and it is the whole point.  `chancall.sh` puts our datapump against
# ANOTHER COPY OF ITSELF; finding 1948 recorded what that cannot do -- it does
# not reproduce the retrain thrashing the bench shows, so it cannot say whether
# our receiver is worse than a good receiver.  Every other peer we can test
# against is a hardware modem down the real SIP path: slow, opaque, and
# confounded with the trunk.  hsfuser is the third thing -- an independent,
# fully readable, known-good V.34 that trains 33600 against itself and runs in
# the emulator.  It makes "is our receiver worse than a good receiver on
# IDENTICAL INPUT" a controlled question.
#
# IT IS NOT A BENCH CALL AND NEVER REPORTS ONE.  chanshim.py ignores the dial
# string entirely, exactly as replay.py does, so no destination guard is or can
# be involved.
#
# IT DOES NEED A REASONABLY IDLE MACHINE, and this is the one place it differs
# from chancall.sh.  There, both ends are self-paced over blocking sockets and
# load cannot perturb the run.  Here HSF is the clock: its timers come from the
# wall clock while its audio comes from its sample count, so a machine that
# cannot keep up makes the call run SLOWER than real time and HSF's V.34
# timeouts then expire on a call that never stalled -- a false failure
# indistinguishable from a channel too poor to train (hsfuser's own sweep.py
# records this).  That is why the summary prints the real-time ratio, and why a
# run below about 0.95x must be read as VOID rather than as a no-connect.  Do
# not run the chancall.sh control alongside it: two free-running slmodemds are
# exactly the load that starves HSF.
#
set -u
BENCH="$(cd "$(dirname "$0")" && pwd)"
SL=${SLMODEMD:-/home/philpem/dev/sip-D-modem/claude_re/build/hybrid-fit/slmodemd-fit}
HSF_ROOT=${HSF_ROOT:-/home/philpem/dev/softmodems/conexant/hsfuser}
L=${1:?usage: hsfcall.sh LABEL [seconds]}
SECS=${2:-60}
PORT=${CHAN_PORT:-$((45000 + RANDOM % 500))}
OUT=$BENCH/captures/$L
#
# HSF_ROLE is HSF's role; ours is the other one.  IT IS NOT A FREE CHOICE:
# HSF's originate path arms an unconditional DialtoneWaitTime and reports NO
# DIALTONE about 5 s after dialling unless something presents 350+440 Hz on
# its receive (pair.c's own comment says ATX cannot disable it, and this rig
# measured it).  Our channel model carries slmodemd's silence, so HSF ANSWERS
# and we dial -- which is the bench's arrangement anyway.  `originate` is left
# reachable so the failure can be re-measured, not because it works.
#
ROLE=${HSF_ROLE:-answer}
case $ROLE in
answer)		OUR_CMDS='ATZ;AT+MS=34,1;ATDT1903'; OUR_ROLE=originate ;;
originate)	OUR_CMDS='ATZ;AT+MS=34,1;ATA';      OUR_ROLE=answer ;;
*)		echo "hsfcall: HSF_ROLE must be answer or originate" >&2; exit 2 ;;
esac

[ -x "$SL" ] || { echo "hsfcall: $SL not executable -- build it with" >&2
                  echo "  tools/hybrid.sh && tools/hybrid_link.sh build/hybrid-fit ..." >&2
                  exit 2; }
[ -x "$HSF_ROOT/build/hsfuser" ] || {
	echo "hsfcall: $HSF_ROOT/build/hsfuser not executable (HSF_ROOT=?)" >&2
	exit 2; }
mkdir -p "$BENCH/captures"

cleanup() {
	for p in ${PIDS:-}; do kill -TERM -"$p" 2>/dev/null; done
	sleep 0.3
	for p in ${PIDS:-}; do kill -KILL -"$p" 2>/dev/null; done
}
trap cleanup EXIT INT TERM
PIDS=""

#
# CHAN_SEED must reach BOTH sides and be the SAME on each: the two shims model
# ONE channel, so different seeds would give the two directions independent
# noise, which no real line does.  chancall.sh's convention, kept.  EXPORTED
# rather than prefixed per command, because the far side's shim is started by
# hsfshim.py and a prefix would stop at the launcher.
#
export CHAN_PORT=$PORT
export CHAN_SEED=${CHAN_SEED:-12345}
export CHAN_DELAY_MS=${CHAN_DELAY_MS:-70}
export CHAN_SNR=${CHAN_SNR:-}
export CHAN_LOSS=${CHAN_LOSS:-0}
export CHAN_SLIP=${CHAN_SLIP:-0}
export CHAN_SLIP_MAX_MS=${CHAN_SLIP_MAX_MS:-500}
export CHAN_TILT=${CHAN_TILT:-0}
export HSF_ROOT

echo "hsfcall: $L, ${SECS}s, port $PORT"
echo "  ours: $SL"
echo "        $(ls -l --time-style=+%Y-%m-%d\ %H:%M "$SL" | awk '{print $6, $7}')  role $OUR_ROLE"
echo "  hsf:  $HSF_ROOT/build/hsfuser  role $ROLE"
echo "  chan: delay ${CHAN_DELAY_MS}ms loss ${CHAN_LOSS} slip ${CHAN_SLIP} tilt ${CHAN_TILT} seed ${CHAN_SEED}"

# ---- our side: slmodemd with chanshim.py in d-modem's place ---------------
rm -f "$OUT.sl.pgid" "$OUT.sl.log" "$OUT.hsf.log" "$OUT.sl.dte"
# DSPLIB_V34_DUMP_PROBE_BINS and DSPLIB_V34_FIT_PREEMP were set on the
# assignment list below.  Master's datapump does not read them -- they are V.34
# bench instrumentation and live on the `v34-instrumentation` branch -- and an
# override that silently does nothing is worse than none, because it makes an
# A/B look like it ran two arms when it ran one arm twice.
#
# CONSEQUENCE, and it is not cosmetic: bandshape.py and linesweep.py read the
# V.34 line probe out of the -d9 log, and NOTHING EMITS IT on a master build.
# They still work on archived logs; to gather NEW probe data, build from that
# branch.  (The note sits above the block because a `#` line inside a `\`
# continuation is joined to the line above and comments out the command.)
CHAN_ROLE=server \
	setsid sh -c 'echo $$ > "$1"; exec "$2" -d9 -e "$3" > "$4" 2>&1' \
	_ "$OUT.sl.pgid" "$SL" "$BENCH/chanshim.py" "$OUT.sl.log" &
sleep 2
PTY=$(grep -a -oE '/dev/pts/[0-9]+' "$OUT.sl.log" | head -1)
SL_PID=$(cat "$OUT.sl.pgid" 2>/dev/null)
PIDS="$SL_PID"
[ -n "$PTY" ] || { echo "hsfcall: slmodemd gave no pty -- see $OUT.sl.log" >&2; exit 1; }

# ---- far side: chanshim.py + hsfuser, joined by a socketpair --------------
CHAN_ROLE=client \
	setsid sh -c 'echo $$ > "$1"; exec python3 -u "$2" "$3" > "$4" 2>&1' \
	_ "$OUT.hsf.pgid" "$BENCH/hsfshim.py" "$ROLE" "$OUT.hsf.log" &
sleep 0.5
HSF_PID=$(cat "$OUT.hsf.pgid" 2>/dev/null)
PIDS="$SL_PID $HSF_PID"
echo "  our pty $PTY   sl pgid $SL_PID   hsf pgid $HSF_PID"

#
# WAIT FOR HSF'S MODEM LOOP, DO NOT SLEEP AT IT.  hsfuser initialises nine
# driver modules before it touches the socket, and until it does, our shim is
# blocked on its peer and slmodemd transmits into a socket buffer -- burning
# V.8 timeouts on a far end that does not exist yet.  `modem[<role>] pid ... ->
# /dev/pts/N` is printed at the instant its loop starts, so that is the trigger.
# It is also block 0 of HSF's clock, which the summary needs.
#
for _ in $(seq 1 300); do
	grep -qa "modem\[$ROLE\] pid" "$OUT.hsf.log" 2>/dev/null && break
	sleep 0.1
done
if ! grep -qa "modem\[$ROLE\] pid" "$OUT.hsf.log" 2>/dev/null; then
	echo "hsfcall: hsfuser never reached its modem loop in 30 s -- STAGE (a)," >&2
	echo "         exec/fds, not a handshake result.  See $OUT.hsf.log" >&2
	exit 1
fi
echo "  hsf modem loop up: $(grep -a "modem\[$ROLE\] pid" "$OUT.hsf.log" | head -1)"

# ---- drive our DTE ---------------------------------------------------------
#
# `ATA`, NOT `ATS0=1`, when it is our side that answers.  Auto-answer is gated
# on `sip_ringing` in modem_main.c, which is set by exactly one thing: an `SR`
# message arriving on the SIP socket from the `-e` child.  That child is
# chanshim.py, which models a wire and knows nothing about call setup, so no
# ring is ever reported and `ATS0=1` waits for a bell that cannot ring.
#
# HSF IS UNDER THE SAME CONSTRAINT AND THE OPPOSITE ANSWER IS RIGHT FOR IT.
# `HSF_NOCTL` swaps its `ATS0=1` for a bare `ATA` (src/main.c) and that does
# not work -- measured, it never lifts.  hsfshim.py rings it properly on its
# own control socketpair instead, so HSF keeps `ATS0=1`.  See that file.
#
DTE_ECHO=1 DTE_CMDS="$OUR_CMDS" timeout $((SECS + 40)) \
	python3 "$BENCH/replaydte.py" "$PTY" "$SECS" >"$OUT.sl.dte" 2>&1 &
DTE=$!
wait $DTE 2>/dev/null || true

#
# ORDERED TEARDOWN, and it is what makes the denominators exist.  TERM the
# LAUNCHER (not the group): hsfshim.py stops the shim first, hsfuser then sees
# EOF on the audio fd and prints `exiting after N blocks (frames in X / out Y)`
# -- the only place HSF's own frame counters appear.  Killing the group flat
# loses it, which is the mistake chanshim.py's docstring already records for
# CHANLAT.
#
kill -TERM "$HSF_PID" 2>/dev/null
sleep 2

# ---- report ----------------------------------------------------------------
#
# THE RATE IS ON THE PTY, NOT IN THE LOG.  slmodemd logs `modem report result:
# 1 (CONNECT)` and drops the speed; only the DTE sees `CONNECT 33600`.
#
rate() {
	grep -a -oE 'CONNECT[ /]*[0-9]+' "$1.dte" 2>/dev/null | head -1 |
		grep -oE '[0-9]+' && return
	grep -aq 'result: 1 (CONNECT)' "$1.log" 2>/dev/null && echo yes || echo none
}
eval "$(python3 "$BENCH/hsfshim.py" --summary "$OUT.hsf.log")"
#
# THE SHIM'S OWN TOTAL COMES FROM OUR SIDE'S LOG, and that is not an accident
# of where it was easy to grep.  chanshim.py prints `N frames, X s of audio`
# only when its loop ENDS, and the far shim is stopped with a signal, so its
# copy of that line never exists (its docstring records the same hazard for
# CHANLAT).  The NEAR shim ends differently: the far one closing the loopback
# gives it ECONNRESET, which is an exception it catches, so it prints.  Same
# frames, counted at the other end of the same wire.
#
CHAN_FRAMES=$(grep -a -oE 'chanshim: [0-9]+ frames, [0-9.]+ s' "$OUT.sl.log" |
	tail -1 | awk '{print $2}')
CHAN_AUDIO_S=$(grep -a -oE 'chanshim: [0-9]+ frames, [0-9.]+ s' "$OUT.sl.log" |
	tail -1 | awk '{print $4}')

echo
printf '  %-10s ours CONNECT %-7s   hsf CONNECT %-7s  %s\n' "$L" \
	"$(rate "$OUT.sl")" "${HSF_CONNECT:-none}" "${HSF_MCR:+mod ${HSF_MCR}}"
printf '  %-10s hsf +MRR rx %s / tx %s bit/s  (rx is what HSF received from US;'\
' tx is what OUR receiver was given)\n' "" "${HSF_MRR_RX:-?}" "${HSF_MRR_TX:-?}"
printf '  %-10s train %s s (HSF %s -> CONNECT)   real time %s (%s s of HSF audio in %s s wall)\n' \
	"" "${HSF_TRAIN:-n/a}" "$ROLE" "${HSF_RTRATIO:-n/a}" \
	"${HSF_AUDIO_S:-?}" "${HSF_WALL_S:-?}"
printf '  %-10s counted: HSF frames in %s / out %s over %s blocks; near shim %s frames (%s s audio); rings sent %s, HSF offhook=%s; %s log lines parsed; run %s s\n' \
	"" "${HSF_FRAMES_IN:-?}" "${HSF_FRAMES_OUT:-?}" "${HSF_BLOCKS:-?}" \
	"${CHAN_FRAMES:-?}" "${CHAN_AUDIO_S:-?}" "${HSF_RINGS:-?}" \
	"${HSF_OFFHOOK:-?}" "${SUMMARY_LINES:-0}" "$SECS"
printf '  %-10s our log: V34DATARATE %s  V34PROBEBINS %s  V34RTNCOUNT %s  (0 may mean the binary above lacks the instrumentation, not that it did not happen)\n' \
	"" "$(grep -ac 'V34DATARATE' "$OUT.sl.log" 2>/dev/null)" \
	"$(grep -ac 'V34PROBEBINS' "$OUT.sl.log" 2>/dev/null)" \
	"$(grep -ac 'V34RTNCOUNT' "$OUT.sl.log" 2>/dev/null)"
case "${HSF_RTRATIO:-}" in
"") ;;
*)  awk -v r="${HSF_RTRATIO%x}" 'BEGIN{ if (r+0 < 0.95)
	print "  WARNING: below 0.95x real time -- HSF was starved, so this run is VOID,\n"\
	      "           not a no-connect.  Re-run on an idle machine." }' ;;
esac
echo "  logs: $OUT.sl.log  $OUT.sl.dte  $OUT.hsf.log"
