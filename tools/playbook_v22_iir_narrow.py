#!/usr/bin/env python3
"""Two complete-TU shared temporary/use-site narrowing controls."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'V22_iir_filt_demod')
    old = '\t\ty = (short)acc;\n\t\tyhist[0] = y;\n\n\t\tsamples[i] = (short)((y * mix[i]) >> 12);'
    assert fn.count(old) == 1 and fn.count('\t\tshort y;\n') == 1
    changed = fn.replace('\t\tshort y;\n', '')
    changed = changed.replace(old, '\t\tyhist[0] = acc;\n\n\t\tsamples[i] = (short)(((short)acc * mix[i]) >> 12);')
    return {'baseline': source,
            'narrow-at-uses': source[:start] + changed + source[end:]}


if __name__ == '__main__':
    driver.REV = '5ec53f63'
    driver.OUT_NAME = 'playbook-v22-iir-narrow'
    driver.SOURCE_PATHS = ('src/pump/v22/v22_iir.c',)
    driver.variants = variants
    driver.main()
