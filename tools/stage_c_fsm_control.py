#!/usr/bin/env python3
"""Cross the original FSM entry snapshots with 16-bit countdown cursors."""
import playbook_small_patterns as d
d.REV='acfa905a';d.SOURCE_PATHS=('src/dsp/fpm_fsm.c',);d.OUT_NAME='stage-c-fsm';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 start=s.index('short\nFPM_FSM_modulate(');end=s.index('\n}\n',start)+2;old=s[start:end]
 cells={'baseline':s}
 for captures,cursors in ((True,False),(False,True),(True,True)):
  f=old
  if captures:
   f=f.replace('\tint total = 0;','\tshort scale = state->cfg.scale;\n\tunsigned short samples_per_sym = state->cfg.samples_per_sym;\n\tint total = 0;')
   f=f.replace('state->cfg.scale);','scale);').replace('total += state->cfg.samples_per_sym;','total += samples_per_sym;').replace('k < state->cfg.samples_per_sym','k < samples_per_sym')
  if cursors:
   f=f.replace('\tint total = 0;\n\tunsigned i;','\tunsigned short total = 0;')
   f=f.replace('for (i = 0; i < nbits; i++)','while (nbits--)').replace('int bit = bits[i] & 1;','int bit = *bits++ & 1;').replace('\t\tint k;','\t\tunsigned short k;')
   sample='samples_per_sym' if captures else '(unsigned short)state->cfg.samples_per_sym'
   f=f.replace('\t\ttotal += '+('samples_per_sym' if captures else 'state->cfg.samples_per_sym')+';','\t\tk = '+sample+';\n\t\ttotal += k;')
   f=f.replace('for (k = 0; k < '+('samples_per_sym' if captures else 'state->cfg.samples_per_sym')+'; k++)','while (k--)')
  cells[('captures' if captures else '')+('-cursors' if cursors else '')]=s[:start]+f+s[end:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
d.variants=variants
if __name__ == '__main__':d.main()
