#!/usr/bin/env python3
"""Cross observed signed-word MRF state updates with its output counter."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_MRF_filter')
    cells = {}
    for state, produced in ((False, False), (False, True), (True, False), (True, True)):
        fn = body
        if produced:
            old = '\tint produced = 0;'
            assert fn.count(old) == 1
            fn = fn.replace(old, '\tshort produced = 0;')
        if state:
            for name in ('branches', 'decimate', 'hlen'):
                old = '\tconst int ' + name + ' ='
                assert fn.count(old) == 1
                fn = fn.replace(old, '\tconst short ' + name + ' =')
            for name in ('phase', 'widx', 'need', 'remaining'):
                old = '\tint ' + name + ' ='
                assert fn.count(old) == 1
                fn = fn.replace(old, '\tshort ' + name + ' =')
        text = source[:start] + fn + source[end:]
        if state:
            old = 'static int\nadvance(int idx, int len)\n{\n\tint next = idx + 1;'
            new = 'static short\nadvance(short idx, short len)\n{\n\tshort next = idx + 1;'
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = ('short-state-produced' if state and produced else
                 'short-state' if state else 'short-produced' if produced else 'baseline')
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = 'cbd16911'
    driver.OUT_NAME = 'playbook-mrf-state-width'
    driver.DUMP_FLAGS = ()
    driver.SOURCE_PATHS = ('src/dsp/fpm_mrf.c',)
    driver.variants = variants
    driver.main()
