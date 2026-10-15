#!/usr/bin/env python3
"""Combine independently recovered diagnostic evaluation and clamp branch."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_adid_first_study_opening import variants as prior


def variants(path, source):
    parent = prior(path, source)['threshold-double-0-upper-guard-1']
    old = '(short)(trn1Sigma * 2.5f + 0.5)'
    assert parent.count(old) == 1
    return {'baseline': source,
            'diagnostic-double-upper-guard': parent.replace(old, '(short)(trn1Sigma * 2.5 + 0.5)')}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
    d.REV = '9aa118e1'
    d.SOURCE_PATHS = ('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
    d.OUT_NAME = 'gcc3-adid-first-study-combined'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
