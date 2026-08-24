#!/bin/bash
# #110 on the BENCH: the near echo canceller against a real hybrid.
#
# The emulator could not answer this -- its channel model has no echo path at
# all (its own README says so), so the near canceller had nothing to mis-adapt
# to and the arms differed by equerr 20 vs 18.  1212's premise is the canceller
# converging on 80-198 RMS against the VG204's real reflection at 161-172 ms.
# Only the bench has that.
#
# ONE BINARY, TWO ENV SETTINGS, ARMS INTERLEAVED.  Interleaved because a batch
# drifts -- the far end warms, the line changes -- and an off-then-on ordering
# would confound that drift with the treatment.
#
# QUIET GATE IS TIGHTER THAN waitquiet's DEFAULT.  That default is CORES*2/3 =
# 8 here, and finding 1951 measured a call DEGRADED at 4.28.  The default would
# not have protected this batch, and a subagent is compiling on this machine
# while it runs.  Threshold 4, and the load is recorded per call so any
# suspect row can be identified afterwards rather than silently averaged in.
set -u
W=/home/philpem/dev/sip-D-modem/claude_re/.claude/worktrees/digiterm
BENCH=/home/philpem/dev/sip-D-modem/claude_re/testbench
SL=$W/build/hybrid-dt/slmodemd-fit
FAR=${FAR:-courier}; EXT=${EXT:-1902}
N=${N:-8}

for i in $(seq 1 "$N"); do
  for arm in off on; do
    [ "$arm" = on ] && dt=1 || dt=0
    # LABEL FROM THE FAR END, so a second batch cannot overwrite the first.
    # It was hardcoded `dtb-`, and relaunching for the Oli'\''Net reused the
    # Courier'\''s labels -- the loop `rm -f`s each label before dialling, so the
    # only thing between eight good pairs and nothing was noticing in time.
    lab="dt20-${FAR}-$arm-$i"
    WAITQUIET_THRESH=4 timeout 900 "$BENCH/waitquiet.sh" 4 30 900 >/dev/null 2>&1 \
      || { echo "$i $arm SKIPPED machine-never-quiet"; continue; }
    load=$(cut -d' ' -f1 /proc/loadavg)
    rm -f "$BENCH/captures/$lab".*
    SLMODEMD=$SL DSPLIB_V34_DIGITAL_TERM=$dt SLMODEMD_IODELAY=240 \
      TTY=$FAR HOLD=45 timeout 260 "$BENCH/row.sh" "$BENCH/captures/$lab" pty "$EXT" \
      >/dev/null 2>&1
    l=$BENCH/captures/$lab.slmodemd.log
    c=$BENCH/captures/$lab.call.log
    # OUR rate is the `pty <<` CONNECT.  `tty <<` is the far end's report and
    # it is NOT the same number and NOT even the same QUANTITY between modems:
    #
    #    25.184 tty << CONNECT 28800/ARQ     the Courier's own line rate
    #    25.274 pty << CONNECT 12000         ours -- the #132 deficit, visible
    #    32.452 tty << CONNECT 115200        the Supra's DTE speed (AT&B1)
    #
    # So the far-end column means "line rate" on one modem and "serial port
    # speed" on another, and 496 of 524 archived call logs put that line FIRST.
    # Any tool taking the first CONNECT reports the wrong thing on 95% of the
    # archive -- silently, because every value is a plausible modem rate.
    rate=$(grep -aoE 'pty << CONNECT[ /]*[0-9]+' "$c" 2>/dev/null | head -1 | grep -oE '[0-9]+$')
    farrate=$(grep -aoE 'tty << CONNECT[ /]*[0-9]+' "$c" 2>/dev/null | head -1 | grep -oE '[0-9]+$')
    # ANCHOR ON THE FIELD NAME.  `grep -oE '[0-9]+' | head -1` took the LOG
    # TIMESTAMP -- every line is `<658.607176> V34DATARATE, equerr = 776,...`
    # so the first number on it is the clock, not the measurement.  It read
    # 658 where the answer is 776, and every value it produced was a plausible
    # equaliser error, which is why four rows of it looked like data.
    eq=$(grep -a 'V34DATARATE, equerr' "$l" 2>/dev/null | tail -1 \
         | grep -oE 'equerr = [0-9]+' | grep -oE '[0-9]+')
    hs=$(grep -ac 'V34DATARATE, automatic' "$l" 2>/dev/null)
    rt=$(grep -ac 'V34TIMING,Retrain started' "$l" 2>/dev/null)
    printf '%s %s ours=%s far=%s equerr=%s hs=%s retrain=%s load=%s\n' \
      "$i" "$arm" "${rate:-none}" "${farrate:-none}" "${eq:-na}" "${hs:-0}" \
      "${rt:-0}" "$load"
  done
done
