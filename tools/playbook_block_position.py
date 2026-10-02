#!/usr/bin/env python3
"""Promoted block-update position carrier with word narrowing at index use."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'FPM_block_update')
    old = '\t\tshort pos = (short)(widx - i);'
    assert body.count(old) == 1
    text = body.replace(old, '\t\tint pos = widx - i;')
    old = '\t\t\tshort idx = (short)(pos < 0 ? (short)(pos + hlen) : pos);'
    assert text.count(old) == 1
    text = text.replace(old, '\t\t\tshort idx = (short)pos;\n\n\t\t\tif (idx < 0)\n\t\t\t\tidx = (short)(idx + hlen);')
    assert text.count('pos = (short)(pos - step);') == 1
    text = text.replace('pos = (short)(pos - step);', 'pos -= step;')
    results = {'baseline':source, 'promoted-position':source[:start] + text + source[end:]}
    assert len(set(results.values())) == 2
    return results


if __name__ == '__main__':
    driver.REV = '397211df'
    driver.OUT_NAME = 'playbook-block-position'
    driver.SOURCE_PATHS = ('src/dsp/fpm_adeq.c',)
    driver.variants = variants
    driver.main()
