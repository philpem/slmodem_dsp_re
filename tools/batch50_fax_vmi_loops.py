#!/usr/bin/env python3
"""Original narrow unsigned frame/FIFO clearing count controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/faxvmi.c',);d.OUT_NAME='batch50-fax-vmi-loops'
def variants(path,source):
 cells={'baseline':source}
 for names,label in [(['FAXVMI_control'],'control'),(['FAXVMI_create'],'create'),(['FAXVMI_control','FAXVMI_create'],'both')]:
  text=source
  for name in names:
   a,z,fn=d.function(text,name);assert '\tint i;' in fn;fn=fn.replace('\tint i;','\tunsigned short i;');text=text[:a]+fn+text[z:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
