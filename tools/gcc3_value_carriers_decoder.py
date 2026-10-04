#!/usr/bin/env python3
"""Retained decoder and an explicitly narrow working-result control."""
import subprocess
import playbook_small_patterns as d

REV = '3dbb1c33'
HEADER = 'include/dsplib/DiffCoder.h'

def variants(path, source):
    return {'baseline': source, 'typed-result': source}

def overlays(path, label):
    if label == 'baseline':
        return {}
    source = subprocess.check_output(['git', 'show', REV + ':' + HEADER], cwd=d.ROOT, text=True)
    start = source.index('void ParallelDifferentialDecoder<T>::process(')
    end = source.index('\n}\n', start) + 2
    fn = source[start:end]
    old = '\t\t*out = x ^ *state;'
    assert fn.count(old) == 1
    fn = fn.replace(old, '\t\tT decoded = x;\n\t\tdecoded ^= *state;\n\t\t*out = decoded;')
    return {'dsplib/DiffCoder.h': source[:start] + fn + source[end:]}

if __name__ == '__main__':
    d.REV = REV
    d.SOURCE_PATHS = ('src/pump/v90/V90SignBitsExtractor.cpp',)
    d.OUT_NAME = 'gcc3-value-carriers-decoder'
    d.DUMP_FLAGS = ('-da',)
    d.variants = variants
    d.HEADER_OVERLAYS = overlays
    d.main()
