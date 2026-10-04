#!/usr/bin/env python3
import re
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
d.OUT_NAME='gcc3-batch50-v90-vector-direct'
def cycle(text):
 for helper in ('sdSymbol','sdNotSymbol'):
  pat=r'static short\n'+helper+r'\([^)]*\)\n\{.*?\n\}';m=re.search(pat,text,re.S);assert m
  fn=m.group().replace('static short','static int',1).replace('\n{\n','\n{\n\tint sample;\n',1)
  fn=re.sub(r'\t\treturn ([^;]+);',r'\t\tsample = \1;\n\t\tbreak;',fn);fn=re.sub(r'\treturn 0;[^\n]*','\treturn sample;',fn)
  text=text[:m.start()]+fn+text[m.end():]
 return text
def variants(path,source):
 cells={'baseline':source}
 vectors={'generateJd':'jdBits','generateJdPhase':'jdV92PhaseBits','generateV92Jd':'jdV92Bits'}
 for label,names in [('jd-direct',('generateJd',)),('vectors-direct',tuple(vectors)),('all-leaves-direct',tuple(vectors)+('generateJdNot',))]:
  text=source
  for name in names:
   start,end,fn=d.function(text,'V90Phase3Modulator::'+name)
   bit='0' if name=='generateJdNot' else vectors[name]+'[(symbolCount - 1u) % 72u]'
   fn=fn[:fn.index('\n{')]+ '\n{\n\tpolarity ^= scrambler.process('+bit+');\n\treturn polarity ? codeLevel : (short)-codeLevel;\n}'
   text=text[:start]+fn+text[end:]
  cells[label]=text;cells[label+'-cycle']=cycle(text)
 return cells
d.variants=variants
if __name__=='__main__':d.main()
