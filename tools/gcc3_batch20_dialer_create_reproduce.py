#!/usr/bin/env python3
"""DialerCreate member modem reread x unsigned grade rejection."""
import sys
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 cells={}
 for reload in (0,1):
  for unsigned in (0,1):
   a,z,fn=driver.function(source,'DialerCreate')
   if reload:
    for name,field in [('SetPulseMakeTime','pulse_make'),('SetPulseBreakTime','pulse_break')]:
     old=name+'(modem, d->cfg.'+field+')';assert fn.count(old)==1
     fn=fn.replace(old,name+'(d->modem, d->cfg.'+field+')')
   if unsigned:
    old='if (d->grade <= DIALER_INVALID)';assert fn.count(old)==1
    fn=fn.replace(old,'if ((unsigned int)d->grade <= DIALER_INVALID)')
   cells['baseline' if not(reload or unsigned) else f'member-modem-{reload}-unsigned-grade-{unsigned}']=source[:a]+fn+source[z:]
 assert len(set(cells.values()))==4
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-dialer-create';driver.SOURCE_PATHS=('src/dialer/Dialer.c',);driver.variants=variants;driver.main()
