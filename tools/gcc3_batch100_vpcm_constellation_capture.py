#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='gcc3-batch100-vpcm-constellation-capture'
def variants(path,source):
 start,end,fn=d.function(source,'VPcmFloModem::getConstellation');cells={'baseline':source}
 for capture,common,label in [(1,0,'inputs-before-clamp'),(0,1,'common-zero'),(1,1,'capture-common-zero')]:
  text=fn
  if capture:
   lines=['\tvalues = (const float *)dem->array_254;\n','\tlane = dem->word_260;\n']
   for line in lines:assert text.count(line)==1;text=text.replace(line,'')
   mark='\tif (n > maxCount)';text=text.replace(mark,lines[1]+lines[0]+mark)
  if common:
   old='\tif (pcmSessionType != 0 && info0Layout == 0)\n\t\treturn 0;';assert text.count(old)==1;text=text.replace(old,'\tn = 0;\n\tif (pcmSessionType == 0 || info0Layout != 0) {')
   mark='\n\treturn n;';assert text.count(mark)==1;text=text.replace(mark,'\n\t}\n'+mark)
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
