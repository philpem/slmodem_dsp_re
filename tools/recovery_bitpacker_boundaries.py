#!/usr/bin/env python3
"""Replay original CP read ages and MP interleaved feedback-state updates."""
import itertools
import re
import sys
from pathlib import Path
import playbook_small_patterns as d
from recovery_bitpacker_topology import PATHS, loop, variants as topology


def cp_ages(text, pairs, buffers):
    if pairs:
        for marker in ('\t\t\tfor (k = 0; k <= 5; k++) {\n\t\t\t\tint lo =',
                       '\t\t\tfor (k = 0; k <= 2; k++) {\n\t\t\t\tint lo ='):
            start, end, body = loop(text, marker)
            low = re.search(r'int lo = ([^;\n]+);', body)[1]
            high = re.search(r'int hi = ([^;\n]+);', body)[1]
            body = body.replace('int lo = ' + low + ';', 'int lo;')
            body = body.replace('int hi = ' + high + ';', 'int hi;')
            body = body.replace('\t\t\t\tpos++;', '\t\t\t\tpos++;\n\t\t\t\tlo = ' + low + ';', 1)
            body = body.replace('\t\t\t\tpos += 8;', '\t\t\t\thi = ' + high + ';\n\t\t\t\tpos += 8;', 1)
            opening = text.index('{', start)
            text = text[:opening + 1] + body + text[end - 1:]
    if buffers:
        old = ('\t\t\t\t\tint w = (short)src[j];\n\n'
               '\t\t\t\t\tpos = bitIndex;\n\t\t\t\t\tbits[pos] = 0;\n'
               '\t\t\t\t\tpos++;\n\t\t\t\t\tbitIndex = pos;')
        assert text.count(old) == 6
        new = old.replace('int w = (short)src[j];', 'int w;') + '\n\t\t\t\t\tw = (short)src[j];'
        text = text.replace(old, new)
    return text


def mp_interleaved(text, encoder, decoder):
    for enabled, name, expected in ((encoder, 'V90MP::infoToBits', 1), (decoder, 'V90MP::bitsToInfo', 2)):
        if not enabled:
            continue
        start, end, fn = d.function(text, name)
        pattern = re.compile(r'(\t+)crc\[0\] = crc\[1\];(?:\n\1crc\[\d+\] = crc\[\d+\];){14}\n\1crc\[3\] = \(unsigned char\)\(\(crc\[3\] \+ x\) & 1\);\n\1crc\[10\] = \(unsigned char\)\(\(crc\[10\] \+ x\) & 1\);\n\1crc\[15\] = \(unsigned char\)\(x & 1\);')
        assert len(pattern.findall(fn)) == expected
        def replacement(match):
            statements = ['crc[%d] = crc[%d];' % (i, i + 1) for i in range(15)]
            for i in (3, 10):
                statements[i] = 'crc[%d] = (unsigned char)((crc[%d] + x) & 1);' % (i, i + 1)
            statements.append('crc[15] = (unsigned char)(x & 1);')
            return '\n'.join(match[1] + statement for statement in statements)
        fn = pattern.sub(replacement, fn)
        text = text[:start] + fn + text[end:]
    return text


def variants(path, source):
    base = topology(path, source)['axis-1-1']
    cells = {'baseline': base}
    for first, second in itertools.product((0, 1), repeat=2):
        if not (first or second):
            continue
        if 'V90CP' in path:
            start, end, fn = d.function(base, 'V90CP::infoToBits')
            text = base[:start] + cp_ages(fn, first, second) + base[end:]
        else:
            text = mp_interleaved(base, first, second)
        cells['axis-%d-%d' % (first, second)] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain') + 1]).is_file()
    d.REV = 'ea95b278'
    d.OUT_NAME = 'recovery-bitpacker-boundaries'
    d.SOURCE_PATHS = PATHS
    d.DUMP_FLAGS = ('-v', '-save-temps', '-dr', '-dc', '-dg')
    d.variants = variants
    d.main()
