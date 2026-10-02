#!/usr/bin/env python3
"""Five full-TU divider denominator-lifetime/return-control cells."""
import playbook_small_patterns as driver
import playbook_getfp_divider as divider


def variants(path, source):
    seed=divider.variants(path,source)['loop-abs']
    cells={'baseline':source}
    for label,inside,branch in [('staged-loop',False,False),('inside-abs',True,False),('branch-return',False,True),('both',True,True)]:
        text=seed
        if inside:
            text=text.replace('\tint vb = abs((int)b);\n','').replace('\t\tremaining -= vb;', '\t\tremaining -= abs((int)b);')
        if branch:
            old='\treturn (short)(((int)a * (int)b) > 0 ? (short)count : (short)(-count));'
            assert text.count(old)==1
            text=text.replace(old,'\tif ((int)a * (int)b > 0)\n\t\treturn count;\n\treturn (short)-count;')
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==5
    return cells


if __name__=='__main__':
    driver.REV='4188d010'
    driver.OUT_NAME='playbook-getfp-lifetime'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
