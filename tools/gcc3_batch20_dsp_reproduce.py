#!/usr/bin/env python3
"""Finite cursor/count and value-read boundaries in two DSP TUs."""
import sys,itertools,re
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 cells={'baseline':source}
 if path.endswith('fpm_iir.c'):
  start,end,fn=driver.function(source,'FPM_iir_filt')
  for countdown,cursors,output in itertools.product((False,True),repeat=3):
   if not any((countdown,cursors,output)):continue
   body=fn
   if countdown:body=body.replace('\tint i;','\tshort i;').replace('for (i = 0; i < sections; i++)','for (i = (short)(sections - 1); i != -1; i--)')
   if cursors:
    body=body.replace('int w1 = state[0];','int w1 = *state;')
    for a,z in [('coeff[0]','*coeff++'),('coeff[1]','*coeff++'),('coeff[2]','*coeff++'),('coeff[3]','*coeff++'),('coeff[4]','*coeff++'),('w2 = state[1];','w2 = state[1];'),('state[0] = (short)w2;','*state++ = (short)w2;'),('state[1] = (short)w;','*state++ = (short)w;')]:body=body.replace(a,z)
    body=body.replace('\n\t\tcoeff += FPM_IIR_COEFF_PER_SECTION;\n\t\tstate += FPM_IIR_STATE_PER_SECTION;','')
   if output:
    body=body.replace('\tint acc_in = x;','\tint acc_in = x;\n\tshort out;').replace('\t\tacc_in = (short)((short)ff + ((', '\t\tout = (short)((short)ff + ((').replace('\n\t\tcoeff +=','\n\t\tacc_in = out;\n\t\tcoeff +=')
    if cursors:body=body.replace('\n\t}\n\n\treturn (short)acc_in;', '\n\t\tacc_in = out;\n\t}\n\n\treturn (short)acc_in;')
    body=body.replace('return (short)acc_in;', 'return out;')
   cells[f'iir-count-{int(countdown)}-cursor-{int(cursors)}-out-{int(output)}']=source[:start]+body+source[end:]
 else:
  for kpost,dstpost in itertools.product((False,True),repeat=2):
   if not(kpost or dstpost):continue
   text=source
   for name in ['FPM_lmsupd','FPM_lmsupd2']:
    start,end,body=driver.function(text,name)
    if kpost:
     body=body.replace('k >= 0; k--','k >= 0;').replace('k > widx; k--','k > widx;').replace('hist[k]', 'hist[k--]')
    if dstpost:
     body=body.replace('\n\t\t*c =', '\n\t\t*c =')
     marker='\t\t*c ='
     # Capture advancing destination before the first product/read in each loop.
     for opening in ['for (k = widx; k >= 0;'+('' if kpost else ' k--')+') {','for (k = (short)(taps - 1); k > widx;'+('' if kpost else ' k--')+') {']:
      assert body.count(opening)==1
      body=body.replace(opening,opening+'\n\t\tshort *dest = c++;')
     body=re.sub(r'\*c\b', '*dest', body).replace('\tshort *dest = coeff;', '\tshort *c = coeff;').replace('\n\t\tc++;','')
    text=text[:start]+body+text[end:]
   cells[f'lms-index-{int(kpost)}-dest-{int(dstpost)}']=text
 assert len(cells)==len(set(cells.values()))==(8 if path.endswith('fpm_iir.c') else 4)
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-dsp';driver.SOURCE_PATHS=('src/dsp/fpm_iir.c','src/dsp/fpm_adeq.c');driver.variants=variants;driver.main()
