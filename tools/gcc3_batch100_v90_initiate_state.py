#!/usr/bin/env python3
import re
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90Modulator.cpp','src/pump/v90/V92Modulator.cpp');d.OUT_NAME='gcc3-batch100-v90-initiate-state'
def variants(path,source):
 text=source;cls=path.rsplit('/',1)[-1][:-4];local='p4state' if cls=='V90Modulator' else 'state'
 for method in ['initiateFPE','initiateRRN']:
  start,end,fn=d.function(text,cls+'::'+method)
  pattern=r'(\t\tedprintf\([^;]+;\n)(\t\t'+local+r' = [^;]+;\n)'
  fn,n=re.subn(pattern,lambda m:m[2]+m[1],fn);assert n==2,(path,method,n)
  text=text[:start]+fn+text[end:]
 return {'baseline':source,'state-before-diagnostic':text}
d.variants=variants
if __name__=='__main__':d.main()
