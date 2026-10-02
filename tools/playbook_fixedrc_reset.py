#!/usr/bin/env python3
"""Cross FixedRC reset's added guard and external memory-call boundary."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    cells = {'baseline': source}
    for label, guard, wrapper in [('nonnull', True, False),
                                  ('external-clear', False, True),
                                  ('nonnull-external-clear', True, True)]:
        text = source
        start, end, fn = driver.function(text, 'RcFixed_Reset')
        if guard:
            old = '\tif (h == NULL || h->state == NULL)\n\t\treturn;\n'
            assert fn.count(old) == 1
            fn = fn.replace(old, '')
        if wrapper:
            assert fn.count('memset(k1->') == 3
            fn = fn.replace('memset(k1->', 'sysdep_memset(k1->')
        text = text[:start] + fn + text[end:]
        if wrapper:
            old = '\tmemset(s->history, 0, sizeof(s->history));'
            assert text.count(old) == 1
            text = text.replace(old, old.replace('memset(', 'sysdep_memset('))
            text = text.replace('#include "dsplib/fixedrc.h"',
                                '#include "dsplib/fixedrc.h"\n#include "dsplib/sysdep.h"')
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = 'a50cc9cf'
    driver.OUT_NAME = 'playbook-fixedrc-reset'
    driver.SOURCE_PATHS = ('src/core/FixedRC.c',)
    driver.variants = variants
    driver.main()
