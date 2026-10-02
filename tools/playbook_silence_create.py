#!/usr/bin/env python3
"""Two complete-TU silence constructor return-carrier controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'silence_create')
    alloc = ('\tif (s == 0) {\n\t\ts = sysdep_malloc(sizeof(*s));\n'
             '\t\tif (s == 0)\n\t\t\treturn 0;\n\t}\n')
    assert fn.count(alloc) == 1
    changed = fn.replace(alloc, '\tif (s == 0)\n\t\ts = sysdep_malloc(sizeof(*s));\n\tif (s != 0) {\n')
    changed = changed.replace('\treturn s;\n}', '\t}\n\treturn s;\n}')
    return {'baseline': source,
            'common-return': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = 'd9a21365'
    driver.OUT_NAME = 'playbook-silence-create'
    driver.SOURCE_PATHS = ('src/service/silence.c',)
    driver.variants = variants
    driver.main()
