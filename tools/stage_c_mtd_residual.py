#!/usr/bin/env python3
"""Bounded original MTD clamp graph and shared return-result controls."""
import playbook_small_patterns as d
d.REV='60277060'
d.SOURCE_PATHS=('src/dsp/fpm_mtd.c',)
d.OUT_NAME='stage-c-mtd-residual'
d.DUMP_FLAGS=('-v','-save-temps','-da','-dP')
CLAMP='\tif (out_of_band < 0)\n\t\tout_of_band = 0;'
RET='''\tif (wideband < state->cfg.min_level)
\t\treturn FPM_MTD_NOSIGNAL;

\t/*
\t * Detected when the energy outside the tone is a small enough fraction
\t * of the total.  The second test is unreachable -- out_of_band is
\t * clamped at or below wideband -- but is reproduced.
\t */
\tthreshold = ((int)state->cfg.ratio * wideband) >> 15;
\tif (out_of_band <= threshold)
\t\treturn FPM_MTD_PRESENT;

\treturn (wideband >= out_of_band) ? FPM_MTD_ABSENT : FPM_MTD_PRESENT;'''
NEWRET='''\t{
\t\tshort result = FPM_MTD_NOSIGNAL;

\t\tif (wideband >= state->cfg.min_level) {
\t\t\tthreshold = ((int)state->cfg.ratio * wideband) >> 15;
\t\t\tresult = FPM_MTD_PRESENT;
\t\t\tif (out_of_band > threshold && wideband >= out_of_band)
\t\t\t\tresult = FPM_MTD_ABSENT;
\t\t}
\t\treturn result;
\t}'''
def variants(path,s):
 assert s.count(CLAMP)==s.count(RET)==1
 cells={'baseline':s}
 for clamp,result in ((True,False),(False,True),(True,True)):
  t=s
  if clamp:t=t.replace(CLAMP,'\tout_of_band &= ~out_of_band >> 15;')
  if result:t=t.replace(RET,NEWRET)
  cells[('mask' if clamp else '')+('-result' if result else '')]=t
 assert len(cells)==len(set(cells.values()))==4
 return cells
d.variants=variants
if __name__ == '__main__':
 d.main()
