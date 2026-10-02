#!/usr/bin/env python3
"""Test the observed transmitter owner reloads across modulation calls."""
import playbook_small_patterns as driver

def variants(path,source):
 candidate=source
 for name in ('ModDataB103','TxNoCarrierB103'):
  start,end,fn=driver.function(candidate,name)
  old='\tstruct b103_dsp *dsp = fp->dsp;\n'
  assert fn.count(old)==1
  fn=fn.replace(old,'').replace('dsp->','fp->dsp->')
  candidate=candidate[:start]+fn+candidate[end:]
 return {'baseline':source,'owner-reload':candidate}

if __name__=='__main__':
 driver.REV='6cde9a9d'
 driver.OUT_NAME='playbook-b103-tx-reload'
 driver.SOURCE_PATHS=('src/pump/b103/B103prc.c',)
 driver.variants=variants
 driver.main()
