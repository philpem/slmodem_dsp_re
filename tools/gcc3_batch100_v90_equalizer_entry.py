#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90Equalizer.cpp',);d.OUT_NAME='gcc3-batch100-v90-equalizer-entry'
def variants(path,source):
 cells={}
 for verdict,capture in itertools.product((False,True),repeat=2):
  text=source
  for method,state in [('enterFPE','V90EQU_STATE_FPE'),('enterRRN','V90EQU_STATE_RRN')]:
   start,end,fn=d.function(text,'V90Equalizer::'+method)
   if capture:
    fn=fn.replace('unsigned int i, n;','unsigned int i, n;\n\tint fixedMode;').replace('\tstate = '+state+';','\tstate = '+state+';\n\tfixedMode = mmxMode;')
    fn=fn.replace('\t\tif (mmxMode != 0) {','\t\tif (fixedMode != 0) {',1)
   if verdict:
    fn=fn.replace('unsigned int i, n;','int result = 0;\n\tunsigned int i, n;',1)
    fn=fn.replace('\tif (state == '+state+')\n\t\treturn 0;','\tif (state != '+state+') {')
    fn=fn.replace('\t\treturn 1;','\t\tresult = 1;')
    fn=fn.replace('\treturn 0;','\t}\n\treturn result;')
   text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('entry-verdict',verdict),('captured-mode',capture)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
