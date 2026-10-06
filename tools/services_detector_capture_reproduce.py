#!/usr/bin/env python3
"""Detector initializer result ownership and child publication capture."""
import itertools,re
import playbook_small_patterns as d

def variants(path,source):
 cells={'baseline':source}
 for child,owner in itertools.product((False,True),repeat=2):
  if not(child or owner):continue
  a,b,body=d.function(source,'detector_create')
  if child:
   body=body.replace('\tint i;', '\tint i;\n\tstruct dtmf *dtmf;')
   body=body.replace('\td->dtmf = create_dtmf(d->dtmf);','\tdtmf = create_dtmf(d->dtmf);')
   body=body.replace('\tcfg = TONEamode_CFG;', '\tcfg = TONEamode_CFG;\n\td->dtmf = dtmf;')
  if owner:
   brace=body.index('{')
   head,rest=body[:brace],body[brace:]
   rest=re.sub(r'\bd\b','result',rest)
   rest=rest.replace('\tstruct fdsp_tone_cfg cfg;', '\tstruct detector *result = d;\n\tstruct fdsp_tone_cfg cfg;',1)
   body=head+rest
  cells[f'child-{int(child)}-owner-{int(owner)}']=source[:a]+body+source[b:]
 return cells
if __name__=='__main__':
 d.REV='e0052eec';d.SOURCE_PATHS=('src/service/Detector.c',);d.OUT_NAME='services-detector-capture'
 d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.main()
