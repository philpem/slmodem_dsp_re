#!/usr/bin/env python3
"""Original single-precision coefficient slots, extended recurrence retained."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
    result={}
    for progress,metric in itertools.product((False,True),repeat=2):
        text=source
        for enabled,old,new in [(progress,'long double b0, b1, b2, b3;','float b0, b1, b2, b3;'),(metric,'long double a0, a1, a2, a3;','float a0, a1, a2, a3;')]:
            assert text.count(old)==1
            if enabled:text=text.replace(old,new)
        result['baseline' if not(progress or metric) else 'coeff-progress-%d-metric-%d'%(progress,metric)]=text
    return result
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-coefficients';d.SOURCE_PATHS=('src/pump/v90/V90SpectralShapingFilter.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
