#!/usr/bin/env python3
"""Finite completed-source loop/owner/default-config controls."""
import playbook_small_patterns as d
d.REV='902f47fa';d.SOURCE_PATHS=('src/fax/V17t_int.c','src/fax/V29t_prc.c','src/fax/faxvmi_utls.c');d.OUT_NAME='batch50-fax-loops'
def variants(path,source):
 cells={'baseline':source}
 name={'V17t_int.c':'SMCv17_init','V29t_prc.c':'GenEQTrnSequenceV29','faxvmi_utls.c':'faxvmi_gen_fcs16'}[path.split('/')[-1]]
 start,end,fn=d.function(source,name)
 if name=='SMCv17_init':
  for branch,order,label in [(True,False,'direct-default'),(False,True,'observed-order'),(True,True,'direct-observed')]:
   f=fn
   if branch:
    old='\tif (cfg == NULL)\n\t\tcfg = SMCv17_CFG;\n\n\tmemcpy(&statep->mode, cfg, sizeof(short[2]));'
    new='\tif (cfg != NULL)\n\t\tmemcpy(&statep->mode, cfg, sizeof(short[2]));\n\telse\n\t\tmemcpy(&statep->mode, SMCv17_CFG, sizeof(short[2]));'
    assert old in f;f=f.replace(old,new)
   if order:f=f.replace('\tstatep->quad = 0;\n\tstatep->state = 0;\n\tstatep->trellis = 0;\n\tstatep->prev = 0;', '\tstatep->state = 0;\n\tstatep->quad = 0;\n\tstatep->prev = 0;\n\tstatep->trellis = 0;')
   cells[label]=source[:start]+f+source[end:]
 elif name=='GenEQTrnSequenceV29':
  for owner,post,label in [(True,False,'owner'),(False,True,'postdec'),(True,True,'owner-postdec')]:
   f=fn
   if owner:
    f=f.replace('\tshort *srp = (short *)(void *)((unsigned char *)blk + V29SCRAM_SR);\n\tint sr = *srp;', '\tint sr = *(short *)(void *)((unsigned char *)blk + V29SCRAM_SR);')
    f=f.replace('*srp = (short)sr;', '*(short *)(void *)((unsigned char *)blk + V29SCRAM_SR) = (short)sr;')
   if post:f=f.replace('for (i = n; i != 0; i--)','for (i = n; i-- != 0;)')
   cells[label]=source[:start]+f+source[end:]
 else:
  f=fn.replace('for (i = count; i != 0; i--)','for (i = count; i-- != 0;)')
  cells['postdec']=source[:start]+f+source[end:]
 assert len(set(cells.values()))==len(cells)
 return cells
d.variants=variants
if __name__=='__main__':d.main()
