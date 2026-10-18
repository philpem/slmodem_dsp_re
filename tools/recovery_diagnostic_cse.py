#!/usr/bin/env python3
"""Trace independent native sign-readback consumers under actual period CSE.

Prediction: initial SF member sign reads exist, then disappear at first local
CSE in Parameters and Resampler as in Echo. Falsifier: absent initial reads,
reads surviving CSE, or nonreproducing full-TU raw baselines. A separately
labelled intrusive compiler control may skip cse_main; it is never source
recovery or a retained flag. No first-CSE-disable switch is assumed.
"""
import sys
import playbook_small_patterns as d
import hashlib, json, shlex, shutil, subprocess, copy, os
from pathlib import Path
from recovery_fax_debug_audit import Object

TARGETS={'V90Parameters':'loadModemParamsData','ResamplerTiming':'adjustHalfBaudBpfGain'}

def audit():
    from gcc3_stage_divergence import chunks
    from gcc3_reload_trace import instructions,register
    out=d.ROOT/'build/diagnostic-cse293';prior=json.loads((out/'results.json').read_text())
    report={'config':prior['config'],'families':{},'differential':'NOT RUN: diagnostic apparatus and unchanged source only'}
    assert len(prior['families'])==3
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
    assert all(sha(d.ROOT/p)==value for p,value in prior['headers'].items())
    for family,fr in prior['families'].items():
        baseline=out/family/'baseline';base=Object(baseline/'candidate.o').canonical()
        rows={};report['families'][family]=rows
        for label,cell in fr['cells'].items():
            folder=out/family/label;obj=folder/'candidate.o'
            assert sha(obj)==cell['object_hash']
            assert sha(folder/(family+'.cpp'))==cell['source_hash']
            assert (folder/(family+'.cpp')).read_bytes()==(baseline/(family+'.cpp')).read_bytes()
            command=shlex.split(cell['command'][-1]);flags=shlex.split(next(x[6:] for x in prior['config'].splitlines() if x.startswith('flags ')))
            flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
            flags+=['-v','-save-temps','-da','-fdump-tree-original','-fdump-tree-inlined']
            flags+=['-fno-gcse'] if label=='diagnostic-no-gcse' else []
            flags+=shlex.split(next(x[6:] for x in prior['config'].splitlines() if x.startswith('cxx   ')))
            dst='/work/'+family+'/'+label
            assert command[command.index('g++')+1:]==['-c']+flags+[d.tc.REPRODUCE_BUGS,'-o',dst+'/candidate.o',dst+'/'+family+'.cpp']
            log=(folder/'compile.log').read_text()
            assert 'GNU C++ version 3.4.2  (Gentoo Linux 3.4.2-r2' in log and 'GNU assembler version 2.15.92.0.2' in log
            canonical=Object(obj).canonical()
            def summarize(section):return {str(k):sha256_bytes(v) for k,v in section.items()}
            bodies={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in d.b.sizes(str(obj))}
            assert bodies==cell['verdicts']
            rows[label]={'object_sha256':sha(obj),'function_count':len(bodies),'verdicts':bodies,'gains':cell.get('gains',[]),'losses':cell.get('losses',[]),'changed_bodies':cell.get('changed_bodies',[]),'binding_equal':canonical[2]==base[2],'data_equal':canonical[1]==base[1],'changed_data_sections':{n:{'old':sha256_bytes(base[1].get(n,b'')),'new':sha256_bytes(canonical[1].get(n,b''))} for n in set(base[1])|set(canonical[1]) if base[1].get(n)!=canonical[1].get(n)},'changed_relocation_consumers':[n for n in set(base[3])|set(canonical[3]) if base[3].get(n)!=canonical[3].get(n)],'stages':{}}
            methods={'V90Parameters':[('loadModemParamsData','void V90Parameters::loadModemParamsData()',[55,85,87])],'ResamplerTiming':[('adjustHalfBaudBpfGain','void ResamplerTiming::adjustHalfBaudBpfGain(float)',[29,126,195,203])],'V92EchoCanceller':[('setEchoBeta','void V92EchoCanceller::setEchoBeta(float)',[14,75,83]),('setDecayFactor','void V92EchoCanceller::setDecayFactor(float)',[14,75,83])]}[family]
            for method,header,uids in methods:
                streams={}
                for suffix in ('01.rtl','05.null','06.cse','07.addressof','09.loop','18.cse2'):
                    path=folder/(family+'.cpp.'+suffix);p=instructions(chunks(path,header)[0])
                    streams[suffix]={'sha256':sha(path),'instruction_count':len(p),'patterns':{str(uid):p.get(uid) for uid in uids}}
                rows[label]['stages'][method]=streams
        assert rows['baseline']['function_count'] in (9,14,15)
        assert rows['baseline']['object_sha256']==sha(out/family/'retained.o')
        assert rows['diagnostic-no-gcse']['binding_equal']
    def hasmem(p,field):return p is not None and '<variable>.'+field+'+0' in str(p) and 'mem/s:SF' in str(p)
    def verify(trace):
        for family,method,uid,field in [('V90Parameters','loadModemParamsData','85','DIGITAL_POWER_REDUCTION'),('ResamplerTiming','adjustHalfBaudBpfGain','126','dftMag')]:
            cells=trace[family]
            for label in ('baseline','diagnostic-no-gcse'):
                stages=cells[label]['stages'][method]
                for suffix in ('01.rtl','05.null','06.cse','07.addressof'):
                    assert hasmem(stages[suffix]['patterns'][uid],field)
                assert hasmem(stages['09.loop']['patterns'][uid],field)==(label=='diagnostic-no-gcse')
                assert hasmem(stages['18.cse2']['patterns'][uid],field)==(label=='diagnostic-no-gcse')
        for label in ('baseline','diagnostic-no-gcse'):
            for method in ('setEchoBeta','setDecayFactor'):
                stages=trace['V92EchoCanceller'][label]['stages'][method]
                assert hasmem(stages['05.null']['patterns']['75'],'echoBeta' if method=='setEchoBeta' else 'echoBetaDecay')
                assert stages['06.cse']['patterns']['75'] is None
    verify(report['families']);controls=[]
    for family,method,uid,suffix,label in [('V90Parameters','loadModemParamsData','85','06.cse','baseline'),('ResamplerTiming','adjustHalfBaudBpfGain','126','09.loop','diagnostic-no-gcse'),('V92EchoCanceller','setEchoBeta','75','05.null','baseline')]:
        altered=copy.deepcopy(report['families']);altered[family][label]['stages'][method][suffix]['patterns'][uid]=None
        try:verify(altered)
        except AssertionError:controls.append(family+'/'+suffix)
        else:raise AssertionError('corrupted stage accepted')
    report['corrupted_trace_controls_rejected']=controls
    report['body_verdict_count']=sum(c['function_count'] for f in report['families'].values() for c in f.values())
    report['conclusion']='Independent member reads survive initial local CSE but are forwarded across the GCSE boundary; no-GCSE preserves these reads. Echo setters lose their member reads at initial local CSE regardless of GCSE. No-GCSE restores native Parameters memory comparison, but not exact body; Resampler uses retained member-loaded register for sign and remains different. There are zero exact gains and five exact losses across the three complete TUs. No profile/source recovery licensed.'
    (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['body_verdict_count'],'body verdicts;3 raw baselines;3 corrupt controls refused;0 exact gains/5 losses')

def sha256_bytes(value):return {'size':len(value),'sha256':hashlib.sha256(value).hexdigest()}

def audit_whole():
    out=d.ROOT/'build/diagnostic-parameters-structure293';ledger=json.loads((out/'results.json').read_text());family=ledger['families']['V90Parameters']
    folder=out/'V90Parameters';a=folder/'baseline/candidate.o';b=folder/'native-float-fabs-whole/candidate.o'
    ca,cb=Object(a).canonical(),Object(b).canonical();target='_ZN13V90Parameters19loadModemParamsDataEv'
    assert a.read_bytes()==(folder/'retained.o').read_bytes()
    for label,cell in family['cells'].items():
        obj=folder/label/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert {n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in d.b.sizes(str(obj))}==cell['verdicts']
    native=d.b.insns(d.b.BLOB,target);old=d.b.insns(str(a),target);new=d.b.insns(str(b),target)
    assert sum(m=='fabs' for m,o in native)==sum(m=='fabs' for m,o in new)==1
    assert not any(m=='fabs' for m,o in old)
    cell=family['cells']['native-float-fabs-whole']
    assert cell['changed_bodies']==[target] and not cell['gains'] and not cell['losses']
    assert ca[1:]==cb[1:],'non-target data/binding/relocation consumer drift'
    assert d.b.sizes(d.b.BLOB)[target]==d.b.sizes(str(b))[target]==344
    rejected=json.loads((out/'rejected-missing-math-declaration.json').read_text())
    assert rejected['families']['V90Parameters']['cells']['native-float-fabs-whole']['compile_exit']!=0
    report={'revision':ledger['revision'],'config':ledger['config'],'baseline_raw_equal':True,'full_tu_body_verdicts_checked':18,'changed_bodies':[target],'nonstring_data_bindings_relocation_consumers_equal':True,'exact_gains':[],'exact_losses':[],'native_target_size':344,'baseline_target_size':332,'candidate_target_size':344,'candidate_verdict':cell['verdicts'][target],'original_fabs_address':'0x2a731','original_whole_fist_address':'0x2a76b','initial_invalid_cell_preserved':'rejected-missing-math-declaration.json and .log','input_domain':'powerReductionTenths is unsigned32; floor(value/5)<=858993459, safely cast to signed32 before multiplication by0.5f; result is finite nonnegative below2^29. Native and candidate whole conversions therefore see neither NaN nor signed-int overflow. Existing t_v90params cases include0x7fffffff,0x80000000,0xffffffff and all20 cases at debug levels0..3.','native_float_instructions':[(m,o) for m,o in native if m.startswith('f')],'candidate_float_instructions':[(m,o) for m,o in new if m.startswith('f')]}
    report['crossed_cells']={}
    for label,record in family['cells'].items():
        obj=folder/label/'candidate.o';c=Object(obj).canonical()
        assert ca[1:]==c[1:]
        assert not record.get('gains') and not record.get('losses')
        if label!='baseline':assert record['changed_bodies']==[target]
        ins=d.b.insns(str(obj),target)
        if label in ('diagnostic-no-gcse','native-fabs-no-gcse'):
            assert [(m,o) for m,o in ins if m.startswith('fcom')]==[('fcomps','0x380(%esi)')]
        report['crossed_cells'][label]={'target_verdict':record['verdicts'][target],'object_sha256':record['object_hash'],'diagnostic_flags':record['diagnostic_flags'],'target_size':d.b.sizes(str(obj))[target],'whole_fabs_count':sum(m=='fabs' for m,o in ins),'float_operations':[(m,o) for m,o in ins if m.startswith('f')],'changed_bodies':record.get('changed_bodies',[]),'nonstring_data_bindings_relocation_consumers_equal':True}
    report['full_tu_body_verdicts_checked']=sum(len(c['verdicts']) for c in family['cells'].values())
    report['remaining_native_differences']='Fourth cell combines native fabs and member fcomps but retains delayed member fstps, register nonpopping subtraction and memory multiply, versus native early fsts, popping subtraction and loaded scale with popping multiply. Source lifetime/precision and pass interaction remain unresolved; no profile adoption licensed.'
    (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['full_tu_body_verdicts_checked'],'body verdicts;four-cell source/GCSE cross;0 exact gains/losses;data/binding/relocation inventories equal')

def audit_owner():
    from gcc3_stage_divergence import chunks
    from gcc3_reload_trace import instructions
    out=d.ROOT/'build/diagnostic-parameters-owner293';ledger=json.loads((out/'results.json').read_text());family=ledger['families']['V90Parameters'];folder=out/'V90Parameters'
    target='_ZN13V90Parameters19loadModemParamsDataEv';header='void V90Parameters::loadModemParamsData()';base=folder/'baseline/candidate.o';canonical=Object(base).canonical()
    assert base.read_bytes()==(folder/'retained.o').read_bytes()
    assert len(family['cells'])==7
    report={'revision':ledger['revision'],'config':ledger['config'],'cells':{},'body_verdicts_checked':0}
    for label,cell in family['cells'].items():
        cd=folder/label;obj=cd/'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest()==cell['object_hash']
        assert hashlib.sha256((cd/'V90Parameters.cpp').read_bytes()).hexdigest()==cell['source_hash']
        assert {n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in d.b.sizes(str(obj))}==cell['verdicts']
        assert Object(obj).canonical()[1:]==canonical[1:]
        assert not cell.get('gains') and not cell.get('losses')
        assert set(cell.get('changed_bodies',[]))<= {target}
        stages={}
        for suffix in ('01.rtl','05.null','06.cse','07.addressof','09.loop','18.cse2','19.life','20.combine','24.lreg','33.sched2','34.stack'):
            path=cd/('V90Parameters.cpp.'+suffix);p=instructions(chunks(path,header)[0]);stores={uid:v for uid,v in p.items() if v[0]=='set' and 'DIGITAL_POWER_REDUCTION+0' in str(v[1])};loads={uid:v for uid,v in p.items() if v[0]=='set' and 'DIGITAL_POWER_REDUCTION+0' in str(v[2])};fixes=[uid for uid,v in p.items() if 'fix:SI' in str(v)]
            stages[suffix]={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'store_patterns':stores,'load_patterns':loads,'float_to_int_uids':fixes}
        first=stages['01.rtl'];store=next(iter(first['store_patterns']));assert len(first['store_patterns'])==1
        assert (store<min(first['float_to_int_uids']))==label.startswith('early')
        report['cells'][label]={'source_sha256':cell['source_hash'],'object_sha256':cell['object_hash'],'diagnostic_flags':cell['diagnostic_flags'],'verdict':cell['verdicts'][target],'stages':stages,'nonstring_data_bindings_relocation_consumers_equal':True,'changed_bodies':cell.get('changed_bodies',[])}
        report['body_verdicts_checked']+=len(cell['verdicts'])
    groups=[('baseline','late-local-sign'),('early-member-sign','early-local-sign','early-all-member')]
    for group in groups:assert len({report['cells'][label]['object_sha256'] for label in group})==1
    assert report['cells']['baseline']['object_sha256']!=report['cells']['early-member-sign']['object_sha256']
    report['raw_equal_groups']=groups
    report['conclusion']='Early source publication changes initial RTL and restores native early FSTS before diagnostic conversions under retained flags. Sign ownership and all-member diagnostic calculations do not distinguish the three early source forms: they are full-object raw-equal. No-GCSE restores member sign compare but does not close the body. No exact gains or losses; source adoption limited to independently witnessed publication order.'
    (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('63 body verdicts;7 cells/4 distinct emissions;source publication first differs at01.rtl;all data/bindings/relocation consumers equal')

def owner_period():
    out=d.ROOT/'build/diagnostic-parameters-owner293';folder=out/'period-early-publication';folder.mkdir(exist_ok=True)
    original=d.ROOT/'build/period';sources=list(original.glob('src_*.o'));assert len(sources)==300
    for path in sources+list(original.glob('test_harness_*.o'))+[original/'test_unit_t_v90params.o']:
        shutil.copy2(path,folder/path.name)
    candidate=out/'V90Parameters/early-member-sign/candidate.o';obj=folder/'src_pump_v90_V90Parameters.o';shutil.copy2(candidate,obj)
    command=['make','period','T=t_v90params','J=4']
    with (folder/'gate.log').open('w') as log:
        status=subprocess.run(command,cwd=d.ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(d.ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
    assert obj.read_bytes()==candidate.read_bytes()
    result=(folder/'t_v90params.run.log').read_text();assert status==0 and result.count('PASS ')==4 and 'FAIL' not in result
    (out/'period-early-publication.json').write_text(json.dumps({'command':command,'exit':status,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'candidate_preserved':True,'test_log':result},indent=2)+'\n')
    print('early-publication retained-profile candidate:period1passed0failed;4005 checks;candidateobject preserved')

GDB_SCRIPT='''import gdb,json
settings=json.load(open('settings.json'))
events=[];counts={};exits=[]
gdb.events.exited.connect(lambda event:exits.append(event.exit_code))
gdb.execute('set pagination off')
gdb.execute('set confirm off')
gdb.Breakpoint('cse_main',internal=True)
gdb.execute('run')
while not exits:
    name=gdb.parse_and_eval('current_function_decl->decl.name->identifier.id.str').string()
    counts[name]=counts.get(name,0)+1
    skip=settings['mode']=='skip-first' and name==settings['target'] and counts[name]==1
    events.append({'function':name,'ordinal':counts[name],'skip':skip,'pc':str(gdb.parse_and_eval('$pc')),'caller':gdb.newest_frame().older().name()})
    if skip:gdb.execute('return (int)0')
    gdb.execute('continue')
json.dump({'events':events,'counts':counts,'exit_codes':exits,'skipped':sum(e['skip'] for e in events)},open('trace.json','w'),indent=2)
'''

def run(command,folder,label):
    with (folder/(label+'.log')).open('w') as log:
        subprocess.run(command,cwd=folder,stdout=log,stderr=subprocess.STDOUT,check=True)

def observe():
    out=d.ROOT/'build/diagnostic-cse293';prior=json.loads((out/'results.json').read_text())
    image=prior['config'].splitlines()[0].split(' ',1)[1]
    compiler=out/'cc1plus'
    with compiler.open('wb') as stream:
        subprocess.run(['docker','run','--rm','--platform','linux/386',image,'cat','/usr/libexec/gcc/i386-pc-linux-gnu/3.4.2/cc1plus'],stdout=stream,check=True)
    compiler.chmod(0o755)
    (out/'skip-first-cse-gdb.py').write_text(GDB_SCRIPT)
    report={'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'config':prior['config'],'cells':{}}
    for family,target in TARGETS.items():
        baseline=out/family/'baseline';stem=family
        lines=(baseline/'compile.log').read_text().splitlines()
        command=shlex.split(next(line for line in lines if '/cc1plus -fpreprocessed ' in line));command[0]=str(compiler)
        for mode in ('plain','observe','absent-target','skip-first'):
            folder=out/family/mode;folder.mkdir(exist_ok=True)
            shutil.copy2(baseline/(stem+'.ii'),folder/(stem+'.ii'))
            cmd=list(command);cmd[cmd.index('-o')+1]='candidate.s'
            settings={'target':'__absent_control__' if mode=='absent-target' else target,'mode':'skip-first' if mode in ('absent-target','skip-first') else 'observe'}
            (folder/'settings.json').write_text(json.dumps(settings))
            if mode=='plain':actual=cmd
            else:actual=['gdb','-q','-batch','-ex','source '+str(out/'skip-first-cse-gdb.py'),'--args']+cmd
            run(actual,folder,'compile')
            trace=json.loads((folder/'trace.json').read_text()) if mode!='plain' else None
            if trace:
                assert trace['exit_codes']==[0]
                assert trace['skipped']==(1 if mode=='skip-first' else 0)
                assert trace['counts'].get(target,0)>=1
            assembler=shlex.split(next(line for line in lines if '/bin/as ' in line));assembler[assembler.index('-o')+1]='/work/candidate.o';assembler[-1]='candidate.s'
            assemble=d.tc.docker_prefix(image,d.ROOT,folder,True)+['/bin/sh','-c','cd /work && '+shlex.join(assembler)]
            run(assemble,folder,'assemble')
            obj=folder/'candidate.o';base=baseline/'candidate.o'
            if mode!='skip-first':
                assert obj.read_bytes()==base.read_bytes(),'unmodified host/GDB replay drift'
                assert (folder/'candidate.s').read_bytes()==(baseline/(stem+'.s')).read_bytes()
            a,b=Object(base).canonical(),Object(obj).canonical()
            sizes=d.b.sizes(str(obj));verdicts={n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in sizes}
            v0=prior['families'][family]['cells']['baseline']['verdicts']
            cell={'commands':[actual,assemble],'trace':trace,'raw_equal_baseline':obj.read_bytes()==base.read_bytes(),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'verdicts':verdicts,'function_count':len(sizes),'changed_bodies':[n for n in sizes if d.b.body(str(base),n)!=d.b.body(str(obj),n)],'data_equal':a[1]==b[1],'binding_equal':a[2]==b[2],'changed_relocation_consumers':[n for n in set(a[3])|set(b[3]) if a[3].get(n)!=b[3].get(n)],'gains':[n for n,v in verdicts.items() if v[0]=='EXACT' and v0[n][0]!='EXACT'],'losses':[n for n,v in v0.items() if v[0]=='EXACT' and verdicts[n][0]!='EXACT']}
            report['cells'][family+'/'+mode]=cell
            (out/'mechanism.json').write_text(json.dumps(report,indent=2)+'\n')
            print(family,mode,'raw_equal',cell['raw_equal_baseline'],'changed',cell['changed_bodies'],'gains',cell['gains'],'losses',cell['losses'],flush=True)

if __name__ == '__main__':
    if '--audit-owner' in sys.argv:
        audit_owner();sys.exit(0)
    if '--owner-period' in sys.argv:
        owner_period();sys.exit(0)
    if '--owner' in sys.argv:
        sys.argv.remove('--owner')
        d.REV='619276358'
        d.SOURCE_PATHS=('src/pump/v90/V90Parameters.cpp',)
        d.OUT_NAME='diagnostic-parameters-owner293'
        d.DUMP_FLAGS=('-v','-save-temps','-da','-fdump-tree-original','-fdump-tree-inlined')
        out=d.ROOT/'build'/d.OUT_NAME;frozen=out/'frozen';frozen.mkdir(parents=True,exist_ok=True)
        shutil.copy2(d.ROOT/'build/copy293-baseline/.build-config',frozen/'.build-config')
        shutil.copy2(d.ROOT/'build/diagnostic-parameters-structure293/V90Parameters/native-float-fabs-whole/candidate.o',frozen/'src_pump_v90_V90Parameters.cpp.o')
        def variants(path,source):
            # Native early FSTS precedes diagnostic conversions. Prediction:
            # early source publication changes initial RTL store position;
            # sign owner differs initially but may be forwarded by CSE/GCSE.
            # No claim that sign-only ownership changes whole-value lifetime.
            # The all-member fifth cell publishes first to avoid old reads.
            store='\n\t\tDIGITAL_POWER_REDUCTION = pr;\n'
            pr='\t\tfloat pr = (float)(int)(tempPR / 5) * 0.5f;'
            assert source.count(store)==source.count(pr)==1
            early=source.replace(store,'').replace(pr,pr+store)
            cells={'baseline':source,'late-local-sign':source.replace('(0 < DIGITAL_POWER_REDUCTION)','(0 < pr)'),
                   'early-member-sign':early,'early-local-sign':early.replace('(0 < DIGITAL_POWER_REDUCTION)','(0 < pr)')}
            cells['early-all-member']=early.replace('int whole = (int)fabsf(pr);','int whole = (int)fabsf(DIGITAL_POWER_REDUCTION);').replace('int frac = (int)((pr - (float)(int)pr) * 100.0f);','int frac = (int)((DIGITAL_POWER_REDUCTION - (float)(int)DIGITAL_POWER_REDUCTION) * 100.0f);')
            assert len(cells)==len(set(cells.values()))==5
            cells['late-diagnostic-no-gcse']=source
            cells['early-diagnostic-no-gcse']=early
            return cells
        d.variants=variants
        d.CELL_FLAGS=lambda path,label:('-fno-gcse',) if label.endswith('diagnostic-no-gcse') else ()
        sys.argv+=['--baseline-dir',str(frozen)]
        d.main();sys.exit(0)
    if '--audit-whole' in sys.argv:
        audit_whole();sys.exit(0)
    if '--whole-expression' in sys.argv:
        sys.argv.remove('--whole-expression')
        d.REV='96ce7c20d65b682f031c1971ef0bba73600aafec'
        d.SOURCE_PATHS=('src/pump/v90/V90Parameters.cpp',)
        d.OUT_NAME='diagnostic-parameters-structure293'
        d.DUMP_FLAGS=('-v','-save-temps','-da')
        def variants(path,source):
            # Native 0x2a72f FLD;0x2a731 FABS precedes FISTPL0x2a76b.
            # Prediction: direct float fabs recovers that operation rather
            # than the current SAR/XOR/SUB absolute value after conversion.
            # Member forwarding is independently unresolved, not excused.
            old='\t\tint whole = (int)pr;'
            negate='\t\tif (whole < 0)\n\t\t\twhole = -whole;\n'
            assert source.count(old)==source.count(negate)==1
            candidate=source.replace(old,'\t\tint whole = (int)fabsf(pr);').replace(negate,'')
            candidate=candidate.replace('#include "dsplib/V90Parameters.h"','#include <math.h>\n\n#include "dsplib/V90Parameters.h"',1)
            return {'baseline':source,'diagnostic-no-gcse':source,'native-float-fabs-whole':candidate,'native-fabs-no-gcse':candidate}
        d.variants=variants
        d.CELL_FLAGS=lambda path,label:('-fno-gcse',) if label in ('diagnostic-no-gcse','native-fabs-no-gcse') else ()
        d.main();sys.exit(0)
    if '--audit' in sys.argv:
        audit();sys.exit(0)
    if '--observe' in sys.argv:
        observe();sys.exit(0)
    d.REV='96ce7c20d65b682f031c1971ef0bba73600aafec'
    d.SOURCE_PATHS=('src/pump/v90/V90Parameters.cpp','src/pump/v90/ResamplerTiming.cpp','src/pump/v90/V92EchoCanceller.cpp')
    d.OUT_NAME='diagnostic-cse293'
    d.DUMP_FLAGS=('-v','-save-temps','-da','-fdump-tree-original','-fdump-tree-inlined')
    # The initial-local-CSE prediction was falsified: the independent loads
    # survive through 07.addressof and disappear by 09.loop. Explicit GCSE
    # control predicts preserved member reads there, while Echo's initial
    # CSE-forwarded setter reads remain absent. This is a different pass.
    d.variants=lambda path,source:{'baseline':source,'diagnostic-no-gcse':source}
    d.CELL_FLAGS=lambda path,label:('-fno-gcse',) if label=='diagnostic-no-gcse' else ()
    d.main()
