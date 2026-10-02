#!/usr/bin/env python3
"""Four complete-TU bit-packer accumulator/position conversion controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'pack_next_bit')
    cells = {}
    for label, wide, position in [('baseline', False, False),
                                   ('wide-accumulator', True, False),
                                   ('position-update', False, True),
                                   ('both', True, True)]:
        text = fn
        if wide:
            old = 'unsigned short acc = (unsigned short)cid->pack_acc;'
            assert text.count(old) == 1
            text = text.replace(old, 'int acc = (unsigned short)cid->pack_acc;')
            old = 'acc = (unsigned short)(acc | ((int)bit << pos));'
            assert text.count(old) == 1
            text = text.replace(old, 'acc |= (int)bit << pos;')
        if position:
            old = '\t\tcid->pack_pos = (short)(pos + 1);'
            assert text.count(old) == 1
            text = text.replace(old, '\t\tpos = (short)(pos + 1);\n\t\tcid->pack_pos = pos;')
            old = 'if ((short)(pos + 1) == 8)'
            assert text.count(old) == 1
            text = text.replace(old, 'if (pos == 8)')
        cells[label] = source[:start] + text + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-cid-pack'
    driver.SOURCE_PATHS = ('src/service/Rxcid.c',)
    driver.variants = variants
    driver.main()
