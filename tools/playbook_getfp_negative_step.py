#!/usr/bin/env python3
"""Four complete-TU negative-absolute-divisor controls."""
import playbook_small_patterns as driver
import playbook_getfp_lifetime as lifetime


def variants(path, source):
    seed=lifetime.variants(path,source)['both']
    cells={'baseline':source,'staged-positive':seed}
    assert seed.count('\t\tremaining -= abs((int)b);')==1
    cells['inside-negative']=seed.replace('\t\tremaining -= abs((int)b);','\t\tremaining += -abs((int)b);')
    cells['outside-negative']=seed.replace('\tshort count = 0;','\tshort count = 0;\n\tint step = -abs((int)b);').replace('\t\tremaining -= abs((int)b);','\t\tremaining += step;')
    assert len(cells)==len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-getfp-negative-step'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
