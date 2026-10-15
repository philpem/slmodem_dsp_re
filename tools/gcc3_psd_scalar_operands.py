#!/usr/bin/env python3
"""Bounded scalar-mode/operand ownership controls after normalization recovery."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_search_psd_boundaries import variants as boundaries


def variants(path, source):
    source = boundaries(path, source)['axis-1-1']
    a, z, fn = d.function(source, 'Psd::process')
    cells = {'baseline': source}
    for scalar, window in ((1, 0), (0, 1), (1, 1)):
        candidate = fn
        if scalar:
            assert candidate.count('(long double)out[i]') == 3
            candidate = candidate.replace('(long double)out[i]', 'out[i]')
            assert candidate.count('(long double)frames') == 2
            candidate = candidate.replace('(long double)frames', 'frames')
        if window:
            old = 'm_fft[1 + j] = in[pos + j] * m_window[j];'
            assert candidate.count(old) == 1
            candidate = candidate.replace(old, 'm_fft[1 + j] = m_window[j] * in[pos + j];')
        cells['axis-%d-%d' % (scalar, window)] = source[:a] + candidate + source[z:]
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd13949ad'
    d.OUT_NAME = 'gcc3-psd-scalar-operands'
    d.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
