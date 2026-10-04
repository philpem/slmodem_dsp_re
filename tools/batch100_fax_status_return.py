#!/usr/bin/env python3
"""Original report positive arm and named initial zero return."""
import playbook_small_patterns as d
d.REV='856c1ecb';d.SOURCE_PATHS=tuple('src/fax/'+n for n in ('V17t_stc.c','V21t_stc.c','V29t_stc.c','V17r_stc.c','V21r_stc.c','V29r_stc.c'));d.OUT_NAME='batch100-fax-status-return'
names={'V17t_stc.c':('V17TX_status','status'),'V21t_stc.c':('V21TX_status','st'),'V29t_stc.c':('V29TX_status','status'),'V17r_stc.c':('V17RX_status','status'),'V21r_stc.c':('V21RX_status','st'),'V29r_stc.c':('V29RX_status','status')}
def variants(path,s):
 n,arg=names[path.rsplit("/",1)[-1]];a,z,f=d.function(s,n)
 f=f.replace('{','{\n\tint result = 0;',1)
 old='if ('+arg+' == '+('NULL' if arg=='st' else '0')+')\n\t\treturn 0;'
 assert old in f,(n,old)
 f=f.replace(old,'if ('+arg+' != 0) {').replace('\treturn 1;','\t\tresult = 1;\n\t}\n\treturn result;')
 return {'baseline':s,'named-zero-positive-arm':s[:a]+f+s[z:]}
d.variants=variants
if __name__=='__main__':d.main()
