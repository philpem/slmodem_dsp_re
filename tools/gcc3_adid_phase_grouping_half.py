#!/usr/bin/env python3
"""Apply the independently recovered ADID half-expression mode to grouping."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_phase_grouping import variants as prior_variants, NAME


def variants(path, source):
    prior = prior_variants(path, source)
    cells = {'baseline':source}
    for label in ('baseline','late-0-group-1-pool-1','late-1-group-1-pool-1'):
        text = prior[label]
        a,z,fn = d.function(text, NAME)
        assert fn.count('r = (short)(mean + 0.5f);') == 1
        fn = fn.replace('r = (short)(mean + 0.5f);','r = (short)(mean + 0.5);')
        cells[label+'-double-half'] = text[:a]+fn+text[z:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '860732a2'
    d.OUT_NAME = 'gcc3-adid-phase-grouping-half'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.DUMP_FLAGS = ('-dr',)
    d.variants = variants
    d.main()
