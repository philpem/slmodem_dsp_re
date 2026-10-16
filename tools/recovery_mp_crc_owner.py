#!/usr/bin/env python3
"""Cross complete existing MP CRC producers in the two call-free consumers."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_topology import loop
from recovery_mp_padding import variants as padding


def variants(path, source):
    base = padding(path, source)['full-padding-bound']
    cells = {'baseline': base}
    for encoder, decoder in itertools.product((0, 1), repeat=2):
        if not (encoder or decoder):
            continue
        text = base
        for active, name in ((encoder, 'V90MP::infoToBits'), (decoder, 'V90MP::bitsToInfo')):
            if not active:
                continue
            start, end, fn = d.function(text, name)
            for unused in range(2):
                marker = '\t\tend = t ?' if name.endswith('infoToBits') else '\t\tfor (k = 0; k <= 15; k++)'
                first = fn.index(marker)
                rest = fn[first:]
                a, b, body = loop(rest, '\t\tfor (i = 0x12; i < end; i++) {')
                assert 'crc[15] = (unsigned char)(x & 1);' in body
                fn = fn[:first] + '\t\tresetCRC();\n\t\tcalcCRC();' + rest[b:]
            text = text[:start] + fn + text[end:]
        cells['encoder-%d-decoder-%d' % (encoder, decoder)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-crc-owner'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
