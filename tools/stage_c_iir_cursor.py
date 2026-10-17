#!/usr/bin/env python3
"""Cross original IIR short countdown and coefficient/history cursors."""
import playbook_small_patterns as d
d.REV='d127aec8';d.SOURCE_PATHS=('src/dsp/fpm_iir.c',);d.OUT_NAME='stage-c-iir';d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
def variants(path,s):
 a=s.index('short\nFPM_iir_filt(');z=s.index('\n}\n',a)+2;old=s[a:z];cells={'baseline':s}
 for count,cursor in ((True,False),(False,True),(True,True)):
  f=old
  if count:f=f.replace('\tint i;\n','').replace('for (i = 0; i < sections; i++)','while (sections--)')
  if cursor:
   for i in range(5):f=f.replace('coeff['+str(i)+']','*coeff++')
   f=f.replace('state[0] = (short)w2;','*state++ = (short)w2;').replace('state[1] = (short)w;','*state++ = (short)w;')
   f=f.replace('\n\t\tcoeff += FPM_IIR_COEFF_PER_SECTION;\n\t\tstate += FPM_IIR_STATE_PER_SECTION;','')
  cells[('countdown' if count else '')+('-cursors' if cursor else '')]=s[:a]+f+s[z:]
 assert len(cells)==len(set(cells.values()))==4
 return cells
d.variants=variants
if __name__ == '__main__':d.main()
