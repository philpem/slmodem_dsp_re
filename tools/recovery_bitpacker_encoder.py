#!/usr/bin/env python3
"""Cross original branch-local MP encoder CRC tails with full-width end."""
import itertools
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_topology import loop
from recovery_bitpacker_end import PATH, variants as end_cells


def split_encoder(fn):
    first, positive_end, positive = loop(fn, '\tif (t != 0) {')
    negative_marker = fn.index('else {', positive_end)
    _, negative_end, negative = loop(fn, 'else {')
    assert negative_marker == fn.index('else {')
    shared_end = fn.index('\n\tlen = seqLength;', negative_end)
    crc = fn[negative_end:shared_end]
    tail_start, tail_positive_end, tail_positive = loop(fn, '\tif (t != 0) {\n\t\tbits[0xbb] = 0;')
    _, tail_negative_end, tail_negative = loop(fn[tail_positive_end:], 'else {')
    tail_negative_end += tail_positive_end
    assert fn[tail_negative_end:].strip() == '}'
    nested_crc = '\n'.join('\t' + line if line else line for line in crc.splitlines())
    long_body = positive + nested_crc + '\n\t\tlen = seqLength;' + tail_positive
    short_body = negative + nested_crc + '\n\t\tlen = seqLength;' + tail_negative
    result = fn[:first] + '\tif (t != 0) {' + long_body + '} else {' + short_body + '}\n}'
    return '\n'.join(line.rstrip() for line in result.splitlines())


def variants(path, source):
    base = end_cells(path, source)['wide-1-late-1']
    cells = {'baseline': base}
    for split, wide in itertools.product((0, 1), repeat=2):
        if not (split or wide):
            continue
        start, end, fn = d.function(base, 'V90MP::infoToBits')
        if wide:
            assert fn.count('\tunsigned int i;') == 1 and fn.count('x, n, end, len;') == 1
            fn = fn.replace('\tunsigned int i;', '\tunsigned int i, end;').replace('x, n, end, len;', 'x, n, len;')
        if split:
            fn = split_encoder(fn)
        cells['split-%d-wide-%d' % (split, wide)] = base[:start] + fn + base[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-encoder'
    d.SOURCE_PATHS = (PATH,)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
