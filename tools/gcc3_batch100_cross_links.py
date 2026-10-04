#!/usr/bin/env python3
"""Cross original output cursor ownership and consumed conversion count."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'CrossDataLinks')
 old='\tfor (i = 0; i < n; i++)\n\t\tlin_out[i] = (short)(flt_in[i] * 32000.0f);'
 assert old in fn
 for cursor,consume in itertools.product((False,True),repeat=2):
  if not(cursor or consume):continue
  if consume:
   loop='\twhile (n > 0) {\n\t\tSTORE;\n\t\t--n;\n\t}'
   if not cursor:loop='\ti = 0;\n'+loop.replace('\t\t--n;','\t\t++i;\n\t\t--n;')
  else:loop='\tfor (i = 0; i < n; i++)\n\t\tSTORE;'
  loop=loop.replace('STORE','*lin_out++ = (short)(*flt_in++ * 32000.0f)' if cursor else 'lin_out[i] = (short)(flt_in[i] * 32000.0f)')
  cells['cursor-%d-consume-%d'%(cursor,consume)]=source[:start]+fn.replace(old,loop)+source[end:]
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Beepgen.c',);d.OUT_NAME='gcc3-batch100-cross-links';d.variants=variants;d.main()
