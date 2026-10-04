#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90SdDetector.cpp',)
d.OUT_NAME='gcc3-batch50-v90-sd-owner'
def variants(path,source):
 cells={}
 for pointers,verdict in itertools.product((False,True),repeat=2):
  start,end,fn=d.function(source,'V90SdDetector::process')
  if pointers:
   fn=fn.replace('\tdo {\n\t\thistory[i] = history[i - 1];\n\t} while (--i != 0);','\tfloat *destination = history + i;\n\tfloat *previous = destination - 1;\n\tdo {\n\t\t*destination-- = *previous--;\n\t} while (--i != 0);')
  if verdict:
   fn=fn.replace('\tunsigned int i = historyLength - 1;','\tint result = 0;\n\tunsigned int i = historyLength - 1;').replace('\t\treturn run < limit ? 0 : 1;','\t\tif (run >= limit)\n\t\t\tresult = 1;\n\t\treturn result;')
  label='-'.join(n for n,v in [('pointer-copy',pointers),('early-verdict',verdict)] if v) or 'baseline'
  cells[label]=source[:start]+fn+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
