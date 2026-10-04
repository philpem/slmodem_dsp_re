#!/usr/bin/env python3
"""Bound original V8 run arithmetic and zero-only push termination."""
import playbook_small_patterns as d


def variants(path, source):
    cells = {'baseline': source}
    for unsigned, zero_only in ((1, 0), (0, 1), (1, 1)):
        x = source
        if unsigned:
            for name in ('drain_run', 'flush_run'):
                a, z, fn = d.function(x, name)
                fn = fn.replace('int run, short bit)', 'unsigned int run, short bit)')
                for local in ('whole', 'rem', 'need', 'n'):
                    fn = fn.replace('int ' + local + ' =', 'unsigned int ' + local + ' =')
                x = x[:a] + fn + x[z:]
            for field in ('mark_run', 'space_run'):
                old = 'if (v->v21.' + field + ' > V8_FSK_RUN)'
                assert x.count(old) == 1
                x = x.replace(old, 'if ((unsigned int)v->v21.' + field + ' > V8_FSK_RUN)')
        if zero_only:
            assert x.count('while (n-- > 0)') == 1
            x = x.replace('while (n-- > 0)', 'while (n-- != 0)')
        label = ('unsigned-runs' if unsigned else 'signed-runs') + ('-zero-only-push' if zero_only else '-positive-push')
        cells[label] = x
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    d.REV = 'b470429e'
    d.SOURCE_PATHS = ('src/v8/V8Dpsk.c',)
    d.OUT_NAME = 'unblock-v8-runs'
    d.DUMP_FLAGS = ('-v', '-save-temps', '-da')
    d.variants = variants
    d.main()
