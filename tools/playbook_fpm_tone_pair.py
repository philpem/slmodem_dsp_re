#!/usr/bin/env python3
"""Four complete-TU quadrature output-lifetime/post-decrement controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'FPM_TONE_generate2')
    cells = {}
    for label, clear, loop in [('baseline', False, False),
                                ('output-lifetime', True, False),
                                ('post-decrement', False, True),
                                ('both', True, True)]:
        text = fn
        if clear:
            assert text.count('\tp.cos = p.sin = 0;\n') == 1
            text = text.replace('\tp.cos = p.sin = 0;\n', '')
        if loop:
            assert text.count('\tint i;') == 1
            text = text.replace('\tint i;', '\tshort i;')
            old = 'for (i = (short)(count - 1); i != -1; i = (short)(i - 1))'
            assert text.count(old) == 1
            text = text.replace(old, 'for (i = count; i-- != 0;)')
        cells[label] = source[:start] + text + source[end:]
    assert len(cells) == len(set(cells.values())) == 4
    return cells


if __name__ == '__main__':
    driver.REV = '1dc5210d'
    driver.OUT_NAME = 'playbook-fpm-tone-pair'
    driver.SOURCE_PATHS = ('src/dsp/fpm_tone.c',)
    driver.variants = variants
    driver.main()
