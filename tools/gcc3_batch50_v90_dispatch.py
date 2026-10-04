#!/usr/bin/env python3
from pathlib import Path
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90Phase3Modulator.cpp',)
d.OUT_NAME='gcc3-batch50-v90-dispatch'
def variants(path,source):
 basewinner=(d.ROOT/'build/gcc3-batch50-v90-vector-direct/V90Phase3Modulator/jd-direct-cycle/V90Phase3Modulator.cpp').read_text()
 cells={'baseline':source}
 for stem,text in [('independent',source),('combined',basewinner)]:
  start,end,fn=d.function(text,'V90Phase3Modulator::generateSymbol');head=fn[:fn.index('\n{')]
  forms={'branch-casts':fn.replace('return generate','return (short)generate'),
   'short-result':head+'\n{\n\tshort sample;\n\tif (sessionFlag != 0)\n\t\tsample = generateV92Symbol();\n\telse\n\t\tsample = generateV90Symbol();\n\treturn sample;\n}',
   'conditional-cast':head+'\n{\n\treturn (short)(sessionFlag ? generateV92Symbol() : generateV90Symbol());\n}'}
  for label,body in forms.items():cells[stem+'-'+label]=text[:start]+body+text[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
