#!/usr/bin/env python3
"""Cross late nibble read, signed byte predicate and explicit hex branches."""
import itertools,sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'data_raw')
 for late,signed,branch in itertools.product((False,True),repeat=3):
  if not(late or signed or branch):continue
  body=fn
  decl='\t\tunsigned char lo = (unsigned char)buf[i] & 0x0f;'
  if late:
   assert body.count(decl)==1;body=body.replace(decl+'\n','')
   marker='\t\tout[2 * i + 1] =';assert body.count(marker)==1
   body=body.replace(marker,decl+'\n\n'+marker)
  if signed:body=body.replace('unsigned char hi =','char hi =').replace('unsigned char lo =','char lo =')
  if branch:
   for field,index in [('hi','2 * i'),('lo','2 * i + 1')]:
    old=f'\t\tout[{index}] = (char)({field} > 9 ? {field} + 0x57 : {field} + 0x30);'
    new=f'\t\tif ({field} > 9)\n\t\t\tout[{index}] = (char)({field} + 0x57);\n\t\telse\n\t\t\tout[{index}] = (char)({field} + 0x30);'
    assert body.count(old)==1;body=body.replace(old,new)
  cells[f'late-{int(late)}-signed-{int(signed)}-branch-{int(branch)}']=source[:start]+body+source[end:]
 assert len(cells)==len(set(cells.values()))==8;return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.SOURCE_PATHS=('src/service/Data.c',);d.OUT_NAME='gcc3-batch50-cid-hex';d.variants=variants;d.main()
