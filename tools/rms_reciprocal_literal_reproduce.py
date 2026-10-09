#!/usr/bin/env python3
"""Test the RMS helper's native C reciprocal expression mode."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path, source):
    start, end, fn = d.function(source, 'fComputeRMSValueFloatBuf')
    old = 'return acc * (1.0f / (float)n);'
    assert fn.count(old) == 1
    return {'baseline': source,
            'native-double-literal': source[:start]+fn.replace(old, 'return acc * (1.0 / (float)n);')+source[end:],
            'reciprocal-first': source[:start]+fn.replace(old, 'return (1.0f / (float)n) * acc;')+source[end:]}

if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = 'a95a6c65'
    d.OUT_NAME = 'rms-reciprocal-operands'
    d.SOURCE_PATHS = ('src/service/Beepgen.c',)
    d.variants = variants
    d.main()
