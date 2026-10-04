#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/Smc.c',);d.OUT_NAME='gcc3-batch100-fax-smc-traversal-wrap'
def variants(path,source):
 cells={'baseline':source}
 for traversal,wrap,label in [(True,False,'counted-cursor'),(False,True,'conditional-clear'),(True,True,'counted-cursor-conditional-clear')]:
  text=source
  if wrap:
   old='return (short)((next < len) ? next : 0);';assert text.count(old)==1;text=text.replace(old,'if (next >= len)\n\t\tnext = 0;\n\treturn next;')
  if traversal:
   for name in ('SMCv17_encoder_dif','SMCv17_encoder_abs','SMCv17_encoder_tcm'):
    start,end,fn=d.function(text,name)
    old='\tunsigned int i;\n';assert fn.count(old)==1;fn=fn.replace(old,'')
    old='for (i = 0; i < count; i++) {';assert fn.count(old)==1;fn=fn.replace(old,'while (count--) {')
    if name.endswith('tcm'):
     fn=fn.replace('data[i]','*data');marker='\t\twidx = smc_ring_advance(widx, len);';assert fn.count(marker)==1;fn=fn.replace(marker,'\t\tdata++;\n'+marker)
    else:
     old='(unsigned int)data[i]';assert fn.count(old)==1;fn=fn.replace(old,'(unsigned int)*data++')
    text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
