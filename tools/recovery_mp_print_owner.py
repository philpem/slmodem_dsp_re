#!/usr/bin/env python3
"""Test complete existing PrintBase2 producer versus specialised print loops."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_mp_decoder import variants as decoder


def variants(path, source):
    base = decoder(path, source)['arms-0-cursor-1']
    cells = {'baseline': base, 'specialised-convert-0': base}
    start, end, fn = d.function(base, 'V90MP::bitsToInfo')
    for pointer in ('p', 'q'):
        declaration = '\t\t\t\tchar *'+pointer+' = str;\n\t\t\t\tunsigned int mask;\n'
        assert fn.count(declaration) == 1
        fn = fn.replace(declaration, '')
        old = '\t\t\t\tfor (mask = 0x2000; mask != 0; mask >>= 1)\n\t\t\t\t\t*'+pointer+"++ = (rateMask & mask) ? '1' : '0';\n\t\t\t\t*"+pointer+' = 0;'
        assert fn.count(old) == 1
        fn = fn.replace(old, '\t\t\t\tPrintBase2(str, (unsigned long)rateMask, 14);')
    candidate = base[:start] + fn + base[end:]
    cells['helper-convert-1'] = candidate
    cells['helper-convert-0'] = candidate
    assert len(cells) == 4 and len(set(cells.values())) == 2
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-print-owner'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-fverbose-asm')
    d.CELL_FLAGS = lambda path, label: ('-fno-if-conversion',) if label.endswith('-0') else ()
    d.variants = variants
    d.main()
