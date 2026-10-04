#!/usr/bin/env python3
"""Original transmitter PPS scale ownership, flag reads and return boundaries."""
from itertools import product
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/V17t_stc.c','src/fax/V27t_stc.c','src/fax/V29t_stc.c');d.OUT_NAME='batch100-fax-tx-controls'
def variants(path,source):
 name=path.split('/')[-1][:3]+'TX_control';a,z,fn=d.function(source,name);cells={}
 if name=='V17TX_control':
  for compound,direct,tail in product((False,True),repeat=3):
   f=fn
   if compound:f=f.replace('pps->cfg.scale = V17TX_PPS_SCALE[mode] * req->scale_mul;','pps->cfg.scale *= V17TX_PPS_SCALE[mode];')
   if direct:f=f.replace('\tstruct fpm_pps *pps;\n','').replace('\tpps = &block->pps;\n','').replace('pps->cfg.scale','block->pps.cfg.scale')
   if tail:f=f.replace('\t\tV17TX_create(fp, fp);\n\t\treturn 1;','\t\tV17TX_create(fp, fp);')
   label='-'.join(n for n,v in [('compound',compound),('block-owner',direct),('common-tail',tail)] if v) or 'baseline';cells[label]=source[:a]+f+source[z:]
 elif name=='V27TX_control':
  for compound,direct,reload in product((False,True),repeat=3):
   f=fn
   if compound:
    old='\tpps->cfg.scale = ctl->scale_mul *\n\t\tV27TX_PPS_SCALE[rate];';assert old in f
    f=f.replace(old,'\tpps->cfg.scale = ctl->scale_mul;\n\tpps->cfg.scale *= V27TX_PPS_SCALE[rate];')
   if direct:
    f=f.replace('\tstruct fpm_pps *pps;','\tstruct v27_tx_block *block;')
    old='\tpps = (struct fpm_pps *)(void *)\n\t\t&((struct v27_tx *)modem)->tx->pps;';assert old in f
    f=f.replace(old,'\tblock = ((struct v27_tx *)modem)->tx;').replace('pps->cfg.scale','block->pps.cfg.scale')
   if reload:f=f.replace('\tunsigned char flags;\n','').replace('\tflags = ctl->flags;\n','').replace('if (flags &','if (ctl->flags &')
   label='-'.join(n for n,v in [('compound',compound),('block-owner',direct),('flags-reload',reload)] if v) or 'baseline';cells[label]=source[:a]+f+source[z:]
 else:
  for compound,carrier in product((False,True),('none','byte','int')):
   f=fn
   if compound:f=f.replace('= V29TX_PPS_SCALE[rate] * req->scale_mul;','*= V29TX_PPS_SCALE[rate];')
   if carrier!='none':
    f=f.replace('\tshort rate;','\tshort rate;\n\t'+('unsigned char' if carrier=='byte' else 'int')+' flags;')
    mark='\tprm->int_0008 =';assert mark in f
    f=f.replace(mark,'\tflags = req->ctl1;\n'+mark).replace('req->ctl1 &','flags &')
   label='-'.join((["compound"] if compound else [])+([carrier+'-flags'] if carrier!='none' else [])) or 'baseline';cells[label]=source[:a]+f+source[z:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
