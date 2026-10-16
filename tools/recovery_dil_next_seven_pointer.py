#!/usr/bin/env python3
"""Cross the proven explicit destination pointer with both seven-bit siblings."""
from itertools import product
import playbook_small_patterns as d
from recovery_dil_next_info import REV, PATHS
from recovery_dil_next_tail import variants as tail


def variants(path, source):
    cells = {}
    v92 = 'V92DIL' in path
    for info, pointer in (product((0, 1), repeat=2) if v92 else ((0, 0), (0, 1))):
        text = tail(path, source)['tail-1-even-1'] if info else source
        if pointer:
            cast = 'unsigned char' if v92 else 'short'
            old = '''\tfor (i = 0; i < 7; i++, value >>= 1)
\t\tbits[at + i] = (%s)(value & 1);
\tbits[at + 7] = 0;''' % cast
            new = '''\tbits += at;
\tfor (i = 0; i < 7; i++, value >>= 1)
\t\t*bits++ = (%s)(value & 1);
\t*bits = 0;''' % cast
            assert text.count(old) == 1
            text = text.replace(old, new)
        label = 'baseline' if not (info or pointer) else 'info-%d-pointer-%d' % (info, pointer)
        cells[label] = text
    return cells


if __name__ == '__main__':
    d.REV = REV
    d.OUT_NAME = 'recovery-dil-next-seven-pointer'
    d.SOURCE_PATHS = PATHS
    d.variants = variants
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.main()
