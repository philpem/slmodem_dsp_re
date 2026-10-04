#!/usr/bin/env python3
import itertools
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='gcc3-batch100-vpcm-tap-owner'
def variants(path,source):
 cells={}
 for cursor,common,owner in itertools.product((0,1),repeat=3):
  text=source
  for method,field in [('getDFE','dfe'),('getLinearEqualizer','linearEqu')]:
   start,end,fn=d.function(text,'VPcmFloModem::'+method)
   if cursor:
    assert fn.count('coefs[i]')==1;fn=fn.replace('coefs[i]','*coefs++')
   if owner:
    line='\tcoefs = eq->'+field+'Coefs;\n';assert fn.count(line)==1;fn=fn.replace(line,'')
    if method=='getDFE':
     mark='\tn = eq->dfeLength;';fn=fn.replace(mark,line+'\n'+mark)
    else:
     mark='\tfor (i = 0; i < n; i++)';fn=fn.replace(mark,'\tcoefs = modem.demodulator->equalizer->linearEquCoefs;\n\n'+mark)
   if common:
    old='\tif (pcmSessionType != 0 && info0Layout == 0)\n\t\treturn 0;';assert fn.count(old)==1;fn=fn.replace(old,'\tn = 0;\n\tif (pcmSessionType == 0 || info0Layout != 0) {')
    mark='\tfor (i = 0; i < n; i++)';assert fn.count(mark)==1;fn=fn.replace(mark,'\t}\n\n'+mark)
   text=text[:start]+fn+text[end:]
  label='-'.join(n for n,v in [('cursor',cursor),('common-zero',common),('original-owner',owner)] if v) or 'baseline';cells[label]=text
 return cells
d.variants=variants
if __name__=='__main__':d.main()
