#!/usr/bin/env python3
"""Replay fixed-owner CP regions and fixed-state MP CRC source crosses."""
import itertools
import re
import sys
from pathlib import Path
import playbook_small_patterns as d

PATHS = ('src/pump/v90/V90CP.cpp', 'src/pump/v90/V90MP.cpp')


def loop(text, marker):
    start = text.index(marker)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return start, end, text[opening + 1:end - 1]


def cp_regions(text, payloads, buffers):
    if payloads:
        marker = '\t\t\tfor (k = 0; k <= 3; k++) {\n\t\t\t\tfor (j = 0; j < n[k]; j++)'
        start, end, body = loop(text, marker)
        expanded = ['\t\t\t{' + re.sub(r'\bk\b', str(k), body) + '}' for k in range(4)]
        text = text[:start] + '\n'.join(expanded) + text[end:]
    if buffers:
        marker = '\t\t\tfor (k = 0; k <= 5; k++) {\n\t\t\t\tunsigned int n = nof_buf[k];'
        start, end, body = loop(text, marker)
        guard = '\n\t\t\t\tif (n == 0)\n\t\t\t\t\tcontinue;\n'
        assert body.count(guard) == 1
        body = body.replace(guard, '\n\t\t\t\tif (n != 0) {\n') + '\t}\n\t\t\t'
        expanded = ['\t\t\t{' + re.sub(r'\bk\b', str(k), body) + '}' for k in range(6)]
        text = text[:start] + '\n'.join(expanded) + text[end:]
    return text


def mp_shifts(text, encoder, decoder):
    pattern = re.compile(r'(\t+)for \(k = 0; k < 15; k\+\+\)\n\1\tcrc\[k\] = crc\[k \+ 1\];')
    for enabled, name, expected in ((encoder, 'V90MP::infoToBits', 1), (decoder, 'V90MP::bitsToInfo', 2)):
        if not enabled:
            continue
        start, end, fn = d.function(text, name)
        assert len(pattern.findall(fn)) == expected
        fn = pattern.sub(lambda match: '\n'.join(match[1] + 'crc[%d] = crc[%d];' % (i, i + 1) for i in range(15)), fn)
        text = text[:start] + fn + text[end:]
    return text


def variants(path, source):
    cells = {'baseline': source}
    for first, second in itertools.product((0, 1), repeat=2):
        if not (first or second):
            continue
        if 'V90CP' in path:
            start, end, fn = d.function(source, 'V90CP::infoToBits')
            text = source[:start] + cp_regions(fn, first, second) + source[end:]
        else:
            text = mp_shifts(source, first, second)
        cells['axis-%d-%d' % (first, second)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-topology'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
