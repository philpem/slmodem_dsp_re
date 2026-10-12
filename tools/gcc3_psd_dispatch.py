#!/usr/bin/env python3
"""Cross the witnessed PSD dispatch idiom with fixed recovered local homes."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'Psd::process')
    old = '\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n\tunsigned int frame, i, j, pos;'
    assert fn.count(old) == 1
    fn = fn.replace(old, '\tunsigned int frame, pos;\n\tunsigned int frames = (count - m_overlap) / (m_length - m_overlap);\n\tunsigned int i, j;')
    baseline = source[:a] + fn + source[z:]
    cases = fn.replace('\tif (option == OUTPUT_DB_PEAK) {',
                       '\tswitch (option) {\n\tcase OUTPUT_DB_PEAK: {')
    cases = cases.replace('\t} else if (option == OUTPUT_LINEAR) {',
                          '\t\tbreak;\n\t}\n\tcase OUTPUT_LINEAR: {')
    cases = cases.replace('\t} else if (option == OUTPUT_DB) {',
                          '\t\tbreak;\n\t}\n\tcase OUTPUT_DB: {')
    assert cases.endswith('\t}\n}')
    cases = cases[:-len('\t}\n}')] + '\t\tbreak;\n\t}\n\tdefault:\n\t\tbreak;\n\t}\n}'
    assert cases.count('case OUTPUT_') == 3
    return {'baseline': baseline, 'switch': source[:a] + cases + source[z:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'b6db3cfc'
    d.OUT_NAME = 'gcc3-psd-dispatch'
    d.SOURCE_PATHS = ('src/dsp/psd.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
