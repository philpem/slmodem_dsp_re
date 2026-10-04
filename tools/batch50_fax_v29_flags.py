#!/usr/bin/env python3
"""Object-supported byte flag lvalues in the V29 receiver."""
import re
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V29r_prc.c','src/fax/V29rx.c');d.OUT_NAME='batch50-fax-v29-flags'
def narrowed(fn):
 pattern=r'(\(\(struct v29_rx \*\)modem\)->result)\.word ([|&])= (~?)(V29_STATUS_[A-Z_]+);'
 def one(m):
  mask=m[4];shift=16 if mask=='V29_STATUS_IDLE' else 8;field='flags2' if shift==16 else 'flags'
  return m[1]+'.byte.'+field+' '+m[2]+'= (unsigned char)'+m[3]+'('+mask+' >> '+str(shift)+');'
 out,count=re.subn(pattern,one,fn);assert count,(fn[:100]);return out

def variants(path,source):
 cells={'baseline':source}
 names=['RxHdxDataV29','RxHdxErrorV29','RxNextStateV29','RxHdxIdleV29','RxHdxPrtcolV29','RxHdxEpochDetV29','RxHdxStartV29'] if 'r_prc' in path else ['V29RX_create']
 for name in names:
  a,z,fn=d.function(source,name);cells[name]=source[:a]+narrowed(fn)+source[z:]
 if 'r_prc' in path:
  text=source
  for name in ['RxHdxErrorV29','RxHdxIdleV29','RxHdxStartV29']:
   a,z,fn=d.function(text,name);text=text[:a]+narrowed(fn)+text[z:]
  cells['combined-winners']=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
