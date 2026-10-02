#!/usr/bin/env python3
"""Compare conventional power-first Boolean control with eager integer AND."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'V90ConstellationPower::getPowerIndexForPower')
    old = '\twhile (index != 0 && averagePowerLimits[index] < power)'
    assert body.count(old) == 1
    cells = {'baseline': source}
    for label, condition in (
        ('power-first-bitwise', '(averagePowerLimits[index] < power) & (index != 0)'),
        ('power-first-logical', 'averagePowerLimits[index] < power && index != 0')):
        fn = body.replace(old, '\twhile (' + condition + ')')
        cells[label] = source[:start] + fn + source[end:]
    assert len(cells) == len(set(cells.values())) == 3
    return cells


if __name__ == '__main__':
    driver.REV = 'fc8641f1'
    driver.OUT_NAME = 'playbook-cpower-logical'
    driver.DUMP_FLAGS = ()
    driver.SOURCE_PATHS = ('src/pump/v90/V90ConstellationPower.cpp',)
    driver.variants = variants
    driver.main()
