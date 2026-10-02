#!/usr/bin/env python3
"""Full-TU cross of three object-observed multi-rate initializer boundaries."""
from itertools import product
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_MRF_init')
    begin = body.index('\tif (fresh) {')
    finish = body.index('\n\tfor (i = 0;', begin)
    control = '''\tif (!fresh && state->history_len < per_phase) {
\t\tif (DSPLIB_DEBUG_ON())
\t\t\tdsplibs_debug_printf("Reallocate FPM_MRF buffer");
\t\tsysdep_free(state->history);
\t\tfresh = 1;
\t}

\tstate->history_len = per_phase;
\tif (fresh)
\t\tstate->history = sysdep_malloc(
\t\t\t(unsigned)per_phase * sizeof(short));
'''
    cells = {}
    for member, counter, fresh in product((False, True), repeat=3):
        text = body
        if fresh:
            text = text[:begin] + control + text[finish:]
            assert text.count('\tint allocate;\n') == 1
            text = text.replace('\tint allocate;\n', '')
        if member:
            assert text.count('cfg->taps / cfg->branches') == 1
            text = text.replace('cfg->taps / cfg->branches', 'state->cfg.taps / state->cfg.branches')
        if counter:
            assert text.count('\tint i;') == 1
            text = text.replace('\tint i;', '\tshort i;')
        label = 'baseline' if not any((member, counter, fresh)) else '-'.join(n for n, yes in zip(('member', 'short-counter', 'fresh-control'), (member, counter, fresh)) if yes)
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    driver.REV = 'b3d741f7'
    driver.OUT_NAME = 'playbook-fpm-mrf-init'
    driver.SOURCE_PATHS = ('src/dsp/fpm_mrf.c',)
    driver.variants = variants
    driver.main()
