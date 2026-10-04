#!/usr/bin/env python3
"""Observe installed Gentoo scratch selection without changing emitted objects."""
import playbook_small_patterns
import argparse,hashlib,json,shlex,subprocess,shutil
from pathlib import Path
import experiment_toolchain as tc
ROOT=Path(__file__).resolve().parents[1]
def run(command,folder,label):
 with (folder/(label+'.log')).open('w') as log:subprocess.run(command,cwd=folder,stdout=log,stderr=subprocess.STDOUT,check=True)
def validate_trace(trace,target):
 assert not trace['errors'] and trace['exit_codes']==[0] and trace['pending']==0,'incomplete compiler observation'
 events=trace['events']
 assert any(e['function']==target for e in events),'target detector did not fire'
 for event in events:
  assert 'selected' in event and 'eligibility' in event,'incomplete scratch event'
 for before,after in zip(events,events[1:]):
  assert before['cursor_after']==after['cursor_before'],'cursor discontinuity'
def self_test():
 fixture={'errors':[],'exit_codes':[0],'pending':0,'events':[{'function':'target','selected':1,'eligibility':{},'cursor_before':0,'cursor_after':2}]}
 validate_trace(fixture,'target')
 controls=[(fixture,'missing-target')]
 for field,value in [('errors',['observer failure']),('exit_codes',[1]),('pending',1),('events',[])]:
  controls.append((dict(fixture,**{field:value}),'target'))
 for trace,target in controls:
  try:validate_trace(trace,target)
  except AssertionError:pass
  else:raise AssertionError('invalid trace accepted')
 print('scratch trace controls: 1 accepted, 5 refused')
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--reproduction-dir',type=Path,default=ROOT/'build/gcc3-mechanism-fdsp');ap.add_argument('--target',default='FDSP_DP_Run');ap.add_argument('--self-test',action='store_true');ap.add_argument('--compiler-kind',choices=('cc1','cc1plus'),default='cc1');ap.add_argument('--out',type=Path,default=ROOT/'build/gcc3-mechanism-scratch');args=ap.parse_args()
 if args.self_test:self_test();return
 source=args.reproduction_dir.resolve();prior=json.loads((source/'results.json').read_text());config=prior['config'];image=config.splitlines()[0].split(' ',1)[1]
 out=args.out.resolve();out.mkdir(exist_ok=True);cc1=out/args.compiler_kind
 with cc1.open('wb') as f:subprocess.run(['docker','run','--rm','--platform','linux/386',image,'cat','/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/'+args.compiler_kind],stdout=f,check=True)
 cc1.chmod(0o755);nm=subprocess.check_output(['nm',str(cc1)],text=True);addresses={line.split()[-1]:int(line.split()[0],16) for line in nm.splitlines() if len(line.split())==3}
 assert 'search_ofs.0' in addresses,'unsupported compiler cursor symbol'
 results={'revision':prior['revision'],'config':config,'compiler_sha256':hashlib.sha256(cc1.read_bytes()).hexdigest(),'cells':{}}
 for family,fr in prior['families'].items():
  for label,cell in fr['cells'].items():
   inputs=source/family/label;folder=out/(family+'-'+label);folder.mkdir(exist_ok=True);lines=(inputs/'compile.log').read_text().splitlines()
   for preprocessed in inputs.glob('*.ii' if args.compiler_kind=='cc1plus' else '*.i'):shutil.copy2(preprocessed,folder/preprocessed.name)
   command=shlex.split(next(line for line in lines if '/'+args.compiler_kind+' -fpreprocessed' in line));assembler=shlex.split(next(line for line in lines if '/bin/as ' in line));command[0]=str(cc1)
   plain=list(command);plain[plain.index('-o')+1]='plain.s';run(plain,folder,'cc1-plain')
   observed=list(command);observed[observed.index('-o')+1]='observed.s'
   (folder/'observer-settings.json').write_text(json.dumps({'target':args.target,'cursor_address':addresses['search_ofs.0']}))
   gdb=['gdb','-q','-batch','-ex','set pagination off','-ex','source '+str(ROOT/'tools/gcc3_mechanism_scratch_gdb.py'),'--args']+observed
   run(gdb,folder,'gdb-scratch')
   stem=Path(fr['source_path']).stem
   assert (inputs/(stem+'.s')).read_bytes()==(folder/'plain.s').read_bytes()==(folder/'observed.s').read_bytes(),'tracing changed assembly'
   assembler[assembler.index('-o')+1]='/work/'+family+'-'+label+'/observed.o';assembler[-1]='observed.s'
   assemble=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote('/work/'+family+'-'+label)+' && '+shlex.join(assembler)];run(assemble,folder,'assemble-observed')
   assert (folder/'observed.o').read_bytes()==(inputs/'candidate.o').read_bytes(),'tracing changed object'
   events=json.loads((folder/'scratch-observe.json').read_text());validate_trace(events,args.target)
   results['cells'][family+'/'+label]={'object_sha256':cell['object_hash'],'commands':[plain,gdb,assemble],'raw_object_unchanged':True,'trace':events}
   (out/('results-'+source.name+'.json')).write_text(json.dumps(results,indent=2)+'\n')
   (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(family,label,len(events['events']),'observational events; raw object unchanged',flush=True)
 print('observed cells:',len(results['cells']))
if __name__=='__main__':main()
