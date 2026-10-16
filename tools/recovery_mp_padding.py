#!/usr/bin/env python3
"""Test the original full-width cached MP padding bound."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_mp_quantizer import variants as quantizer


def variants(path, source):
    base = quantizer(path, source)['encoder-1-decoder-1']
    start, end, fn = d.function(base, 'V90MP::infoToBits')
    assert fn.count('unsigned int i, end;') == 1 and fn.count('unsigned char x, n, len;') == 1
    fn = fn.replace('unsigned int i, end;', 'unsigned int i, end, len;').replace('unsigned char x, n, len;', 'unsigned char x, n;')
    return {'baseline': base, 'full-padding-bound': base[:start] + fn + base[end:]}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-padding'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
