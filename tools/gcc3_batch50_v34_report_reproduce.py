#!/usr/bin/env python3
"""V34 diagnostic signed scan, captured owner and per-print guard cross."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v34_energy_reproduce import variants as energy

def variants(path,source):
 seed=energy(path,source)['cursor-1-countdown-0'];cells={'baseline':source}
 for signed,capture,guards in itertools.product((0,1),repeat=3):
  a,z,fn=d.function(seed,'V34EchoReportCoeff')
  if signed:fn=fn.replace('unsigned n =','int n =').replace('unsigned k;','int k;')
  if capture:fn=fn.replace('int i;','int i;\n\tconst short *coeff = e->coeff;',1).replace('e->coeff[','coeff[')
  if guards:
   fn=fn.replace('if (!DSPLIB_DEBUG_ON())\n\t\treturn;\n\n\tdsplibs_debug_printf','if (DSPLIB_DEBUG_ON())\n\t\tdsplibs_debug_printf')
   fn=fn.replace('if (!DSPLIB_DEBUG_ON())\n\t\t\treturn;\n\t\tdsplibs_debug_printf','if (DSPLIB_DEBUG_ON())\n\t\t\tdsplibs_debug_printf')
  cells[f'signed-{signed}-capture-{capture}-guards-{guards}']=seed[:a]+fn+seed[z:]
 assert len(set(cells.values()))==9
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-report';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
