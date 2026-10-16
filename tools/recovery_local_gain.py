#!/usr/bin/env python3
"""Replay the original GetGain private rounded-result lifetime, in its full TU."""
import playbook_small_patterns as driver

driver.REV = '14f852aa'
driver.SOURCE_PATHS = ('src/service/Beepgen.c',)
driver.OUT_NAME = 'recovery-local-gain'
driver.DUMP_FLAGS = ('-da', '-dP')


def variants(path, source):
    old = ('\t*gain2 = (float)(pow(10.0, (double)atten * -0.05 + base) * 0.276);\n'
           '\t*gain1 = (float)(pow(10.0, (double)-diff * 0.05) * *gain2);')
    new = ('\tfloat level = (float)(pow(10.0, (double)atten * -0.05 + base) * 0.276);\n'
           '\t*gain2 = level;\n'
           '\t*gain1 = (float)(pow(10.0, (double)-diff * 0.05) * level);')
    assert path == driver.SOURCE_PATHS[0] and source.count(old) == 1
    candidate = source.replace(old, new)
    assert candidate != source
    return {'baseline': source, 'private-float-result': candidate}


driver.variants = variants
if __name__ == '__main__':
    driver.main()
