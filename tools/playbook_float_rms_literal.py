#!/usr/bin/env python3
"""Two complete-TU controls for the floating RMS reciprocal literal type."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'fComputeRMSValueFloatBuf')
    old = 'return acc * (1.0f / (float)n);'
    assert fn.count(old) == 1
    return {'baseline': source,
            'double-literal': source[:start] + fn.replace(old, 'return acc * (1.0 / (float)n);') + source[end:]}


if __name__ == '__main__':
    driver.REV = 'd9a21365'
    driver.OUT_NAME = 'playbook-float-rms-literal'
    driver.SOURCE_PATHS = ('src/service/Beepgen.c',)
    driver.variants = variants
    driver.main()
