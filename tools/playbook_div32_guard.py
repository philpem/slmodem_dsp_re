#!/usr/bin/env python3
"""One staged guarded normalization helper and complete-TU controls."""
import playbook_small_patterns as driver
import playbook_div32_normalize as prior


def variants(path, source):
    cells = prior.variants(path, source)
    old = "\tint n = 0;\n\t*count = 0;\n\twhile ((int)denom >= 0) {\n\t\tdenom += denom;\n\t\tn++;\n\t}\n\t*count = (unsigned short)n;"
    new = "\t*count = 0;\n\tif ((int)denom >= 0) {\n\t\tint n = 0;\n\t\tdo {\n\t\t\tdenom += denom;\n\t\t\tn++;\n\t\t} while ((int)denom >= 0);\n\t\t*count = (unsigned short)n;\n\t}"
    helper = cells['helper-word-index']
    assert helper.count(old) == 1
    results = {'baseline': source, 'helper-word-index': helper, 'guarded-helper-word-index': helper.replace(old,new)}
    assert len(set(results.values())) == 3
    return results


if __name__ == '__main__':
    driver.REV = '397211df'
    driver.OUT_NAME = 'playbook-div32-guard'
    driver.SOURCE_PATHS = ('src/dsp/fpm_div32.c',)
    driver.variants = variants
    driver.main()
