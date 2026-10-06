#!/usr/bin/env python3
"""Discriminate conditional-value cap and literal overflow zero exit."""
import playbook_small_patterns as d
import fse_getdiag_selection_reproduce as prior


def variants(path, source):
    control = prior.variants(path, source)['positive-overflow-arm']
    start, end, body = d.function(control, 'FSE_getdiag')
    split = body.index('\tcase 1:')
    arm, rest = body[:split], body[split:]
    cap = '\t\t\tif (n > max)\n\t\t\t\tn = max;'
    overflow = '\t\t} else {\n\t\t\tstate->diag_n = 0;\n\t\t}'
    assert arm.count(cap) == arm.count(overflow) == 1
    cells = {'baseline': source}
    for conditional in (False, True):
        for literal_exit in (False, True):
            value = arm
            if conditional:
                value = value.replace(cap, '\t\t\tn = (n > max) ? max : n;')
            if literal_exit:
                value = value.replace(overflow, overflow[:-3] + '\t\t\treturn 0;\n\t\t}')
            label = 'conditional-%d-literal-exit-%d' % (conditional, literal_exit)
            cells[label] = control[:start] + value + rest + control[end:]
    return cells


if __name__ == '__main__':
    d.REV = '6b4509bd'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'fse-getdiag-cap-exit'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
