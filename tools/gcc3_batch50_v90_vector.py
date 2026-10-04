#!/usr/bin/env python3
import re
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
d.OUT_NAME='gcc3-batch50-v90-vector'
def variants(path,source):
 cells={'baseline':source}
 for label,allcalls in [('public-direct',False),('all-direct',True)]:
  text=source
  if allcalls:
   text,n=re.subn(r'vectorBit\((jdBits|jdV92Bits|jdV92PhaseBits), symbolCount\)',r'\1[(symbolCount - 1u) % 72u]',text);assert n==9,n
  else:
   for name in ('generateJd','generateJdPhase','generateV92Jd'):
    start,end,fn=d.function(text,'V90Phase3Modulator::'+name)
    fn,n=re.subn(r'vectorBit\((jdBits|jdV92Bits|jdV92PhaseBits), symbolCount\)',r'\1[(symbolCount - 1u) % 72u]',fn);assert n==1
    text=text[:start]+fn+text[end:]
  cells[label]=text
  for helper in ('sdSymbol','sdNotSymbol'):
   pat=r'static short\n'+helper+r'\([^)]*\)\n\{.*?\n\}'
   m=re.search(pat,text,re.S);assert m
   fn=m.group().replace('static short','static int',1).replace('\n{\n','\n{\n\tint sample;\n',1)
   fn=re.sub(r'\t\treturn ([^;]+);',r'\t\tsample = \1;\n\t\tbreak;',fn)
   fn=re.sub(r'\treturn 0;[^\n]*','\treturn sample;',fn)
   text=text[:m.start()]+fn+text[m.end():]
  cells[label+'-cycle']=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
