#!/usr/bin/env python3
"""2x2 ordinary case-body reintegration x independently validated snapshot index.

Native case66 is inside v34handshak; the outlined helper is reconstruction
apparatus. Reuse one physical body include for direct case and fixture facade.
No always_inline, optimization option, register constraint or copied body.
All exits rejoin the existing single while test; F11523 refutes exit classes.
"""
import argparse,json,hashlib,subprocess,shutil,shlex,re,sys,os
from pathlib import Path
def parse_args():
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--audit',action='store_true')
    ap.add_argument('--period',action='store_true')
    ap.add_argument('--out',type=Path,default=root/'build/v34-trnseg4a-case293')
    ap.add_argument('--baseline-dir',type=Path,default=root/'build/snapshot-dfc-baseline')
    ap.add_argument('--snapshot-object',type=Path,default=Path('/tmp/pr293-v34-snapshot-word/index-word/V34hshak.o'))
    return ap.parse_args()

if __name__=='__main__':
    CLI_ARGS=parse_args()

import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
ROOT=d.ROOT;OUT=ROOT/'build/v34-trnseg4a-case293';REV='dfc23b7d1e92f5913b22b124771211d3e4650771';SOURCE='src/pump/v34/V34hshak.c';HEADER='include/dsplib/v34hstx1_arms.h';BODY='dsplib/v34tx1_trnseg4a_body.inc'
BASELINE=ROOT/'build/snapshot-dfc-baseline'
SNAPSHOT=Path('/tmp/pr293-v34-snapshot-word/index-word/V34hshak.o')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(path):return subprocess.check_output(['git','show',REV+':'+path],cwd=ROOT)
def generate():
    source=git(SOURCE).decode();old=git(HEADER).decode();needle='\tint sum = 0, i, t;';assert old.count(needle)==1;word=old.replace(needle,'\tint sum = 0, t;\n\tshort i;');cells={}
    for snapshot,header in [('si',old),('hi',word)]:
        start=header.index('static int\nv34tx1_trnseg4a(');end=header.index('\n}\n',start)+3;fn=header[start:end];a=fn.index('{\n')+2;body=fn[a:fn.rindex('\n}')];body,n=re.subn(r'\breturn ([^;]+);',r'V34_TRNSEG4A_EXIT(\1);',body);assert n==4
        facade='static int\nv34tx1_trnseg4a(void *objp)\n{\n#define V34_TRNSEG4A_EXIT(value) return (value)\n#include "'+BODY+'"\n#undef V34_TRNSEG4A_EXIT\n}\n'
        newheader=header[:start]+facade+header[end:]
        call='\t\tcase V34HS_TRNSEG4A:\t\t/* 66 0x62e28 */\n\t\t\tv34tx1_trnseg4a(obj);\n\t\t\tbreak;';assert source.count(call)==1
        direct='\t\tcase V34HS_TRNSEG4A:\t\t/* 66 0x62e28 */\n\t\t{\n\t\t\tvoid *objp = obj;\n#define V34_TRNSEG4A_EXIT(value) do { (void)(value); goto trnseg4a_done; } while (0)\n#include "'+BODY+'"\n#undef V34_TRNSEG4A_EXIT\ntrnseg4a_done:\n\t\t\t;\n\t\t}\n\t\t\tbreak;'
        cells[snapshot+'-outlined']={'source':source,'headers':{'dsplib/v34hstx1_arms.h':header}}
        cells[snapshot+'-case']={'source':source.replace(call,direct),'headers':{'dsplib/v34hstx1_arms.h':newheader,BODY:body+'\n'}}
    # Additional bounded discriminator: ordinary nested case bodies, each with
    # exactly one physical home shared with the direct fixture facade.
    for snapshot in ('si', 'hi'):
        cell=cells[snapshot+'-case']; header=cell['headers']['dsplib/v34hstx1_arms.h']; nested=dict(cell['headers']); arm=nested[BODY]
        for name,signature in [('tx1_ts_snapshot','static void'),('tx1_ts_rates','static int')]:
            start=header.index(signature+'\n'+name+'('); end=header.index('\n}\n',start)+3
            fn=header[start:end]; brace=fn.index('{\n')+2; body=fn[brace:fn.rindex('\n}')]; inc='dsplib/'+name+'_body.inc'
            if name=='tx1_ts_rates':
                body,n=re.subn(r'\breturn ([^;]+);',r'V34_TRNSEG4A_EXIT(\1);',body); assert n==1
                facade=fn[:brace]+'#define V34_TRNSEG4A_EXIT(value) return (value)\n#include "'+inc+'"\n#undef V34_TRNSEG4A_EXIT\n}\n'
                arm=arm.replace('V34_TRNSEG4A_EXIT(tx1_ts_rates(o, rx, cfg, rec));','{\n#include "'+inc+'"\n}')
            else:
                assert not re.search(r'\breturn\b',body)
                facade=fn[:brace]+'#include "'+inc+'"\n}\n'
                arm=arm.replace('tx1_ts_snapshot(o, rx, rec);','{\n#include "'+inc+'"\n}')
            nested[inc]=body+'\n'; header=header[:start]+facade+header[end:]
        nested['dsplib/v34hstx1_arms.h']=header; nested[BODY]=arm
        cells[snapshot+'-nested']={'source':cell['source'],'headers':nested}
    return cells

def run(cmd,folder,label):
    with (folder/(label+'.log')).open('w') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
def compile_cells():
    OUT.mkdir(exist_ok=True);frozen=OUT/'frozen';frozen.mkdir(exist_ok=True)
    inputs={'.build-config':BASELINE/'.build-config','dfc.o':BASELINE/'src_pump_v34_V34hshak.c.o','snapshot.o':SNAPSHOT}
    for name,path in inputs.items():
        if not (frozen/name).exists():shutil.copy2(path,frozen/name)
    headers=frozen/'headers'
    if not headers.exists():
        headers.mkdir();archive=subprocess.check_output(['git','archive',REV,'include','tools/toolchain/period_compat.h'],cwd=ROOT);subprocess.run(['tar','xf','-','-C',str(headers)],input=archive,check=True)
    manifest={'source_pin':REV,'inputs':{n:sha(frozen/n) for n in inputs},'fixture_sha256':hashlib.sha256(git('test/unit/t_v34hstx1.c')).hexdigest()}
    assert (ROOT/'test/unit/t_v34hstx1.c').read_bytes()==git('test/unit/t_v34hstx1.c'),'fixture source drift'
    if (frozen/'manifest.json').exists():assert json.loads((frozen/'manifest.json').read_text())==manifest,'immutable input drift'
    else:(frozen/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    config=(frozen/'.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1];flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')));flags=['-I/work/frozen/headers/include' if f=='-Iinclude' else '/work/frozen/headers/tools/toolchain/period_compat.h' if f=='tools/toolchain/period_compat.h' else f for f in flags];flags+=['-v','-save-temps','-da','-dP'];report={'revision':REV,'config':config,'prediction':__doc__,'frozen':{n:sha(frozen/n) for n in inputs},'cells':{}}
    if (OUT/'results.json').exists():
        previous=json.loads((OUT/'results.json').read_text())
        assert previous['revision']==REV and previous['config']==config,'report provenance drift'
        report['cells'].update(previous.get('cells',{}))
    identity=d.tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c','export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; gcc --version; p=$(gcc -print-prog-name=as); "$p" --version'];run(identity,OUT,'identity');report['identity_command']=identity
    for label,cell in generate().items():
        folder=OUT/label;folder.mkdir(exist_ok=True);(folder/'V34hshak.c').write_text(cell['source']);record={'source_sha256':sha(folder/'V34hshak.c'),'headers':{},'commands':[]};report['cells'][label]=record
        for name,text in cell['headers'].items():
            path=folder/'include'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text);record['headers'][name]=sha(path)
        dst='/work/'+label;command=d.tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,['-I'+dst+'/include']+flags,dst+'/V34hshak.o',dst+'/V34hshak.c')];run(command,folder,'compile');record['commands'].append(command);record['object_sha256']=sha(folder/'V34hshak.o')
        if label.endswith('outlined'):assert (folder/'V34hshak.o').read_bytes()==(frozen/('dfc.o' if label.startswith('si') else 'snapshot.o')).read_bytes(),'raw full-TU baseline mismatch '+label
        # Same physical body is exercised by the unchanged direct-arm fixture.
        fixture_flags=[x for x in flags if x not in ('-da','-dP','-save-temps')]+['-I/src/test/harness','-std=gnu99'];command=d.tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c',d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,['-I'+dst+'/include']+fixture_flags,dst+'/fixture.o','test/unit/t_v34hstx1.c')];run(command,folder,'fixture-compile');record['commands'].append(command);record['fixture_sha256']=sha(folder/'fixture.o')
        (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(label,'complete TU + direct fixture compiled',flush=True)
    audit()
def audit():
    ledger=json.loads((OUT/'results.json').read_text());report=json.loads((OUT/'audit.json').read_text()) if (OUT/'audit.json').exists() else {'cells':{}}
    for label in generate():
        folder=OUT/label;path=folder/'V34hshak.o';base=OUT/(label.split('-')[0]+'-outlined')/'V34hshak.o';bc=Object(base).canonical();c=Object(path).canonical();sizes=d.b.sizes(str(path));bs=d.b.sizes(str(base));assert c[2]==bc[2],'strong binding drift';common=set(sizes)&set(bs);record={'object_sha256':sha(path),'functions':len(sizes),'added_functions':sorted(set(sizes)-set(bs)),'removed_functions':sorted(set(bs)-set(sizes)),'changed_bodies':[n for n in sorted(common) if d.b.body(str(path),n)!=d.b.body(str(base),n)],'verdicts':{n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(path),n))) for n in sizes if n in d.b.sizes(d.b.BLOB)},'changed_data_sections':[n for n in set(c[1])|set(bc[1]) if c[1].get(n)!=bc[1].get(n)],'changed_relocation_consumers':[n for n in set(c[3])|set(bc[3]) if c[3].get(n)!=bc[3].get(n)],'fixture_raw_equal_outlined':(folder/'fixture.o').read_bytes()==(base.parent/'fixture.o').read_bytes(),'handshake_instructions':d.b.insns(str(path),'v34handshak')};report['cells'][label]=record
        print(label,record['verdicts']['v34handshak'],'functions',len(sizes),'removed',record['removed_functions'],'fixtureequal',record['fixture_raw_equal_outlined'],flush=True)
    (OUT/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
def period():
    stable=ROOT/'build/period';sources=list(stable.glob('src_*.o'));assert len(sources)==300
    report=json.loads((OUT/'period.json').read_text()) if (OUT/'period.json').exists() else {}
    for label in generate():
        folder=OUT/('period-'+label);folder.mkdir(exist_ok=True)
        for path in sources+list(stable.glob('test_harness_*.o')):
            shutil.copy2(path,folder/path.name)
        selected=folder/'src_pump_v34_V34hshak.o';shutil.copy2(OUT/label/'V34hshak.o',selected)
        fixture=folder/'test_unit_t_v34hstx1.o';shutil.copy2(OUT/label/'fixture.o',fixture)
        before=(sha(selected),sha(fixture));command=['make','period','T=t_v34hstx1','J=4']
        with (folder/'gate.log').open('w') as log:
            status=subprocess.run(command,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert before==(sha(selected),sha(fixture)),'experiment objects overwritten'
        text=(folder/'t_v34hstx1.run.log').read_text()
        report[label]={'command':command,'exit':status,'object_sha256':before[0],'fixture_sha256':before[1],'checks':sum(map(int,re.findall(r'(\d+) checks',text))),'run_log':text,'boundary':'Existing direct-arm component probes plus reference once-per-block tail; synthetic states, not modem lifecycle evidence.'}
        (OUT/'period.json').write_text(json.dumps(report,indent=2)+'\n')
        assert status==0 and 'FAIL' not in text and 'PASS ' in text
        print(label,'period PASS',report[label]['checks'],flush=True)

if __name__=='__main__':
    args=CLI_ARGS;OUT=args.out.resolve();BASELINE=args.baseline_dir.resolve();SNAPSHOT=args.snapshot_object.resolve()
    if args.period:period()
    elif args.audit:audit()
    else:compile_cells()
