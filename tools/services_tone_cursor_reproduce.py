#!/usr/bin/env python3
"""Service tone filter cursor, capture and converted mask boundaries."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for cursor,mask,wide,capture in itertools.product((False,True),repeat=4):
  if not(cursor or mask or wide or capture):continue
  a,b,body=d.function(source,'TONE_filter')
  if wide:body=body.replace('\tshort idx = t->fir_idx;','\tint idx = t->fir_idx;')
  if mask:
   old='\t\tidx = (short)((short)(idx + 1) < len ? (short)(idx + 1) : 0);';assert old in body
   body=body.replace(old,'\t\tidx = (short)(idx + 1);\n\t\tif ((short)idx >= len)\n\t\t\tidx = 0;')
  if capture:
   body=body.replace('\tshort i;', '\tshort i;\n\tconst float *coef = t->fir_coef;')
   body=body.replace('const float *c = t->fir_coef;', 'const float *c = coef;')
  if cursor:
   body=body.replace('\t\tint j;', '\t\tint j;\n\t\tconst float *history;')
   body=body.replace('\t\tfor (j = idx; j >= 0; j--)\n\t\t\tacc += *c++ * dly[j];\n\t\tfor (j = len - 1; j > idx; j--)\n\t\t\tacc += *c++ * dly[j];', '\t\thistory = dly + idx;\n\t\tfor (j = idx; j >= 0; j--)\n\t\t\tacc += *c++ * *history--;\n\t\thistory = dly + len - 1;\n\t\tfor (j = len - 1; j > idx; j--)\n\t\t\tacc += *c++ * *history--;')
  cells[f'cursor-{int(cursor)}-mask-{int(mask)}-wide-{int(wide)}-capture-{int(capture)}']=source[:a]+body+source[b:]
 return cells
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/service/TONE.c',);d.OUT_NAME='services-tone-cursor'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
