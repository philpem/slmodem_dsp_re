#!/usr/bin/env python3
"""V23 constructor literal and read-only table initializer-visibility cross."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    old = '#define BWCH_TONE_RATIO\t\t28996\t/* 0.885 in Q15 */'
    assert source.count(old) == 1
    declaration = 'static const int block_size_table[12]'
    start = source.index(declaration)
    end = source.index('\n};', start) + len('\n};')
    table = source[start:end]
    cells = {}
    for literal, deferred in product((False, True), repeat=2):
        text = source
        if deferred:
            text = text[:start] + declaration + ';' + text[end:]
            _, finish, _ = driver.function(text, 'BwChDem_Create')
            text = text[:finish] + '\n\n' + table + text[finish:]
        if literal: text = text.replace(old, old.replace('28996', '29000'))
        label = 'baseline' if not (literal or deferred) else '-'.join(n for n, yes in zip(('blob-ratio','deferred-table'),(literal,deferred)) if yes)
        cells[label] = text
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '70bf8bc8'
    driver.OUT_NAME = 'playbook-bwch-create'
    driver.SOURCE_PATHS = ('src/pump/v23/bwchdem.c',)
    driver.variants = variants
    driver.main()
