#!/usr/bin/env python3
"""Cross direct witnessed segment and header iteration regions."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_dil_topology import PATHS
from recovery_dil_boundaries import variants as boundaries


def variants(path, source):
    v90 = 'V90DIL' in path
    label = 'narrow-1-reread-1-crc-1' if v90 else 'narrow-0-reread-0-crc-1'
    base = boundaries(path, source)[label]
    cast = 'short' if v90 else 'unsigned char'
    cells = {'baseline': base}
    for segments, headers in itertools.product((0, 1), repeat=2):
        if not (segments or headers):
            continue
        text = base
        if segments:
            for index in range(16):
                offset = 8 * index + index // 2
                member = 'segmentSize[%d]' % index if index < 8 else 'segmentCode[%d]' % (index - 8)
                old = '\tdilPackSeven(bits, segmentAt + %d, desc->%s);' % (offset, member)
                assert text.count(old) == 1
                replacement = ('\tn = desc->%s;\n\tfor (i = 0; i <= 6; i++, n >>= 1)\n'
                               '\t\tbits[DIL_SEQ1_AT + seq1Bits + seq2Bits + %d + i] = (%s)(n & 1);\n'
                               '\tbits[DIL_SEQ1_AT + seq1Bits + seq2Bits + %d] = 0;') % (member, offset, cast, offset + 7)
                text = text.replace(old, replacement)
        if headers:
            for at, member in ((35, 'seq1Length'), (43, 'seq2Length')):
                old = '\tdilPackSeven(bits, %d, desc->%s - 1);' % (at, member)
                assert text.count(old) == 1
                replacement = ('\tn = desc->%s - 1;\n\tfor (i = %d; i <= %d; i++, n >>= 1)\n'
                               '\t\tbits[i] = (%s)(n & 1);\n\tbits[%d] = 0;') % (member, at, at + 6, cast, at + 7)
                text = text.replace(old, replacement)
        cells['segments-%d-headers-%d' % (segments, headers)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = '4169cb3b'
    d.OUT_NAME = 'recovery-dil-regions'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
