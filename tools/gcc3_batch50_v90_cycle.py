#!/usr/bin/env python3
import re
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp','src/pump/v90/V92Phase3Modulator.cpp')
d.OUT_NAME='gcc3-batch50-v90-cycle'
def variants(path,source):
 names=('sdSymbol','sdNotSymbol') if 'V90Phase3' in path else ('ruSymbol','ruNotSymbol','suSymbol','suNotSymbol')
 def cell(mode):
  text=source
  for name in names:
   pattern=r'static short\n'+name+r'\([^)]*\)\n\{.*?\n\}'
   match=re.search(pattern,text,re.S);assert match,name
   fn=match.group().replace('static short','static int',1)
   if mode!='width-only':
    fn=fn.replace('\n{\n','\n{\n\tint sample'+(' = 0' if mode=='initialized-common' else '')+';\n',1)
    fn=re.sub(r'\t\treturn ([^;]+);',r'\t\tsample = \1;\n\t\tbreak;',fn)
    fn=re.sub(r'\treturn 0;[^\n]*', '\treturn sample;',fn)
   text=text[:match.start()]+fn+text[match.end():]
  return text
 return {'baseline':source,**{m:cell(m) for m in ('width-only','initialized-common','covered-common')}}
d.variants=variants
if __name__=='__main__':d.main()
