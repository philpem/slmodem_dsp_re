#!/usr/bin/env python3
"""Assert the actual wrapper byte/RTL boundary, without program execution."""
import json,re
import playbook_small_patterns as d
root=d.ROOT/'build/gcc3-batch100-fdsp-conversion-helpers/Beepgen/in-1-out-1'
name='FDSP_DP_Run'
def function_dump(path,name):
 text=path.read_text();marker=';; Function '+name
 assert text.count(marker)==1,(path,name,'dump missing/ambiguous function')
 return text.split(marker,1)[1].split(';; Function',1)[0]
x,xr=d.b.body(d.b.BLOB,name);y,yr=d.b.body(str(root/'candidate.o'),name)
assert len(x)==len(y)==138 and xr==yr
masked=set()
for off in xr:masked.update(range(off,off+4))
differences=[(i,a,b)for i,(a,b)in enumerate(zip(x,y))if i not in masked and a!=b]
assert differences==[(135,0x59,0x5a)],differences
assert d.b.alpha_why(d.b.insns(d.b.BLOB,name),d.b.insns(str(root/'candidate.o'),name))is None
stages={}
for stage in ['27.flow2','28.peephole2','30.rnreg','35.mach']:
 path=root/('Beepgen.c.'+stage);body=function_dump(path,name)
 tail=body.split('NOTE_INSN_EPILOGUE_BEG',1)[1]
 scan=body if stage=='35.mach'else tail
 scratch=bool(re.search(r'\(set \(reg:SI 1 dx\)\s*\(mem:SI \(reg/f:SI 7 sp\)',scan))
 assert scratch==(stage!='27.flow2'),(stage,'scratch boundary')
 if stage=='28.peephole2':assert 'REG_UNUSED (reg:SI 1 dx)'in tail
 stages[stage]={'scratch_pop_edx':scratch,'tail':tail}
try:function_dump(root/'Beepgen.c.28.peephole2','not_a_function')
except AssertionError:missing_refused=True
else:raise AssertionError('missing-function control did not fire')
(d.ROOT/'build/batch100-fdsp-scratch-trace.json').write_text(json.dumps({'function':name,'nonrelocated_differences':differences,'alpha_equivalent':True,'stage_controls':stages,'missing_function_refused':missing_refused},indent=2)+'\n')
print('138-byte original/candidate:1 nonrelocated byte at135, POP ECX/EDX; alpha equivalent;4 RTL stages and1 missing-function refusal; first scratch atpeephole2')
