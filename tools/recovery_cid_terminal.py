#!/usr/bin/env python3
"""Replay the four bounded CID history-wrap ownership cells on complete TUs."""
import itertools
import sys
from pathlib import Path

import playbook_small_patterns as driver
import gcc3_batch50_service_cid_fsd_reproduce as archived


def variants(path, source):
    # Fixed duplicated-rate-arm/pointer seed. Only wrap ownership is crossed.
    seed = archived.variants(path, source)['arms-1-cursor-1']
    # A postdecrement from element zero forms a forbidden before-array C
    # pointer. Use one-past/predecrement throughout all four cells instead.
    # First-loop terminal cursor is now element zero, hence +5/+17 on wrap.
    for field, index, extent in [('ac_hist', 'idx', 5), ('lpf_hist', 'lidx', 17)]:
        seed = seed.replace('h = cid->%s + %s;' % (field, index),
                            'h = cid->%s + %s + 1;' % (field, index))
        seed = seed.replace('h = cid->%s + %d;' % (field, extent - 1),
                            'h = cid->%s + %d;' % (field, extent))
    seed = seed.replace('*h-- * *coef++', '*--h * *coef++')
    assert '*h--' not in seed
    cells = {'baseline': source}
    for correlation, lowpass in itertools.product([False, True], repeat=2):
        candidate = seed
        for terminal, field, extent in [(correlation, 'ac_hist', 5),
                                         (lowpass, 'lpf_hist', 17)]:
            if terminal:
                old = 'h = cid->%s + %d;' % (field, extent)
                assert candidate.count(old) == (2 if field == 'ac_hist' else 1)
                candidate = candidate.replace(old, 'h += %d;' % extent)
        cells['correlation-%d-lowpass-%d' % (correlation, lowpass)] = candidate
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    driver.REV = '426814ea'
    driver.OUT_NAME = 'recovery-cid-terminal'
    driver.SOURCE_PATHS = ('src/service/Cidfsd.c',)
    driver.variants = variants
    driver.main()
