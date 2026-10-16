#!/usr/bin/env python3
"""Replay calcK logarithm evaluation/narrowing controls on the full period TU.

Use --historical-headers when replaying after unrelated header changes. Supply
--baseline-dir with the faithful 1d4b891e production objects and .build-config.
The shared runner requires literal full-TU baseline reproduction and records
all shared bodies, exact gains/losses, exports and compiler commands.
"""
import playbook_small_patterns as d


def variants(path, source):
    start, end, fn = d.function(source, 'V90ConstellationDesigner::calcK')
    old = ('\tl2 = (float)log10l((long double)2.0f);\n'
           '\treturn (float)log10l((long double)k) * (1.0f / l2);')
    assert fn.count(old) == 1
    logk = '\tfloat logK = (float)log10l((long double)k);'
    l2 = '\tl2 = (float)log10l((long double)2.0f);'
    expression = ('\treturn (float)log10l((long double)k) *\n'
                  '\t    (1.0f / (float)log10l((long double)2.0f));')
    forms = {
        'capture-after': '\n'.join((l2, logk, '\treturn logK * (1.0f / l2);')),
        'capture-before': '\n'.join((logk, l2, '\treturn logK * (1.0f / l2);')),
        'single-expression': expression,
        'k-capture-only': logk + '\n\treturn logK * '
                          '(1.0f / (float)log10l((long double)2.0f));',
    }
    cells = {'baseline': source}
    for label, body in forms.items():
        cells[label] = source[:start] + fn.replace(old, body) + source[end:]
    final = fn.replace(old, expression).replace('\tfloat l2;\n', '')
    cells['expression-no-dead-local'] = source[:start] + final + source[end:]
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    d.REV = '1d4b891e'
    d.OUT_NAME = 'recovery-v90-calck-expression'
    d.SOURCE_PATHS = ('src/pump/v90/V90ConstellationDesigner.cpp',)
    d.DUMP_FLAGS = ('-da',)
    d.variants = variants
    d.main()
