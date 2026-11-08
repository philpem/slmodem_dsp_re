#!/usr/bin/env python3
"""Validate prior F11741 whole-config capture, without reopening its palette.

Default compiles pinned baseline/copy using immutable Gentoo baseline inputs.
Then --audit or --period. Mutation validation from the original study is
archived; no production source is generated implicitly.
"""
import argparse,json,sys,subprocess,shutil,hashlib,os,re
from pathlib import Path
import playbook_small_patterns as d
from batch20_v22_fse_copy_reproduce import variants
from recovery_fax_debug_audit import Object
ROOT=d.ROOT;OUT=ROOT/'build/v22-fse-capture293';REV='6b16d64d90891138f9891067bb3e41cad136853d';SOURCE='src/pump/v22/v22_fse.c'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def ordering(ins):
    cfg=next(a.split(',')[-1] for op,a in ins[:15] if op=='mov' and a.startswith('0x24(%esp),'))
    state=next(a.split(',')[-1] for op,a in ins[:15] if op=='mov' and a.startswith('0x20(%esp),'))
    reads=[i for i,(op,a) in enumerate(ins[:25]) if op=='mov' and a.split(',')[0] in ('('+cfg+')','0x4('+cfg+')')]
    writes=[i for i,(op,a) in enumerate(ins[:25]) if op=='mov' and a.split(',')[-1] in ('('+state+')','0x4('+state+')')]
    assert len(reads)==len(writes)==2
    return {'cfg_register':cfg,'state_register':state,'reads':reads,'stores':writes,'both_reads_before_either_store':max(reads)<min(writes)}

def audit():
    report={'prior':'F11741 / 7b436a0ef; withheld for strict identity, not measured runtime contradiction','cells':{}};base=OUT/'v22_fse/baseline/candidate.o';bc=Object(base).canonical();sizes=d.b.sizes(str(base))
    for label in ('baseline','copy'):
        p=OUT/'v22_fse'/label/'candidate.o';c=Object(p).canonical();assert c[1:]==bc[1:],'data/binding/relocation drift';assert set(c[0])==set(bc[0])
        report['cells'][label]={'sha256':sha(p),'function_count':len(sizes),'verdicts':{n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(p),n))) for n in sizes},'changed_bodies':[n for n in sizes if d.b.body(str(p),n)!=d.b.body(str(base),n)],'data_bindings_relocation_consumers_equal':True,'target_instructions':d.b.insns(str(p),'V22_FSE_init')}
    report['native_target_instructions']=d.b.insns(d.b.BLOB,'V22_FSE_init');report['ordering']={'native':ordering(report['native_target_instructions']),**{k:ordering(v['target_instructions']) for k,v in report['cells'].items()}};assert report['ordering']['native']['both_reads_before_either_store'] and report['ordering']['copy']['both_reads_before_either_store'] and not report['ordering']['baseline']['both_reads_before_either_store'];(OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print([(k,v['verdicts']['V22_FSE_init'],v['changed_bodies']) for k,v in report['cells'].items()])
def gate(labels):
    stable=ROOT/'build/period';report={}
    for label in labels:
        folder=OUT/('period-'+label);folder.mkdir(exist_ok=True)
        for p in list(stable.glob('src_*.o'))+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v22_fse.o',stable/'test_unit_t_v22rate.o']:shutil.copy2(p,folder/p.name)
        obj=folder/'src_pump_v22_v22_fse.o';shutil.copy2(OUT/'v22_fse'/label/'candidate.o',obj);digest=sha(obj);cmd=['make','period','T=t_v22_fse t_v22rate','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert sha(obj)==digest,'candidate overwritten';logs={n:(folder/(n+'.run.log')).read_text() for n in ('t_v22_fse','t_v22rate')};assert status==0,(label,status)
        report[label]={'command':cmd,'exit':status,'object_sha256':digest,'logs':logs,'checks':{n:sum(map(int,re.findall(r'(\d+) checks',t))) for n,t in logs.items()}};print(label,status,report[label]['checks'],flush=True)
    (OUT/('mutant-period.json' if 'mutant' in labels else 'period.json')).write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--audit',action='store_true');ap.add_argument('--period',action='store_true');args=ap.parse_args()
    if args.audit:audit()
    elif args.period:gate(('baseline','copy'))
    else:
        manifest={'revision':REV,'source_sha256':hashlib.sha256(subprocess.check_output(['git','show',REV+':'+SOURCE],cwd=ROOT)).hexdigest(),'frozen':{name:sha(OUT/'frozen'/name) for name in ('.build-config','src_pump_v22_v22_fse.c.o')}}
        if (OUT/'validation-pin.json').exists():assert json.loads((OUT/'validation-pin.json').read_text())==manifest,'frozen input drift'
        else:(OUT/'validation-pin.json').write_text(json.dumps(manifest,indent=2)+'\n')
        d.REV=REV;d.SOURCE_PATHS=(SOURCE,);d.OUT_NAME=OUT.name;d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;sys.argv=[sys.argv[0],'--domain','F11741 prior baseline/copy validation only','--baseline-dir',str(OUT/'frozen')];d.main();audit()
