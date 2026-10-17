#!/usr/bin/env python3
"""Test the original IIR return-copy boundary with one short accumulator control."""
import playbook_small_patterns as d
d.REV='37a77c90';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.OUT_NAME='stage-c-iir-acc';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 old='\tint acc_in = x;';assert s.count(old)==1
 return {'baseline':s,'short-accumulator':s.replace(old,'\tshort acc_in = x;')}
d.variants=variants;d.main()
