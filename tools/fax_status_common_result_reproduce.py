#!/usr/bin/env python3
"""Reproduce the original shared status result through two callback arms."""
import playbook_small_patterns as d


def variants(path, source):
    start, end, function = d.function(source, 'fax_class1_status')
    assert function.count('\treturn 1;') == 2 and function.count('\treturn 0;') == 1
    body = function.replace('{\n', '{\n\tint result = 0;\n', 1)
    body = body.replace('\t\treturn 1;', '\t\tresult = 1;')
    body = body.replace('\t}\n\tif ((unsigned)', '\t} else if ((unsigned)')
    body = body.replace('\treturn 0;', '\treturn result;')
    return {'baseline': source, 'shared-result': source[:start]+body+source[end:]}


if __name__ == '__main__':
    d.REV = '9025b8d8'
    d.SOURCE_PATHS = ('src/fax/class1.c',)
    d.OUT_NAME = 'fax-status-common-result'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
