#!/usr/bin/env python3
"""Four full-TU production/staged/index-storage TONE_read controls."""
import playbook_small_patterns as driver
import playbook_tone_read as width


def variants(path, source):
    seed=width.variants(path,source)['p1-i1-b1']
    cells={'baseline':source,'staged-inline':seed}
    for label,local in [('phase-storage',False),('index-local',True)]:
        text=seed
        for condition,expression,negative in [('p >= 0x201 && p <= 0x400','0x400 - p',True),('p >= 0x401 && p <= 0x600','p - 0x400',True),('p >= 0x601 && p <= 0x7ff','0x800 - p',False)]:
            result='(short)-FPTONE' if negative else 'FPTONE'
            old='\tif ('+condition+')\n\t\treturn '+result+'[(short)('+expression+')];'
            assert text.count(old)==1
            assignment='short index = '+expression+';' if local else 'p = '+expression+';'
            index='index' if local else 'p'
            replacement='\tif ('+condition+') {\n\t\t'+assignment+'\n\t\treturn '+result+'['+index+'];\n\t}'
            text=text.replace(old,replacement)
        cells[label]=text
    assert len(cells)==len(set(cells.values()))==4
    return cells


if __name__=='__main__':
    driver.REV='599f0fa4'
    driver.OUT_NAME='playbook-tone-read-storage'
    driver.SOURCE_PATHS=('src/dsp/FP_math.c',)
    driver.variants=variants
    driver.main()
