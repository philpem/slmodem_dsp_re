#!/usr/bin/env python3
"""Full-TU FSM cached configuration, countdown and total-width controls."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_FSM_modulate')
    cells = {}
    for cache, outer, inner, total in product((False, True), repeat=4):
        text = body
        if cache:
            text = text.replace('\tint total = 0;', '\tshort scale = state->cfg.scale;\n\tshort samples = state->cfg.samples_per_sym;\n\tint total = 0;', 1)
            text = text.replace('state->cfg.scale);', 'scale);').replace('total += state->cfg.samples_per_sym;', 'total += samples;').replace('k < state->cfg.samples_per_sym', 'k < samples')
        if outer:
            text = text.replace('\tunsigned i;\n', '').replace('for (i = 0; i < nbits; i++)', 'while (nbits--)').replace('bits[i] & 1', '*bits++ & 1')
        if inner:
            samples = 'samples' if cache else 'state->cfg.samples_per_sym'
            text = text.replace('\t\tint k;', '\t\tunsigned short k;').replace('for (k = 0; k < '+samples+'; k++)', 'k = '+samples+';\n\t\twhile (k--)')
        if total: text = text.replace('\tint total = 0;', '\tunsigned short total = 0;')
        label = 'baseline' if not any((cache, outer, inner, total)) else '-'.join(n for n, yes in zip(('cached','outer-countdown','inner-countdown','short-total'), (cache,outer,inner,total)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 16
    return cells


if __name__ == '__main__':
    driver.REV = '70bf8bc8'
    driver.OUT_NAME = 'playbook-fpm-fsm-modulate'
    driver.SOURCE_PATHS = ('src/dsp/fpm_fsm.c',)
    driver.variants = variants
    driver.main()
