#!/usr/bin/env python3
"""Four staged ring-result carrier controls, preserving pre-compare narrowing."""
import playbook_small_patterns as driver
from playbook_v32_smc_ifcvt import variants as cfg_variants


def variants(path, source):
    parent = cfg_variants(path, source)['traversal-all-short-tags-conditional']
    cells = {}
    for label, helper, caller in [('baseline', False, False),
                                   ('helper-int', True, False),
                                   ('caller-int', False, True),
                                   ('both', True, True)]:
        text = parent
        if helper:
            old = 'static short\nring_advance(short widx, short limit)\n{\n\tshort next = (short)(widx + 1);'
            new = 'static int\nring_advance(int widx, short limit)\n{\n\tint next = (short)(widx + 1);'
            assert text.count(old) == 1
            text = text.replace(old, new)
        if caller:
            assert text.count('short widx = out->widx;') == 3
            text = text.replace('short widx = out->widx;', 'int widx = out->widx;')
        cells[label] = text
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '6eff516d'
    driver.OUT_NAME = 'playbook-v32-smc-carrier'
    driver.SOURCE_PATHS = ('src/pump/v32/V32SMC_TX.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'V32SMC_TX'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-v32-smc-ifcvt/V32SMC_TX/traversal-all-short-tags-conditional/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
