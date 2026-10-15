#!/usr/bin/env python3
"""Bound the original ACK detector's ideal-energy and verdict lifetimes."""
import playbook_small_patterns as d


def variants(path, source):
    a, z, fn = d.function(source, 'Detect_Rmloop2_ACK')
    cells = {'baseline': source}
    for square, late in ((1, 0), (0, 1), (1, 1)):
        text = fn
        if square:
            marker = '\tshort sumsq = 0;'
            assert text.count(marker) == 1
            text = text.replace(marker, '\tint idealEnergy = ideal * ideal;\n' + marker)
            old = 'energy + ideal * ideal'
            assert text.count(old) == 1
            text = text.replace(old, 'energy + idealEnergy')
        if late:
            old = '\tunsigned short ms = 0;\n'
            assert text.count(old) == 1
            text = text.replace(old, '')
            marker = '\tif ((short)iabs(2 * corr - sumsq)'
            assert text.count(marker) == 1
            text = text.replace(marker, '\tunsigned short ms = 0;\n\n' + marker)
        cells[f'square-{square}-late-{late}'] = source[:a] + text + source[z:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'df4b23b9'
    d.OUT_NAME = 'residual-v22-ack'
    d.SOURCE_PATHS = ('src/pump/v22/v22prc.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
