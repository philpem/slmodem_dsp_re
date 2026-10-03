#!/usr/bin/env python3
"""Replay issue246's four-cell energy-update/quotient-width source cross."""
import sys
import playbook_small_patterns as driver


def variants(path, source):
    start, end, original = driver.function(source, 'updateAlpha')
    old = '\t\tr = (short)((1 << (shift + 0x15))\n\t\t\t    / ((energy + 0x8000) >> 16));'
    assert original.count(old) == 1
    assert original.count('\t\tint r;') == 1
    changed = original.replace(old, '\t\tenergy += 0x8000;\n\t\tenergy >>= 16;\n\t\tr = (short)((1 << (shift + 0x15)) / energy);')
    forms = {'baseline': original,
             'short-quotient': original.replace('\t\tint r;', '\t\tshort r;'),
             'energy-update': changed,
             'energy-update-short': changed.replace('\t\tint r;', '\t\tshort r;')}
    result = {label: source[:start] + text + source[end:] for label, text in forms.items()}
    assert len(set(result.values())) == 4
    return result


if __name__ == '__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/246#issuecomment-')
    driver.REV = '47174bf3'
    driver.OUT_NAME = 'gcc3-alpha-energy'
    driver.SOURCE_PATHS = ('src/pump/v34/V34TX.c',)
    driver.variants = variants
    driver.main()
