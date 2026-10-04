#!/usr/bin/env python3
"""Place reciprocal count initialization at its observed helper boundary."""
import playbook_small_patterns as d
import gcc3_normalization_reciprocal_reproduce as reciprocal

def variants(path,source):
    prior=reciprocal.variants(path,source)['loop-carried-word-output']
    assert prior.count('unsigned short count = 0;')==1
    changed=prior.replace('unsigned short count = 0;','unsigned short count;')
    marker='normalize16(unsigned short denom, unsigned short *mantissa, unsigned short *count)\n{\n'
    assert changed.count(marker)==1
    changed=changed.replace(marker,marker+'\t*count = 0;\n')
    return {'baseline':source,'early-init-control':prior,'helper-owned-init':changed}

if __name__=='__main__':
    d.REV='b470429e';d.SOURCE_PATHS=('src/dsp/fpm_div.c',);d.OUT_NAME='gcc3-normalization-guard'
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
