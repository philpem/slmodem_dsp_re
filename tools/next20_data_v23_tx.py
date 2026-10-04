#!/usr/bin/env python3
"""V23 zero-only sample loop, mute cursor and sample-local assignment."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'v23FP_tx_progress')
    assert fn.count('int bit = 0;') == fn.count('while (count > 0)') == 1
    mute = '\t\tfor (i = 0; i < count; i++)\n\t\t\tout[i] = 0;'
    assert fn.count(mute) == 1
    cells = {'baseline': source}
    for flags in range(1, 8):
        x = fn
        labels = []
        if flags & 1:
            x = x.replace('while (count > 0)', 'while (count != 0)')
            labels.append('zero-only-count')
        if flags & 2:
            x = x.replace(mute, '\t\tfor (i = 0; i < count; i++)\n\t\t\t*out++ = 0;')
            labels.append('mute-walking-out')
        if flags & 4:
            x = x.replace('int bit = 0;', 'int bit;')
            labels.append('bit-assigned-at-sample')
        cells['-'.join(labels)] = source[:a] + x + source[z:]
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v23/v23tx.c',)
    d.OUT_NAME = 'next20-data-v23-tx'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
