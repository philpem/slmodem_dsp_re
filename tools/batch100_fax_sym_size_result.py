#!/usr/bin/env python3
"""Bounded original shared-result source family in Class1 rate mapping."""
import re
import playbook_small_patterns as d
from batch100_fax_idle_bound import variants as state_variants
d.REV='856c1ecb';d.SOURCE_PATHS=('src/fax/class1.c',);d.OUT_NAME='batch100-fax-sym-size-result'
def variants(path,s):
 x=state_variants(path,s)['split-idle-named-bound'];a,z,f=d.function(x,'_sym_size');assert len(re.findall(r'\t\treturn [0-6];',f))==7
 new=f.replace('{\n\tswitch','{\n\tint result;\n\n\tswitch',1);new=re.sub(r'\t\treturn ([0-6]);',r'\t\tresult = \1;\n\t\tbreak;',new);assert new.endswith('\t}\n}');new=new[:-1]+'\treturn result;\n}'
 return{'baseline':s,'shared-result-switch':x[:a]+new+x[z:]}
d.variants=variants
if __name__=='__main__':d.main()
