#!/usr/bin/env python3
"""Cross witnessed MP signed-bit predicates with late scheduling diagnostics."""
import sys
from pathlib import Path
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, 'V90MP::infoToBits')
    old = '\tfor (i = 0x24; i <= 0x31; i++)\n\t\tbits[i] = (unsigned char)(((rateMask >> (i - 0x24)) & 1) != 0);'
    assert fn.count(old) == 1
    producers = {
        'mask': old,
        'remainder': old.replace('& 1', '% 2'),
        'statement': '\tfor (i = 0x24; i <= 0x31; i++) {\n\t\tif ((rateMask >> (i - 0x24)) & 1)\n\t\t\tbits[i] = 1;\n\t\telse\n\t\t\tbits[i] = 0;\n\t}',
    }
    result = {'baseline': source}
    for name, replacement in producers.items():
        text = source[:start] + fn.replace(old, replacement) + source[end:]
        for scheduling in (1, 0):
            if name == 'mask' and scheduling:
                continue
            result[name + '-schedule-' + str(scheduling)] = text
    assert len(result) == 6 and len(set(result.values())) == 3
    return result


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-predicate'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-fverbose-asm')
    d.CELL_FLAGS = lambda path, label: ('-fno-schedule-insns2',) if label.endswith('-0') else ()
    d.variants = variants
    d.main()
