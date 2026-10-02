#!/usr/bin/env python3
"""Compare shared MRF result declarations while preserving signed input/count use."""
import playbook_small_patterns as driver

SOURCES=('src/dsp/fpm_mrf.c','src/pump/v32/V32int.c','src/pump/v23/v23rx.c','src/pump/b103/B103prc.c','src/service/Rxcid.c','src/fax/V17r_int.c','src/fax/V21r_int.c','src/fax/V21t_int.c','src/fax/V27r_int.c','src/fax/V29r_int.c')

def variants(path,source):
 cells={'baseline':source}
 for label,result in (('unsigned-result','unsigned short'),('wide-result','int')):
  text=source
  if path=='src/dsp/fpm_mrf.c':
   assert text.count('short\nFPM_MRF_filter(')==1
   text=text.replace('short\nFPM_MRF_filter(',result+'\nFPM_MRF_filter(')
   start,end,fn=driver.function(text,'FPM_MRF_filter')
   assert fn.count('return (short)produced;')==1
   fn=fn.replace('return (short)produced;','return (unsigned short)produced;')
   text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells

def overlays(path,label):
 if label=='baseline':return {}
 rel='dsplib/fpm_mrf.h';text=(driver.ROOT/'include'/rel).read_text()
 old='short FPM_MRF_filter(';assert text.count(old)==1
 result='unsigned short' if label=='unsigned-result' else 'int'
 return {rel:text.replace(old,result+' FPM_MRF_filter(')}

if __name__=='__main__':
 driver.REV='fe0cace1'
 driver.OUT_NAME='playbook-mrf-result'
 driver.DUMP_FLAGS=()
 driver.SOURCE_PATHS=SOURCES
 driver.variants=variants
 driver.HEADER_OVERLAYS=overlays
 driver.main()
