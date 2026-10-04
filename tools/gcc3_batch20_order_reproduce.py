#!/usr/bin/env python3
"""Two anchored original-emission-order controls, excluding closed SDM."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 cells={'baseline':source}
 if path.endswith('v34diag.cpp'):
  a,z,fn=driver.function(source,'VPcmV34GetVisualDiagnostics')
  a=driver.function(source,'VPcmV34GetDiagnostics')[1]
  assert 'unsigned long\nVPcmV34GetVisualDiagnostics' in source[a:z]
  block=source[a:z];text=source[:a]+source[z:]
  where=text.index('\nvoid\nVPcmV34GetDiagnostics')+1
  cells['blob-order']=text[:where]+block+'\n\n'+text[where:]
 else:
  a=source.index('\nvoid\nV90SdDetector::reset()')+1;z=source.index('\n}\n',a)+2
  block=source[a:z];text=source[:a]+source[z:]
  where=text.index('/*\n * THE FOUR ARGUMENTS')
  cells['blob-order']=text[:where]+block+'\n\n'+text[where:]
 assert len(cells)==len(set(cells.values()))==2
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-order';driver.SOURCE_PATHS=('src/pump/v34/v34diag.cpp','src/pump/v90/V90SdDetector.cpp');driver.variants=variants;driver.main()
