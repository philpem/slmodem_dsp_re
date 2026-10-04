#!/usr/bin/env python3
"""Single independent TCM previous-transition word comparison control."""
import playbook_small_patterns as d
from next20_dsp_smc_mask import variants as seed

def variants(path,source):
 control=seed(path,source)['mask1-cursor1-wide1']
 a,z,fn=d.function(control,'SMCv17_encoder_tcm')
 assert fn.count('\tint prev = statep->prev;')==1
 fn=fn.replace('\tint prev = statep->prev;','\tshort prev = statep->prev;')
 return {'baseline':source,'prior-control':control,'short-prev':control[:a]+fn+control[z:]}

if __name__=='__main__':
 d.REV='8af3af53';d.SOURCE_PATHS=('src/fax/Smc.c',);d.OUT_NAME='next20-dsp-smc-prev';d.DUMP_FLAGS=('-v','-save-temps','-da');d.variants=variants;d.main()
