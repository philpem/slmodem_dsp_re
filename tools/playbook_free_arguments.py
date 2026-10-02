#!/usr/bin/env python3
"""Restore observed ignored free arguments, with isolated header overlays."""
from pathlib import Path
import re
import playbook_small_patterns as driver

SOURCE_PATHS = ('src/dsp/fpm_mrf.c','src/dsp/fpm_fsd.c','src/service/cidcore/cid.c','src/pump/v23/v23rx.c')


def variants(path,source):
    results = {}
    for label in ('baseline','mrf','fsd','both'):
        text = source
        for family in ('MRF','FSD'):
            if label not in (family.lower(),'both'):
                continue
            name = 'FPM_' + family + '_free'
            if path == 'src/dsp/fpm_' + family.lower() + '.c':
                old = name + '(struct fpm_' + family.lower() + ' *state)\n{'
                assert text.count(old) == 1
                text = text.replace(old,old.replace('*state)','*state, int unused)')+'\n\t(void)unused;')
            else:
                positions = list(re.finditer(r'(?m)^[ \t]+' + name + r'\(',text))
                for match in reversed(positions):
                    depth=1;end=match.end()
                    while depth:
                        if text[end] == '(': depth+=1
                        elif text[end] == ')': depth-=1
                        end+=1
                    value = 0 if path == 'src/pump/v23/v23rx.c' else 1
                    text = text[:end-1] + ', ' + str(value) + text[end-1:]
        results[label] = text
    return results


def overlays(path,label):
    result={}
    for family in ('MRF','FSD'):
        if label not in (family.lower(),'both'): continue
        rel = 'dsplib/fpm_' + family.lower() + '.h'
        text=(driver.ROOT/'include'/rel).read_text()
        old = 'void FPM_' + family + '_free(struct fpm_' + family.lower() + ' *state);'
        assert text.count(old) == 1
        result[rel] = text.replace(old,old.replace('*state);','*state, int unused);'))
    return result


if __name__ == '__main__':
    driver.REV = '97c06e4e'
    driver.OUT_NAME = 'playbook-free-arguments'
    driver.SOURCE_PATHS = SOURCE_PATHS
    driver.variants = variants
    driver.HEADER_OVERLAYS = overlays
    driver.main()
