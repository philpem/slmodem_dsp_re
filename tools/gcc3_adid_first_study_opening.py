#!/usr/bin/env python3
"""Cross the first-study opening literal mode and upper-clamp join."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_first_study import variants as prior, NAME


def variants(path, source):
    parent = prior(path, source)['explicit-double-late-0']
    a, z, fn = d.function(parent, NAME)
    lower = '\tt = trn1Sigma * 2.5f;'
    upper = '\tsqrDiffThresh = ((float)neighborUcodeMaxDistance >= t)\n\t\t? t : (float)neighborUcodeMaxDistance;'
    assert fn.count(lower) == fn.count(upper) == 1
    cells = {'baseline': source}
    for mode, guard in itertools.product((False, True), repeat=2):
        text = fn
        if mode:
            text = text.replace(lower, '\tt = trn1Sigma * 2.5;')
        if guard:
            text = text.replace(upper, '\tsqrDiffThresh = t;\n\tif (!((float)neighborUcodeMaxDistance >= sqrDiffThresh))\n\t\tsqrDiffThresh = (float)neighborUcodeMaxDistance;')
        cells['threshold-double-%d-upper-guard-%d' % (mode, guard)] = parent[:a]+text+parent[z:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '9aa118e1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-opening'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
