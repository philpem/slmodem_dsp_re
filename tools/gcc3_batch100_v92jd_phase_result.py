#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V92Jd.cpp',);d.OUT_NAME='gcc3-batch100-v92jd-phase-result'
def variants(path,source):
 start,end,fn=d.function(source,'V92Jd::getJdPhase');old='\treturn (float)phase / 65536.0f;';assert fn.count(old)==1
 fn=fn.replace(old,'\tfloat result = (float)phase / 65536.0f;\n\treturn result;')
 return {'baseline':source,'float-result':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
