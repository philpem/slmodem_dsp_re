#!/usr/bin/env python3
"""Cross threshold and diagnostic multiplication modes at distinct consumers."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_first_study import variants as prior, NAME


def variants(path, source):
    parent = prior(path, source)['explicit-double-late-0']
    a, z, fn = d.function(parent, NAME)
    threshold = '\tt = trn1Sigma * 2.5f;'
    diagnostic = '(short)(trn1Sigma * 2.5f + 0.5)'
    assert fn.count(threshold) == fn.count(diagnostic) == 1
    cells = {'baseline': source}
    for first, second in itertools.product((False, True), repeat=2):
        text = fn
        if first:
            text = text.replace(threshold, '\tt = trn1Sigma * 2.5;')
        if second:
            text = text.replace(diagnostic, '(short)(trn1Sigma * 2.5 + 0.5)')
        cells['threshold-double-%d-diagnostic-double-%d' % (first, second)] = parent[:a]+text+parent[z:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '9aa118e1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-literals'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
