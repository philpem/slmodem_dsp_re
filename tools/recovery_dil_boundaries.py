#!/usr/bin/env python3
"""Replay independently witnessed ceiling, read-age and CRC-shift boundaries."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS, variants as topology


def variants(path, source):
    base = topology(path, source)['expand-1-absolute-0']
    cells = {'baseline': base}
    axes = itertools.product((0, 1), repeat=3) if 'V90DIL' in path else ((0, 0, 0), (0, 0, 1))
    for narrow, reread, crc in axes:
        if not (narrow or reread or crc):
            continue
        text = base
        if narrow:
            assert text.count('dilCeiling(int n, float reciprocal)') == 1
            text = text.replace('dilCeiling(int n, float reciprocal)', 'dilCeiling(short n, float reciprocal)')
        if reread:
            old = '\tdilFrames = dilCeiling(desc->dilCount, 0.5f);\n\tfor (i = 0; i < dilFrames; i++) {'
            assert text.count(old) == 1
            text = text.replace(old, '\tfor (i = 0; i < dilCeiling(desc->dilCount, 0.5f); i++) {')
            text = text.replace('DIL_FRAME * dilFrames - 1;', 'DIL_FRAME * dilCeiling(desc->dilCount, 0.5f) - 1;')
            text = text.replace('seq2Bits, dilFrames, segmentAt', 'seq2Bits, segmentAt')
        if crc:
            old = '\tfor (i = 0; i < 15; i++)\n\t\tcrc[i] = crc[i + 1];'
            assert text.count(old) == 1
            text = text.replace(old, '\n'.join('\tcrc[%d] = crc[%d];' % (i, i + 1) for i in range(15)))
            text = text.replace('\tint stage15 = feedback & 1;\n\tint i;', '\tint stage15 = feedback & 1;')
        cells['narrow-%d-reread-%d-crc-%d' % (narrow, reread, crc)] = text
    assert len(cells) == len(set(cells.values())) == (8 if 'V90DIL' in path else 2)
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-boundaries'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
