#!/usr/bin/env python3
"""Bounded FSM array-owner, signed length and word-return overlay cross."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver
import gcc3_batch20_fsm_reproduce as first

def variants(path,source):
 cells={'baseline':source};seed=first.variants(path,source)['count-1-cursor-1-config-1-total-1'];a,z,fn=driver.function(seed,'FPM_FSM_modulate')
 for pointer,signed,ret in itertools.product((False,True),repeat=3):
  body=fn
  if pointer:body=body.replace('\tunsigned short i;', '\tunsigned short i;\n\tconst short *freq = state->scaled;').replace('state->scaled[bit]', 'freq[bit]')
  if signed:body=body.replace('\tunsigned short length = state->cfg.samples_per_sym;', '\tshort length = state->cfg.samples_per_sym;')
  text=seed[:a]+body+seed[z:]
  if ret:text=text.replace('short\nFPM_FSM_modulate(', 'unsigned short\nFPM_FSM_modulate(')
  cells[f'pointer-{int(pointer)}-signed-{int(signed)}-return-{int(ret)}']=text
 assert len(cells)==len(set(cells.values()))==9
 return cells

def overlays(path,label):
 if label.endswith('return-1'):
  text=(driver.ROOT/'include/dsplib/fpm_fsm.h').read_text();assert text.count('short FPM_FSM_modulate(')==1
  return {'dsplib/fpm_fsm.h':text.replace('short FPM_FSM_modulate(', 'unsigned short FPM_FSM_modulate(')}
 return {}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-fsm-followup';driver.SOURCE_PATHS=('src/dsp/fpm_fsm.c',);driver.variants=variants;driver.HEADER_OVERLAYS=overlays;driver.main()
