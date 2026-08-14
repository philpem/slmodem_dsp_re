#!/bin/bash
#
# One row of the hardware interop matrix.
#
#   row.sh <logprefix> pty|tty [dialnumber]
#
# The second argument is who ORIGINATES: `pty` is slmodemd (the blob or our
# reconstruction, over SIP via d-modem), `tty` is the real SupraExpress 56e PRO.
#
# TEARDOWN IS BY PID, NEVER BY PATTERN.  `pkill -f d-modem` matches the path
# /home/philpem/dev/sip-D-modem and will kill unrelated work in this tree -- it
# took out an agent once.  slmodemd runs in its own process group and only that
# group is signalled.
#
# DESTINATIONS ARE RESTRICTED.  The PBX is live and can reach the PSTN.  The
# allow-list is enforced twice: dmodem-guard-fork.sh for anything that arrives
# in argv, and dmodem_dest_allowed() inside d-modem.c for the number that
# arrives over the socket, which is the only one that exists in this
# configuration.
#
set -u
ROOT=${ROOT:-/home/philpem/dev/D-Modem-fork}
#
# WHICH slmodemd, so a HYBRID build can be put on the bench without moving
# anything in the source tree.  Task #89 substitutes our reconstructed
# datapump for the blob's by weakening the blob's symbols and linking ours
# alongside; the result is a binary somewhere else entirely, and every run
# needs to say which one it was.  Defaults to the fork's, so nothing changes
# for an ordinary run.
#
SLMODEMD=${SLMODEMD:-$ROOT/slmodemd/slmodemd}
# HOLD DEFAULTS TO SIXTY SECONDS, and it is not politeness to the modem.
#
# Finding 1902: four of six ninety-second calls ended at roughly DOUBLE the
# rate they reported at CONNECT -- 16800, 19200, 21600, 24000 against 12000 --
# with the far end's ATI11 confirming the final figure every time.  The first
# renegotiation lands anywhere from +23 s to +91 s.  This bench used to tear
# calls down about 17.5 s after CONNECT, so it saw almost none of that, and
# every rate it has ever recorded is a lower bound of unknown tightness
# (which limits findings 1466 and 1476).
#
# `HOLD=0` restores the old behaviour for a test that genuinely wants a short
# call -- but a rate measurement is not one of those.
BENCH=/home/philpem/dev/sip-D-modem/testbench
. "$BENCH/modems.sh"
# `supra` is the SupraExpress 56e PRO on ext 1901, `courier` the USR on 1902.
# Resolved by adapter serial number, NOT by ttyUSBn -- see modems.sh for the
# replug that renumbered both and would have swapped them.  TTY= still takes a
# role name or a raw /dev path for an ad-hoc run.
MODEM=${TTY:-supra}
# Default the artefacts into the repo, not /tmp: the recordings are the record
# of a call that cannot be replayed, and /tmp does not survive a reboot.
CAPTURES=/home/philpem/dev/sip-D-modem/testbench/captures
mkdir -p "$CAPTURES"
LOG=${1:-$CAPTURES/row}
WHO=${2:-pty}
DIAL=${3:-1901}
SLPGID=""
PIDF=""		# set before slmodemd starts; cleanup() runs on early exits too

# THIRD guard, and the only one that covers the hardware modem.  When the
# SupraExpress originates (row 1) the number goes straight from the AT command
# to the PBX: dmodem-guard-fork.sh never sees it and d-modem's allow-list is on
# the wrong side of the call.
#
# The authorised numbers, and NOTHING ELSE.  This PBX can reach the PSTN, so a
# typo in a harness is a real call to a real number.  Each entry is here
# because Phil named it:
#
#     1901  SupraExpress 56e PRO   (Rockwell RCV56DPF)
#     1902  USR Courier            (USR's own DSP)
#     1903  Oli'Net V92 Ready      (Conexant CX06827-11), added 2026-08-13
#     4242  our own SIP registration
#
# This guard caught four calls to 1903 before it was authorised, which is
# exactly what it is for -- do not widen it to a pattern.
case " 1901 1902 1903 4242 " in
	*" $DIAL "*) ;;
	*)
		echo "REFUSING to dial '$DIAL': not 1901, 1902, 1903 or 4242" >&2
		exit 2 ;;
esac

# PRE-FLIGHT, and it comes BEFORE slmodemd starts and before anything dials.
#
# The five-call asymmetry run went out into an empty line -- the whole USB hub
# had dropped -- and printed five rows of `no connect`, indistinguishable in the
# summary from five rows of "V.34 failed".  The old check was an open() inside
# each call, i.e. after the dial had already gone out, and it did not stop the
# run.  This asks the modem whether it is there, once, and refuses to start if
# it is not.  Exit 3, distinct from the allow-list's 2.
echo "=== pre-flight"
TTY=$(modem_require "$MODEM") || {
	echo "REFUSING to dial: no modem behind '$MODEM'." >&2
	echo "  $BENCH/modems.sh list   -- what is attached" >&2
	echo "  $BENCH/modems.sh pin    -- re-pin after a replug" >&2
	exit 3
}

cleanup() {
	echo "--- teardown"
	if [ -n "$SLPGID" ]; then
		kill -TERM -"$SLPGID" 2>/dev/null || true
		sleep 2
		kill -KILL -"$SLPGID" 2>/dev/null || true
		echo "--- signalled process group $SLPGID only"
		# Verify it actually died.  Silence here used to mean "signalled
		# something", not "nothing is left"; a survivor stays registered to
		# the PBX as 4242 and quietly contends with the next run.
		sleep 1
		if kill -0 -"$SLPGID" 2>/dev/null; then
			echo "--- WARNING: process group $SLPGID SURVIVED; kill it before the next run" >&2
		fi
	else
		echo "--- WARNING: no pgid recorded, slmodemd may still be running" >&2
	fi
	rm -f "$PIDF"
	# The blob reopens these every run, so a later call would overwrite the
	# evidence for this one.  Three calls were analysed before this was
	# noticed and the 48-versus-240 comparison was lost.
	# NO .raw COPIES.  A .wav here is the same samples plus a 44-byte header,
	# so keeping both doubled the size of every capture for nothing -- 2003
	# files and 1.9 GB of it, on a disk that later hit 100% full and broke
	# other sessions' builds.  The wav is the useful one: it carries its own
	# sample rate and channel count, so anything can open it without being
	# told 9600-versus-8000 out of band, which the raw always needed.
	#
	# The conversion reads /tmp directly and writes the wav in one step.  The
	# blob reopens /tmp/modem_*.raw every run, so a later call overwrites the
	# evidence for this one -- the wav written here IS the durable copy, and
	# it must be written before the next call starts.
	# Mono per direction, plus a STEREO pair per rate: left = received (the far
	# end), right = transmitted (us).  Both directions are written from the same
	# loop on the same timebase, so the two channels line up sample for sample
	# and the interaction can be read -- or listened to -- as one signal rather
	# than two files someone has to align by eye.
	python3 - "$LOG" <<'PY'
import sys, wave

log = sys.argv[1]


def read(p):
    try:
        return open(p, 'rb').read()
    except OSError:
        return None


def write(path, chans, rate):
    w = wave.open(path, 'wb')
    w.setnchannels(len(chans)); w.setsampwidth(2); w.setframerate(rate)
    if len(chans) == 1:
        w.writeframes(chans[0])
    else:
        n = max(len(c) for c in chans)
        # zero-pad the shorter side rather than truncating the longer one: the
        # tail is usually the far end still transmitting after we have stopped,
        # which is exactly what a failed handshake looks like
        chans = [c + b'\0' * (n - len(c)) for c in chans]
        out = bytearray(n * 2)
        for i, c in enumerate(chans):
            out[i * 2::4] = c[0::2]
            out[i * 2 + 1::4] = c[1::2]
        w.writeframes(bytes(out))
    w.close()


for suffix, rate in (("_8k", 8000), ("", 9600)):
    rx = read("/tmp/modem_rx%s.raw" % suffix)
    tx = read("/tmp/modem_tx%s.raw" % suffix)
    for name, data in (("rx", rx), ("tx", tx)):
        if data:
            write("%s.modem_%s%s.wav" % (log, name, suffix), [data], rate)
    if rx and tx:
        write("%s.stereo%s.wav" % (log, suffix), [rx, tx], rate)
        print("    %s.stereo%s.wav  (L=received, R=transmitted, %d Hz)"
              % (log, suffix, rate))
PY
	echo "--- and per-direction mono beside them"
}
trap cleanup EXIT INT TERM

# Greppable, so a summary script or a later reader can recover which modem
# produced this log without re-deriving it from the pre-flight text.
echo "MODEM: $(modem_provenance "$MODEM" 2>/dev/null || echo "$MODEM")"

echo "=== slmodemd, own process group, guarded exec"
echo "  BINARY: $SLMODEMD"
# The process group is recorded BY the process, not inferred from $!.
# `setsid` may fork and exit immediately, in which case $! is already dead when
# we look up its pgid, the lookup yields an empty string, and teardown then
# signals nothing -- leaving a whole slmodemd + d-modem group alive, still
# registered to the PBX as 4242, to collide with the next run.  That happened.
# Here the shell setsid() puts in charge writes its own pid (which IS the new
# pgid) and then execs, so the value is exact and cannot go stale.
PIDF="$LOG.pgid"
rm -f "$PIDF"
#
# THE CLOCK ANCHOR, written into the log as its first line.
#
# slmodemd stamps `<NNN.NNN>`; pjmedia and d-modem stamp wall clock, and both
# land in this one file.  This line was added believing the two could not be
# converted.  They can: finding 1952 established that slmodemd's stamp is
# **Unix epoch seconds modulo 1000** -- `epoch % 1000` equals a log's first
# `<t>` in 24 of 24 logs checked -- so the clocks were always the same clock.
#
# THE ANCHOR IS STILL WORTH WRITING.  Modulo 1000 wraps every 16 minutes, and
# resolving which window a call sat in otherwise means trusting the file's
# mtime, which a copy, an rsync or an archive run destroys.  One line here
# makes each capture self-describing instead.
#
# Do NOT align these logs by file position: d-modem's stdout is a FILE, so
# glibc gives it 4 KB block buffering and a burst of its lines can land
# anywhere relative to slmodemd's.  That part of the original reasoning holds.
printf 'BENCHANCHOR wallclock=%s epoch=%s slmodemd=%s\n' \
	"$(date -Is)" "$(date +%s.%N)" "$SLMODEMD" > "$LOG.slmodemd.log"
setsid sh -c 'echo $$ > "$1"; exec "$2" -d9 -e "$3" >> "$4" 2>&1' \
	_ "$PIDF" "$SLMODEMD" "$BENCH/dmodem-guard-fork.sh" \
	"$LOG.slmodemd.log" &
sleep 4
SLPGID=$(cat "$PIDF" 2>/dev/null)
PTY=$(grep -a -oE '/dev/pts/[0-9]+' "$LOG.slmodemd.log" | head -1)
echo "  pgid ${SLPGID:-NONE} pty ${PTY:-NONE}"
[ -z "$PTY" ] && { tail -8 "$LOG.slmodemd.log"; exit 1; }

# The SupraExpress powers up with +A8E "0,0" -- both V.8 origination and V.8
# answer negotiation reading as disabled -- and its own +A8E=? does not even
# list 0 as a permitted value.  That looked like the whole explanation for the
# missing CM and JM.  IT IS NOT: setting 1,1 and redialling reproduced the
# failure exactly, same status sequence and same audio to the second.  The
# command is kept because it costs nothing and removes a variable, NOT because
# it was shown to matter.  Note it survives ATZ, AT&F, &F1 and &F2, so it is
# set explicitly per run rather than trusted either way.
TTY_EXTRA=${TTY_EXTRA:-AT+A8E=1,1}

echo "=== call: $WHO originates, dialling $DIAL"
# Propagate call.py's status.  It was discarded, so an ABORT before dialling
# -- the far end not in auto-answer, say -- exited 0 and reached the summary as
# an ordinary "no connect" row, indistinguishable from a negotiation failure.
# The whole point of aborting is that the caller can tell the difference.
python3 "$BENCH/call.py" --tty "$TTY" --pty "$PTY" \
	--originator "$WHO" --dial "$DIAL" --log "$LOG.call.log" \
	--tty-extra "$TTY_EXTRA" --pty-extra "${PTY_EXTRA:-}" \
	--tty-retrain "${TTY_RETRAIN:-0}" --hold "${HOLD:-60}"
CALLRC=$?
[ "$CALLRC" -ne 0 ] && echo "=== call.py exited $CALLRC"

echo "=== slmodemd: V.8 and datapump progress"
grep -a -E 'v8shim|new state: DP_|modem report result' "$LOG.slmodemd.log" \
	| sed 's/engine=[^ ]*//' | tail -12

exit "$CALLRC"
