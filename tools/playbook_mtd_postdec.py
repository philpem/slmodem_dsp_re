#!/usr/bin/env python3
"""Two staged postdecrement controls and unchanged complete-TU baseline."""
import playbook_small_patterns as driver
import playbook_mtd_detect as energy


def variants(path, source):
    cells = energy.variants(path, source)
    results = {'baseline': source}
    for label in ('countdown', 'countdown-words'):
        old = 'for (i = (short)(count - 1); i != -1; i--)'
        assert cells[label].count(old) == 1
        results[label.replace('countdown', 'postdecrement')] = cells[label].replace(old, 'for (i = count; i-- != 0;)')
    assert len(results) == len(set(results.values())) == 3
    return results


if __name__ == '__main__':
    driver.REV = '43ef6841'
    driver.OUT_NAME = 'playbook-mtd-postdec'
    driver.SOURCE_PATHS = ('src/dsp/fpm_mtd.c',)
    driver.variants = variants
    driver.main()
