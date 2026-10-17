#!/usr/bin/env python3
"""Test SI library abs and original denominator work under numerator guard."""
import playbook_small_patterns as d
import stage_c_getfp_loop as fp
d.REV='acfa905a';d.SOURCE_PATHS=('src/dsp/FP_math.c',);d.OUT_NAME='stage-c-getfp-abs';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 t=fp.variants(path,s)['negative-add'].replace('int va = a < 0 ? -a : a;','int va = abs((int)a);').replace('int vb = b < 0 ? -b : b;','int vb = abs((int)b);')
 t=t.replace('#include "dsplib/fp_math.h"','#include <stdlib.h>\n\n#include "dsplib/fp_math.h"')
 u=t.replace('\tint vb = abs((int)b);\n','').replace('int step = -vb;','int step = -abs((int)b);')
 assert u!=t
 return {'baseline':s,'library-abs':t,'guarded-negative-abs':u}
d.variants=variants;d.main()
