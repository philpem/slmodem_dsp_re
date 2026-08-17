#!/usr/bin/env python3
"""
ladder.py -- the impairment ladder, two peers, one channel model.

    ladder.py slip           the jitter-buffer INSERTION ladder  (primary)
    ladder.py loss           the packet-SUBSTITUTION ladder      (#181)
    ladder.py slip --n 4     a short shakedown

WHY TWO IMPAIRMENTS AND WHY SLIP IS FIRST.  Finding 6701, as corrected, splits
what d-modem's jitter buffer does into two mechanisms that look identical in
`stream.c` and are not:

  * `PJMEDIA_JB_MISSING_FRAME` consumes a sender frame and emits zeros in its
    place -- SUBSTITUTION, alignment preserved.  `CHAN_LOSS` models this.
  * `PJMEDIA_JB_ZERO_EMPTY_FRAME` consumes NOTHING and emits zeros anyway
    (`jbuf.c:276` returns PJ_FALSE on an empty framelist without removing a
    head) -- INSERTION.  Everything after it is a frame late, for good.
    `CHAN_SLIP` models this.

1941 measured both on the real bench at roughly one each per four seconds, so
~0.25 insertions/s with zero network loss.  hsfuser measures a SINGLE-SAMPLE
insertion at 0.08/s as enough to stop V.34 connecting; pjmedia's insertion is a
whole frame, 80 or 160 samples.  That makes the slip ladder the one that could
actually explain the bench, so it runs first.

WHY TWO PEERS.  6901 established that our datapump and the Conexant HSF train
33600 at each other on a clean modelled channel, so the PEER is not a candidate
explanation for the bench's collapse.  What that leaves untested is the
IMPAIRED case: if our rate falls where HSF's holds on the identical channel and
seed, the difference is our receiver, and that is the first quantified defect
signature this investigation would have.  If both fall together, the impairment
is simply hard and our receiver is not unusual.

METHOD, and the two rules it exists to obey:

  * REPORT THE MEDIAN NEGOTIATED RATE, not connect/no-connect.  hsfuser's own
    channel-results.md records why: the modem keeps connecting well past the
    point where the rate has collapsed, so a binary outcome saturates and hides
    the whole effect.  Connect fraction is reported too, as a second column,
    never as the headline.
  * A DISTINCT SEED PER RUN.  chanshim.py's seed defaulted to a constant once,
    and an A/B of eight calls per arm came back with all eight rates identical
    -- n=1 reported as n=8.  Seeds here are deterministic from (cell, index) so
    a run is reproducible, and never shared between reps.

Emulator only.  No PBX, no hardware, no dialling: chanshim.py ignores the dial
string, so no destination guard is involved and none can be.  Does not need a
quiet machine -- both ends are self-paced over blocking sockets -- but it does
take cores, so concurrency defaults to half of them.
"""

import argparse
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from statistics import median

BENCH = os.path.dirname(os.path.abspath(__file__))

LADDERS = {
    # events/second.  Brackets 1941's measured ~0.25/s on the real bench.
    'slip': ('CHAN_SLIP', [0.0, 0.1, 0.25, 0.5, 1.0]),
    # fraction.  Matches 1937's points and hsfuser's channel-results sec 4.
    'loss': ('CHAN_LOSS', [0.0, 0.005, 0.01, 0.02, 0.04]),
}

PEERS = {
    'ours': 'chancall.sh',   # our datapump both ends
    'hsf': 'hsfcall.sh',     # ours against the Conexant HSF
}

RE_RATE = re.compile(r'CONNECT\s+(\d+)')
RE_RTN = re.compile(r'retrains\s+(\d+)|V34RTNCOUNT\s+(\d+)')


def run_one(peer, var, value, idx, secs, port):
    """One emulated call. Returns (rate|None, retrains|None, ok)."""
    label = f'ladder-{peer}-{var.lower()}-{value}-{idx}'
    env = dict(os.environ)
    env[var] = str(value)
    env['CHAN_SEED'] = str(1000 + abs(hash((peer, var, value, idx))) % 100000)
    env['CHAN_PORT'] = str(port)
    env.setdefault('CHAN_DELAY_MS', '70')     # the bench's measured one way
    try:
        p = subprocess.run([os.path.join(BENCH, PEERS[peer]), label, str(secs)],
                           env=env, capture_output=True, text=True,
                           timeout=secs + 180)
        out = p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        return None, None, False
    rates = [int(m) for m in RE_RATE.findall(out)]
    rtn = RE_RTN.search(out)
    retr = next((int(g) for g in (rtn.groups() if rtn else []) if g), None)
    return (min(rates) if rates else None), retr, True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ladder', choices=sorted(LADDERS))
    ap.add_argument('--n', type=int, default=10)
    ap.add_argument('--secs', type=int, default=60)
    ap.add_argument('--peers', default='ours,hsf')
    ap.add_argument('--jobs', type=int,
                    default=max(1, (os.cpu_count() or 4) // 2))
    a = ap.parse_args()

    var, points = LADDERS[a.ladder]
    peers = [p for p in a.peers.split(',') if p]
    for p in peers:
        exe = os.path.join(BENCH, PEERS[p])
        if not os.access(exe, os.X_OK):
            sys.exit(f'ladder: {exe} is not executable')

    cells = [(p, v) for p in peers for v in points]
    total = len(cells) * a.n
    print(f'ladder: {var}, {len(points)} points, peers {"+".join(peers)}, '
          f'n={a.n} -> {total} calls, {a.jobs} at a time, {a.secs}s each')
    print(f'  rough wall clock: {total * (a.secs + 25) / a.jobs / 60:.0f} min\n')
    sys.stdout.flush()

    jobs, port = [], 46000
    for p, v in cells:
        for i in range(a.n):
            port += 1
            jobs.append((p, v, i, port))

    t0 = time.time()
    results = {}
    done = [0]
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(run_one, p, var, v, i, a.secs, pt): (p, v)
                for p, v, i, pt in jobs}
        for f in futs:
            pass
        for f, key in list(futs.items()):
            rate, retr, ok = f.result()
            results.setdefault(key, []).append((rate, retr, ok))
            done[0] += 1
            if done[0] % 10 == 0:
                print(f'  {done[0]}/{total} '
                      f'({(time.time()-t0)/60:.0f} min)', flush=True)

    print(f'\n{var} ladder, {a.secs}s calls, CHAN_DELAY_MS='
          f'{os.environ.get("CHAN_DELAY_MS", "70")}, '
          f'completed in {(time.time()-t0)/60:.0f} min\n')
    print(f'  {"peer":<6} {var.lower():<10} {"n":>3} {"connected":>10} '
          f'{"median":>8} {"p25":>7} {"p75":>7} {"retrains":>9}')
    for p in peers:
        for v in points:
            rows = results.get((p, v), [])
            ran = [r for r in rows if r[2]]
            rates = sorted(r[0] for r in ran if r[0] is not None)
            retrs = [r[1] for r in ran if r[1] is not None]
            if not ran:
                print(f'  {p:<6} {v:<10} {0:>3}  ALL RUNS FAILED TO EXECUTE')
                continue
            q = lambda f: (rates[min(int(len(rates)*f), len(rates)-1)]
                           if rates else 0)
            print(f'  {p:<6} {v:<10} {len(ran):>3} {len(rates):>4}/{len(ran):<5} '
                  f'{median(rates) if rates else 0:>8.0f} {q(.25):>7} '
                  f'{q(.75):>7} '
                  f'{(sum(retrs)/len(retrs) if retrs else float("nan")):>9.1f}')

    executed = sum(1 for rows in results.values() for r in rows if r[2])
    print(f'\n  denominator: {executed} of {total} calls executed; '
          f'{total - executed} timed out or failed to launch.')
    if executed == 0:
        sys.exit('REFUSING A CLEAN REPORT ON ZERO EXECUTED CALLS.')
    print('  `connected` counts calls that reported a CONNECT rate; the median '
          'is over\n  those only, so a cell with few connects has a median '
          'from a survivor set.\n  Read the two columns together -- that is '
          'the whole point of not using a binary.')


if __name__ == '__main__':
    main()
