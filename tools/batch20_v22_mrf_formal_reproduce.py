#!/usr/bin/env python3
"""V22 MRF original unsigned caller slot / signed callee use boundary."""
import subprocess
import playbook_small_patterns as d
d.REV='93d7eee1';d.SOURCE_PATHS=('src/pump/v22/V22int.c','src/pump/v22/v22_mrf.c');d.OUT_NAME='batch20-v22-mrf-formal'
def variants(path,source):
 out={}
 for formal,cast in [(False,False),(False,True),(True,False),(True,True)]:
  text=source
  if path.endswith('/v22_mrf.c') and formal:
   needle='V22_MRF_filter(struct v22_mrf *state, const short *in, short *out, short count)';assert text.count(needle)==1
   text=text.replace(needle,needle.replace('short count','int count'))
   assert text.count('short remaining = count;')==1;text=text.replace('short remaining = count;','short remaining = (short)count;')
  if path.endswith('/V22int.c') and cast:
   a,b,fn=d.function(text,'DemodDataV22');start=fn.index('V22_MRF_filter(');end=fn.index(';',start);call=fn[start:end];assert call.count('(short)count')==1
   fn=fn[:start]+call.replace('(short)count','count')+fn[end:];text=text[:a]+fn+text[b:]
  label='baseline' if not(formal or cast) else ('int' if formal else '')+('-count' if cast else '')
  out[label]=text
 return out
def overlays(path,label):
 if not label.startswith('int'):return {}
 s=subprocess.check_output(['git','show',d.REV+':include/dsplib/v22_mrf.h'],cwd=d.ROOT,text=True);needle='\t\t     short count);';assert s.count(needle)==1
 return {'dsplib/v22_mrf.h':s.replace(needle,needle.replace('short count','int count'))}
d.HEADER_OVERLAYS=overlays;d.variants=variants
if __name__=='__main__':d.main()
