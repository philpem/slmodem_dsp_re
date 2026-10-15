#!/usr/bin/env python3
"""Test the two original comparison orientations after search-boundary recovery."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_search_psd_boundaries import variants as boundaries


def variants(path, source):
    source = boundaries(path, source)['axis-1-1']
    a,z,fn = d.function(source, 'getConstellationsIndex')
    cells = {'baseline': source}
    for size, loop in ((1, 0), (0, 1), (1, 1)):
        candidate = fn
        if size:
            old = 'if (params->constellationSize[k] != length)'
            assert candidate.count(old) == 1
            candidate = candidate.replace(old, 'if (length != params->constellationSize[k])')
        if loop:
            old = 'for (n = 0; n < length; n++)'
            assert candidate.count(old) == 1
            candidate = candidate.replace(old, 'for (n = 0; length > n; n++)')
        cells['axis-%d-%d' % (size, loop)] = source[:a] + candidate + source[z:]
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd13949ad'
    d.OUT_NAME = 'gcc3-mapping-comparison'
    d.SOURCE_PATHS = ('src/pump/v90/V90MappingParamsInt.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
