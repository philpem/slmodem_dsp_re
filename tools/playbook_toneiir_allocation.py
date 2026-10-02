#!/usr/bin/env python3
"""Four complete-TU controls for toneiir_create's missing allocation guard."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'toneiir_create')
    old = '\t\tst = sysdep_malloc(sizeof(*st));\n'
    assert fn.count(old) == 1
    new = old + '\tif (st == 0)\n\t\treturn 0;\n'
    cells = {'baseline': source,
             'allocation-guard': source[:start] + fn.replace(old, new) + source[end:]}
    allocated = '\tif (st == 0)\n' + old
    assert fn.count(allocated) == 1
    for label, result in [('nested-zero', '0'), ('nested-pointer', 'st')]:
        guarded = ('\tif (st == 0) {\n'
                   '\t\tst = sysdep_malloc(sizeof(*st));\n'
                   '\t\tif (st == 0)\n'
                   '\t\t\treturn ' + result + ';\n\t}\n')
        cells[label] = source[:start] + fn.replace(allocated, guarded) + source[end:]
    return cells



if __name__ == '__main__':
    driver.REV = '96ef2bb6'
    driver.OUT_NAME = 'playbook-toneiir-allocation'
    driver.SOURCE_PATHS = ('src/callprog/toneiir.c',)
    driver.variants = variants
    driver.main()
