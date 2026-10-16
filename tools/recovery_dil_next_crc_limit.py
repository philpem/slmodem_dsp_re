#!/usr/bin/env python3
"""Cross the original CRC quotient lifetime with the V92 info-region graph."""
from itertools import product
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS
from recovery_dil_next_tail import variants as tail


def variants(path, source):
    cells = {}
    for info, quotient in (product((0, 1), repeat=2) if 'V92DIL' in path else ((0, 0), (0, 1))):
        text = tail(path, source)['tail-1-even-0'] if info else source
        if quotient:
            old = '\tframes = (crcAt - 18) / DIL_FRAME + 1;\n\tfor (j = 0; j < frames; j++)'
            new = '\tframes = (crcAt - 18) / DIL_FRAME;\n\tfor (j = 0; j <= frames; j++)'
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (info or quotient) else 'info-%d-quotient-%d' % (info, quotient)
        cells[label] = text
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-crc-limit'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
