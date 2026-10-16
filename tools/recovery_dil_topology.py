#!/usr/bin/env python3
"""Replay the declared two-packer expansion/position cross on complete TUs."""
import sys
from pathlib import Path
import playbook_small_patterns as d

PATHS = ('src/pump/v90/V90DILdesPCK.cpp',
         'src/pump/v90/V92DILdesPCK.cpp')


def variants(path, source):
    start = source.index('\tfor (j = 0; j < 16; j++) {')
    end = source.index('\n\t}\n', start) + len('\n\t}')
    expanded = []
    for index in range(16):
        offset = 8 * index + index // 2
        member = 'segmentSize[%d]' % index if index < 8 else 'segmentCode[%d]' % (index - 8)
        expanded.append('\tdilPackSeven(bits, segmentAt + %d, desc->%s);' % (offset, member))
        if index & 1:
            expanded.append('\tbits[segmentAt + %d] = 0;' % (offset + 8))
    cells = {'baseline': source}
    for expansion, absolute in ((0, 1), (1, 0), (1, 1)):
        text = source
        if expansion:
            text = text[:start] + '\n'.join(expanded) + text[end:]
        if absolute:
            old = ('\tfor (i = 0; i < 7; i++, value >>= 1)\n'
                   '\t\tbits[at + i] = ')
            assert text.count(old) == 1
            text = text.replace(old, '\tfor (i = at; i < at + 7; i++, value >>= 1)\n\t\tbits[i] = ')
        cells['expand-%d-absolute-%d' % (expansion, absolute)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-topology'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
