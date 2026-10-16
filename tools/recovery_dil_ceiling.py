#!/usr/bin/env python3
"""Compare the observed nonpositive fallthrough with the matched product graph."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_conversion import PATH, variants as conversion


def variants(path, source):
    base = conversion(path, source)['outer-repeat-double']
    old = ('\treturn (int)((float)(short)n * reciprocal > 0.0f\n'
           '\t\t? (double)(short)n * reciprocal + 0.99999999\n'
           '\t\t: (float)(short)n * reciprocal);')
    assert base.count(old) == 1
    new = ('\treturn (int)((float)(short)n * reciprocal <= 0.0f\n'
           '\t\t? (float)(short)n * reciprocal\n'
           '\t\t: (double)(short)n * reciprocal + 0.99999999);')
    return {'baseline': base, 'nonpositive-first': base.replace(old, new)}


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-ceiling'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
