#!/usr/bin/env python3
"""Transfer the witnessed scalar-mode lever to PSD's expansion temporary."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a,z,fn = d.function(source,'Psd::getFrequencies')
    cells = {'baseline':source}
    for scalar,compare in ((1,0),(0,1),(1,1)):
        candidate = fn
        if scalar:
            old = '(long double)i * sampleRate *\n\t\t\t\t  (1.0L / m_length)'
            assert candidate.count(old) == 1
            candidate = candidate.replace(old,'i * sampleRate *\n\t\t\t\t  (1.0f / m_length)')
        if compare:
            old = 'for (i = 0; i < bins; i++)'
            assert candidate.count(old) == 1
            candidate = candidate.replace(old,'for (i = 0; bins > i; i++)')
        cells['axis-%d-%d' % (scalar,compare)] = source[:a]+candidate+source[z:]
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '5cd55f7a'
    d.OUT_NAME = 'gcc3-psd-frequency-scalar'
    d.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    d.DUMP_FLAGS = ('-v','-save-temps','-dr','-dc','-dg')
    d.variants = variants
    d.main()
