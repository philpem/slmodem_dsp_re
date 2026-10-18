#!/usr/bin/env python3
"""Two prior V32 unsigned-count endpoints, complete-TU collateral and stage trace."""
import argparse,json,sys,subprocess,shutil,hashlib,os,re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_boundary_transfer import variants
from recovery_fax_debug_audit import Object
ROOT=d.ROOT;OUT=ROOT/'build/v32-mrf-count293';REV='6b16d64d90891138f9891067bb3e41cad136853d';SOURCE='src/pump/v32/V32int.c'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def count_boundary(ins):
    call=next(i for i,(op,a) in enumerate(ins) if op=='call' and 'FPM_MRF_filter' in a)
    loads=[(i,op,a.split(',')[-1]) for i,(op,a) in enumerate(ins[:call]) if op in ('movzwl','movswl') and a.startswith('0x3c(%esp),')]
    assert len(loads)==1
    index,mode,register=loads[0];stores=[i for i,(op,a) in enumerate(ins[index+1:call],index+1) if op=='mov' and a==register+',0xc(%esp)'];assert len(stores)==1
    return {'formal_load_index':index,'mode':mode,'fullword_outgoing_store_index':stores[0],'call_index':call,'unsigned_word_to_int_slot':mode=='movzwl'}

def audit():
    report={'cells':{}};base=OUT/'V32int/baseline/candidate.o';bc=Object(base).canonical();sizes=d.b.sizes(str(base))
    for label in ('baseline','count'):
        p=OUT/'V32int'/label/'candidate.o';c=Object(p).canonical();assert c[1:]==bc[1:],'data/binding/relocation drift';assert set(c[0])==set(bc[0])
        report['cells'][label]={'sha256':sha(p),'function_count':len(sizes),'verdicts':{n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(p),n))) for n in sizes},'changed_bodies':[n for n in sizes if d.b.body(str(p),n)!=d.b.body(str(base),n)],'data_bindings_relocation_consumers_equal':True,'target_instructions':d.b.insns(str(p),'DemodDataV32')}
    report['native_target_instructions']=d.b.insns(d.b.BLOB,'DemodDataV32');report['count_boundary']={'native':count_boundary(report['native_target_instructions']),**{k:count_boundary(v['target_instructions']) for k,v in report['cells'].items()}};assert report['count_boundary']['native']['unsigned_word_to_int_slot'] and report['count_boundary']['count']['unsigned_word_to_int_slot'] and not report['count_boundary']['baseline']['unsigned_word_to_int_slot'];(OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print([(k,v['verdicts']['DemodDataV32'],v['changed_bodies']) for k,v in report['cells'].items()])
def gate(labels):
    stable=ROOT/'build/period';report={}
    for label in labels:
        folder=OUT/('period-'+label);folder.mkdir(exist_ok=True)
        for p in list(stable.glob('src_*.o'))+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v32demod.o',stable/'test_unit_t_v32fpsub.o',stable/'test_unit_t_v32fpctl.o']:shutil.copy2(p,folder/p.name)
        obj=folder/'src_pump_v32_V32int.o';shutil.copy2(OUT/'V32int'/label/'candidate.o',obj);digest=sha(obj);cmd=['make','period','T=t_v32demod t_v32fpsub t_v32fpctl','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(cmd,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert sha(obj)==digest,'candidate overwritten';logs={n:(folder/(n+'.run.log')).read_text() for n in ('t_v32demod','t_v32fpsub','t_v32fpctl')};assert status==0,(label,status)
        report[label]={'command':cmd,'exit':status,'object_sha256':digest,'logs':logs,'checks':{n:sum(map(int,re.findall(r'(\d+) checks',t))) for n,t in logs.items()}};print(label,status,report[label]['checks'],flush=True)
    (OUT/'period.json').write_text(json.dumps(report,indent=2)+'\n')

def trace():
    import difflib
    from gcc3_value_carriers_audit import function_chunk
    base=OUT/'V32int/baseline';candidate=base.parent/'count';report={'normalization':'Only artifact directory names and compiler process addresses on NOTE_INSN_BLOCK markers and named declaration nodes. UIDs, registers, modes, constants and source lines retained.','functions':{}}
    def normalize(text):
        text=re.sub(r'(<(?:function|var|type)_decl )0x[0-9a-f]+',r'\1DECL',text)
        return re.sub(r'0x[0-9a-f]+(?= NOTE_INSN_BLOCK_)','BLOCK',re.sub(r'/work/V32int/(baseline|count)/','/cell/',text))
    for name in ('SetAdaptEqV32','SetAdaptEcV32','SetECRndTripDelayV32'):
        rows=[];first=None
        for path in sorted(base.glob('V32int.c.*')):
            if not re.search(r'\.\d\d\.',path.name) or '.00.cgraph' in path.name:continue
            a=normalize(function_chunk(path,name));b=normalize(function_chunk(candidate/path.name,name));equal=a==b
            row={'stage':path.name,'equal':equal,'baseline_sha256':hashlib.sha256(a.encode()).hexdigest(),'count_sha256':hashlib.sha256(b.encode()).hexdigest()};rows.append(row)
            if not equal and first is None:
                first=path.name;row['first_diff']=list(difflib.unified_diff(a.splitlines(),b.splitlines(),n=2));(OUT/(name+'-first-stage.diff')).write_text('\n'.join(row['first_diff'])+'\n')
        insa=d.b.insns(str(base/'candidate.o'),name);insb=d.b.insns(str(candidate/'candidate.o'),name)
        report['functions'][name]={'first_differing_stage':first,'stages':rows,'same_source':d.function((base/'V32int.c').read_text(),name)[2]==d.function((candidate/'V32int.c').read_text(),name)[2],'final_paired_instruction_differences':[{'index':i,'baseline':a,'count':b} for i,(a,b) in enumerate(zip(insa,insb)) if a!=b],'final_instruction_counts':[len(insa),len(insb)]}
    assert report['functions']['SetAdaptEqV32']['first_differing_stage']=='V32int.c.28.peephole2'
    assert all(r['same_source'] for r in report['functions'].values())
    (OUT/'trace.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v['first_differing_stage'] for k,v in report['functions'].items()})

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--baseline-dir',type=Path,default=ROOT/'build/copy293-baseline',help='archived pre-adoption raw baseline used only to initialize immutable frozen inputs');ap.add_argument('--trace',action='store_true');ap.add_argument('--audit',action='store_true');ap.add_argument('--period',action='store_true');args=ap.parse_args()
    if args.trace:trace()
    elif args.audit:audit()
    elif args.period:gate(('baseline','count'))
    else:
        OUT.mkdir(exist_ok=True);frozen=OUT/'frozen';frozen.mkdir(exist_ok=True)
        for name in ('.build-config','src_pump_v32_V32int.c.o'):
            if not (frozen/name).exists():shutil.copy2(args.baseline_dir/name,frozen/name)
        manifest={'revision':REV,'source_sha256':hashlib.sha256(subprocess.check_output(['git','show',REV+':'+SOURCE],cwd=ROOT)).hexdigest(),'frozen':{n:sha(frozen/n) for n in ('.build-config','src_pump_v32_V32int.c.o')}}
        if (OUT/'manifest.json').exists():assert json.loads((OUT/'manifest.json').read_text())==manifest
        else:(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        d.REV=REV;d.SOURCE_PATHS=(SOURCE,);d.OUT_NAME=OUT.name;d.DUMP_FLAGS=('-v','-save-temps','-da','-dP');d.variants=variants;sys.argv=[sys.argv[0],'--domain','Prior unsigned MRF count only; inspect unchanged compiler-state collateral','--baseline-dir',str(frozen)];d.main();audit()
