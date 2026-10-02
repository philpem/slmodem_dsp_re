#!/usr/bin/env python3
"""Eight staged full-TU period-lifetime/elapsed-width/predicate controls."""
from pathlib import Path
import playbook_small_patterns as driver
from playbook_fpm_tone_sine import variants as generation_variants


def variants(path, source):
    source = generation_variants(path, source)['scale-lifetime-loop']
    start, end, fn = driver.function(source, 'FPM_TONE_generate')
    cells = {}
    for late in (False, True):
        for narrow in (False, True):
            for order in (False, True):
                text = fn
                if late:
                    old = '\tint period = state->cfg.rev_period;'
                    assert text.count(old) == 1
                    text = text.replace(old, '\tint period;')
                    text = text.replace('\telapsed = state->rev_count', '\tperiod = state->cfg.rev_period;\n\telapsed = state->rev_count')
                if narrow:
                    assert text.count('\tint elapsed;') == 1
                    text = text.replace('\tint elapsed;', '\tshort elapsed;')
                if order:
                    old = 'if (period > 0 && period <= elapsed)'
                    assert text.count(old) == 1
                    text = text.replace(old, 'if (period <= elapsed && period > 0)')
                label = '-'.join(x for x, on in [('late', late), ('short', narrow), ('reference-order', order)] if on) or 'baseline'
                cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    driver.REV = '54bf1179'
    driver.OUT_NAME = 'playbook-fpm-tone-sine-reversal'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    previous = driver.ROOT/'build/playbook-fpm-tone-sine/fpm_tone/scale-lifetime-loop/candidate.o'
    saved = driver.ROOT/'build'/driver.OUT_NAME/'fpm_tone/retained.o'
    saved.parent.mkdir(parents=True, exist_ok=True)
    if saved.exists():
        assert saved.read_bytes() == previous.read_bytes()
    else:
        saved.write_bytes(previous.read_bytes())
    driver.main()
