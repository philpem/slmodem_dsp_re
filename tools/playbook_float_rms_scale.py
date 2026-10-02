#!/usr/bin/env python3
"""Three complete-TU controls for a typed floating RMS reciprocal local."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, fn = driver.function(source, 'fComputeRMSValueFloatBuf')
    old = 'return acc * (1.0f / (float)n);'
    mean = '\tmean = sum / (float)n;\n'
    assert fn.count(old) == fn.count(mean) == 1
    cells = {'baseline': source}
    for label, numerator in [('float-scale', '1.0f'), ('double-scale', '1.0')]:
        changed = fn.replace('float sum = 0.0f, acc = 0.0f, mean;',
                             'float sum = 0.0f, acc = 0.0f, mean, inv;')
        changed = changed.replace(mean, mean + '\tinv = ' + numerator + ' / (float)n;\n')
        changed = changed.replace(old, 'return acc * inv;')
        cells[label] = source[:start] + changed + source[end:]
    return cells


if __name__ == '__main__':
    driver.REV = 'd9a21365'
    driver.OUT_NAME = 'playbook-float-rms-scale'
    driver.SOURCE_PATHS = ('src/service/Beepgen.c',)
    driver.variants = variants
    driver.main()
