#!/usr/bin/env python3
"""Delete member reload and frame-ID default-result controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/faxvmi.c','src/fax/T30frames.c');d.OUT_NAME='batch50-fax-delete-id'
def variants(path,source):
 cells={'baseline':source}
 if 'faxvmi.c' in path:
  a,z,fn=d.function(source,'FAXVMI_delete')
  for lk,fr,label in [(True,False,'link-reloads'),(False,True,'framer-reloads'),(True,True,'both-reloads')]:
   f=fn
   if lk:f=f.replace('\tstruct faxvmi_link *lk = vmi->link;\n','').replace('lk->','vmi->link->').replace('(lk)','(vmi->link)')
   if fr:f=f.replace('\tstruct faxvmi_framer *fr = vmi->framer;\n','').replace('fr->','vmi->framer->').replace('(fr)','(vmi->framer)')
   cells[label]=source[:a]+f+source[z:]
 else:
  a,z,fn=d.function(source,'GetT30FrameIDFromBuffer')
  old='\tif (address != 0xff)\n\t\treturn 0xff;\n\n';assert old in fn
  body=fn[fn.index('\tif (control =='):fn.rindex('\n}')]
  for label in ['positive-guard','default-result','positive-default']:
   f=fn
   if label=='positive-guard':
    f=fn[:fn.index(old)]+'\tif (address == 0xff) {\n'+body+'\n\t}\n\treturn 0xff;\n}'
   elif label=='default-result':
    f=f.replace('\tint marker = 0;','\tint marker = 0;\n\tint result = 0xff;').replace(old,'\tif (address != 0xff)\n\t\treturn result;\n\n').replace('\treturn (int)id | marker;','\tresult = (int)id | marker;\n\treturn result;')
   else:
    f=fn[:fn.index(old)].replace('\tint marker = 0;','\tint marker = 0;\n\tint result = 0xff;')+'\tif (address == 0xff) {\n'+body.replace('\treturn (int)id | marker;','\tresult = (int)id | marker;')+'\n\t}\n\treturn result;\n}'
   cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
