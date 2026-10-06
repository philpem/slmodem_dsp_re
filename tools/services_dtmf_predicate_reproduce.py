#!/usr/bin/env python3
"""DTMF eager acceptance predicate and early accumulator lifetime cross."""
import itertools
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for eager,early in itertools.product((False,True),repeat=2):
  if not(eager or early):continue
  t=source;start,end,body=d.function(t,'dtmf_test')
  if eager:
   assert body.count('&&')==4;body=body.replace('&&','&')
  if early:
   body=body.replace('\tthreshold = (mode', '\tmax_lo = 0.0f;\n\tmax_hi = 0.0f;\n\tsum = 0.0f;\n\n\tthreshold = (mode',1)
   for marker in ('\tmin_lo = DTMF_HUGE;\n\tmax_lo = 0.0f;', '\tmin_hi = DTMF_HUGE;\n\tmax_hi = 0.0f;'):
    assert body.count(marker)==1;body=body.replace(marker,marker.split('\n')[0])
   marker='\n\tsum = 0.0f;\n\tfor (i = 0; i <= 7; i++)';assert body.count(marker)==1;body=body.replace(marker,'\n\tfor (i = 0; i <= 7; i++)')
  t=t[:start]+body+t[end:]
  cells[f'eager-{int(eager)}-early-{int(early)}']=t
 return cells
if __name__=='__main__':
 d.REV='240481e6';d.SOURCE_PATHS=('src/service/Dtmf.c',);d.OUT_NAME='services-dtmf-predicate'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
