#!/usr/bin/env python3
"""Original second detector arm and forward countdown clear source."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=('src/pump/v22/v22prc.c',);d.OUT_NAME='batch100-v22-detect-clear'
def variants(path,s):
 a,z,f=d.function(s,'Detect_v22');out={}
 for arm,walk in ((False,False),(True,False),(False,True),(True,True)):
  label='baseline'if not(arm or walk)else'-'.join(x for yes,x in((arm,'zero-verdict-fallthrough'),(walk,'forward-countdown-clear'))if yes);x=f
  if arm:
   old='\t\tif (vb != 0)\n\t\t\trun_b = 0;\n\t\telse\n\t\t\trun_b = (short)(run_b + 1);'
   new='\t\tif (vb == 0)\n\t\t\trun_b = (short)(run_b + 1);\n\t\telse\n\t\t\trun_b = 0;';assert old in x;x=x.replace(old,new)
  if walk:
   old='\t\tif (run_a <= V22_DETECT_CLEAR_HOLD)\n\t\t\tfor (j = 0; j <= V22_DETECT_SUBBLOCK - 1; j++)\n\t\t\t\tchunk[j] = 0;'
   new='\t\tif (run_a <= V22_DETECT_CLEAR_HOLD) {\n\t\t\tshort *p = chunk;\n\n\t\t\tfor (j = V22_DETECT_SUBBLOCK; j-- != 0;)\n\t\t\t\t*p++ = 0;\n\t\t}';assert old in x;x=x.replace(old,new)
  out[label]=s[:a]+x+s[z:]
 assert len(set(out.values()))==4
 return out
d.variants=variants
if __name__=='__main__':d.main()
