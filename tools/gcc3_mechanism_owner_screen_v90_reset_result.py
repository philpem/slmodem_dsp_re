#!/usr/bin/env python3
"""Cross terminal return and original probe-mode DilType result ownership."""
import playbook_small_patterns as d
d.REV='9f1199b57d3c91d1f26627eabd9eea496355798d'
d.SOURCE_PATHS=('src/pump/v90/V90Modem.cpp',)
d.OUT_NAME='gcc3-mechanism-owner-screen-v90-reset-result'
def variants(path,source):
 start,end,fn=d.function(source,'V90Modem::reset');cells={'baseline':source}
 mark='\t\t\tqcFlag = 0;\n\t\t}\n\n\t\tif (qcFlag)\n\t\t\tdilType = DIL_TYPE_ADI_QC;\n\t\telse\n\t\t\tdilType = DIL_TYPE_ADI;'
 replacement='\t\t\tqcFlag = 0;\n\t\t\tdilType = DIL_TYPE_ADI;\n\t\t} else if (qcFlag)\n\t\t\tdilType = DIL_TYPE_ADI_QC;\n\t\telse\n\t\t\tdilType = DIL_TYPE_ADI;'
 assert fn.count(mark)==1
 for terminal,result,label in [(1,0,'terminal-default-return'),(0,1,'probe-result-arm'),(1,1,'terminal-probe-result')]:
  text=fn
  if result:text=text.replace(mark,replacement)
  if terminal:
   endmark='\t\tbreak;\n\t}\n}';assert text.count(endmark)==1;text=text.replace(endmark,'\t\treturn;\n\t}\n}')
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
