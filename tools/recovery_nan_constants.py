#!/usr/bin/env python3
"""Recover witnessed constant NaN sentinels, removing unmatched library calls.

Revisits prior no-gain controls as call-graph fidelity on the current baseline;
no type palette, profile change or exact-function gain is presumed.
"""
import playbook_small_patterns as d

d.REV = '95422cd2'
d.OUT_NAME = 'recovery-nan-constants'
d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',
                  'src/pump/v90/V90ConstellationDesigner.cpp')
d.DUMP_FLAGS = ()


def variants(path, source):
    expected = 1 if 'AutoDigital' in path else 2
    assert source.count('nanf("")') == expected
    return {'baseline': source, 'original-quiet-nan': source.replace('nanf("")', 'NAN')}


d.variants = variants
if __name__ == '__main__':
    d.main()
