#!/usr/bin/env python3
"""Cross independently witnessed coefficient and input-sample ownership."""
import playbook_small_patterns as d

def variants(path,source):
    text=source
    for old,new in [('long double b0, b1, b2, b3;','float b0, b1, b2, b3;'),('long double a0, a1, a2, a3;','float a0, a1, a2, a3;'),('long double sample = *in++;','short sample = *in++;'),('long double v = *in++;','short v = *in++;')]:
        assert text.count(old)==1;text=text.replace(old,new)
    return {'baseline':source,'coeff-sample-cross':text}
if __name__=='__main__':
    d.REV='8af3af53';d.OUT_NAME='next20-v90-filter-cross';d.SOURCE_PATHS=('src/pump/v90/V90SpectralShapingFilter.cpp',)
    d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
