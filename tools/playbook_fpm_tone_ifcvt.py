#!/usr/bin/env python3
"""Twelve full-TU tone-filter ring/outer-width and conditional-update controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_filter')
    old = 'idx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;'
    new = 'idx = (short)(idx + 1);\n\t\tif ((short)idx >= taps)\n\t\t\tidx = 0;'
    assert fn.count(old) == 1
    cells = {}
    for ring in ('int', 'short', 'short-taps'):
        for outer in ('int', 'short'):
            for cfg in ('ternary', 'conditional'):
                text = fn
                if ring != 'int':
                    text = text.replace('int taps = state->cfg.len;', 'short taps = state->cfg.len;')
                if ring == 'short':
                    text = text.replace('int idx = state->hist_idx;', 'short idx = state->hist_idx;')
                if outer == 'short':
                    text = text.replace('\tint i;', '\tshort i;')
                if cfg == 'conditional':
                    text = text.replace(old, new)
                label = 'baseline' if (ring, outer, cfg) == ('int', 'int', 'ternary') else '-'.join((ring, outer, cfg))
                cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 12
    return cells


if __name__ == '__main__':
    driver.REV = '5b1bed17'
    driver.OUT_NAME = 'playbook-fpm-tone-ifcvt'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
