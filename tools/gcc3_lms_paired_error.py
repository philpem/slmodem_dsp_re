#!/usr/bin/env python3
"""Replay the original paired-error lifetime across the first opaque LMS call."""
import playbook_small_patterns as d


def variants(path, source):
    start = source.index('\t\t\tFPM_lmsupd(icoeff')
    end = source.index('\n\t\t}', start)
    old = source[start:end]
    assert old.count('FPM_lmsupd(') == 2
    expressions = ['(short)((err_' + axis + ' * mu + FPM_FSE_MU_ROUND)\n'
                   '\t\t\t\t\t   >> FPM_FSE_MU_SHIFT)' for axis in ('i', 'q')]
    assert all(old.count(e) == 1 for e in expressions)
    cells = {'baseline': source}
    for label, names in [('paired-locals', ['update_i', 'update_q']),
                         ('paired-existing', ['err_i', 'err_q'])]:
        declaration = 'short ' if label == 'paired-locals' else ''
        prefix = ''.join('\t\t\t' + declaration + name + ' = ' + expr + ';\n'
                         for name, expr in zip(names, expressions)) + '\n'
        body = old
        for expr, name in zip(expressions, names):
            body = body.replace(expr, name)
        cells[label] = source[:start] + prefix + body + source[end:]
    assert len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    d.REV = '4de483e2'
    d.SOURCE_PATHS = ('src/dsp/fpm_fse.c',)
    d.OUT_NAME = 'gcc3-lms-paired-error'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
