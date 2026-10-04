#!/usr/bin/env python3
"""Replay both declared full-TU detector initialization domains."""
import sys
import itertools
import argparse
from pathlib import Path
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'detectorinit')
    old = '''\td->coeff = coeff;
\td->polarity = polarity;
\td->armed = 0;
\td->count = (short)-warmup;
\td->limit = limit;
\td->state = V34_DET_STATE_WARMUP;
\td->thresh_hi = thresh_hi;
\td->thresh_lo = thresh_lo;
\td->level = 0;'''
    new = '''\td->polarity = polarity;
\td->count = (short)-warmup;
\td->state = V34_DET_STATE_WARMUP;
\td->armed = 0;
\td->limit = limit;
\td->coeff = coeff;
\td->thresh_lo = thresh_lo;
\td->thresh_hi = thresh_hi;
\td->level = 0;'''
    assert fn.count(old) == 1 and fn.count('\tint section, tap;') == 1
    forms = {'baseline': fn,
             'short-counters': fn.replace('\tint section, tap;', '\tshort section, tap;'),
             'tail-order': fn.replace(old, new),
             'short-tail': fn.replace(old, new).replace('\tint section, tap;', '\tshort section, tap;')}
    if ORDER_CROSS:
        short = fn.replace('\tint section, tap;', '\tshort section, tap;')
        forms = {'baseline': fn, 'short-counters': short}
        lines = dict((line.split('->')[1].split(' =')[0], line) for line in old.splitlines())
        for middle in itertools.permutations(('state', 'armed', 'limit')):
            for tail in [('thresh_hi', 'thresh_lo'), ('thresh_lo', 'thresh_hi')]:
                order = ('coeff', 'polarity', 'count') + middle + tail + ('level',)
                label = '-'.join(middle) + '-' + ('hi-lo' if tail[0]=='thresh_hi' else 'lo-hi')
                forms[label] = short.replace(old, '\n'.join(lines[n] for n in order))
    result = {label: source[:start] + body + source[end:] for label, body in forms.items()}
    assert len(result) == len(set(result.values())) == (14 if ORDER_CROSS else 4)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--order-cross', action='store_true')
    opts, remaining = parser.parse_known_args()
    ORDER_CROSS = opts.order_cross
    sys.argv = sys.argv[:1] + remaining
    assert '--domain' in sys.argv
    declaration = Path(sys.argv[sys.argv.index('--domain') + 1])
    assert declaration.is_file(), 'local pre-compile declaration missing'
    driver.REV = '803138ff'
    driver.OUT_NAME = 'gcc3-v34-detector-boundaries' + ('-order' if ORDER_CROSS else '')
    driver.SOURCE_PATHS = ('src/pump/v34/detector.c',)
    driver.variants = variants
    driver.main()
