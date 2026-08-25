#!/usr/bin/env python3
"""
jbtiming.py -- is the jitter buffer's insertion rate lower during TRAINING?

THE QUESTION, and it is the one thing standing between 6902 and a conclusion.

6701 (as corrected) established that `PJMEDIA_JB_ZERO_EMPTY_FRAME` consumes no
sender frame and therefore INSERTS one, and 1941 measured about 0.25 of them
per second on the real bench with zero network loss.  6902 then measured what
insertions at that rate do: at 0.25/s our datapump connects 4 times in 10 and
the Conexant HSF 2 in 10, against 10/10 on a clean channel.

**But the bench does not behave like that.** Bench calls CONNECT and then
thrash; the emulator at the bench's own measured rate mostly fails to connect
at all.  So the model is harsher than reality somewhere, and the most likely
place is the assumption that 0.25/s applies during TRAINING.  1941 measured it
over the CONNECTED period, and 1941's own data shows the buffer decaying
4 -> 3 -> 2 -> 1 frames as the call goes on.  A fuller buffer runs dry less
often.  If the training-window rate is much lower than the steady-state rate,
the discrepancy is explained and 6902's severity does not transfer to the
handshake.

WHY THIS NEEDS NO NEW CALL, AND CANNOT HAVE ONE.  The `JBSTAT` instrumentation
was in our modified `d-modem.c`; master reverted to cryan209's tree and it is
gone (`grep -c JBSTAT` is 0).  So the 76 archived logs that carry it are the
entire available evidence, and re-measuring would mean re-modifying a tree we
deliberately restored.

WHY IT NEEDS NO CROSS-LOG CLOCK ALIGNMENT EITHER, which is the trap here.
d-modem's stdout is block-buffered, so its lines cannot be aligned to
slmodemd's by file position (1941 records that), and aligning by wall clock
needs row.sh's BENCHANCHOR.  All of that is avoidable: JBSTAT ticks carry their
OWN wall-clock timestamps and their own deltas, so elapsed-time-within-the-call
is computable from the JBSTAT series alone.  d-modem starts at call setup, so
early elapsed time is setup-plus-training and late elapsed time is data mode.
That is a blunter instrument than a CONNECT-anchored split and it is entirely
self-contained, which is worth more here.

WHAT IS REPORTED.  Events per second in each elapsed-time bucket, summed across
calls, with the seconds observed in that bucket as the denominator -- because a
late bucket has fewer calls still running in it and a rate computed against the
wrong denominator would show a spurious decay.

Usage:  jbtiming.py [capture-dir]
"""

import os
import re
import sys
from collections import defaultdict

RE_TICK = re.compile(
    r'(\d\d):(\d\d):(\d\d)\.(\d+)\s+.*?JBSTAT tick: '
    r'discard=\d+ \(\+(\d+)\) lost=\d+ \(\+(\d+)\) empty=\d+ \(\+(\d+)\) '
    r'size=(\d+).*?avg_delay=(\d+)ms')

# Elapsed-time buckets, seconds from the first tick of that call.
BUCKETS = [(0, 10), (10, 20), (20, 30), (30, 45), (45, 60), (60, 10**9)]


def secs(h, m, s, frac):
    return h * 3600 + m * 60 + s + float('0.' + frac)


def scan(path):
    """Yield (elapsed, d_lost, d_empty, size, avg_delay) for one log."""
    t0 = None
    prev = None
    for raw in open(path, 'rb'):
        m = RE_TICK.search(raw.decode('utf-8', 'replace'))
        if not m:
            continue
        h, mi, s, fr, dd, dl, de, sz, ad = m.groups()
        t = secs(int(h), int(mi), int(s), fr)
        if t0 is None:
            t0 = t
        elif prev is not None and t < prev - 3600:
            t += 86400.0          # midnight wrap, seen in the archive
        prev = t
        yield t - t0, int(dl), int(de), int(sz), int(ad)


def main():
    root = (sys.argv[1] if len(sys.argv) > 1
            else '/home/philpem/dev/sip-D-modem/claude_re/testbench/captures')
    if not os.path.isdir(root):
        sys.exit(f'jbtiming: not a directory: {root}')

    files = scanned = 0
    # bucket -> [seconds observed, lost, empty, sum(size), n_size]
    agg = defaultdict(lambda: [0.0, 0, 0, 0, 0])
    for name in sorted(os.listdir(root)):
        if not name.endswith('.log'):
            continue
        files += 1
        rows = list(scan(os.path.join(root, name)))
        if len(rows) < 5:            # a call too short to say anything about
            continue
        scanned += 1
        for i, (el, dl, de, sz, ad) in enumerate(rows):
            span = (rows[i][0] - rows[i-1][0]) if i else 1.0
            if not (0 < span < 10):  # a gap means the log stalled; skip it
                continue
            for lo, hi in BUCKETS:
                if lo <= el < hi:
                    a = agg[(lo, hi)]
                    a[0] += span; a[1] += dl; a[2] += de
                    a[3] += sz; a[4] += 1
                    break

    print('jitter-buffer events by elapsed time within the call\n')
    print(f'  {root}')
    print(f'  {files} .log files, {scanned} carried a usable JBSTAT series\n')
    if not scanned:
        sys.exit('REFUSING A CLEAN REPORT ON ZERO SERIES. No JBSTAT ticks '
                 'found -- wrong directory, or an archive predating that '
                 'instrumentation (findings F134, F2400, F3100).')

    print(f'  {"elapsed":>12} {"sec obs":>9} {"empty/s":>9} {"lost/s":>8} '
          f'{"mean size":>10}')
    tot_e = tot_l = 0
    for lo, hi in BUCKETS:
        sec, lost, empty, ssum, sn = agg[(lo, hi)]
        if sec <= 0:
            continue
        tot_e += empty; tot_l += lost
        label = f'{lo}-{hi}s' if hi < 10**8 else f'{lo}s+'
        print(f'  {label:>12} {sec:9.0f} {empty/sec:9.3f} {lost/sec:8.3f} '
              f'{(ssum/sn if sn else 0):10.2f}')
    print(f'\n  totals: {tot_e} empty (insertions), {tot_l} lost '
          f'(substitutions), over '
          f'{sum(a[0] for a in agg.values()):.0f} seconds observed.')
    print('\n  `empty` is the INSERTION -- jb_framelist_get returned PJ_FALSE '
          'on an empty\n  list having consumed nothing, so the sender stream '
          'slips a whole frame\n  (6701 as corrected).  `lost` is the '
          'substitution and is harmless by\n  comparison.  Read the first '
          'bucket against the last: that ratio is what\n  decides whether '
          "6902's severity transfers to the handshake.")


if __name__ == '__main__':
    main()
