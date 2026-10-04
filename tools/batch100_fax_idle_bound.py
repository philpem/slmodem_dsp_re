#!/usr/bin/env python3
"""Known named-bound GCC3 lever transferred to the separate fixed idle arm."""
import playbook_small_patterns as d
from batch100_fax_class1_state_cfg import variants as predecessor
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/class1.c',);d.OUT_NAME='batch100-fax-idle-bound'
def variants(path,s):
 x=predecessor(path,s)['tone-fallthrough-literal-idle-arms'];a,z,f=d.function(x,'_idle_state');old='\tfor (i = 0; i < CLASS1_BLOCK_SAMPLES; i++)';assert f.count(old)==1;new=f.replace(old,'\tn = CLASS1_BLOCK_SAMPLES;\n\tfor (i = 0; i < n; i++)');return{'baseline':s,'split-idle-named-bound':x[:a]+new+x[z:]}
d.variants=variants
if __name__=='__main__':d.main()
