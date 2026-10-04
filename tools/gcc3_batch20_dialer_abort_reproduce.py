#!/usr/bin/env python3
"""DialerAbort object-observed unsigned progress-state range guard."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 old='if (d->progress_state > 10)';assert source.count(old)==1
 return {'baseline':source,'unsigned-range':source.replace(old,'if ((unsigned int)d->progress_state > 10u)')}
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-dialer-abort';driver.SOURCE_PATHS=('src/dialer/Dialer.c',);driver.variants=variants;driver.main()
