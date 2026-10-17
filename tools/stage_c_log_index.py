#!/usr/bin/env python3
"""Test the original low-word index carrier after the normalizer cross."""
import playbook_small_patterns as d
from stage_c_log_normalizer import variants as normalization


def variants(path, source):
    control = normalization(path, source)['pointed1-writeback1']
    assert control.count('\tint idx;') == 1
    candidate = control.replace('\tint idx;', '\tunsigned short idx;')
    return {'baseline':source, 'pointed-writeback':control, 'word-index':candidate}


if __name__ == '__main__':
    d.REV = 'f8c8225c'
    d.SOURCE_PATHS = ('src/dsp/fpm_log10.c',)
    d.OUT_NAME = 'stage-c-log-index'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
