#!/usr/bin/env python3
"""Cross the original common sequence sum with the recovered helper producer."""
from itertools import product
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS
from recovery_dil_next_publication import variants as publication


def variants(path, source):
    cells = {}
    for recovered, common_sum in (product((0, 1), repeat=2) if 'V92DIL' in path else ((0, 0), (0, 1))):
        text = publication(path, source)['info-1-owner-1'] if recovered else source
        if common_sum:
            old = 'segmentAt = DIL_SEQ1_AT + seq1Bits + seq2Bits;'
            assert text.count(old) == 1
            at = text.index(old)
            before, after = text[:at], text[at + len(old):]
            text = before + 'segmentAt = seq1Bits + seq2Bits;' + \
                   after.replace('segmentAt', '(DIL_SEQ1_AT + segmentAt)')
        label = 'baseline' if not (recovered or common_sum) else 'recovered-%d-sum-%d' % (recovered, common_sum)
        cells[label] = text
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-base'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
