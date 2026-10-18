#!/usr/bin/env python3
"""Prior dp_wrapper unsigned/reload endpoint across every header consumer.

Baseline versus the previously measured final endpoint only. Header/source are
pinned; frozen complete canonical inputs never follow later production changes.
"""
import argparse,json,sys,subprocess,shutil,hashlib,os,re,shlex
from pathlib import Path
import playbook_small_patterns as d
from contract_wrapper_unsigned_reproduce import variants as prior
from recovery_fax_debug_audit import Object
ROOT=d.ROOT;OUT=ROOT/'build/dp-wrapper-unsigned293';REV='dfc23b7d1';HEADER='include/dsplib/dp_wrapper.h';TARGET='src/core/dp_wrapper.c'
SOURCES=('src/core/dp_init.c',TARGET,'src/pump/b103/b103.c','src/pump/v22/v22.c','src/pump/v23/v23.c','src/pump/v32/V32int.c','src/pump/v32/V32mod.c','src/pump/v32/V32stc.c','src/pump/v32/v32.c')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def variants(path,source):return {'baseline':source,'unsigned-fragment':prior(path,source)['unsigned-1-reload-1'] if path==TARGET else source}
def overlay(path,label):
    if label=='baseline':return {}
    text=subprocess.check_output(['git','show',REV+':'+HEADER],cwd=ROOT,text=True);assert text.count('\tint host_frag;')==1
    return {'dsplib/dp_wrapper.h':text.replace('\tint host_frag;','\tunsigned int host_frag;')}
def consumers():
    files=[p for tree in ('src','include') for p in (ROOT/tree).rglob('*') if p.suffix in ('.h','.c','.cpp')];edges={}
    for p in files:
        deps=[]
        for name in re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]',p.read_text(),re.M):
            for q in (p.parent/name,ROOT/'include'/name):
                if q.exists():deps.append(q.resolve());break
        edges[p.resolve()]=deps
    affected={ROOT/HEADER}
    while True:
        added={p for p,ds in edges.items() if any(q in affected for q in ds)}-affected
        if not added:break
        affected|=added
    result=sorted(str(p.relative_to(ROOT)) for p in affected if p.suffix in ('.c','.cpp'));assert set(result)==set(SOURCES);return result

def witness(ins):
    divisions=[]
    for i,(op,a) in enumerate(ins):
        if op not in ('div','divl','idiv','idivl'):continue
        call=max(j for j,(m,operand) in enumerate(ins[:i]) if m=='call');window=ins[call:i+1]
        divisions.append({'instruction':i,'operation':op,'last_call':ins[call],'post_call_fragment_loads':[x for x in window if x[0]=='mov' and '0x314(' in x[1]],'window':window})
    return {'unsigned_min_branches':sum(op=='jbe' for op,a in ins),'unsigned_fragment_branches':sum(op=='jb' for op,a in ins),'divisions':divisions,'two_unsigned_divisions_with_post_copy_fragment_loads':len(divisions)==2 and all(x['operation'] in ('div','divl') and 'sysdep_memcpy' in x['last_call'][1] and x['post_call_fragment_loads'] for x in divisions)}

def audit():
    ledger=json.loads((OUT/'results.json').read_text());report={'revision':REV,'production_header_consumers':consumers(),'families':{}};total=0
    for path in SOURCES:
        family=Path(path).stem;base=OUT/family/'baseline/candidate.o';candidate=OUT/family/'unsigned-fragment/candidate.o';bc=Object(base).canonical();c=Object(candidate).canonical();assert c[1:]==bc[1:],'data/binding/relocation drift '+path;assert set(c[0])==set(bc[0]);sizes=d.b.sizes(str(base));changed=[n for n in sizes if d.b.body(str(base),n)!=d.b.body(str(candidate),n)];total+=2*len(sizes)
        if path!=TARGET:assert base.read_bytes()==candidate.read_bytes(),'header consumer changed '+path
        record={'body_count_per_cell':len(sizes),'raw_header_consumer_equal':base.read_bytes()==candidate.read_bytes(),'changed_bodies':changed,'data_bindings_relocation_consumers_equal':True,'verdicts':{label:{n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(p),n))) for n in sizes} for label,p in [('baseline',base),('unsigned-fragment',candidate)]}}
        if path==TARGET:
            record['instructions']={label:d.b.insns(str(p),'dp_wrapper_run') for label,p in [('native',d.b.BLOB),('baseline',base),('unsigned-fragment',candidate)]}
            record['witness']={label:witness(ins) for label,ins in record['instructions'].items()}
            assert all(record['witness'][label]['unsigned_min_branches']==3 and record['witness'][label]['unsigned_fragment_branches']==1 and record['witness'][label]['two_unsigned_divisions_with_post_copy_fragment_loads'] for label in ('native','unsigned-fragment'))
            assert not record['witness']['baseline']['two_unsigned_divisions_with_post_copy_fragment_loads'],'negative control did not fire'
        report['families'][family]=record
    report['body_verdict_denominator']=total;(OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print('audited9TUs/18cells',total,'body verdicts',flush=True)

def period():
    stable=ROOT/'build/period';report={}
    for label in ('baseline','unsigned-fragment'):
        folder=OUT/('period-'+label);folder.mkdir(exist_ok=True)
        for p in list(stable.glob('src_*.o'))+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_dp_wrapper.o']:shutil.copy2(p,folder/p.name)
        selected={}
        for path in SOURCES:
            dst=folder/(path.replace('/','_').replace('.c.o','.o') if path.endswith('.c.o') else path[:-2].replace('/','_')+'.o');shutil.copy2(OUT/Path(path).stem/label/'candidate.o',dst);selected[dst.name]=sha(dst)
        config=(OUT/'frozen/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1];flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')));snapshot='/work/historical-headers-'+subprocess.check_output(['git','rev-parse',REV],cwd=ROOT,text=True).strip();flags=['-I'+snapshot+'/include' if f=='-Iinclude' else snapshot+'/tools/toolchain/period_compat.h' if f=='tools/toolchain/period_compat.h' else f for f in flags]+['-I/src/test/harness','-std=gnu99']
        if label!='baseline':flags=['-I/work/dp_wrapper/'+label+'/include']+flags
        fixture=folder/'test_unit_t_dp_wrapper.o';dst='/work/'+folder.name+'/test_unit_t_dp_wrapper.o';fixture_command=d.tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c',d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,flags,dst,'test/unit/t_dp_wrapper.c')]
        with (folder/'fixture-compile.log').open('w') as log:subprocess.run(fixture_command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
        assert fixture.read_bytes()==(stable/'test_unit_t_dp_wrapper.o').read_bytes(),'fixture header overlay changed code; inspect before interpreting'
        selected[fixture.name]=sha(fixture)
        cmd=['make','period','T=t_dp_wrapper','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert all(sha(folder/n)==digest for n,digest in selected.items()),'candidate overwritten';text=(folder/'t_dp_wrapper.run.log').read_text();assert status==0 and 'FAIL' not in text
        report[label]={'command':cmd,'exit':status,'checks':sum(map(int,re.findall(r'(\d+) checks',text))),'selected_objects':selected,'fixture_command':fixture_command,'fixture_raw_equal_production':True,'log':text,'boundary':'Constructor-created wrapper plus synthetic datapump callbacks; existing rate/ragged-fragment/status fixture, not modem end-to-end.'};(OUT/'period.json').write_text(json.dumps(report,indent=2)+'\n');print(label,'period',report[label]['checks'],flush=True)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,default=OUT);ap.add_argument('--baseline-dir',type=Path,default=OUT/'frozen',help='immutable pre-adoption object/config archive for initializing a fresh output directory');ap.add_argument('--audit',action='store_true');ap.add_argument('--period',action='store_true');args=ap.parse_args();OUT=args.out.resolve()
    if args.audit:audit()
    elif args.period:period()
    else:
        OUT.mkdir(exist_ok=True);frozen=OUT/'frozen';frozen.mkdir(exist_ok=True)
        names=['.build-config']+[p.replace('/','_')+'.o' for p in SOURCES]
        for name in names:
            if not (frozen/name).exists():shutil.copy2(args.baseline_dir/name,frozen/name)
        manifest={'revision':subprocess.check_output(['git','rev-parse',REV],cwd=ROOT,text=True).strip(),'sources':{p:hashlib.sha256(subprocess.check_output(['git','show',REV+':'+p],cwd=ROOT)).hexdigest() for p in SOURCES},'frozen':{n:sha(frozen/n) for n in names}}
        if (OUT/'manifest.json').exists():assert json.loads((OUT/'manifest.json').read_text())==manifest,'frozen drift'
        else:(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        d.REV=REV;d.SOURCE_PATHS=SOURCES;d.OUT_NAME=OUT.name;d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;d.HEADER_OVERLAYS=overlay;sys.argv=[sys.argv[0],'--domain','Prior final unsigned-fragment endpoint; complete transitive header consumer control','--baseline-dir',str(frozen),'--historical-headers'];d.main();audit()
