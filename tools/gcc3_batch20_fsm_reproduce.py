#!/usr/bin/env python3
"""FSM count/cursor/configuration-read cross."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 a,z,fn=driver.function(source,'FPM_FSM_modulate');cells={'baseline':source}
 for count,cursor,config,total in itertools.product((False,True),repeat=4):
  if not any((count,cursor,config,total)):continue
  body=fn
  if count:
   body=body.replace('\tunsigned i;', '\tunsigned short i;').replace('\t\tint k;', '\t\tunsigned short k;')
   body=body.replace('for (i = 0; i < nbits; i++)','for (i = nbits; i--; )').replace('for (k = 0; k < state->cfg.samples_per_sym; k++)','for (k = (unsigned short)state->cfg.samples_per_sym; k--; )')
  if cursor:body=body.replace('bits[i]', '*bits++')
  if config:
   body=body.replace('\tunsigned'+(' short' if count else '')+' i;', '\tunsigned'+(' short' if count else '')+' i;\n\tshort scale = state->cfg.scale;\n\tunsigned short length = state->cfg.samples_per_sym;')
   body=body.replace('FPM_TONE_set_scale(state->tone, state->cfg.scale);','FPM_TONE_set_scale(state->tone, scale);').replace('total += state->cfg.samples_per_sym;', 'total += length;').replace('k < state->cfg.samples_per_sym','k < length').replace('(unsigned short)state->cfg.samples_per_sym; k--','length; k--')
  if total:body=body.replace('\tint total = 0;', '\tunsigned short total = 0;')
  cells[f'count-{int(count)}-cursor-{int(cursor)}-config-{int(config)}-total-{int(total)}']=source[:a]+body+source[z:]
 assert len(cells)==len(set(cells.values()))==16
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-fsm';driver.SOURCE_PATHS=('src/dsp/fpm_fsm.c',);driver.variants=variants;driver.main()
