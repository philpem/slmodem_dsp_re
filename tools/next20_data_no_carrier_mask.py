#!/usr/bin/env python3
"""Original positive-count SETG/NEG/AND handoff in the no-carrier state."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'TxHdxNoCarrier')
    clear = '\tif (hdx->state_left <= 0)\n\t\tcount = 0;'
    assert fn.count(clear) == fn.count('\tunsigned short count;') == 1
    cells = {'baseline': source}
    for wide, mask in ((1, 0), (0, 1), (1, 1)):
        x = fn
        if wide:
            x = x.replace('\tunsigned short count;', '\tunsigned int count;')
        if mask:
            x = x.replace(clear, '\t{\n\t\tunsigned int active = hdx->state_left > 0;\n\n\t\tcount &= -active;\n\t}')
        label = ('wide-count-' if wide else '') + ('captured-active-mask' if mask else 'conditional-clear')
        cells[label] = source[:a] + x + source[z:]
    return cells

if __name__ == '__main__':
    d.REV = '8af3af53'
    d.SOURCE_PATHS = ('src/pump/v32/V32TXHDX.c',)
    d.OUT_NAME = 'next20-data-no-carrier-mask'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
