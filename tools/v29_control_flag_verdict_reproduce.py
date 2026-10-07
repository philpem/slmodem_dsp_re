#!/usr/bin/env python3
"""Hold original flag age while testing conventional SETNE verdict expressions."""
import playbook_small_patterns as d
from fax_control_age_transfer_reproduce import variants as prior


def variants(path, source):
    fixed = prior(path, source)['scale1-flags1']
    start, end, function = d.function(fixed, 'V29TX_control')
    old = '\tint enabled = 0;\n\tif (flags & V29TXCTL_CTL1_BIT4)\n\t\tenabled = 1;\n\tprm->int_0008 = enabled;'
    assert function.count(old) == 1
    cells = {'baseline': source, 'captured-flags-if': fixed}
    for label, expression in (('boolean', '(flags & V29TXCTL_CTL1_BIT4) != 0'),
                              ('conditional', '(flags & V29TXCTL_CTL1_BIT4) ? 1 : 0')):
        body = function.replace(old, '\tprm->int_0008 = '+expression+';')
        cells[label] = fixed[:start]+body+fixed[end:]
    return cells


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/V29t_stc.c',)
    d.OUT_NAME = 'v29-control-flag-verdict'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
