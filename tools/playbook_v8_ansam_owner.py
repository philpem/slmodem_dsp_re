#!/usr/bin/env python3
"""Test the observed typed tone owner retained across ANSam scaling."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'v8_ansaminit')
    changed = fn.replace('\n{\n', '\n{\n\tstruct v8_tone *t = &v->tone;\n\n', 1)
    for field in ('carrier_phase', 'envelope_step', 'carrier_step',
                  'reversal_count', 'amplitude', 'reversal_enable'):
        assert changed.count('v->tone.' + field) == 1
        changed = changed.replace('v->tone.' + field, 't->' + field)
    return {'baseline': source, 'tone-owner':
            source[:start] + changed + source[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain') + 1].startswith(
        'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV = '5000b4aa'
    driver.OUT_NAME = 'playbook-v8-ansam-owner'
    driver.SOURCE_PATHS = ('src/v8/V8.c',)
    driver.variants = variants
    driver.main()
