#!/usr/bin/env python3
"""Twelve direct-period counter/predicate/private-phase lifetime controls."""
import playbook_small_patterns as driver
from playbook_fpm_tone_sine_guards import variants as guard_variants


def variants(path, source):
    parents = guard_variants(path, source)
    cells = {}
    for carrier, parent_name in [('short-local', 'baseline'), ('int-use', 'int-use-conjunction'), ('owner-field', 'owner-field-conjunction')]:
        parent = parents[parent_name]
        start, end, fn = driver.function(parent, 'FPM_TONE_generate')
        value = {'short-local': 'elapsed', 'int-use': '(short)elapsed', 'owner-field': '(short)state->rev_count'}[carrier]
        for order in (False, True):
            for after in (False, True):
                text = fn
                if order:
                    old = 'state->cfg.rev_period > 0 && state->cfg.rev_period <= ' + value
                    assert text.count(old) == 1
                    text = text.replace(old, 'state->cfg.rev_period <= ' + value + ' && state->cfg.rev_period > 0')
                if after:
                    old = '\t\tint phase = (short)p.phase;'
                    assert text.count(old) == 1
                    text = text.replace(old, '\t\tint phase;')
                    old = '\t\tstate->rev_count = 0;'
                    assert text.count(old) == 1
                    text = text.replace(old, old + '\n\t\tphase = (short)p.phase;')
                label = carrier + ('-reference-order' if order else '-positive-first') + ('-capture-after' if after else '-capture-before')
                if carrier == 'short-local' and not order and not after:
                    label = 'baseline'
                cells[label] = parent[:start] + text + parent[end:]
    assert len(cells) == len(set(cells.values())) == 12
    return cells


if __name__ == '__main__':
    driver.REV = '54bf1179'
    driver.OUT_NAME = 'playbook-fpm-tone-sine-order'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    previous = driver.ROOT/'build/playbook-fpm-tone-sine-guards/fpm_tone/baseline/candidate.o'
    saved = driver.ROOT/'build'/driver.OUT_NAME/'fpm_tone/retained.o'
    saved.parent.mkdir(parents=True, exist_ok=True)
    if saved.exists():
        assert saved.read_bytes() == previous.read_bytes()
    else:
        saved.write_bytes(previous.read_bytes())
    driver.main()
