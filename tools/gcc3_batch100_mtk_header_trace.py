#!/usr/bin/env python3
"""Preserve Gentoo header implementation and preprocessed controlled sources."""
import json,subprocess
import playbook_small_patterns as d
root=d.ROOT/'build/gcc3-batch100-mtk-inline-header';r=json.loads((root/'results.json').read_text());trace={}
for label,cell in r['families']['PHASOR']['cells'].items():
 command=list(cell['command']);assert command[-1].count('exec gcc -c ')==1
 command[-1]=command[-1].replace('exec gcc -c ','exec gcc -E ',1).replace(' -da ',' ').replace('/candidate.o ','/candidate.i ')
 target=root/'PHASOR'/label/'preprocess.log'
 with target.open('w') as f:subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,check=True)
 p=root/'PHASOR'/label/'candidate.i';s=p.read_text();has_inline='1:\\tfprem' in s or '1:\tfprem' in s
 assert has_inline==label.startswith('inline-'),label
 trace[label]={'command':command,'inline_fmodf_asm_present':has_inline,'preprocessed_bytes':p.stat().st_size}
image=r['config'].splitlines()[0].split(' ',1)[1]
for name in ('bits/mathinline.h','math.h'):
 command=d.tc.docker_prefix(image,d.ROOT,root,True)+['/bin/sh','-c','cat /usr/include/'+name]
 with (root/('gentoo-'+name.replace('/','-'))).open('w') as f:subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,check=True)
trace['header_provenance']='Gentoo image original system include/math.h and bits/mathinline.h; fmodf asm enabled via __FAST_MATH__ and GCC<3.5'
(root/'header-trace.json').write_text(json.dumps(trace,indent=2)+'\n')
print('4/4 preprocessed controls validate inline-math selection; complete Gentoo math/mathinline headers preserved')
