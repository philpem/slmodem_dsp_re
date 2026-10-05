#!/usr/bin/env python3
"""Transfer explained output declaration storage to the sqrt normalizer."""
import playbook_small_patterns as d
import gcc3_normalization_loop_reproduce as prior

def variants(path,source):
    control=prior.variants(path,source)['word-output-while']
    old="\tunsigned short exponent;\n\tunsigned short mantissa;"
    assert control.count(old)==1
    pair=control.replace(old,"\tunsigned short mantissa;\n\tunsigned short exponent;")
    return {'baseline':source,'pointed-control':control,'pointed-sibling-outputs':pair}

if __name__=='__main__':
    d.REV='f7b95e35';d.OUT_NAME='gcc3-output-storage-sqrt';d.SOURCE_PATHS=('src/dsp/fpm_sqrt.c',)
    d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
