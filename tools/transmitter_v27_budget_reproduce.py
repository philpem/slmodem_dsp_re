#!/usr/bin/env python3
"""Transfer the independently observed unsigned budget comparison to V27."""
import playbook_small_patterns as d

NAMES = ('TxHdxSCR1V27', 'TxHdxEQCondV27', 'TxHdxAltV27', 'TxHdxQuietV27')


def variants(path, source):
    text = source
    for name in NAMES:
        a, z, fn = d.function(text, name)
        old = 'countdown <= *budget'
        assert fn.count(old) == 1
        text = text[:a] + fn.replace(old, 'countdown <= (unsigned short)*budget') + text[z:]
    return {'baseline': source, 'unsigned-budget': text}


if __name__ == '__main__':
    d.REV = '1b0a3097'
    d.SOURCE_PATHS = ('src/fax/V27t_prc.c',)
    d.OUT_NAME = 'transmitter-v27-budget'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
