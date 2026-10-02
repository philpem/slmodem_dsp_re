#!/usr/bin/env python3
"""Eight full-TU sine-generator scale/output-lifetime/traversal controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_generate')
    cells = {}
    for scale in (False, True):
        for lifetime in (False, True):
            for loop in (False, True):
                text = fn
                if scale:
                    assert text.count('\tint scale = state->cfg.scale;\n') == 1
                    text = text.replace('\tint scale = state->cfg.scale;\n', '')
                    text = text.replace('scale * p.sin', 'state->cfg.scale * p.sin')
                if lifetime:
                    assert text.count('\tp.cos = p.sin = 0;\n') == 1
                    text = text.replace('\tp.cos = p.sin = 0;\n', '')
                if loop:
                    assert text.count('\tint i;') == 1
                    text = text.replace('\tint i;', '\tshort i;')
                    assert text.count('for (i = 0; i < count; i++)') == 1
                    text = text.replace('for (i = 0; i < count; i++)', 'for (i = count; i-- != 0;)')
                    text = text.replace('out[i] =', '*out++ =')
                label = '-'.join(x for x, on in [('scale', scale), ('lifetime', lifetime), ('loop', loop)] if on) or 'baseline'
                cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 8
    return cells


if __name__ == '__main__':
    driver.REV = '54bf1179'
    driver.OUT_NAME = 'playbook-fpm-tone-sine'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
