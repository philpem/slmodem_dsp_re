#!/usr/bin/env python3
"""Test the observed guarded history cursor/countdown energy loop."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V34EchoEstimateDelayLineEnergy')
    assert fn.count('\tunsigned k;') == 1
    changed = fn.replace('\tunsigned k;',
        '\tconst short *hist = e->hist;\n\tunsigned remaining = e->taps;')
    old = ('\tfor (k = 0; k < e->taps; k++)\n'
           '\t\tacc = (int)((unsigned)acc\n'
           '\t\t\t    + (unsigned)((e->hist[k] * e->hist[k]) >> 5));')
    assert changed.count(old) == 1
    new = ('\tif (remaining > 0) {\n'
           '\t\tdo {\n'
           '\t\t\tshort x = *hist++;\n\n'
           '\t\t\tacc = (int)((unsigned)acc\n'
           '\t\t\t\t    + (unsigned)((x * x) >> 5));\n'
           '\t\t} while (--remaining);\n'
           '\t}')
    changed = changed.replace(old, new)
    return {'baseline': source, 'history-countdown':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'e70c56b9'
    driver.OUT_NAME = 'playbook-v34-echo-energy'
    driver.SOURCE_PATHS = ('src/pump/v34/v34filters.c',)
    driver.variants = variants
    driver.main()
