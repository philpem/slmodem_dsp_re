#!/usr/bin/env python3
"""Eleven diagnostic TUs: known-callee alignment, definition availability, raw replay."""
import hashlib,json,shlex,shutil,subprocess
from pathlib import Path
import experiment_toolchain as tc
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/gcc3-alignment-micro'
def run(cmd,folder,label):
 with (folder/(label+'.log')).open('w') as log:subprocess.run(cmd,cwd=folder,stdout=log,stderr=subprocess.STDOUT,check=True)
def main():
 OUT.mkdir(exist_ok=True)
 config=(ROOT/'build/production-before/.build-config').read_text();lines=dict(x.split(' ',1) for x in config.splitlines() if x.strip());image=lines['image']
 flags=shlex.split(lines['flags'])+shlex.split(lines['cxx'])
 flags=[x.replace('-Iinclude','-I/src/include').replace('tools/toolchain/period_compat.h','/src/tools/toolchain/period_compat.h') for x in flags]+['-DDSPLIB_REPRODUCE_BUGS','-v','-save-temps']
 compiler=OUT/'cc1plus'
 with compiler.open('wb') as stream:
  subprocess.run(['docker','run','--rm','--platform','linux/386',image,'cat','/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1plus'],stdout=stream,check=True)
 compiler.chmod(0o755)
 prefix=tc.docker_prefix(image,ROOT,OUT,True)
 run(prefix+['/bin/sh','-c','set -e; export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; g++ --version; a=$(g++ -print-prog-name=as); "$a" --version'],OUT,'identity')
 bodies={'leaf':'return x + 1;', 'double':'volatile double slot = x; return (int)slot;', 'double-aligned':'volatile double slot __attribute__((aligned(8))) = x; return (int)slot;', 'library':'return align_external(x) + 1;', 'weak':'return x + 1;', 'template':'return x + 1;'}
 result={'config':config,'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'cells':{}}
 for kind,body in bodies.items():
  for order in (('before',) if kind=='template' else ('before','after')):
   label=kind+'-'+order;folder=OUT/label;folder.mkdir(exist_ok=True)
   decl='extern "C" int align_external(int);\nextern "C" __attribute__((noinline)) int align_callee(int);\n'
   callee='extern "C" __attribute__((noinline)) int align_callee(int x) { '+body+' }\n'
   caller='extern "C" __attribute__((noinline)) int align_caller(int x) { return align_callee(x) + x; }\n'
   if kind=='weak':
    decl=decl.replace('((noinline))','((weak,noinline))');callee=callee.replace('((noinline))','((weak,noinline))')
   if kind=='template':
    decl='template<class T> __attribute__((noinline)) int align_template(T);\n'
    callee='template<class T> __attribute__((noinline)) int align_template(T x) { return x+1; }\ntemplate int align_template<int>(int);\n'
    caller=caller.replace('align_callee(x)','align_template<int>(x)')
   (folder/'mini.cpp').write_text(decl+(callee+caller if order=='before' else caller+callee))
   command=prefix+['/bin/sh','-c','cd /work/'+label+' && export PATH=/usr/i386-pc-linux-gnu/gcc-bin/3.4:$PATH; exec g++ '+shlex.join(flags)+' -c mini.cpp -o driver.o']
   run(command,folder,'driver')
   log=(folder/'driver.log').read_text().splitlines();cc=shlex.split(next(x for x in log if '/cc1plus -fpreprocessed' in x));asm=shlex.split(next(x for x in log if '/bin/as ' in x));cc[0]=str(compiler)
   plain=list(cc);plain[plain.index('-o')+1]='plain.s';run(plain,folder,'plain')
   observed=list(cc);observed[observed.index('-o')+1]='observed.s';gdb=['gdb','-q','-batch','-ex','set pagination off','-ex','source '+str(ROOT/'tools/gcc3_alignment_micro_gdb.py'),'--args']+observed;run(gdb,folder,'gdb')
   assert (folder/'mini.s').read_bytes()==(folder/'plain.s').read_bytes()==(folder/'observed.s').read_bytes()
   asm[asm.index('-o')+1]='observed.o';asm[-1]='observed.s';assemble=prefix+['/bin/sh','-c','cd /work/'+label+' && '+shlex.join(asm)];run(assemble,folder,'assembler');assert (folder/'driver.o').read_bytes()==(folder/'observed.o').read_bytes()
   defined=subprocess.check_output(['nm','--defined-only',str(folder/'driver.o')],text=True).splitlines();functions=[line.split()[-1] for line in defined if line.split()[-2] in ('T','t','W')];assert len(functions)==2 and 'align_caller' in functions
   trace=json.loads((folder/'stack-observe.json').read_text());assert trace['exit_codes']==[0] and not trace['errors'] and not trace['pending'] and trace['frames'] and trace['callees']
   result['cells'][label]={'commands':[command,plain,gdb,assemble],'raw_assembly_and_objects_identical':True,'defined_function_names':functions,'trace':trace,'object_sha256':hashlib.sha256((folder/'driver.o').read_bytes()).hexdigest()}
   (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');print(label, 'raw replay verified', flush=True)
 for label, cell in result['cells'].items():
  trace=cell['trace'];interposable=label.startswith(('weak-','template-'));expected=128 if label.startswith(('library-','weak-','template-')) else 32
  transfer=[e for e in trace['callees'] if e['caller']=='align_caller' and e['callee'] in ('align_callee','align_template')]
  assert any(e['asm_written']==0 and e['known_incoming_boundary'] is None for e in transfer)
  assert any(e['asm_written']==1 and e['known_incoming_boundary']==(0 if interposable else expected) for e in transfer)
  assert all(f['preferred_stack_boundary']==expected for f in trace['frames'] if f['function']=='align_caller')
  binding=[e for e in trace['bindings'] if e['function'] in ('align_callee','align_template')]
  assert binding and all(e['binds_local']==(not interposable) for e in binding)
  assert all(not e['shlib'] for e in binding)
  if label.startswith('library-'):
   unknown=[e for e in trace['callees'] if e['callee']=='align_external'];assert unknown and all(e['asm_written']==0 and e['known_incoming_boundary'] is None for e in unknown)
 for kind in bodies:
  if kind=='template':continue
  assert (OUT/(kind+'-before')/'driver.o').read_bytes()==(OUT/(kind+'-after')/'driver.o').read_bytes()
 result['summary']={'valid_TUs':11,'defined_functions_per_TU':2,'observer_raw_replays':11,'order_raw_equal_pairs':5,'leaf_transfer_bits':32,'double_slot_transfer_bits':32,'library_transfer_bits':128,'early_unwritten_refusal_TUs':11,'unknown_external_refusal_TUs':2,'known_zero_binding_controls':3}
 (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result['summary']))
if __name__=='__main__':main()
