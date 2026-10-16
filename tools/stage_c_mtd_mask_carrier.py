#!/usr/bin/env python3
"""Test original pre-mask narrowing and post-guard comparison narrowing."""
import playbook_small_patterns as d
d.REV='3d869983'
d.SOURCE_PATHS=('src/dsp/fpm_mtd.c',)
d.OUT_NAME='stage-c-mtd-carrier'
d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 assert s.count('\tshort out_of_band;')==1
 assert s.count('if (out_of_band <= threshold)')==1
 assert s.count('(wideband >= out_of_band)')==1
 t=s.replace('\tshort out_of_band;','\tint out_of_band;').replace('if (out_of_band <= threshold)','if ((short)out_of_band <= threshold)').replace('(wideband >= out_of_band)','(wideband >= (short)out_of_band)')
 return {'baseline':s,'post-mask-int-carrier':t}
d.variants=variants
d.main()
