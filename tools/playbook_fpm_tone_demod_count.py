#!/usr/bin/env python3
"""Two staged signed-word demod-generator loop-carrier controls."""
import playbook_small_patterns as driver
from playbook_fpm_tone_demod import variants as demod_variants


def variants(path, source):
    parent = demod_variants(path, source)['both']
    start, end, fn = driver.function(parent, 'FPM_TONE_generate_demod')
    assert fn.count('\tint i;') == 1
    return {'baseline': parent,
            'short-counter': parent[:start] + fn.replace('\tint i;', '\tshort i;') + parent[end:]}


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-fpm-tone-demod-count'
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
