#!/usr/bin/env python3
"""Isolate floating helper return from the original integer caller consumers."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_conversion import PATH, variants as conversion


def variants(path, source):
    base = conversion(path, source)['outer-repeat-double']
    old = 'static int\ndilCeiling(unsigned char n, float reciprocal)'
    assert base.count(old) == 1
    text = base.replace(old, 'static double\ndilCeiling(unsigned char n, float reciprocal)')
    text = text.replace('\treturn (int)((float)(short)n * reciprocal > 0.0f', '\treturn ((float)(short)n * reciprocal > 0.0f')
    assert text.count('dilCeiling(desc->') == 4
    text = text.replace('dilCeiling(desc->', '(int)dilCeiling(desc->')
    return {'baseline': base, 'floating-return': text}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-return'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
