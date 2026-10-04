#!/usr/bin/env python3
"""V34 observed copy/dot cursor cross on exact energy seed."""
import sys
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v34_energy_reproduce import variants as energy

def variants(path,source):
 seed=energy(path,source)['cursor-1-countdown-0'];cells={'baseline':source}
 for copy in (0,1):
  for dot in (0,1):
   a,z,fn=d.function(seed,'V34EchoFilter')
   if copy:
    fn=fn.replace('if (taps != 1)\n\t\tfor (k = 0; k < taps - 1; k++)\n\t\t\thist[k] = hist[k + 1];', 'if (taps != 1) {\n\t\tshort *dest = hist;\n\t\tfor (k = 0; k < taps - 1; k++) {\n\t\t\t*dest = dest[1];\n\t\t\tdest++;\n\t\t}\n\t}')
   if dot:
    fn=fn.replace('for (k = 0; k < taps; k++)\n\t\tacc += e->coeff[k] * hist[k];','{\n\t\tconst short *coeff = e->coeff;\n\t\tconst short *sample = hist;\n\t\tfor (k = 0; k < taps; k++)\n\t\t\tacc += *coeff++ * *sample++;\n\t}')
   cells[f'copy-{copy}-dot-{dot}']=seed[:a]+fn+seed[z:]
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-echo';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
