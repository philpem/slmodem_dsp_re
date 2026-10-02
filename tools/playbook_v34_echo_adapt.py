#!/usr/bin/env python3
"""Compare indexed echo adaptation with the object's counted forward cursors."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V34EchoAdapt')
    head = '\tunsigned k;'
    loop = '\tfor (k = 0; k < e->taps; k++) {'
    assert fn.count(head) == fn.count(loop) == 1
    changed = fn.replace(head,
        '\tunsigned remaining = e->taps;\n'
        '\tshort *coeff = e->coeff;\n'
        '\tshort *fraction = e->coeff_frac;\n'
        '\tconst short *hist = e->hist;')
    changed = changed.replace(loop, '\tif (remaining > 0) {\n\t\tdo {')
    changed = changed.replace('(unsigned)e->coeff[k]', '(unsigned)*coeff')
    changed = changed.replace('(unsigned short)e->coeff_frac[k]', '(unsigned short)*fraction')
    changed = changed.replace('e->hist[k] * err', '*hist++ * err')
    changed = changed.replace('e->coeff[k] =', '*coeff++ =')
    changed = changed.replace('e->coeff_frac[k] =', '*fraction++ =')
    tail = '\t}\n}'
    assert changed.endswith(tail)
    changed = changed[:-len(tail)] + '\t\t} while (--remaining != 0);\n\t}\n}'
    return {'baseline': source, 'counted-cursors':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '20821d7c'
    driver.OUT_NAME = 'playbook-v34-echo-adapt'
    driver.SOURCE_PATHS = ('src/pump/v34/v34filters.c',)
    driver.variants = variants
    driver.main()
