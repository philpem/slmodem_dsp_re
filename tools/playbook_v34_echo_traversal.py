#!/usr/bin/env python3
"""Cross two independently observed counted cursor loops in the echo filter."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V34EchoFilter')
    shift = ('\tif (taps != 1)\n'
             '\t\tfor (k = 0; k < taps - 1; k++)\n'
             '\t\t\thist[k] = hist[k + 1];')
    walk_shift = ('\tif (taps != 1) {\n'
                  '\t\tunsigned remaining = taps - 1;\n'
                  '\t\tshort *walk = hist;\n\n'
                  '\t\tdo {\n'
                  '\t\t\t*walk = walk[1];\n'
                  '\t\t\twalk++;\n'
                  '\t\t} while (--remaining != 0);\n'
                  '\t}')
    dot = ('\tfor (k = 0; k < taps; k++)\n'
           '\t\tacc += e->coeff[k] * hist[k];')
    walk_dot = ('\tif (taps != 0) {\n'
                '\t\tunsigned remaining = taps;\n'
                '\t\tconst short *coeff = e->coeff;\n'
                '\t\tconst short *walk = hist;\n\n'
                '\t\tdo {\n'
                '\t\t\tacc += *coeff++ * *walk++;\n'
                '\t\t} while (--remaining != 0);\n'
                '\t}')
    assert fn.count(shift) == fn.count(dot) == 1
    cells = {}
    for shift_cursor in (False, True):
        for dot_cursor in (False, True):
            body = fn
            if shift_cursor:
                body = body.replace(shift, walk_shift)
            if dot_cursor:
                body = body.replace(dot, walk_dot)
            label = ('baseline' if not (shift_cursor or dot_cursor) else
                     'shift-%s_dot-%s' % (
                         'cursor' if shift_cursor else 'index',
                         'cursor' if dot_cursor else 'index'))
            cells[label] = source[:start] + body + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '20821d7c'
    driver.OUT_NAME = 'playbook-v34-echo-traversal'
    driver.SOURCE_PATHS = ('src/pump/v34/v34filters.c',)
    driver.variants = variants
    driver.main()
