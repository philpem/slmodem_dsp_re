#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/VpcmFloModem.cpp',);d.OUT_NAME='gcc3-batch100-vpcm-phase2-owner-report'
def variants(path,source):
 start,end,fn=d.function(source,'VPcmFloModem::setPhaseIIinfo');cells={'baseline':source}
 begin=fn.index('\tif (DSPLIB_DEBUG_ON())');finish=fn.index('\n\t/*',begin);reports=fn[begin:finish];assert reports.count('if (DSPLIB_DEBUG_ON())')==5
 nested=reports.replace('if (DSPLIB_DEBUG_ON())','if (DSPLIB_DEBUG_ON()) {')+'\n'+('\t}\n'*5)
 for owners,prefix,label in [(1,0,'direct-child-owners'),(0,1,'diagnostic-prefix'),(1,1,'owners-prefix')]:
  text=fn
  if prefix:text=text[:begin]+nested+text[finish:]
  if owners:
   for line in ['\tV90Phase2Info *p90;\n','\tV92Phase2Info *p92;\n','\tp90 = modem.phase2Info;\n','\tp92 = v92modem.phase2Info;\n']:
    assert text.count(line)==1;text=text.replace(line,'')
   text=text.replace('p90->','modem.phase2Info->').replace('p92->','v92modem.phase2Info->')
  cells[label]=source[:start]+text+source[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
