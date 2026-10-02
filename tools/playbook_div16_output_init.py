#!/usr/bin/env python3
"""Discriminate initialization ownership after the divisor-zero guard."""
import sys,subprocess
import playbook_small_patterns as driver
import playbook_div16_normalize as normalization

def variants(path,source):
    old=normalization.variants(path,source)['helper-word']
    assert old.count('\tunsigned short count = 0;')==1
    assert old.count('\tint n = 0;')==1
    text=old.replace('\tunsigned short count = 0;','\tunsigned short count;').replace('\tint n = 0;','\tint n = 0;\n\t*count = 0;')
    return {'baseline':source,'helper-word':old,'helper-initializes':text}

if __name__=='__main__':
    assert '--domain' in sys.argv
    assert sys.argv[sys.argv.index('--domain')+1].startswith('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-')
    driver.REV='a7dfe334'
    driver.OUT_NAME='playbook-div16-output-init'
    driver.SOURCE_PATHS=('src/dsp/fpm_div.c',)
    driver.variants=variants
    for path in driver.SOURCE_PATHS:
        variants(path,subprocess.check_output(['git','show',driver.REV+':'+path],cwd=driver.ROOT,text=True))
    driver.main()
