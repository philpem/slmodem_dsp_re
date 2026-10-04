#!/usr/bin/env python3
import playbook_small_patterns as d
import gcc3_batch100_v90_std_verdict as parent
d.OUT_NAME='gcc3-batch100-v90-std-fraction'
def variants(path,source):
 cells={'baseline':source,'early-verdict-seed':parent.variants(path,source)['verdict-before-diagnostic']}
 for label,s in list(cells.items()):
  start,end,fn=d.function(s,'V90ConnectionEvaluator::evaluateMeanErrorStdPhase3')
  old='\t\tint frac = (int)((std - (float)(int)std) * 10000.0f);\n';assert fn.count(old)==1;fn=fn.replace(old,'')
  old='(frac < 0) ? -frac : frac';assert fn.count(old)==1;fn=fn.replace(old,'__builtin_abs((int)((std - (float)(int)std) * 10000.0f))')
  cells['argument-fraction'+('-early-verdict' if label!='baseline' else '')]=s[:start]+fn+s[end:]
 return cells
d.variants=variants
if __name__=='__main__':d.main()
