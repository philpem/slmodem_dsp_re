#!/usr/bin/env python3
"""Restore original query-owner reads after callbacks and at each gain call."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source};start,end,fn=d.function(source,'voice_set_rx')
 head='\tunsigned int (*query)(void *obj, int what) =\n\t    (unsigned int (*)(void *, int))v->cfg.get_sreg;\n'
 assert head in fn
 marker='\tdetector_set_enable(v->detector, (short)v->detector_enable_rx);'
 for late,direct in itertools.product((False,True),repeat=2):
  if not(late or direct):continue
  body=fn
  if late:body=body.replace(head,'').replace(marker,marker+'\n\n'+head)
  if direct:
   for name in ('FMT1','FMT3','OTHER'):
    old='query(v->cfg.modem, VOICE_PARAM_RX_GAIN_'+name+')'
    assert old in body;body=body.replace(old,'(unsigned int)v->cfg.get_sreg(v->cfg.modem, VOICE_PARAM_RX_GAIN_'+name+')')
  cells['late-%d-direct-%d'%(late,direct)]=source[:start]+body+source[end:]
 # Full owner form removes the remaining pre-callback snapshot instead of a late local.
 body=fn.replace(head,'').replace('v->cfg.modem, query);','v->cfg.modem,\n\t    (unsigned int (*)(void *, int))v->cfg.get_sreg);')
 for name in ('FMT1','FMT3','OTHER'):
  body=body.replace('query(v->cfg.modem, VOICE_PARAM_RX_GAIN_'+name+')','(unsigned int)v->cfg.get_sreg(v->cfg.modem, VOICE_PARAM_RX_GAIN_'+name+')')
 cells['owner-every-use']=source[:start]+body+source[end:]
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 d.REV='856c1ecb';d.SOURCE_PATHS=('src/service/Rx.c',);d.OUT_NAME='gcc3-batch100-rx-query';d.variants=variants;d.main()
