#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90AutoDigitalImpDetector.cpp',)
d.OUT_NAME='gcc3-batch50-v90-adid-verdict'
d.DUMP_FLAGS=('-dr',)
def variants(path,source):
 cells={}
 for anyphase,alt in itertools.product((False,True),repeat=2):
  text=source
  if anyphase:
   start,end,fn=d.function(text,'V90AutoDigitalImpDetector::isThereAnyAltRbsPhase')
   fn=fn.replace('\tshort sum = 0;','\tint result = 0;\n\tshort sum = 0;').replace('\treturn sum > 0 ? 1 : 0;','\tif (sum > 0)\n\t\tresult = 1;\n\treturn result;')
   text=text[:start]+fn+text[end:]
  if alt:
   start,end,fn=d.function(text,'V90AutoDigitalImpDetector::isAltRbs')
   fn=fn.replace('\tint d;','\tint result = 0;\n\tint d;').replace('\tif (altRbsFlag[phase] == 0)\n\t\treturn 0;','\tif (altRbsFlag[phase] != 0) {').replace('\treturn d > altRbsDistanceThresh ? 1 : 0;','\t\tif (d > altRbsDistanceThresh)\n\t\t\tresult = 1;\n\t}\n\treturn result;')
   text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('phase-verdict',anyphase),('sample-verdict',alt)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
