#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/v8/V8Interface.c',);d.OUT_NAME='batch20-v8-control'
def variants(path,source):
 a,b,f=d.function(source,'V8Control');out={}
 for u,p in [(False,False),(True,False),(False,True),(True,True)]:
  fn=f
  if u:fn=fn.replace('switch (what)','switch ((unsigned)what)')
  if p:
   fn=fn.replace('int rc;','int rc = -1;')
   for bad,good in [('v->side != 0 || v->rx_state != 0x19 || v->cm_ready != 0','v->side == 0 && v->rx_state == 0x19 && v->cm_ready == 0'),('v->rx_substate != V8_HS_TAKEN_RX','v->rx_substate == V8_HS_TAKEN_RX'),('v->rx_substate != V8_HS_TAKEN_TX','v->rx_substate == V8_HS_TAKEN_TX')]:
    needle='if ('+bad+') {\n\t\t\trc = -1;\n\t\t} else {';assert fn.count(needle)==1;fn=fn.replace(needle,'if ('+good+') {')
  label='baseline' if not(u or p) else ('unsigned' if u else '')+('-positive' if p else '')
  out[label]=source[:a]+fn+source[b:]
 return out
d.variants=variants
if __name__=='__main__':d.main()
