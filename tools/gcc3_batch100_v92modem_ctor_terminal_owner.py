#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V92Modem.cpp',);d.OUT_NAME='gcc3-batch100-v92modem-ctor-terminal-owner'
def variants(path,source):
 start,end,fn=d.function(source,'V92Modem::V92Modem');cells={'baseline':source}
 for terminal,owner,label in [(1,0,'terminal-return'),(0,1,'descriptor-parameter'),(1,1,'terminal-parameter')]:
  text=fn
  if terminal:
   mark='\t\tbreak;\n\t}\n}';assert text.count(mark)==1;text=text.replace(mark,'\t\treturn;\n\t}\n}')
  if owner:
   mark='(V92Ja *)ja, dil,';assert text.count(mark)==1;text=text.replace(mark,'(V92Ja *)ja, dilDescriptor,')
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
