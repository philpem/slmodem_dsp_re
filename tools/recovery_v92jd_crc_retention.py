#!/usr/bin/env python3
"""Validate the previously unretained F11837 V92 CRC structural recovery.

Two prior endpoints only: pointer-helper/rolled versus direct-member/expanded.
No new source palette or flags. Prediction: expanded member CRC promotes all
16words and increases packer cost enough to preserve native wrapper calls.
Native TU filename/visibility is not inferred from the missing baseline call.
Compile: python3 tools/recovery_v92jd_crc_retention.py --baseline-dir build/copy293-baseline
Then --audit or --period. Source stays isolated until parent coordination.
"""
import sys,json,hashlib,shutil,subprocess,re,os,argparse
from pathlib import Path
import playbook_small_patterns as d
import batch_cpp_v92jd_crc_reproduce as prior
from recovery_fax_debug_audit import Object
ROOT=d.ROOT;REV='0ffc6b0952100b7cd1b12df77b1bee7fd6069bd0';OUT=ROOT/'build/v92jd-crc-retention293';PREFIX='_ZN5V92Jd'
TARGETS=[PREFIX+x for x in ('10packJdDataEv','15packJdPhaseDataEv','14getJdBitVectorEv','19getJdPhaseBitVectorEv')]
def variants(path,source):
    cells=prior.variants(path,source)
    return {k:cells[k] for k in ('baseline','direct-members-expanded')}
def audit():
    ledger=json.loads((OUT/'results.json').read_text());family=OUT/'V92Jd';bp=family/'baseline/candidate.o';base=Object(bp).canonical();report={'prior_credit':'F11837 / durable7113753ad docs/batch-cpp-v92jd-crc-results.md; prior runtime/mutation unexecuted, no contradictory source evidence reported.','cells':{}}
    for label in variants('',subprocess.check_output(['git','show',REV+':src/pump/v90/V92Jd.cpp'],cwd=ROOT,text=True)):
        folder=family/label;path=folder/'candidate.o';c=Object(path).canonical();assert c[1]==base[1] and c[2]==base[2],'data/bindings';assert all(c[3].get(n)==v for n,v in base[3].items() if n not in TARGETS),'unrelated relocation consumers';assert all(n in TARGETS or n in base[3] for n in c[3]),'added unrelated consumers';assert set(c[0])==set(base[0])
        text='\n'.join(p.read_text() for p in folder.glob('*.00.cgraph'));record={'function_count':len(c[0]),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'changed_bodies':[n for n in c[0] if d.b.body(str(path),n)!=d.b.body(str(bp),n)],'data_equal':True,'bindings_equal':True,'nontext_consumers_equal':True,'cost_records':sorted(set(re.findall(r'^.*V92Jd::.*insns.*$',text,re.M))),'targets':{}}
        for n in TARGETS:
            record['targets'][n]={'verdict':list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(path),n))),'native_instructions':d.b.insns(d.b.BLOB,n),'candidate_instructions':d.b.insns(str(path),n)}
            target=record['targets'][n];target['instruction_counts']={'native':len(target['native_instructions']),'candidate':len(target['candidate_instructions'])};target['mnemonic_sequence_equal']=[x[0] for x in target['native_instructions']]==[x[0] for x in target['candidate_instructions']]
        report['cells'][label]=record
    (OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print('audit',[(k,v['function_count'],v['changed_bodies']) for k,v in report['cells'].items()])
def period():
    stable=ROOT/'build/period';report={}
    for label in ('baseline','direct-members-expanded'):
        folder=OUT/('period-'+label);folder.mkdir(exist_ok=True)
        for p in list(stable.glob('src_*.o'))+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v92jd.o']:
            shutil.copy2(p,folder/p.name)
        selected=folder/'src_pump_v90_V92Jd.o';shutil.copy2(OUT/'V92Jd'/label/'candidate.o',selected);digest=hashlib.sha256(selected.read_bytes()).hexdigest();cmd=['make','period','T=t_v92jd','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert hashlib.sha256(selected.read_bytes()).hexdigest()==digest,'candidate overwritten';text=(folder/'t_v92jd.run.log').read_text();assert status==0 and 'FAIL' not in text
        report[label]={'command':cmd,'exit':status,'checks':sum(map(int,re.findall(r'(\d+) checks',text))),'object_sha256':digest,'log':text,'boundary':'Existing component constructor/packer/unpacker fixtures, not modem end-to-end.'};(OUT/'period.json').write_text(json.dumps(report,indent=2)+'\n');print(label,'period',report[label]['checks'],flush=True)

def mutation_rows(source):
    rows=json.loads(subprocess.check_output(['git','show',REV+':test/mutations/v92jdstd.json'],cwd=ROOT,text=True));result=[];changed=[]
    for row in rows:
        if 'find' not in row:result.append(row);continue
        old=row;row=dict(row);label=row['label']
        if 'CRC tap ' in label:
            for vector,title in [('bits','data'),('phaseBits','phase')]:
                first='\t\t\tint t = '+vector+'[V90JD_GROUP1 + 1 + g * V90JD_GROUP + k] + crc[0];\n'
                tail='\t\t\tint tap3 = (crc[4] + t) & 1;\n\t\t\tint tap10 = (crc[11] + t) & 1;'
                find=first+tail;replace=find.replace('crc[4]','crc[5]') if 'tap 3 ' in label else find.replace('crc[11]','crc[12]')
                new=dict(row,label=label+' ('+title+')',find=find,replace=replace);assert source.count(find)==1;result.append(new);changed.append(new['label'])
            continue
        if source.count(row['find'])==0:
            vector='phaseBits' if 'phase' in label else 'bits'
            first='\t\t\tint t = '+vector+'[V90JD_GROUP1 + 1 + g * V90JD_GROUP + k] + crc[0];'
            group='for (g = 0; g <= 1; g++) {\n\t\tint k;\n\t\tfor (k = 0; k <= 15; k++) {\n'+first
            if 'covers only' in label:row['find']=group;row['replace']=group.replace('g <= 1','g <= 0')
            elif 'starts one bit' in label:row['find']=first;row['replace']=first.replace('GROUP1 + 1','GROUP1 + 2')
            elif 'preload all ones' in label:
                row['find']='for (i = 0; i <= 15; i++)\n\t\tcrc[i] = 1;\n\n\t'+group;row['replace']=row['find'].replace('crc[i] = 1','crc[i] = 0')
            else:raise AssertionError(label)
        assert source.count(row['find'])==1,(label,source.count(row['find']));result.append(row)
        if row!=old:changed.append(label)
    return result,changed

def mutations():
    source=variants('',subprocess.check_output(['git','show',REV+':src/pump/v90/V92Jd.cpp'],cwd=ROOT,text=True))['direct-members-expanded'];rows,changed=mutation_rows(source);(OUT/'v92jdstd-candidate.json').write_text(json.dumps(rows,indent=1)+'\n')
    stable=ROOT/'build/period';report={'changed_fault_labels':changed,'candidate_anchors':sum('find' in x for x in rows),'mutants':[]};ledger=json.loads((OUT/'results.json').read_text());template=ledger['families']['V92Jd']['cells']['direct-members-expanded']['command']
    # Existing driver uses command as a single Docker argv list.
    for index,row in enumerate(r for r in rows if r.get('label') in changed):
        label='mutant-'+str(index);folder=OUT/'V92Jd'/label;folder.mkdir(exist_ok=True);(folder/'V92Jd.cpp').write_text(source.replace(row['find'],row['replace']))
        command=[x.replace('/direct-members-expanded/','/'+label+'/') for x in template]
        with (folder/'compile.log').open('w') as log:subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
        target=OUT/('period-'+label);target.mkdir(exist_ok=True)
        for p in list(stable.glob('src_*.o'))+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v92jd.o']:shutil.copy2(p,target/p.name)
        selected=target/'src_pump_v90_V92Jd.o';shutil.copy2(folder/'candidate.o',selected);digest=hashlib.sha256(selected.read_bytes()).hexdigest();cmd=['make','period','T=t_v92jd','J=4']
        with (target/'gate.log').open('w') as log:status=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(target.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert hashlib.sha256(selected.read_bytes()).hexdigest()==digest;assert status!=0,'mutant survived';text=(target/'t_v92jd.run.log').read_text();assert 'FAIL' in text
        report['mutants'].append({'label':row['label'],'command':command,'period_command':cmd,'exit':status,'object_sha256':digest,'log':text});(OUT/'mutations.json').write_text(json.dumps(report,indent=2)+'\n');print('CAUGHT',row['label'],flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--mutations',action='store_true');ap.add_argument('--audit',action='store_true');ap.add_argument('--period',action='store_true');ap.add_argument('--baseline-dir',type=Path,default=ROOT/'build/copy293-baseline');args=ap.parse_args()
    if args.mutations:mutations()
    elif args.audit:audit()
    elif args.period:period()
    else:
        OUT.mkdir(exist_ok=True);frozen=OUT/'frozen';frozen.mkdir(exist_ok=True)
        for name in ('.build-config','src_pump_v90_V92Jd.cpp.o'):
            if not (frozen/name).exists():shutil.copy2(args.baseline_dir/name,frozen/name)
        args.baseline_dir=frozen
        manifest={'revision':REV,'source_sha256':hashlib.sha256(subprocess.check_output(['git','show',REV+':src/pump/v90/V92Jd.cpp'],cwd=ROOT)).hexdigest(),'baseline_config_sha256':hashlib.sha256((args.baseline_dir/'.build-config').read_bytes()).hexdigest(),'baseline_object_sha256':hashlib.sha256((args.baseline_dir/'src_pump_v90_V92Jd.cpp.o').read_bytes()).hexdigest(),'generator_sha256':hashlib.sha256(Path(prior.__file__).read_bytes()).hexdigest()}
        if (OUT/'manifest.json').exists():assert json.loads((OUT/'manifest.json').read_text())==manifest,'immutable input drift'
        else:(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        d.REV=REV;d.SOURCE_PATHS=('src/pump/v90/V92Jd.cpp',);d.OUT_NAME=OUT.name;d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants
        sys.argv=[sys.argv[0],'--domain','PriorF11837 CRC endpoints structural validation; no new palette','--baseline-dir',str(args.baseline_dir.resolve())];d.main();audit()
