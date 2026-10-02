#!/usr/bin/env python3
"""Four full-TU signed-short ring/countdown carrier controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_filter')
    cells = {}
    for label, ring, outer in [('baseline', False, False),
                                ('ring-width', True, False),
                                ('outer-width', False, True),
                                ('both', True, True)]:
        changed = fn
        if ring:
            changed = changed.replace('int taps = state->cfg.len;', 'short taps = state->cfg.len;')
            changed = changed.replace('int idx = state->hist_idx;', 'short idx = state->hist_idx;')
        if outer:
            changed = changed.replace('\tint i;', '\tshort i;')
        cells[label] = source[:start] + changed + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '9a16b620'
    driver.OUT_NAME = 'playbook-fpm-tone-width'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
