#!/usr/bin/env python3
"""Three full-TU controls for TxHdxTRN's low-two-bit input fold."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'TxHdxTRN')
    old = 'trn[data[i] & 3]'
    assert fn.count(old) == 1
    forms = {'baseline': old,
             'unsigned-word': 'trn[(unsigned short)data[i] & 3]',
             'unsigned-mask': 'trn[data[i] & 3u]'}
    return {label: source[:start] + fn.replace(old, form) + source[end:]
            for label, form in forms.items()}


if __name__ == '__main__':
    driver.REV = 'f206063c'
    driver.OUT_NAME = 'playbook-txhdxtrn'
    driver.SOURCE_PATHS = ('src/pump/v32/V32TXHDX.c',)
    driver.variants = variants
    driver.main()
