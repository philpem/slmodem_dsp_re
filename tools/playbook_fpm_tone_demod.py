#!/usr/bin/env python3
"""Four full-TU demod-generator scale-read/output-initialization controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_generate_demod')
    cells = {}
    for label, scale, clear in [('baseline', False, False),
                                ('direct-scale', True, False),
                                ('output-lifetime', False, True),
                                ('both', True, True)]:
        text = fn
        if scale:
            assert text.count('\tint scale = state->cfg.scale;\n') == 1
            assert text.count('scale * p.cos') == 1
            text = text.replace('\tint scale = state->cfg.scale;\n', '')
            text = text.replace('scale * p.cos', 'state->cfg.scale * p.cos')
        if clear:
            assert text.count('\tp.cos = p.sin = 0;\n') == 1
            text = text.replace('\tp.cos = p.sin = 0;\n', '')
        cells[label] = source[:start] + text + source[end:]
    assert len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '9cb6d002'
    driver.OUT_NAME = 'playbook-fpm-tone-demod'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
