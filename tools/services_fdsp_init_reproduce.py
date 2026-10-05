#!/usr/bin/env python3
"""FDSP initializer float-clear helper and channel recapture cross."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for helper,recapture in itertools.product((False,True),repeat=2):
  if not(helper or recapture):continue
  t=source
  if helper:
   for channel in ('a','b'):
    for member,bound in [('dly','FDSP_DLY'),('coef','240')]:
     old=f'\tfor (i = 0; i < {bound}; i++)\n\t\t{channel}->{member}[i] = 0.0f;'
     assert t.count(old)==1;t=t.replace(old,f'\tzFLTUTL_FloatMemSet(0.0f, {channel}->{member}, {bound});')
  if recapture:
   start=t.index('FDSP_Kernel_InitObj(struct fdsp_kernel *k)');end=t.index('\n}',start)
   body=t[start:end]
   body=body.replace('struct fdsp_channel *a, *b;', 'struct fdsp_channel *a, *b;\n\tstruct fdsp_channel *delay_a;')
   body=body.replace('\ta->short_1690 = 0;', '\tdelay_a = k->chan_a;\n\ta->short_1690 = 0;')
   body=body.replace('a->dly[i]', 'delay_a->dly[i]').replace('a->coef[i]', 'delay_a->coef[i]')
   body=body.replace('0.0f, a->dly,', '0.0f, delay_a->dly,').replace('0.0f, a->coef,','0.0f, delay_a->coef,')
   t=t[:start]+body+t[end:]
  cells[f'helper-{int(helper)}-recapture-{int(recapture)}']=t
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/service/Fdspkrnl.c',);d.OUT_NAME='services-fdsp-init'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
