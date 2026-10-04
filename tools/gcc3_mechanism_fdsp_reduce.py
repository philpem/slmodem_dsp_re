#!/usr/bin/env python3
"""Three diagnostic predecessor reductions; never reconstruction source changes."""
import playbook_small_patterns as d
import hashlib,json,shlex,subprocess
from pathlib import Path
import gcc3_batch100_fdsp_conversion_helpers as forms
forms.d.REV='9f1199b5'
ROOT=Path(__file__).resolve().parents[1]
source=subprocess.check_output(['git','show','9f1199b5:src/service/Beepgen.c'],cwd=ROOT,text=True)
helper=forms.variants('src/service/Beepgen.c',source)['in-1-out-1']
def definition(name):
 a,z,_=d.function(helper,name);a=helper.rfind('\n',0,a-1)+1
 return helper[a:z]+'\n'
includes='\n'.join(line for line in source.splitlines() if line.startswith('#include'))+'\n'
functions=['zFLTUTL_Float2Linear','zFLTUTL_Linear2Float']
config=(ROOT/'build/production-before/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
flags=shlex.split(next(line[6:]for line in config.splitlines()if line.startswith('flags ')))
flags=['-I/src/include'if x=='-Iinclude'else '/src/'+x if x=='tools/toolchain/period_compat.h'else x for x in flags]
out=ROOT/'build/gcc3-mechanism-fdsp-reduced';out.mkdir(exist_ok=True)
result={'revision':'9f1199b5','config':config,'diagnostic_only':True,'removed_exports_expected':True,'families':{'Beepgen':{'source_path':'src/service/Beepgen.c','cells':{}}}}
for label,names in [('helpers-only',functions+['FDSP_DP_Run']),('helpers-with-correlation',functions+['FindCorrelation','FDSP_DP_Run']),('full-minus-correlation',None)]:
 folder=out/'Beepgen'/label;folder.mkdir(parents=True,exist_ok=True)
 if names is None:
  a,z,_=d.function(helper,'FindCorrelation');a=helper.rfind('\n',0,a-1)+1;text=helper[:a]+helper[z:]
 else:text=includes+'\n'.join(definition(n)for n in names)
 path=folder/'Beepgen.c';path.write_text(text)
 virtual='/work/Beepgen/'+label;command=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(virtual)+' && '+d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,flags+['-v','-save-temps','-da'],virtual+'/candidate.o',virtual+'/Beepgen.c')]
 with (folder/'compile.log').open('w')as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
 obj=folder/'candidate.o';verdict=d.b.verdict(*d.b.body(d.b.BLOB,'FDSP_DP_Run'),*d.b.body(str(obj),'FDSP_DP_Run'))
 result['families']['Beepgen']['cells'][label]={'source_hash':hashlib.sha256(text.encode()).hexdigest(),'object_hash':hashlib.sha256(obj.read_bytes()).hexdigest(),'command':command,'emitted_functions':sorted(d.b.sizes(str(obj))),'target_verdict':verdict}
 (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(label,verdict,'diagnostic reduction only',flush=True)
