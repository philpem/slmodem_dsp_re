#!/usr/bin/env python3
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v90/V90ConnectionEvaluator.cpp',);d.OUT_NAME='gcc3-batch100-v90-std-verdict'
def variants(path,source):
 start,end,fn=d.function(source,'V90ConnectionEvaluator::evaluateMeanErrorStdPhase3')
 assert fn.count('\t\tret = 5;')==1
 fn=fn.replace('\t\tret = 5;\n','').replace('\t\tint frac =','\t\tret = 5;\n\t\tint frac =',1)
 return {'baseline':source,'verdict-before-diagnostic':source[:start]+fn+source[end:]}
d.variants=variants
if __name__=='__main__':d.main()
