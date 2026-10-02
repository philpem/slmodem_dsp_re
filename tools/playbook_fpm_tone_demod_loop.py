#!/usr/bin/env python3
"""Three staged demod-generator counter/post-decrement loop controls."""
import playbook_small_patterns as driver
from playbook_fpm_tone_demod_count import variants as count_variants


def variants(path, source):
    cells = count_variants(path, source)
    parent = cells['short-counter']
    start, end, fn = driver.function(parent, 'FPM_TONE_generate_demod')
    old = 'for (i = (short)(count - 1); i != -1; i = (short)(i - 1))'
    assert fn.count(old) == 1
    fn = fn.replace(old, 'for (i = count; i-- != 0;)')
    cells['post-decrement'] = parent[:start] + fn + parent[end:]
    assert len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-fpm-tone-demod-loop'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    cell = driver.ROOT/'build'/driver.OUT_NAME/'fpm_tone'
    cell.mkdir(parents=True, exist_ok=True)
    prior = driver.ROOT/'build/playbook-fpm-tone-demod/fpm_tone/both/candidate.o'
    saved = cell/'retained.o'
    if saved.exists():
        assert saved.read_bytes() == prior.read_bytes()
    else:
        saved.write_bytes(prior.read_bytes())
    driver.main()
