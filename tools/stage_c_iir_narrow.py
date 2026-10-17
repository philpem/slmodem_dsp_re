#!/usr/bin/env python3
"""Test witnessed feedforward short conversion before the IIR clamp."""
import playbook_small_patterns as d
import stage_c_iir_cursor as cursor
d.REV='d127aec8';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.OUT_NAME='stage-c-iir-narrow';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 t=cursor.variants(path,s)['countdown-cursors'];old='\t\tff += (*coeff++ * w2) >> 14;';assert t.count(old)==1
 u=t.replace(old,old+'\n\t\tff = (short)ff;')
 return {'baseline':s,'countdown-cursors':t,'ff-before-clamp':u}
d.variants=variants;d.main()
