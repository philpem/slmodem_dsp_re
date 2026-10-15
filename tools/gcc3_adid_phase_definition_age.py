#!/usr/bin/env python3
"""Bound initial-definition history after recovering ADID's operation graph."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_phase_grouping_consumer import variants as prior_variants
from gcc3_adid_phase_grouping import NAME


def variants(path, source):
    previous = prior_variants(path,source)['direct-store-double-1']
    a,z,fn = d.function(previous, NAME)
    initial = '\tcount = 1;\n\tbest = 0;'
    zero = '\ttotal = 0;\n\tfsum = 0.0f;\n\tfsq = 0.0f;'
    assert fn.count(initial) == fn.count(zero) == 1
    cells = {'baseline': source}
    for best_first, float_first in itertools.product((False,True), repeat=2):
        text = fn
        if best_first:
            text = text.replace(initial, '\tbest = 0;\n\tcount = 1;')
        if float_first:
            text = text.replace(zero, '\tfsum = 0.0f;\n\tfsq = 0.0f;\n\ttotal = 0;')
        cells['best-first-%d-float-first-%d' % (best_first,float_first)] = previous[:a]+text+previous[z:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '860732a2'
    d.OUT_NAME = 'gcc3-adid-phase-definition-age'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.DUMP_FLAGS = ('-dr',)
    d.variants = variants
    d.main()
