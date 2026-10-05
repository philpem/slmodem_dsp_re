#!/usr/bin/env python3
"""Cross observed V8 unsigned tap indices with countdown input cursor."""
import playbook_small_patterns as d
import v8_unsigned_decision_reproduce as prior


def index(source):
    a, z, fn = d.function(source, 'v8_fskdemodulate')
    assert fn.count('\t\tint j;') == 1
    fn = fn.replace('\t\tint j;', '\t\tunsigned int j;')
    return source[:a]+fn+source[z:]


def cursor(source):
    a, z, fn = d.function(source, 'v8_fskdemodulate')
    assert fn.count('\tint pos;') == 1
    fn = fn.replace('\tint pos;', '\tint pos;\n\tconst short *input = v->rx_stage;')
    assert fn.count('for (i = 0; i < V8_QUEUE_BLOCK; i++)') == 1
    fn = fn.replace('for (i = 0; i < V8_QUEUE_BLOCK; i++)',
                    'for (i = V8_QUEUE_BLOCK - 1; i >= 0; i--)')
    assert fn.count('v->rx_stage[i]') == 1
    fn = fn.replace('v->rx_stage[i]', '*input++')
    return source[:a]+fn+source[z:]


def variants(path, source):
    control = prior.variants(path, source)['unsigned-switch-zero-bit']
    return {'baseline': source, 'decision-control': control,
            'unsigned-tap-index': index(control), 'countdown-input-cursor': cursor(control),
            'unsigned-index-and-cursor': cursor(index(control))}


if __name__ == '__main__':
    d.REV = '10ae345a'
    d.SOURCE_PATHS = ('src/v8/V8Dpsk.c',)
    d.OUT_NAME = 'v8-index-cursor'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
