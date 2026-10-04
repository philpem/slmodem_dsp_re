#!/usr/bin/env python3
"""Two-cell original whole-config sampling boundary in V22 FSE init."""
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v22/v22_fse.c',);d.OUT_NAME='batch20-v22-fse-copy'
def variants(path,source):
 a,b,fn=d.function(source,'V22_FSE_init');assert fn.count('\tshort i;')==1
 fn=fn.replace('\tshort i;','\tstruct v22_fse_cfg copied = *cfg;\n\tshort i;')
 for field in ['icoff','qcoff']:
  needle='state->'+field+' = cfg->'+field;assert fn.count(needle)==1;fn=fn.replace(needle,'state->'+field+' = copied.'+field)
 return {'baseline':source,'copy':source[:a]+fn+source[b:]}
d.variants=variants
if __name__=='__main__':d.main()
