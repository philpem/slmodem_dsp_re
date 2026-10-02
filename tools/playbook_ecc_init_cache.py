#!/usr/bin/env python3
"""Replay nine full-TU ECC initializer local lifetime/width controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_ECC_init')
    cells = {}
    for delay, fill in product((None, 'short', 'int'), repeat=2):
        text = body
        declarations = ''
        assignments = ''
        for field, width, owner in (('near_delay', delay, 'state->near_delay'),
                                     ('fill', fill, 'state->cfg.fill')):
            if width:
                declarations += '\t' + width + ' cached_' + field + ';\n'
                assignments += '\tcached_' + field + ' = ' + owner + ';\n'
                expected = 2 if field == 'near_delay' else 1
                assert text.count(owner) == expected
                text = text.replace(owner, 'cached_' + field)
        text = text.replace('\tint back;\n', '\tint back;\n' + declarations, 1)
        marker = '\t\tstate->cfg = ECC_CFG;\n'
        assert text.count(marker) == 1
        text = text.replace(marker, marker + '\n' + assignments, 1) if assignments else text
        label = 'baseline' if not (delay or fill) else '-'.join(
            name + '-' + width for name, width in (('delay', delay), ('fill', fill)) if width)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 9
    return cells


if __name__ == '__main__':
    driver.REV = '5a54307b'
    driver.OUT_NAME = 'playbook-ecc-init-cache'
    driver.SOURCE_PATHS = ('src/dsp/fpm_ecc.c',)
    driver.variants = variants
    driver.main()
