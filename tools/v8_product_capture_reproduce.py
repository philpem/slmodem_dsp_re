#!/usr/bin/env python3
"""Bound V8 coefficient-first expressions against a named sample owner."""
import playbook_small_patterns as d
import v8_index_cursor_reproduce as prior


def capture(source, named_coefficient):
    a, z, fn = d.function(source, 'v8_fskdemodulate')
    for sample in ('v->v21.inbuf[pos - j]', 'v->v21.delay[V8_V21_DELAY + pos - j]'):
        declaration = '\t\t\tint s = '+sample+';'
        assert fn.count(declaration) == 1
        products = '\n'.join('\t\t\ta%d += v->v21.%s[j] * s;' % (i, field)
                             for i, field in enumerate('abcd'))
        start = fn.index(declaration)
        end = fn.index(products, start)+len(products)
        block = fn[start:end]
        assert block.count(products) == 1
        if named_coefficient:
            block = block.replace(declaration, '\t\t\tint coefficient = v->v21.a[j];\n'+declaration, 1)
            block = block.replace(products, products.replace('v->v21.a[j] * s', 'coefficient * s'), 1)
        else:
            block = block.replace(declaration+'\n\n', '', 1)
            block = block.replace(products, products.replace(' * s;', ' * '+sample+';'), 1)
        fn = fn[:start]+block+fn[end:]
    return source[:a]+fn+source[z:]


def variants(path, source):
    control = prior.variants(path, source)['unsigned-index-and-cursor']
    return {'baseline': source, 'index-cursor-control': control,
            'coefficient-first-expressions': capture(control, False),
            'coefficient-first-owner': capture(control, True)}


if __name__ == '__main__':
    d.REV = '8e914a86'
    d.SOURCE_PATHS = ('src/v8/V8Dpsk.c',)
    d.OUT_NAME = 'v8-product-capture'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da', '-dP')
    d.variants = variants
    d.main()
