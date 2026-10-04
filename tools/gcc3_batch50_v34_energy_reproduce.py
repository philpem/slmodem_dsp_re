#!/usr/bin/env python3
"""Observable V34 history cursor advancement crossed with countdown."""
import sys
from pathlib import Path
import playbook_small_patterns as d

def variants(path,source):
 a,z,fn=d.function(source,'V34EchoEstimateDelayLineEnergy');cells={}
 for cursor in (0,1):
  for countdown in (0,1):
   text=fn
   if cursor:text=text.replace('unsigned k;','unsigned k;\n\tconst short *hist = e->hist;')
   if countdown:text=text.replace('for (k = 0; k < e->taps; k++)','for (k = e->taps; k > 0; k--)')
   if cursor:
    text=text.replace('for (k = '+('e->taps; k > 0; k--' if countdown else '0; k < e->taps; k++')+')\n', 'for (k = '+('e->taps; k > 0; k--' if countdown else '0; k < e->taps; k++')+') {\n\t\tint sample = *hist++;\n')
    text=text.replace('e->hist[k] * e->hist[k]', 'sample * sample').replace('\n\treturn acc;','\t}\n\n\treturn acc;')
   label='baseline' if not(cursor or countdown) else f'cursor-{cursor}-countdown-{countdown}'
   cells[label]=source[:a]+text+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-energy';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
