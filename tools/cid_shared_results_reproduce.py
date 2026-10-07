#!/usr/bin/env python3
"""Shared mark-balance and channel-seizure position values on the complete TU."""
import playbook_small_patterns as d

def variants(path, source):
    a, z, fn = d.function(source, 'pack_next_bit')
    mark = '\t\tif (bit == 1)\n\t\t\tcid->mark_bal = (short)(cid->mark_bal + 1);\n\t\telse\n\t\t\tcid->mark_bal = (short)(cid->mark_bal - 1);'
    pos = '\t\tif (bit != 0)\n\t\t\tcid->pack_pos = 0;\n\t\telse\n\t\t\tcid->pack_pos = (short)(cid->pack_pos + 1);'
    assert fn.count(mark) == fn.count(pos) == 1
    cells = {}
    for m, p in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        body = fn
        if m:
            body = body.replace(mark, '\t\tcid->mark_bal = bit == 1 ? (short)(cid->mark_bal + 1)\n\t\t    : (short)(cid->mark_bal - 1);')
        if p:
            body = body.replace(pos, '\t\tcid->pack_pos = bit != 0 ? 0 : (short)(cid->pack_pos + 1);')
        cells['baseline' if not (m or p) else f'mark-{m}-position-{p}'] = source[:a]+body+source[z:]
    assert len(set(cells.values())) == 4
    return cells

if __name__ == '__main__':
    d.REV = '04eee73f'
    d.OUT_NAME = 'cid-shared-results'
    d.SOURCE_PATHS = ('src/service/Rxcid.c',)
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
