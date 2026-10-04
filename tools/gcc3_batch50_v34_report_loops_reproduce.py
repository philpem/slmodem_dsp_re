#!/usr/bin/env python3
"""Explicit signed scan entry and exclusive report row bound."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as d
from gcc3_batch50_v34_report_found_reproduce import variants as previous

def variants(path,source):
 seed=previous(path,source)['direct-found-edge'];cells={'baseline':source}
 for guard,bound in itertools.product((0,1),repeat=2):
  a,z,fn=d.function(seed,'V34EchoReportCoeff')
  if guard:fn=fn.replace('for (k = 0; k < n; k++)','if (n > 0)\n\tfor (k = 0; k < n; k++)',1)
  if bound:fn=fn.replace('i <= V34_ECHO_REPORT_TAPS - V34_ECHO_REPORT_COLS','i < V34_ECHO_REPORT_TAPS')
  cells[f'entry-{guard}-bound-{bound}']=seed[:a]+fn+seed[z:]
 assert len(set(cells.values()))==5
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 d.REV='902f47fa';d.OUT_NAME='gcc3-batch50-v34-report-loops';d.SOURCE_PATHS=('src/pump/v34/v34filters.c',);d.variants=variants;d.main()
