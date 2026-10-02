#!/usr/bin/env python3
"""Two full-TU Dual_TONE_create early/common-return controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'Dual_TONE_create')
    old = ('\tif (st == 0)\n\t\treturn 0;\n\n'
           '\tsysdep_memset(st, 0, sizeof(*st));\n'
           '\tst->ratio = 226;\n\tst->min_energy = 1;')
    new = ('\tif (st != 0) {\n'
           '\t\tsysdep_memset(st, 0, sizeof(*st));\n'
           '\t\tst->ratio = 226;\n\t\tst->min_energy = 1;\n\t}')
    assert fn.count(old) == 1
    return {'baseline': source,
            'common-return': source[:start] + fn.replace(old, new) + source[end:]}


if __name__ == '__main__':
    driver.REV = '2be7a9e8'
    driver.OUT_NAME = 'playbook-dualtone-create'
    driver.SOURCE_PATHS = ('src/callprog/DualTone_Detector.c',)
    driver.variants = variants
    driver.main()
