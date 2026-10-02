#!/usr/bin/env python3
"""Twelve full-TU tag/traversal controls crossed with conditional ring update."""
import playbook_small_patterns as driver
from playbook_v32_smc_tag import variants as tag_variants


def variants(path, source):
    cells = {}
    old = '\treturn (short)((next < limit) ? next : 0);'
    new = '\tif (next >= limit)\n\t\tnext = 0;\n\treturn next;'
    for label, text in tag_variants(path, source).items():
        assert text.count(old) == 1
        cells[label] = text
        cells[label + '-conditional'] = text.replace(old, new)
    assert len(cells) == len(set(cells.values())) == 12
    return cells


if __name__ == '__main__':
    driver.REV = '6eff516d'
    driver.OUT_NAME = 'playbook-v32-smc-ifcvt'
    driver.SOURCE_PATHS = ('src/pump/v32/V32SMC_TX.c',)
    driver.variants = variants
    driver.main()
