#!/usr/bin/env python3
"""Sixteen B103 receive controls for observed result/index/owner/AGC boundaries."""
from itertools import product
import playbook_small_patterns as driver
from recovery_sre_boundary import REV


def variants(path, source):
    start, end, body = driver.function(source, 'DemodDataB103')
    cells = {}
    for result, index, owner, agc in product((0, 1), repeat=4):
        label = 'baseline' if not any((result, index, owner, agc)) else \
            'u%d_i%d_o%d_a%d' % (result, index, owner, agc)
        text = body
        if result:
            old = 'nbits = FPM_FSD_demodulate('
            assert text.count(old) == 1
            text = text.replace(old, 'nbits = (unsigned short)FPM_FSD_demodulate(')
        if index:
            assert text.count('int i;') == 1
            text = text.replace('int i;', 'short i;')
        if owner:
            assert text.count('struct b103_dsp *dsp = fp->dsp;') == 1
            text = text.replace('\tstruct b103_dsp *dsp = fp->dsp;\n', '')
            text = text.replace('dsp->', 'fp->dsp->')
        if agc:
            assert text.count('count, original_count);') == 1
            text = text.replace('count, original_count);', 'count, 1);')
            assert text.count('short original_count = (short)count;') == 1
            text = text.replace('\tshort original_count = (short)count;\n', '')
        cells[label] = source[:start]+text+source[end:]
    assert len(cells) == len(set(cells.values())) == 16
    return cells


if __name__ == '__main__':
    driver.REV = REV
    driver.OUT_NAME = 'recovery-b103-return-boundaries'
    driver.SOURCE_PATHS = ('src/pump/b103/B103prc.c',)
    driver.variants = variants
    driver.DUMP_FLAGS = ()
    driver.main()
