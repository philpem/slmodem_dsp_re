#!/usr/bin/env python3
"""Cross original MP print-store arms and type-bit cursor publication."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_mp_predicate import variants as predicate


def variants(path, source):
    base = predicate(path, source)['statement-schedule-1']
    start, end, fn = d.function(base, 'V90MP::bitsToInfo')
    cells = {'baseline': base}
    for arms, cursor in itertools.product((0, 1), repeat=2):
        if not (arms or cursor):
            continue
        changed = fn
        if arms:
            for pointer in ('p', 'q'):
                old = '\t\t\t\tfor (mask = 0x2000; mask != 0; mask >>= 1)\n\t\t\t\t\t*'+pointer+"++ = (rateMask & mask) ? '1' : '0';"
                assert changed.count(old) == 1
                new = '\t\t\t\tfor (mask = 0x2000; mask != 0; mask >>= 1) {\n\t\t\t\t\tif (rateMask & mask)\n\t\t\t\t\t\t*'+pointer+" = '1';\n\t\t\t\t\telse\n\t\t\t\t\t\t*"+pointer+" = '0';\n\t\t\t\t\t"+pointer+'++;\n\t\t\t\t}'
                changed = changed.replace(old, new)
        if cursor:
            old = '\t\ttype = (char)bit;\n\t\tbits[bitIndex] = (unsigned char)bit;\n\t\tbitIndex++;'
            new = '\t\ti = bitIndex;\n\t\ttype = (char)bit;\n\t\tbitIndex++;\n\t\tbits[i] = (unsigned char)bit;'
            assert changed.count(old) == 1
            changed = changed.replace(old, new)
        cells['arms-%d-cursor-%d' % (arms, cursor)] = base[:start] + changed + base[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-decoder'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
