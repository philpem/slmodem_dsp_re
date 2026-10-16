#!/usr/bin/env python3
"""Cross MTD's witnessed sign mask with explicit terminal decision arms."""
import playbook_small_patterns as d
import stage_c_mtd_residual as first
d.OUT_NAME='stage-c-mtd-branches'
OLD='\treturn (wideband >= out_of_band) ? FPM_MTD_ABSENT : FPM_MTD_PRESENT;'
NEW='\tif (wideband >= out_of_band)\n\t\treturn FPM_MTD_ABSENT;\n\treturn FPM_MTD_PRESENT;'
def variants(path,s):
 assert s.count(OLD)==s.count(first.CLAMP)==1
 cells={'baseline':s}
 for mask,branch in ((True,False),(False,True),(True,True)):
  t=s
  if mask:t=t.replace(first.CLAMP,'\tout_of_band &= ~out_of_band >> 15;')
  if branch:t=t.replace(OLD,NEW)
  cells[('mask' if mask else '')+('-branches' if branch else '')]=t
 assert len(cells)==len(set(cells.values()))==4
 return cells
d.variants=variants
d.main()
