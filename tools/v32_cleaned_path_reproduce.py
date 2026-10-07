#!/usr/bin/env python3
"""Test the original cleaned-sample getter's accepted-count fallthrough."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'V32FP_GetCleanedSamples')
    before = ('if (have > V32FP_CLEAN_MAX)\n\t\t*n = 0;\n'
              '\telse\n\t\t*n = (short)have;')
    after = ('if (have <= V32FP_CLEAN_MAX)\n\t\t*n = (short)have;\n'
             '\telse\n\t\t*n = 0;')
    assert fn.count(before) == 1
    return {'baseline': source,
            'accepted-count-first': source[:start]+fn.replace(before, after)+source[end:]}

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'a95a6c65'
    d.OUT_NAME = 'v32-cleaned-path'
    d.SOURCE_PATHS = ('src/pump/v32/V32.c',)
    d.variants = variants
    d.main()
