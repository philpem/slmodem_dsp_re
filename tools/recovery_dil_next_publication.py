#!/usr/bin/env python3
"""Separate the advancing seven-bit payload pointer from its separator owner."""
from itertools import product
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS
from recovery_dil_next_seven_pointer import variants as pointer


def variants(path, source):
    prepared = pointer(path, source)
    v92 = 'V92DIL' in path
    cells = {}
    for info, owner in (product((0, 1), repeat=2) if v92 else ((0, 0), (0, 1))):
        text = prepared['info-%d-pointer-1' % info]
        if owner:
            cast = 'unsigned char' if v92 else 'short'
            old = '''\tbits += at;
\tfor (i = 0; i < 7; i++, value >>= 1)
\t\t*bits++ = (%s)(value & 1);
\t*bits = 0;''' % cast
            new = '''\t%s *out = bits + at;

\tfor (i = 0; i < 7; i++, value >>= 1)
\t\t*out++ = (%s)(value & 1);
\tbits[at + 7] = 0;''' % (cast, cast)
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (info or owner) else 'info-%d-owner-%d' % (info, owner)
        cells[label] = text
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-publication'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
