#!/usr/bin/env python3
"""Test independently witnessed SI absolute values and genuine FSM array base."""
import playbook_small_patterns as d
import stage_c_fsm_control as fsm
import stage_c_getfp_loop as fp
d.REV='acfa905a';d.SOURCE_PATHS=('src/dsp/fpm_fsm.c','src/dsp/FP_math.c');d.OUT_NAME='stage-c-dsp-operands';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 if path.endswith('FP_math.c'):
  recurrence=fp.variants(path,s)['negative-add']
  newer=recurrence.replace('int va = a < 0 ? -a : a;','int va = __builtin_abs((int)a);').replace('int vb = b < 0 ? -b : b;','int vb = __builtin_abs((int)b);')
  assert newer!=recurrence
  return {'baseline':s,'recurrence':recurrence,'si-abs':newer}
 recurrence=fsm.variants(path,s)['captures-cursors']
 marker='\tshort scale = state->cfg.scale;'
 assert recurrence.count(marker)==1
 newer=recurrence.replace(marker,'\tconst short *scaled = state->scaled;\n'+marker).replace('state->scaled[bit]','scaled[bit]')
 return {'baseline':s,'captured-countdowns':recurrence,'entry-array-base':newer}
d.variants=variants;d.main()
