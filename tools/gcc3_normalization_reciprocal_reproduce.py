#!/usr/bin/env python3
"""Transfer observed loop-memory promotion to both reciprocal helpers."""
import playbook_small_patterns as d
import playbook_div16_normalize as div16
import playbook_div32_normalize as div32

def variants(path,source):
    parent=(div32.variants(path,source)['helper-word-index'] if 'div32' in path else div16.variants(path,source)['helper-word'])
    assert parent.count('\tint n = 0;')==parent.count('n++;')==1
    pointed=parent.replace('\tint n = 0;\n','').replace('n++;','(*count)++;')
    assert pointed.count('*count = (unsigned short)n;')==1
    pointed=pointed.replace('\t\t*count = (unsigned short)n;\n','').replace('\t*count = (unsigned short)n;\n','')
    return {'baseline':source,'terminal-publication-control':parent,'loop-carried-word-output':pointed}

if __name__=='__main__':
    d.REV='b470429e';d.SOURCE_PATHS=('src/dsp/fpm_div.c','src/dsp/fpm_div32.c')
    d.OUT_NAME='gcc3-normalization-reciprocal';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
