#!/usr/bin/env python3
"""Six staged period-promotion/read-boundary controls on complete tone TU."""
import playbook_small_patterns as driver
from playbook_fpm_tone_sine_reversal import variants as reversal_variants


def variants(path, source):
    parents = reversal_variants(path, source)
    cells = {'baseline': parents['late-short'],
             'int-reference-order': parents['late-short-reference-order']}
    for order in (False, True):
        parent = parents['late-short-reference-order' if order else 'late-short']
        start, end, fn = driver.function(parent, 'FPM_TONE_generate')
        assert fn.count('\tint period;') == 1
        fn = fn.replace('\tint period;', '\tshort period;')
        for after in (False, True):
            text = fn
            if after:
                assignment = '\tperiod = state->cfg.rev_period;\n'
                assert text.count(assignment) == 1
                text = text.replace(assignment, '')
                marker = '\t\t  + (count >> 3);'
                assert text.count(marker) == 1
                text = text.replace(marker, marker + '\n' + assignment.rstrip())
            label = 'short-' + ('after' if after else 'before') + ('-reference-order' if order else '-positive-first')
            cells[label] = parent[:start] + text + parent[end:]
    assert len(cells) == len(set(cells.values())) == 6
    return cells


if __name__ == '__main__':
    driver.REV = '54bf1179'
    driver.OUT_NAME = 'playbook-fpm-tone-sine-narrow'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    previous = driver.ROOT/'build/playbook-fpm-tone-sine-reversal/fpm_tone/late-short/candidate.o'
    saved = driver.ROOT/'build'/driver.OUT_NAME/'fpm_tone/retained.o'
    saved.parent.mkdir(parents=True, exist_ok=True)
    if saved.exists():
        assert saved.read_bytes() == previous.read_bytes()
    else:
        saved.write_bytes(previous.read_bytes())
    driver.main()
