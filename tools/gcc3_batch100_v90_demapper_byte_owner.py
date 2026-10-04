#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90Demapper.cpp',);d.OUT_NAME='gcc3-batch100-v90-demapper-byte-owner'
def variants(path,source):
 cells={'baseline':source}
 for funcs,label in [(('resetNoSpectral',),'no-spectral-byte-owner'),(('reset',),'spectral-byte-owner'),(('reset','resetNoSpectral'),'both-byte-owners')]:
  text=source
  for name in funcs:
   start,end,fn=d.function(text,'V90Demapper::'+name)
   old='\t\t\t\tunsigned char c = mapp->constellation[i][j];\n';assert fn.count(old)==1;fn=fn.replace(old,'');assert fn.count('[c]')==6;fn=fn.replace('[c]','[mapp->constellation[i][j]]')
   text=text[:start]+fn+text[end:]
  cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
