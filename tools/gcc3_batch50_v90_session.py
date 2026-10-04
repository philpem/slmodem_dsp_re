#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='902f47fa'
d.SOURCE_PATHS=('src/pump/v90/V90Modem.cpp',)
d.OUT_NAME='gcc3-batch50-v90-session'
def variants(path,source):
 start,end,fn=d.function(source,'V90Modem::setSessionFlag');head=fn[:fn.index('\tif (which')];cells={'baseline':source}
 for label,first in [('switch-break','break;'),('switch-first-return','return;')]:
  body=head+'\tswitch (which) {\n\tcase 0:\n\t\tmodulator->setSessionFlag(flag);\n\t\t'+first+'\n\tcase 1:\n\t\tdemodulator->setSessionFlag(flag);\n\t\tbreak;\n\t}\n}'
  cells[label]=source[:start]+body+source[end:]
 body=head+'\tif (which == 0) {\n\t\tmodulator->setSessionFlag(flag);\n\t\treturn;\n\t}\n\tif (which == 1)\n\t\tdemodulator->setSessionFlag(flag);\n}'
 cells['first-early-return']=source[:start]+body+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
