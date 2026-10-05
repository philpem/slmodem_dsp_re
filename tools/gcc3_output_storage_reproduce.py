#!/usr/bin/env python3
"""Trace sibling-supported normalization output declarations, not late registers."""
import playbook_small_patterns as d
import gcc3_normalization_reciprocal_reproduce as prior

def paired(text):
    old="\tunsigned short count;\n\tunsigned short mantissa;"
    assert text.count(old)==1
    return text.replace(old,"\tunsigned short mantissa;\n\tunsigned short count;")

def variants(path,source):
    control=prior.variants(path,source)['loop-carried-word-output']
    retained_old="\tunsigned short count = 0;\n\tunsigned short mantissa;"
    assert source.count(retained_old)==1
    baseline_pair=source.replace(retained_old,"\tunsigned short mantissa;\n\tunsigned short count = 0;")
    supported=paired(control)
    grouped=supported.replace("\tunsigned short mantissa;\n\tunsigned short count;",
                              "\tunsigned short mantissa, count;")
    cells={'baseline':source,'pointed-control':control,'baseline-sibling-outputs':baseline_pair,
           'pointed-sibling-outputs':supported,'pointed-grouped-outputs':grouped}
    assert len(cells)==len(set(cells.values()))==5
    return cells

if __name__=='__main__':
    d.REV='f7b95e35';d.OUT_NAME='gcc3-output-storage';d.SOURCE_PATHS=('src/dsp/fpm_div32.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
