#!/usr/bin/env python3
"""Four complete-TU pulse-shaper index lifetime and rail placement controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'V22_PPS_init')
    cells = {}
    for early, rails in product((False, True), repeat=2):
        text = body
        if early:
            assert text.count('\tshort i, j, k;') == text.count('\tk = 0;') == 1
            text = text.replace('\tshort i, j, k;', '\tshort i, j, k = 0;').replace('\tk = 0;\n', '')
        if rails:
            old = '\tshort work_q[V22_PPS_COEFFS];\n\tshort work_i[V22_PPS_COEFFS];'
            new = '\tshort work_i[V22_PPS_COEFFS];\n\tshort work_q[V22_PPS_COEFFS];'
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (early or rails) else '-'.join(n for n, yes in (('early',early),('rails',rails)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '43ef6841'
    driver.OUT_NAME = 'playbook-v22-pps-init'
    driver.SOURCE_PATHS = ('src/pump/v22/v22_pps.c',)
    driver.variants = variants
    driver.main()
