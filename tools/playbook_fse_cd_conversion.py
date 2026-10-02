#!/usr/bin/env python3
"""Ten CD decision-lifetime/counter-conversion controls on the complete TU."""
import playbook_small_patterns as driver
from playbook_fse_cd_load import variants as load_variants


def variants(path, source):
    parents = load_variants(path, source)
    start, end, fn = driver.function(source, 'FSE_decision_CD')
    old = '\t*mag = 0x3299;\n\treturn (unsigned short)((m->count & 1) ? 3 : 0);'
    changed = fn.replace('\tshort n = (short)(m->count + 1);',
                         '\tshort n = (short)(m->count + 1);\n\tshort decision;')
    changed = changed.replace(old, '\tdecision = (short)m->count;\n\t*mag = 0x3299;\n\tdecision = (decision & 1) ? 3 : 0;\n\treturn (unsigned short)decision;')
    parents['short-count-carrier'] = source[:start] + changed + source[end:]
    cells = {}
    for label, parent in parents.items():
        cells[label] = parent
        start, end, fn = driver.function(parent, 'FSE_decision_CD')
        assert fn.count('short n = (short)(m->count + 1);') == 1
        assert fn.count('if (n > 0x0e)') == 1
        fn = fn.replace('short n = (short)(m->count + 1);', 'int n = m->count + 1;')
        fn = fn.replace('if (n > 0x0e)', 'if ((short)n > 0x0e)')
        cells[label + '-counter-use'] = parent[:start] + fn + parent[end:]
    assert len(cells) == len(set(cells.values())) == 10
    return cells


if __name__ == '__main__':
    driver.REV = 'b1632e79'
    driver.OUT_NAME = 'playbook-fse-cd-conversion'
    driver.SOURCE_PATHS = ('src/pump/v32/V32dec.c',)
    driver.variants = variants
    driver.main()
