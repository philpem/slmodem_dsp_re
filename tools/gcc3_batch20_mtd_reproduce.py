#!/usr/bin/env python3
"""MTD observed countdown/cursor, narrow energy and shared return boundaries."""
import sys,itertools
from pathlib import Path
import playbook_small_patterns as driver

def variants(path,source):
 a,z,fn=driver.function(source,'FPM_MTD_detect');cells={'baseline':source}
 for count,cursor,shorts,tail in itertools.product((False,True),repeat=4):
  if not any((count,cursor,shorts,tail)):continue
  body=fn
  if count:body=body.replace('\tint i;', '\tshort i;').replace('for (i = 0; i < count; i++)', 'for (i = count; i--; )')
  if cursor:body=body.replace('samples[i]', '*samples++')
  if shorts:
   body=body.replace('\tint wideband = state->wideband;', '\tshort wideband = state->wideband;').replace('\tint tone;', '\tshort tone;').replace('\tint out_of_band;', '\tshort out_of_band;')
  if tail:body=body.replace('\treturn (wideband >= out_of_band) ? FPM_MTD_ABSENT : FPM_MTD_PRESENT;', '\tif (wideband >= out_of_band)\n\t\treturn FPM_MTD_ABSENT;\n\treturn FPM_MTD_PRESENT;')
  cells[f'count-{int(count)}-cursor-{int(cursor)}-short-{int(shorts)}-tail-{int(tail)}']=source[:a]+body+source[z:]
 assert len(cells)==len(set(cells.values()))==16
 return cells
if __name__=='__main__':
 assert '--domain' in sys.argv and Path(sys.argv[sys.argv.index('--domain')+1]).is_file()
 driver.REV='93d7eee1';driver.OUT_NAME='gcc3-batch20-mtd';driver.SOURCE_PATHS=('src/dsp/fpm_mtd.c',);driver.variants=variants;driver.main()
