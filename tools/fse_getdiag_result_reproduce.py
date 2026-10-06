#!/usr/bin/env python3
"""Cross an entry-default diagnostic result with the witnessed case1 guard."""
import playbook_small_patterns as d
import playbook_fse_getdiag_guard as prior


def shared_result(source):
    start, end, body = d.function(source, 'FSE_getdiag')
    assert body.count('\n\t\treturn 0;') == 0  # Overflow return has three tabs.
    assert body.count('\t\t\treturn 0;') == 1
    assert body.count('\t\treturn n;') == 2
    assert body.count('\n\treturn 0;') == 1
    body = body.replace('\tint n, i;', '\tint n, i;\n\tint result = 0;')
    body = body.replace('\t\t\treturn 0;', '\t\t\tbreak;')
    body = body.replace('\t\treturn n;', '\t\tresult = n;\n\t\tbreak;')
    body = body.replace('\treturn 0;', '\treturn result;')
    return source[:start] + body + source[end:]


def variants(path, source):
    guarded = prior.variants(path, source)['positive-guard']
    return {'baseline': source, 'guard-control': guarded,
            'entry-result': shared_result(source),
            'entry-result-guard': shared_result(guarded)}


if __name__ == '__main__':
    d.REV = '6b4509bd'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'fse-getdiag-result'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
