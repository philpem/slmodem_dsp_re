#!/usr/bin/env python3
"""Cross original unsigned decision dispatch with observed bit zero extension."""
import playbook_small_patterns as d
import v8_decision_owner_reproduce as prior
import v8_bit_extension_reproduce as bit


def variants(path, source):
    control = prior.variants(path, source)['wrapped-captured-switch']
    assert control.count('int decision =') == 1
    unsigned = control.replace('int decision =', 'unsigned int decision =')
    return {'baseline': source, 'signed-switch-control': control,
            'unsigned-switch': unsigned, 'unsigned-switch-zero-bit': bit.bits(unsigned)}


if __name__ == '__main__':
    d.REV = '38626248'
    d.SOURCE_PATHS = ('src/v8/V8Dpsk.c',)
    d.OUT_NAME = 'v8-unsigned-decision'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
