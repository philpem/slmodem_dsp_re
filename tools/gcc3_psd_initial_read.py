#!/usr/bin/env python3
"""Test the original overlap-before-length initialization boundary."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_psd_scalar_operands import variants as scalar


def variants(path, source):
    source = scalar(path, source)['axis-1-1']
    line = '\tunsigned int bins = m_length >> 1;\n'
    assert source.count(line) == 2
    a,z,fn = d.function(source, 'Psd::process')
    assert fn.count(line) == 1
    division = '\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n'
    assert fn.count(division) == 1
    changed = fn.replace(line, '').replace(division, division + line)
    return {'baseline':source, 'bins-after-division':source[:a]+changed+source[z:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'd13949ad'
    d.OUT_NAME = 'gcc3-psd-initial-read'
    d.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
