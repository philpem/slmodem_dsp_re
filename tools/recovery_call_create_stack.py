#!/usr/bin/env python3
"""One allocation-order control backed by original stack object positions."""
import playbook_small_patterns as d

def variants(path, source):
    old='\tstruct callprog_cfg cfg;\n\tchar dialstr[64];'
    assert source.count(old)==1
    return {'baseline':source,'buffer-first':source.replace(old,'\tchar dialstr[64];\n\tstruct callprog_cfg cfg;')}

if __name__=='__main__':
    d.REV='e0e56d23'
    d.SOURCE_PATHS=('src/call/call.c',)
    d.OUT_NAME='recovery-call-create-stack'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
    d.variants=variants
    d.main()
