#!/usr/bin/env python3
"""Characterise the bounded source-C dialect difference without adopting flags."""
from pathlib import Path
import argparse
import sys, json, shlex, hashlib, subprocess, difflib, collections
ROOT=Path(__file__).resolve().parents[1]
import os
os.chdir(ROOT)
sys.path.insert(0,str(ROOT/'tools'))
import playbook_small_patterns as d
import experiment_toolchain as tc
from gcc3_value_carriers_audit import inspect
b=d.b
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-dir',type=Path,required=True)
parser.add_argument('--period-dir',type=Path,help='optionally require raw reproduction of these period objects')
parser.add_argument('--revision',default='13f477a9')
parser.add_argument('--out',type=Path,default=ROOT/'build/period-dialect-audit')
args=parser.parse_args()
OUT=args.out.resolve();OUT.mkdir(parents=True,exist_ok=True)
BASE=args.baseline_dir.resolve()
header_paths=subprocess.check_output(['git','ls-files','--','include','src','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).splitlines()
headers={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in header_paths if p.endswith('.h')}
config=(BASE/'.build-config').read_text()
image=config.splitlines()[0].split(' ',1)[1]
flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
paths=['src/fax/V17t_prc.c','src/fax/class1.c','src/pump/v34/V34hshak.c']
assert not subprocess.check_output(['git','diff',args.revision,'--','src','include','tools/toolchain/period_compat.h'],text=True).strip()
result={'baseline_revision':args.revision,'config':config,'hypothesis':'Adding only -std=gnu99 to the canonical C profile reproduces all three observed period objects.','falsifier':'Any unchanged baseline fails raw reproduction, or any gnu99 object differs from its period object.','declared_cells':6,'source_headers_unchanged':True,'header_hashes':headers,'sources':{}}
with (OUT/'toolchain-identity.log').open('w') as log:
    cmd=tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c','export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH; command -v gcc; gcc --version; gcc -dumpmachine; dialect_as=$(gcc -print-prog-name=as); printf "assembler: %s\\n" "$dialect_as"; "$dialect_as" --version']
    result['identity_command']=cmd;subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
for source in paths:
    family=Path(source).stem;folder=OUT/family;folder.mkdir(exist_ok=True)
    rawbaseline=BASE/(source.replace('/','_')+'.o')
    period=args.period_dir/(source.replace('/','_').rsplit('.',1)[0]+'.o') if args.period_dir else None
    row={'source_hash':hashlib.sha256((ROOT/source).read_bytes()).hexdigest(),'cells':{}}
    result['sources'][source]=row
    for label,extra in [('baseline',[]),('gnu99',['-std=gnu99'])]:
        out=folder/(label+'.o');shell=tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+extra,'/work/'+family+'/'+label+'.o',source)
        cmd=tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c',shell]
        with (folder/(label+'.compile.log')).open('w') as log:
            code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        row['cells'][label]={'command':cmd,'compile_exit':code,'sha256':hashlib.sha256(out.read_bytes()).hexdigest() if out.exists() else None}
        (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n');assert code==0,(source,label)
        assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==value for p,value in headers.items()),'header drift'
        assert hashlib.sha256((ROOT/source).read_bytes()).hexdigest()==row['source_hash'],'source drift'
        if label=='baseline':
            row['raw_baseline_reproduced']=out.read_bytes()==rawbaseline.read_bytes();assert row['raw_baseline_reproduced'],source
    a=folder/'baseline.o';c=folder/'gnu99.o';ai=inspect(a);ci=inspect(c)
    names=sorted(b.sizes(str(a)));other=sorted(b.sizes(str(c)))
    common=sorted(set(names)&set(other));changed=[n for n in common if b.body(str(a),n)!=b.body(str(c),n)]
    row.update(gnu99_raw_equal_period=c.read_bytes()==period.read_bytes() if period else None,shared_bodies=len(common),new_bodies=sorted(set(other)-set(names)),missing_bodies=sorted(set(names)-set(other)),changed_bodies=changed,body_sizes={n:[b.sizes(str(a))[n],b.sizes(str(c))[n]] for n in changed},invariant_equal={key:ai[key]==ci[key] for key in ['records','allocated','nobits','relocations']},metadata_delta={n:[ai['records'].get(n),ci['records'].get(n)] for n in sorted(set(ai['records'])|set(ci['records'])) if ai['records'].get(n)!=ci['records'].get(n)})
    row['body_grades']={n:{'baseline':b.verdict(*b.body(b.BLOB,n),*b.body(str(a),n)),'gnu99':b.verdict(*b.body(b.BLOB,n),*b.body(str(c),n))} for n in common if n in b.sizes(b.BLOB)}
    baseline_exact=sorted(n for n,v in row['body_grades'].items() if v['baseline'][0]=='EXACT')
    gnu99_exact=sorted(n for n,v in row['body_grades'].items() if v['gnu99'][0]=='EXACT')
    row.update(original_shared_body_denominator=len(row['body_grades']),baseline_exact=baseline_exact,gnu99_exact=gnu99_exact,exact_gains=sorted(set(gnu99_exact)-set(baseline_exact)),exact_losses=sorted(set(baseline_exact)-set(gnu99_exact)))
    for label,path in [('baseline',a),('gnu99',c)]:
        (folder/(label+'.elf-audit.json')).write_text(json.dumps(inspect(path),indent=2,default=lambda v:v.hex() if isinstance(v,bytes) else str(v))+'\n')
        for name in changed:
            output=subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),str(path),name],text=True)
            (folder/(label+'.'+name+'.dis')).write_text(output)
    assert row['gnu99_raw_equal_period'] is not False,source
    (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n')
result['compiled_cells']=sum(len(r['cells']) for r in result['sources'].values())
result['shared_body_denominator']=sum(r['shared_bodies'] for r in result['sources'].values())
(OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n')


# Preprocess the same six source/profile cells; no additional object variants.
root=ROOT;out=OUT;r=result;records={}
for src in r['sources']:
 family=Path(src).stem
 for label,extra in [('baseline',[]),('gnu99',['-std=gnu99'])]:
  cmd=tc.docker_prefix(image,root,out,True)+['/bin/sh','-c','export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH; exec gcc -E '+shlex.join(tc.add_reproduce_bugs(flags+extra))+' '+shlex.quote(src)]
  with (out/family/(label+'.i')).open('w') as log:subprocess.run(cmd,stdout=log,check=True)
  records[family+'/'+label]=cmd
 a=(out/family/'baseline.i').read_text().splitlines();b=(out/family/'gnu99.i').read_text().splitlines();delta=list(difflib.unified_diff(a,b));(out/family/'preprocess.diff').write_text('\n'.join(delta)+'\n');print(family,'preprocess diff lines',len(delta));print('\n'.join(delta[:95]))
(out/'preprocess-commands.json').write_text(json.dumps(records,indent=2)+'\n')

b=d.b
report={}
for src,row in r['sources'].items():
 folder=out/Path(src).stem;a=json.loads((folder/'baseline.elf-audit.json').read_text());c=json.loads((folder/'gnu99.elf-audit.json').read_text());item={'preprocessed_source_identical':(folder/'baseline.i').read_bytes()==(folder/'gnu99.i').read_bytes(),'metadata_binding_equal':a['records']==c['records'],'NOBITS_equal':a['nobits']==c['nobits'],'changed_data':{},'changed_body_relocations':{}}
 for section in set(a['allocated'])|set(c['allocated']):
  if a['allocated'].get(section)==c['allocated'].get(section):continue
  x=bytes.fromhex(a['allocated'][section]);y=bytes.fromhex(c['allocated'][section]);item['changed_data'][section]={'bytes':[len(x),len(y)],'equal_NUL_string_multiset':collections.Counter(x.split(b'\0'))==collections.Counter(y.split(b'\0'))}
 normalized=lambda values:collections.Counter(str((v[0],v[2],v[3][:-1] if v[3][0]=='audited-code-destination' else v[3])) for v in values)
 item['nontext_relocation_count']=[len(a['relocations']),len(c['relocations'])];item['nontext_relocation_destination_owners_equal']=normalized(a['relocations'])==normalized(c['relocations'])
 for name in row['changed_bodies']:
  x=b.body(str(folder/'baseline.o'),name)[1];y=b.body(str(folder/'gnu99.o'),name)[1];calls=lambda relocs:collections.Counter(repr(target) for kind,target in relocs.values() if kind=='R_386_PC32');targets=lambda relocs:collections.Counter(repr((kind,target)) for kind,target in relocs.values());cx,cy=calls(x),calls(y);tx,ty=targets(x),targets(y)
  item['changed_body_relocations'][name]={'relocation_counts':[len(x),len(y)],'removed_external_calls':dict(cx-cy),'added_external_calls':dict(cy-cx),'removed_relocation_targets':dict(tx-ty),'added_relocation_targets':dict(ty-tx)}
  aa=(folder/('baseline.'+name+'.dis')).read_text().splitlines();bb=(folder/('gnu99.'+name+'.dis')).read_text().splitlines();(folder/(name+'.diff')).write_text('\n'.join(difflib.unified_diff(aa,bb))+'\n')
 report[src]=item
report['conclusion']='Six full-TU compiles; three raw canonical controls. The optional raw period comparisons are recorded in results.json. Identical preprocessed sources exclude conditional header expansion as the cause. Precise compiler lowering/optimization mechanism remains unresolved. No profile adopted.'
report['scope']={'TUs':len(result['sources']),'cells':result['compiled_cells'],'shared_bodies':result['shared_body_denominator'],'changed_bodies':sum(len(row['changed_bodies']) for row in result['sources'].values()),'original_shared_bodies':sum(row['original_shared_body_denominator'] for row in result['sources'].values()),'exact_gains':{src:row['exact_gains'] for src,row in result['sources'].items()},'exact_losses':{src:row['exact_losses'] for src,row in result['sources'].items()}}
report['profile_status']='Characterization only. Source/test dialect alignment requires distinct profile cache/stamps and a full period differential run before adoption.'
(out/'complete-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

assert result['compiled_cells']==6
assert result['shared_body_denominator']>0
assert all(row['raw_baseline_reproduced'] and row['gnu99_raw_equal_period'] is not False for row in result['sources'].values())
assert all(row['preprocessed_source_identical'] for key,row in report.items() if key in result['sources'])
changed=sum(len(row['changed_bodies']) for row in result['sources'].values())
print('period dialect audit:6/6 compiles;3/3 raw canonical controls;%d shared bodies;%d changed;%d unchanged;3/3 identical preprocess controls;no profile adopted' % (result['shared_body_denominator'],changed,result['shared_body_denominator']-changed))

period_checks=sum(row['gnu99_raw_equal_period'] is not None for row in result['sources'].values())
period_matches=sum(row['gnu99_raw_equal_period'] is True for row in result['sources'].values())
original=sum(row['original_shared_body_denominator'] for row in result['sources'].values())
gains=sum(len(row['exact_gains']) for row in result['sources'].values())
losses=sum(len(row['exact_losses']) for row in result['sources'].values())
print('period raw controls:%d/%d matched;original bodies:%d;exact gains:%d;exact losses:%d' % (period_matches,period_checks,original,gains,losses))
