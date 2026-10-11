#!/usr/bin/env python3
"""Bound original word-only rounded-value use without changing float conversion."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_phase_grouping import variants as prior_variants, NAME


def variants(path, source):
    text = prior_variants(path,source)['late-1-group-1-pool-1']
    a,z,fn = d.function(text,NAME)
    cells = {'baseline': source}
    for consumer, double in itertools.product(('unsigned-carrier','direct-store'),(False,True)):
        half = '0.5' if double else '0.5f'
        result = fn
        if consumer == 'unsigned-carrier':
            result = result.replace('\tshort r;', '\tunsigned short r;')
            result = result.replace('r = (short)(mean + 0.5f);','r = (short)(mean + '+half+');')
        else:
            result = result.replace('\tshort r;\n','').replace('\tr = (short)(mean + 0.5f);\n','')
            assert result.count('linMapp[j][at] = r;') == 1
            result = result.replace('linMapp[j][at] = r;', 'linMapp[j][at] = (short)(mean + '+half+');')
        cells[consumer+'-double-%d' % double] = text[:a]+result+text[z:]
    assert len(cells) == len(set(cells.values())) == 5
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '860732a2'
    d.OUT_NAME = 'gcc3-adid-phase-grouping-consumer'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.DUMP_FLAGS = ('-dr',)
    d.variants = variants
    d.main()
