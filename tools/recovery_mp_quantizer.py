#!/usr/bin/env python3
"""Cross the existing exact sequence-length producer in its two MP consumers."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_topology import loop
from recovery_mp_print_owner import variants as print_owner


def variants(path, source):
    base = print_owner(path, source)['helper-convert-1']
    cells = {'baseline': base}
    for encoder, decoder in itertools.product((0, 1), repeat=2):
        if not (encoder or decoder):
            continue
        text = base
        for active, name in ((encoder, 'V90MP::infoToBits'), (decoder, 'V90MP::bitsToInfo')):
            if not active:
                continue
            start, end, fn = d.function(text, name)
            marker = '\t{\n\t\tunsigned int want' if encoder and name.endswith('infoToBits') else '\t\t{\n\t\t\tunsigned int want'
            a, b, body = loop(fn, marker)
            assert 'want / groupSize' in body and 'seqLength = (unsigned char)want;' in body
            replacement = '\tcalcSequenceLength();' if name.endswith('infoToBits') else '\t\tcalcSequenceLength();'
            text = text[:start] + fn[:a] + replacement + fn[b:] + text[end:]
        cells['encoder-%d-decoder-%d' % (encoder, decoder)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'd9995470'
    d.OUT_NAME = 'recovery-mp-next-quantizer'
    d.SOURCE_PATHS = ('src/pump/v90/V90MP.cpp',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
