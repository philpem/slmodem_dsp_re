#!/usr/bin/env python3
"""2x2 complete-TU ownership/visibility diagnostic for V34 status SNR.

Factors: diagnostics merged into Main vs separate; SNR definition visible to
its caller vs unavailable. All four cells preserve the strong public SNR
export and every definition once. Native TU filename remains unknown.
Prediction: definition-visible cells inline SNR; unavailable cells retain the
call. Reject interpretation on raw baseline drift or missing/weak exports.
"""
import argparse,hashlib,json,shlex,subprocess,sys,shutil,os
from pathlib import Path
import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
ROOT=d.ROOT
PATHS=('src/pump/v34/VPcmV34Main.cpp','src/pump/v34/v34diag.cpp')
TARGET='VPcmV34GetDiagnostics';HELPER='VPcmV34GetSNR'
REV='a0c079f61'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command,folder,label):
    with (folder/(label+'.log')).open('w') as log:subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
def configs(out):
    config=(out/'frozen/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    flags+=['-v','-save-temps','-da','-fdump-tree-original','-fdump-tree-inlined']
    flags+=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
    return config,image,flags

def generate():
    source={Path(p).name:subprocess.check_output(['git','show',REV+':'+p],cwd=ROOT,text=True) for p in PATHS};main,diag=(source[Path(p).name] for p in PATHS)
    start=main.index('\nint\nVPcmV34GetSNR(')+1;end=main.index('\n}\n',start)+3;body=main[start:end];without=main[:start]+main[end:]
    position=diag.index('\nvoid\nVPcmV34GetDiagnostics(')+1
    visible=diag[:position]+body+'\n'+diag[position:]
    helper_only='#include "dsplib/v34fsk.h"\n#include "dsplib/v34pcmif.h"\n\n'+body
    cells={'baseline':source,'separate-visible':{'VPcmV34Main.cpp':without,'v34diag.cpp':visible},'merged-unavailable':{'VPcmV34Main.cpp':without+'\n'+diag,'v34diag.cpp':helper_only},'merged-visible':{'VPcmV34Main.cpp':main+'\n'+diag,'v34diag.cpp':'/* Diagnostics moved into the complete Main TU. */\n'}}
    # Native has no persistent ratecfg LEA; its field accesses use obj directly.
    # Cross the supported owner spelling with helper availability, no flags.
    def direct(text):
        start=text.index('\nVPcmV34GetDiagnostics(');end=text.index('\n}\n',start)+3
        body=text[start:end]
        declaration='\tconst struct v34_ratecfg *cfg = &obj->ratecfg;\n'
        assert body.count(declaration)==1
        body=body.replace(declaration,'').replace('cfg->','obj->ratecfg.')
        return text[:start]+body+text[end:]
    cells['separate-direct-unavailable']={'VPcmV34Main.cpp':main,'v34diag.cpp':direct(diag)}
    cells['separate-direct-visible']={'VPcmV34Main.cpp':without,'v34diag.cpp':direct(visible)}
    assert len({tuple(c.values()) for c in cells.values()})==6
    assert all(sum(text.count('\nVPcmV34GetSNR(') for text in c.values())==1 for c in cells.values())
    return cells

def compile_cells(out,baseline_dir):
    out.mkdir(exist_ok=True);frozen=out/'frozen';frozen.mkdir(exist_ok=True)
    assert not subprocess.check_output(['git','diff',REV,'--name-only','--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).strip(),'pinned header drift'
    if not (frozen/'.build-config').exists():shutil.copy2(baseline_dir/'.build-config',frozen/'.build-config')
    for p in PATHS:
        if not (frozen/(Path(p).name+'.o')).exists():shutil.copy2(baseline_dir/(p.replace('/','_')+'.o'),frozen/(Path(p).name+'.o'))
    config,image,flags=configs(out)
    report={'config':config,'revision':subprocess.check_output(['git','rev-parse',REV],cwd=ROOT,text=True).strip(),'workspace_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'source_hashes':{p:hashlib.sha256(subprocess.check_output(['git','show',REV+':'+p],cwd=ROOT)).hexdigest() for p in PATHS},'headers':{str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'include').rglob('*.h'))},'cells':{}}
    manifest={'revision':report['revision'],'config_sha256':sha(frozen/'.build-config'),'objects':{Path(p).name+'.o':sha(frozen/(Path(p).name+'.o')) for p in PATHS},'source_hashes':report['source_hashes'],'headers':report['headers']}
    if (frozen/'manifest.json').exists():assert json.loads((frozen/'manifest.json').read_text())==manifest,'frozen baseline changed'
    else:(frozen/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    identity=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; g++ --version; p=$(g++ -print-prog-name=as); "$p" --version; p=$(g++ -print-prog-name=ld); "$p" --version']
    run(identity,out,'identity');report['identity_command']=identity
    for label,cell in generate().items():
        folder=out/label;folder.mkdir(exist_ok=True);record={'sources':{},'commands':[]};report['cells'][label]=record
        for filename,text in cell.items():
            (folder/filename).write_text(text);record['sources'][filename]=hashlib.sha256(text.encode()).hexdigest();dst='/work/'+label
            shell=d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,flags,dst+'/'+filename+'.o',dst+'/'+filename).replace('exec gcc -c ','exec g++ -c ',1)
            command=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+shell]
            run(command,folder,filename+'.compile');record['commands'].append(command)
            if label=='baseline':assert (folder/(filename+'.o')).read_bytes()==(frozen/(filename+'.o')).read_bytes(),'raw baseline mismatch '+filename
        link=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; exec gcc -nostdlib -r -o /work/'+label+'/pair.o /work/'+label+'/VPcmV34Main.cpp.o /work/'+label+'/v34diag.cpp.o']
        run(link,folder,'partial-link');record['commands'].append(link);record['pair_sha256']=sha(folder/'pair.o')
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(label,'compiled two complete TUs and period partial pair',flush=True)
    audit(out)

def audit(out):
    ledger=json.loads((out/'results.json').read_text());report={'config':ledger['config'],'cells':{},'source_ownership':'Original FILE attribution remains dp_wrapper.c|V34.c ambiguous; no recovered original filename claimed.'}
    base=out/'baseline/pair.o';baseobj=Object(base).canonical();baseline_sizes=d.b.sizes(str(base));baseline_globals=baseobj[2]
    baseline_verdicts={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(base),n))) for n in baseline_sizes}
    for label,cell in ledger['cells'].items():
        obj=out/label/'pair.o';current=Object(obj).canonical();sizes=d.b.sizes(str(obj))
        assert sha(obj)==cell['pair_sha256'];assert set(sizes)==set(baseline_sizes),'missing/new functions'
        assert current[2]==baseline_globals,'export binding/visibility changed'
        verdicts={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in sizes}
        assert any(n==HELPER and bind=='STB_GLOBAL' and kind=='STT_FUNC' for n,bind,kind,vis in current[2])
        call=any(m=='call' and HELPER in operands for m,operands in d.b.insns(str(obj),TARGET))
        assert call==(label in ('baseline','merged-unavailable','separate-direct-unavailable')),'visibility prediction falsified'
        record={'function_count':len(sizes),'pair_sha256':sha(obj),'verdicts':verdicts,'sizes':sizes,'changed_bodies':[n for n in sizes if d.b.body(str(obj),n)!=d.b.body(str(base),n)],'gains':[n for n,v in verdicts.items() if v[0]=='EXACT' and baseline_verdicts[n][0]!='EXACT'],'losses':[n for n,v in verdicts.items() if v[0]!='EXACT' and baseline_verdicts[n][0]=='EXACT'],'snr_call':call,'bindings_equal':True,'data_equal':current[1]==baseobj[1],'changed_data_sections':{n:{'old':{'size':len(baseobj[1].get(n,b'')),'sha256':hashlib.sha256(baseobj[1].get(n,b'')).hexdigest()},'new':{'size':len(current[1].get(n,b'')),'sha256':hashlib.sha256(current[1].get(n,b'')).hexdigest()}} for n in set(current[1])|set(baseobj[1]) if current[1].get(n)!=baseobj[1].get(n)},'changed_relocation_consumers':[n for n in set(current[3])|set(baseobj[3]) if current[3].get(n)!=baseobj[3].get(n)],'target_native_instructions':d.b.insns(d.b.BLOB,TARGET),'target_current_instructions':d.b.insns(str(obj),TARGET)}
        report['cells'][label]=record
        (out/label/'target.dis').write_text(subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),str(obj),TARGET],text=True,stderr=subprocess.DEVNULL))
        print(label,'target',verdicts[TARGET],'SNRcall',call,'gains',record['gains'],'losses',record['losses'],flush=True)
    report['body_verdicts_checked']=sum(c['function_count'] for c in report['cells'].values());report['differential']='NOT RUN'
    (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')

def period(out):
    import re
    report={}
    stable=ROOT/'build/period';sources=list(stable.glob('src_*.o'));assert len(sources)==300
    for label in ('separate-visible','separate-direct-visible'):
        folder=out/('period-'+label);folder.mkdir(exist_ok=True)
        for path in sources+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v34diag.o',stable/'test_unit_t_v34pcmif.o']:
            shutil.copy2(path,folder/path.name)
        selected={}
        for filename in ('VPcmV34Main.cpp','v34diag.cpp'):
            src=out/label/(filename+'.o');dst=folder/('src_pump_v34_'+filename.removesuffix('.cpp')+'.o');shutil.copy2(src,dst);selected[dst]=sha(src)
        command=['make','period','T=t_v34diag t_v34pcmif','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(command,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert all(sha(path)==value for path,value in selected.items()),'candidate objects overwritten'
        logs={name:(folder/(name+'.run.log')).read_text() for name in ('t_v34diag','t_v34pcmif')}
        assert status==0 and all('FAIL' not in text and 'PASS ' in text for text in logs.values())
        checks={name:sum(map(int,re.findall(r'(\d+) checks',text))) for name,text in logs.items()}
        report[label]={'command':command,'exit':status,'selected_object_sha256':{str(k):v for k,v in selected.items()},'checks':checks,'logs':logs,'fixture_scope':'Existing exploratory component fixtures include planted states; passing does not independently establish modem lifecycle reachability.'}
        (out/'period.json').write_text(json.dumps(report,indent=2)+'\n')
        print(label,'period2passed0failed',checks,flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,default=ROOT/'build/v34-diag-visibility293');ap.add_argument('--baseline-dir',type=Path,default=ROOT/'build/tc_repro',help='used only to initialize immutable frozen baseline; later rebuilds never overwrite it');ap.add_argument('--audit',action='store_true');ap.add_argument('--period',action='store_true');args=ap.parse_args()
    if args.period:period(args.out)
    elif args.audit:audit(args.out)
    else:compile_cells(args.out,args.baseline_dir)
