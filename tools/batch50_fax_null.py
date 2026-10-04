#!/usr/bin/env python3
"""Signed count use and explicit initialization return ownership."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/faxvmi_null.c','src/fax/cDATArx.c');d.OUT_NAME='batch50-fax-null'
def variants(path,source):
 cells={'baseline':source}
 if 'null' in path:
  a,z,fn=d.function(source,'null_process')
  for signed,guard,label in [(True,False,'signed-use'),(False,True,'guarded-owner'),(True,True,'signed-guarded')]:
   f=fn
   if signed:f=f.replace('i < *count','i < (short)*count')
   if guard:
    f=f.replace('\tunsigned short *dst = dp->ptr_0000;\n','')
    old='\tfor (i = 0;';new='\tif ('+('(short)' if signed else '')+'*count > 0) {\n\t\tunsigned short *dst = dp->ptr_0000;\n\n'+old
    f=f.replace(old,new).replace('\n\treturn -1;', '\n\t}\n\treturn -1;')
   cells[label]=source[:a]+f+source[z:]
 else:
  a,z,fn=d.function(source,'_rx_look_carrier_init');old='\tctx->countdown = 0;\n\treturn 0;';assert old in fn
  cells['return-assignment']=source[:a]+fn.replace(old,'\treturn ctx->countdown = 0;')+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
