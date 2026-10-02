#!/usr/bin/env python3
"""Shared constant verdict versus initial-RTL ternary boolean lowering."""
import playbook_small_patterns as driver
import playbook_mtd_postdec as loop


def variants(path, source):
    control = loop.variants(path, source)['postdecrement-words']
    start, end, body = driver.function(control, 'FPM_MTD_detect')
    old = '''\tif (wideband < state->cfg.min_level)
\t\treturn FPM_MTD_NOSIGNAL;
'''
    assert body.count(old) == 1
    text = body.replace('\tint threshold;', '\tint threshold;\n\tshort verdict = FPM_MTD_NOSIGNAL;')
    text = text.replace(old, '\tif (wideband >= state->cfg.min_level) {\n')
    old_tail = '''\tif (out_of_band <= threshold)
\t\treturn FPM_MTD_PRESENT;

\treturn (wideband >= out_of_band) ? FPM_MTD_ABSENT : FPM_MTD_PRESENT;'''
    new_tail = '''\tif (out_of_band <= threshold)
\t\tverdict = FPM_MTD_PRESENT;
\telse if (wideband >= out_of_band)
\t\tverdict = FPM_MTD_ABSENT;
\telse
\t\tverdict = FPM_MTD_PRESENT;
\t}
\treturn verdict;'''
    assert text.count(old_tail) == 1
    text = text.replace(old_tail, new_tail)
    results = {'baseline': source, 'postdecrement-words': control,
               'shared-verdict': control[:start] + text + control[end:]}
    assert len(results) == len(set(results.values())) == 3
    return results


if __name__ == '__main__':
    driver.REV = '43ef6841'
    driver.OUT_NAME = 'playbook-mtd-result'
    driver.SOURCE_PATHS = ('src/dsp/fpm_mtd.c',)
    driver.variants = variants
    driver.main()
