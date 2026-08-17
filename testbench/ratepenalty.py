#!/usr/bin/env python3
"""
ratepenalty.py -- did the -2 rate penalty fire, and on which handshakes?

`tx1_ts_rates` (src/pump/v34/v34hstx1.cpp:2688) drops the requested rate by TWO
indices -- 4800 bit/s -- when the handshake lands within 96,000 samples of the
timer mark AND `TX1_F2218 <= 3`.  Line 2699 then assigns that penalised value to
`cfg->rxbits`, which IS the receive rate we ask the far end for.  So this branch
is a candidate mechanism for the whole receive-rate deficit (#132, #187).

NOTHING LOGS THE BRANCH DIRECTLY.  The `V34DATARATE, automatic:` line prints the
rate AFTER the penalty, so it cannot distinguish "the channel supports 19200"
from "the channel supports 28800 and we docked ourselves two".

BUT THE OBJECT LEAVES A WITNESS, and it is exact rather than inferential:

    2687    rec[1] = (short)0xfffd;                     <- unconditional
    2695            rec[1] = rec[1] & ~1u;              <- ONLY inside the branch
    2781    "V34DATARATE, txmp bits 0x%x,0x%x,..."      <- prints rec[1] as word 1

`rec[1]` is written in exactly those two places and read in exactly one (checked
with awk over the whole file, not by eye).  So word 1 of the `txmp bits` line
is a one-bit flag for this branch:

    0xfffd  bit 0 set    -> penalty NOT taken
    0xfffc  bit 0 clear  -> penalty TAKEN

That makes 162 archived logs a complete experiment with no bench time at all.

WHICH HANDSHAKE EACH ONE WAS.  `V34HSINIT` logs the `mode` argument to
`v34handshakinit`, and `datapumpv34` sets `DP_MODE` from that call immediately
afterwards (v34hshak.c:10010/10024/10042/10058):

    HSINIT mode 0  cold start           DP_MODE 0 (memset)   <=3  ARMED
    HSINIT mode 1  our own retrain      DP_MODE 2 or 3       <=3  ARMED
    HSINIT mode 3  far end asked        DP_MODE 4                 not armed
    HSINIT mode 2  either renegotiation DP_MODE 5                 not armed

So the prediction is sharp: the penalty may fire after mode 0 or 1 and must
NEVER fire after mode 2 or 3.  A single counter-example kills the reading of
the mode mapping; agreement makes the branch's reach a measured quantity
instead of an argument.

CAVEAT KEPT IN THE OUTPUT, not buried here: `v34handshakinit` has fourteen call
sites (finding 1928) and only `datapumpv34`'s four set `DP_MODE`.  The `from =`
address in the HSINIT line distinguishes them, so the report groups by it and
prints how many distinct call sites appear.  A mode seen from an unexpected
site is not evidence about DP_MODE.

Usage:  ratepenalty.py [capture-dir]
"""

import os
import re
import sys
from collections import Counter, defaultdict

RE_HSINIT = re.compile(r'V34HSINIT, mode = (-?\d+), from = (0x[0-9a-f]+)')
RE_TXMP = re.compile(r'V34DATARATE, txmp bits '
                     r'0x([0-9a-f]+),0x([0-9a-f]+),')
RE_AUTO = re.compile(r'V34DATARATE, automatic: (\d+), min (\d+), max (\d+)')

ARMED = {0: 'cold start', 1: 'our own retrain'}
NOT_ARMED = {2: 'renegotiation', 3: 'far end asked'}


def scan(path):
    """Yield (mode, from_addr, penalty, rate) per txmp-bits line in one log."""
    mode, addr, rate = None, None, None
    try:
        with open(path, 'rb') as fh:
            for raw in fh:
                line = raw.decode('utf-8', 'replace')
                m = RE_HSINIT.search(line)
                if m:
                    mode, addr = int(m.group(1)), m.group(2)
                    continue
                m = RE_AUTO.search(line)
                if m:
                    rate = int(m.group(1))
                    continue
                m = RE_TXMP.search(line)
                if m:
                    w1 = int(m.group(2), 16)
                    # bit 0 clear == the branch ran
                    yield mode, addr, (w1 & 1) == 0, rate
    except OSError as e:
        print(f'  ! {path}: {e}', file=sys.stderr)


def main():
    root = (sys.argv[1] if len(sys.argv) > 1
            else '/home/philpem/dev/sip-D-modem/claude_re/testbench/captures')
    if not os.path.isdir(root):
        sys.exit(f'ratepenalty: not a directory: {root}')

    files_seen = files_with_txmp = 0
    rows = []
    for name in sorted(os.listdir(root)):
        if not name.endswith('.log'):
            continue
        files_seen += 1
        got = list(scan(os.path.join(root, name)))
        if got:
            files_with_txmp += 1
            rows.extend((name,) + r for r in got)

    print('the -2 rate penalty (v34hstx1.cpp:2688), from the archive\n')
    print(f'  {root}')
    print(f'  {files_seen} .log files scanned, '
          f'{files_with_txmp} carried a `txmp bits` line')
    print(f'  {len(rows)} rate decisions recorded\n')

    if not rows:
        sys.exit('REFUSING TO REPORT A CLEAN RESULT ON ZERO OBSERVATIONS.\n'
                 'No `V34DATARATE, txmp bits` line was found. Either the '
                 'archive predates that instrumentation or the path is wrong; '
                 'a detector with no denominator is indistinguishable from a '
                 'broken one (findings 134, 2400, 3100).')

    # Does the witness actually take both values?  If not, say so loudly --
    # a flag that is constant is not a measurement.
    fired = sum(1 for r in rows if r[3])
    print(f'  penalty TAKEN     {fired:5d}  ({100.0*fired/len(rows):.1f}%)')
    print(f'  penalty not taken {len(rows)-fired:5d}  '
          f'({100.0*(len(rows)-fired)/len(rows):.1f}%)\n')
    if fired == 0 or fired == len(rows):
        print('  ** THE WITNESS IS CONSTANT ACROSS THE WHOLE ARCHIVE. **')
        print('  That is a result, but check it is not a dead detector: the '
              'bit is only\n  meaningful if some run flips it. Treat with the '
              'suspicion findings 2400/2401\n  earned.\n')

    # GROUP BY THE LOW 12 BITS, NOT THE WHOLE ADDRESS.  `from` is
    # __builtin_return_address(0) and the library is loaded at a different
    # base every run, so the raw addresses are per-process noise: a first cut
    # reported 258 "call sites" from 162 files, which is ASLR and not code.
    # ASLR shifts by whole pages, so the page offset is invariant and IS the
    # call site.  (finding 1928: v34handshakinit has fourteen of them.)
    sites = Counter(int(r[2], 16) & 0xfff for r in rows if r[2])
    print(f'  v34handshakinit call sites seen: {len(sites)} '
          f'(by page offset -- the full address is ASLR noise)')
    for off, n in sites.most_common():
        print(f'    ...{off:03x}  {n}')
    print()

    print('  by handshake mode (the DP_MODE the branch tests is set from this):')
    print('    mode                       n   taken   not      predicted')
    by_mode = defaultdict(lambda: [0, 0])
    for _, mode, _, pen, _ in ((r[0], r[1], r[2], r[3], r[4]) for r in rows):
        by_mode[mode][0 if pen else 1] += 1
    for mode in sorted(by_mode, key=lambda m: (m is None, m)):
        taken, not_taken = by_mode[mode]
        if mode in ARMED:
            label, pred = ARMED[mode], 'CAN fire'
        elif mode in NOT_ARMED:
            label, pred = NOT_ARMED[mode], 'must NOT fire'
        else:
            label, pred = ('no HSINIT seen first' if mode is None
                           else 'unexpected mode'), '-'
        print(f'    {mode!s:>4} {label:<22} {taken+not_taken:5d} '
              f'{taken:6d} {not_taken:6d}   {pred}')
    print()

    violations = [r for r in rows if r[1] in NOT_ARMED and r[3]]
    if violations:
        print(f'  ** {len(violations)} COUNTER-EXAMPLES: the penalty fired '
              f'after a mode that\n     should not arm it. The mode->DP_MODE '
              f'mapping is wrong, or another\n     call site sets DP_MODE. '
              f'First five:')
        for name, mode, addr, _, rate in violations[:5]:
            print(f'       {name}  mode={mode} from={addr} rate={rate}')
    else:
        print('  no counter-examples: the penalty never fired after mode 2 '
              'or 3.')
    print()

    def summarise(rates, label):
        if not rates:
            print(f'    {label:<28} (none)')
            return
        rates = sorted(rates)
        print(f'    {label:<28} n={len(rates):<5} median={rates[len(rates)//2]:<6}'
              f' min={rates[0]:<6} max={rates[-1]}')

    print('  rate requested, ALL modes pooled -- CONFOUNDED, see below:')
    for pen, label in ((True, 'penalty taken'), (False, 'penalty not taken')):
        summarise([r[4] for r in rows if r[3] is pen and r[4] is not None],
                  label)
    print()
    print('    Pooling is the wrong comparison and is printed only to be')
    print('    argued with. The penalty can only fire on a self-raised')
    print('    retrain, and a self-raised retrain happens on a call that is')
    print('    already going badly -- so the "taken" group is drawn from')
    print('    worse conditions whether or not the branch costs anything.')
    print()

    print('  THE COMPARISON THAT IS NOT CONFOUNDED -- within mode 1 alone,')
    print('  where every observation is a self-raised retrain and the only')
    print('  difference is whether the timer window was open:')
    for pen, label in ((True, 'mode 1, penalty taken'),
                       (False, 'mode 1, penalty not taken')):
        summarise([r[4] for r in rows
                   if r[1] == 1 and r[3] is pen and r[4] is not None], label)
    print()
    print('  NOTE: `automatic:` prints AFTER the penalty, so the "taken" row')
    print('  is already docked. If the branch costs what it looks like it')
    print('  costs, the two mode-1 rows should differ by about 4800 bit/s.')


if __name__ == '__main__':
    main()
