#!/usr/bin/env python3
"""Cross original copied and absolute sequence-padding cursor graphs."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_regions import variants as regions


def variants(path, source):
    base = regions(path, source)['segments-0-headers-1']
    cells = {'baseline': base}
    for first, second in itertools.product((0, 1), repeat=2):
        if not (first or second):
            continue
        text = base
        if first:
            old = '\twhile (n < seq1Bits)\n\t\tbits[DIL_SEQ1_AT + n++] = 0;'
            assert text.count(old) == 1
            text = text.replace(old, '\tfor (i = n; i < seq1Bits; i++)\n\t\tbits[DIL_SEQ1_AT + i] = 0;')
        if second:
            old = '\twhile (n < seq2Bits)\n\t\tbits[DIL_SEQ1_AT + seq1Bits + n++] = 0;'
            assert text.count(old) == 1
            text = text.replace(old, '\tfor (i = DIL_SEQ1_AT + seq1Bits + n;\n\t     i < DIL_SEQ1_AT + seq1Bits + seq2Bits; i++)\n\t\tbits[i] = 0;')
        cells['first-%d-second-%d' % (first, second)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-padding'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
