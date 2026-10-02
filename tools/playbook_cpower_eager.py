#!/usr/bin/env python3
"""Two complete-TU controls for the power ladder's eager Boolean latch."""
import playbook_small_patterns as driver


def variants(path, source):
    start, end, body = driver.function(source, 'V90ConstellationPower::getPowerIndexForPower')
    old = '\twhile (index != 0 && averagePowerLimits[index] < power)'
    assert body.count(old) == 1
    candidate = body.replace(old, '\twhile ((index != 0) & (averagePowerLimits[index] < power))')
    cells = {'baseline': source,
             'eager-latch': source[:start] + candidate + source[end:]}
    assert len(set(cells.values())) == 2
    return cells


if __name__ == '__main__':
    driver.REV = 'fc8641f1'
    driver.OUT_NAME = 'playbook-cpower-eager'
    driver.DUMP_FLAGS = ()
    driver.SOURCE_PATHS = ('src/pump/v90/V90ConstellationPower.cpp',)
    driver.variants = variants
    driver.main()
