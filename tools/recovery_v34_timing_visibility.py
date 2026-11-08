#!/usr/bin/env python3
"""Four complete-TU TimingV34 helper graph/visibility controls, no adopted flags.

Prior five-cell role experiment already established direct-report two-switch
source restores helper calls. This cross tests unchanged-helper separation and
order against that source mechanism on the current arithmetic reconstruction.
Prediction: order retains inline; separation and native-graph restore calls.
Original helper TU ownership is unknown; separation is diagnostic only.
Fresh replay after source adoption:
  python3 tools/recovery_v34_timing_visibility.py --out build/v34-timing-visibility293-fresh --baseline-dir build/copy293-baseline
Follow with --out ... --collateral and --tables. --period runs existing focused
synthetic component fixtures, including captured TimingV34 debug transcripts.
Pinned 0ffc source equals 96ce7c20d for this TU; archived raw baseline mandatory.
"""
import argparse,hashlib,json,shlex,shutil,subprocess,os,re
from pathlib import Path
import playbook_small_patterns as d
from recovery_fax_debug_audit import Object
ROOT=d.ROOT
REV='0ffc6b0952100b7cd1b12df77b1bee7fd6069bd0'
SOURCE='src/pump/v34/V34hshak.c'
HELPER='setTimingStateParameters';TARGET='TimingV34'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def generate():
    s=subprocess.check_output(['git','show',REV+':'+SOURCE],cwd=ROOT,text=True)
    a=s.index('void\nsetTimingStateParameters(');z=s.index('\n}\n',a)+3;fn=s[a:z];without=s[:a]+s[z:]
    head='#include "dsplib/v34fsk.h"\n#include "dsplib/v34pcmif.h"\n'
    lines=['void','setTimingStateParameters(void *objp)','{','\tstruct v34_object *obj = (struct v34_object *)objp;','\tstruct v34_receiver *rx = &obj->receiver;']
    # Native falls through the non-0x65 table and branches to the 0x65 table.
    for arm,variant in enumerate((False,True)):
        lines+=['\tif (obj->role != 0x65) {' if arm==0 else '\t} else {','\t\tswitch ((int)(short)rx->pllcnt) {','\t\tcase 0:','\t\tcase 1:','\t\t\tbreak;']
        values={2:(0x36b0,0,0x190),3:(0x2ee0,0xd2,0x3e8),4:(0x1770,0x5a,0x3e8),5:(0xdac,0x1e,-1 if variant else 0x3e8),6:(0xdac,3,0x7d0) if variant else (0x7d0,0xa,0x7d0),7:(0x3e8,2,0x7d0) if variant else (0x5dc,2,0xfa0),8:(0x1f4,1,-1)}
        for state,vs in values.items():
            lines+=['\t\tcase %d:'%state]
            lines+=['\t\t\trx->%s = %s;'%(field,hex(v) if v>=0 else '-1') for field,v in zip(('timing_p_gain','timing_i_gain','dwell_limit'),vs)]
            if state in (5,8):lines+=['\t\t\tVPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));']
            lines+=['\t\t\tbreak;']
        lines+=['\t\tdefault:','\t\t\tbreak;','\t\t}']
    lines+=['\t}','\tif ((unsigned short)rx->pllcnt == 2)','\t\trx->report_interval = (short)(obj->ratecfg.rx_baud >> 3);','}','']
    cells={'baseline':{'V34hshak.c':s},'helper-after':{'V34hshak.c':without+'\n'+fn},'helper-separate':{'V34hshak.c':without,'timing-helper.c':head+fn},'native-graph':{'V34hshak.c':s[:a]+'\n'.join(lines)+s[z:]}}
    assert len({tuple(c.values()) for c in cells.values()})==4
    return cells

def period(out):
    report={};stable=ROOT/'build/period';sources=list(stable.glob('src_*.o'));assert len(sources)==300
    for label in ('baseline','helper-separate','native-graph'):
        folder=out/('period-'+label);folder.mkdir(exist_ok=True)
        for path in sources+list(stable.glob('test_harness_*.o'))+[stable/'test_unit_t_v34rx.o']:
            shutil.copy2(path,folder/path.name)
        selected=folder/'src_pump_v34_V34hshak.o';shutil.copy2(out/label/'union.o',selected);digest=sha(selected)
        command=['make','period','T=t_v34rx','J=4']
        with (folder/'gate.log').open('w') as log:status=subprocess.run(command,cwd=ROOT,env=dict(os.environ,PERIOD_OUT=str(folder.relative_to(ROOT))),stdout=log,stderr=subprocess.STDOUT).returncode
        assert sha(selected)==digest,'candidate overwritten'
        text=(folder/'t_v34rx.run.log').read_text();assert status==0 and 'FAIL' not in text and 'PASS ' in text
        report[label]={'command':command,'exit':status,'object_sha256':digest,'checks':sum(map(int,re.findall(r'(\d+) checks',text))),'log':text,'scope':'Existing component synthetic state sweeps, not modem lifecycle evidence; TimingV34 covers captured debug transcripts.'}
        (out/'period.json').write_text(json.dumps(report,indent=2)+'\n');print(label,'period PASS',report[label]['checks'],flush=True)

def table_audit(out):
    """Execute actual relocated native/candidate dispatch tables, replacing only log API."""
    from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ESP
    from elftools.elf.elffile import ELFFile
    def probe(path,corrupt=False):
        with open(path,'rb') as stream:
            elf=ELFFile(stream);sections=list(elf.iter_sections());syms=list(elf.get_section_by_name('.symtab').iter_symbols())
            loc={i:0x100000 if s.name=='.text' else 0x300000 for i,s in enumerate(sections) if s.name in ('.text','.rodata')}
            raw={i:bytearray(sections[i].data()) for i in loc};fn=next(s for s in syms if s.name==HELPER);start=loc[fn['st_shndx']]+fn['st_value'];tables=[]
            for rels in sections:
                if rels['sh_type']!='SHT_REL' or rels['sh_info'] not in raw:continue
                ix=rels['sh_info'];tab=elf.get_section(rels['sh_link'])
                for r in rels.iter_relocations():
                    at=r['r_offset'];symbol=tab.get_symbol(r['r_info_sym']);add=int.from_bytes(raw[ix][at:at+4],'little',signed=True)
                    value=loc.get(symbol['st_shndx'],0x700000)+symbol['st_value'] if isinstance(symbol['st_shndx'],int) else 0x700000
                    if ix==fn['st_shndx'] and fn['st_value']<=at<fn['st_value']+fn['st_size'] and r['r_info_type']==1 and isinstance(symbol['st_shndx'],int) and sections[symbol['st_shndx']].name=='.rodata':tables.append((symbol['st_shndx'],symbol['st_value']+add))
                    if symbol.name=='VPcmV34LogTimingOffset':value=0x700000
                    value+=add
                    if r['r_info_type']==2:value-=loc[ix]+at
                    raw[ix][at:at+4]=(value&0xffffffff).to_bytes(4,'little')
            table_targets=[[int.from_bytes(raw[ix][at+4*k:at+4*k+4],'little')-start for k in range(9)] for ix,at in tables]
            if corrupt:
                ix,at=tables[0];raw[ix][at+8:at+12]=raw[ix][at:at+4]
        uc=Uc(UC_ARCH_X86,UC_MODE_32)
        for ix,data in raw.items():uc.mem_map(loc[ix],0x200000 if loc[ix]==0x100000 else 0x100000);uc.mem_write(loc[ix],bytes(data))
        for address in (0x500000,0x600000,0x700000):uc.mem_map(address,0x10000)
        uc.mem_write(0x700000,b'\xc3');calls=[]
        def hook(uc,at,size,user):
            if at==0x700000:
                sp=uc.reg_read(UC_X86_REG_ESP);args=bytes(uc.mem_read(sp+4,8));calls.append((int.from_bytes(args[:4],'little')-0x500000,int.from_bytes(args[4:],'little',signed=True)))
        uc.hook_add(UC_HOOK_CODE,hook);results=[]
        states=(0,1,2,3,4,5,6,7,8,9,100,-1,-8,32767,-32768)
        for role in (0x11,0x65):
            for state in states:
                for ppm in (-3000,-1500,0,1500,3000):
                    uc.mem_write(0x500000,bytes(0xb000));uc.mem_write(0x50359c,role.to_bytes(2,'little'));uc.mem_write(0x500424,(state&65535).to_bytes(2,'little'));uc.mem_write(0x500434,(ppm&65535).to_bytes(2,'little'));uc.mem_write(0x50aa96,(400).to_bytes(2,'little'))
                    uc.mem_write(0x608000,(0x700100).to_bytes(4,'little')+(0x500000).to_bytes(4,'little'));uc.reg_write(UC_X86_REG_ESP,0x608000);calls.clear();uc.emu_start(start,0x700100,count=1000)
                    fields={hex(off):int.from_bytes(uc.mem_read(0x500000+off,2),'little',signed=True) for off in (0x496,0x498,0x49a,0x436)}
                    results.append({'role':role,'state':state,'ppm':ppm,'fields':fields,'calls':list(calls)})
        return {'table_targets_relative_to_helper':table_targets,'cases':results}
    native=probe(Path(d.b.BLOB));candidate=probe(out/'native-graph/union.o');negative=probe(out/'native-graph/union.o',True)
    assert len(native['table_targets_relative_to_helper'])==len(candidate['table_targets_relative_to_helper'])==2
    (out/'table-dispatch-raw.json').write_text(json.dumps({'native':native,'candidate':candidate},indent=2)+'\n')
    assert native['cases']==candidate['cases'],'actual dispatch/store/callback mismatch'
    rejected=[i for i,(a,b) in enumerate(zip(native['cases'],negative['cases'])) if a!=b];assert rejected,'table negative control did not fire'
    report={'boundary':'Machine helper with external log API recorded; synthetic state sweep, not lifecycle reachability. All table pointers actually relocated and executed.','case_denominator':len(native['cases']),'table_entry_denominator':18,'native':native,'candidate':candidate,'negative_control':{'mutation':'first table case2 pointer replaced with case0 pointer','rejected_cases':rejected,'case_denominator':150}}
    (out/'table-audit.json').write_text(json.dumps(report,indent=2)+'\n');print('actual tables/cases PASS',len(native['cases']),'negative rejects',len(rejected),flush=True)

def collateral(out):
    def trimmed(obj):
        parsed=Object(obj);data=next(bytes(raw) for name,raw in parsed.sections.values() if name=='.rodata');remove=set()
        for ix,at,symbol,kind in parsed.relocations:
            if parsed.sections[ix][0]!='.rodata':continue
            add=int.from_bytes(data[at:at+4],'little',signed=True);payload=parsed.payload(symbol,add)
            if payload[0]=='code' and payload[1] in (HELPER,TARGET):remove.update(range(at,at+4))
        return bytes(b for i,b in enumerate(data) if i not in remove).rstrip(b'\0'),len(remove)//4
    baseline=out/'baseline/union.o';bp=Object(baseline).canonical();old,entries=trimmed(baseline);report={'baseline_removed_table_entries':entries,'unrelated_rodata_sha256':hashlib.sha256(old).hexdigest(),'cells':{}}
    for label in generate():
        obj=out/label/'union.o';parsed=Object(obj).canonical();other,count=trimmed(obj);assert other==old,'non-target rodata changed'
        for section,raw in bp[1].items():
            if section not in ('.rodata','.comment'):assert parsed[1].get(section)==raw,(label,section)
        for owner,relocs in bp[3].items():
            if owner not in ('.rodata',HELPER,TARGET):assert parsed[3].get(owner)==relocs,(label,owner)
        text='\n'.join(p.read_text() for p in (out/label).glob('*.00.cgraph'))
        report['cells'][label]={'removed_target_table_entries':count,'unrelated_rodata_equal':True,'unrelated_relocation_consumers_equal':True,'other_data_equal':True,'helper_frontend_records':sorted(set(re.findall(r'^setTimingStateParameters:.*$',text,re.M))),'section_hashes':{n:hashlib.sha256(raw).hexdigest() for n,raw in parsed[1].items()},'metadata_exception':'.comment is duplicated by the diagnostic separate helper compilation' if label=='helper-separate' else None}
    (out/'collateral.json').write_text(json.dumps(report,indent=2)+'\n');print('unrelated data/consumer audit PASS4cells',flush=True)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--out',type=Path,default=ROOT/'build/v34-timing-visibility293');ap.add_argument('--baseline-dir',type=Path,default=ROOT/'build/copy293-baseline',help='archived pre-adoption object/config; initializes immutable frozen inputs only');ap.add_argument('--collateral',action='store_true');ap.add_argument('--tables',action='store_true');ap.add_argument('--period',action='store_true');args=ap.parse_args();out=args.out.resolve();args.baseline_dir=args.baseline_dir.resolve()
    if args.collateral:return collateral(out)
    if args.tables:return table_audit(out)
    if args.period:return period(out)
    out.mkdir(exist_ok=True);frozen=out/'frozen';frozen.mkdir(exist_ok=True)
    assert not subprocess.check_output(['git','diff',REV,'--name-only','--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).strip(),'header drift'
    for src,name in [(args.baseline_dir/'.build-config','.build-config'),(args.baseline_dir/'src_pump_v34_V34hshak.c.o','baseline.o')]:
        if not (frozen/name).exists():shutil.copy2(src,frozen/name)
    manifest={'revision':REV,'source_sha256':hashlib.sha256(subprocess.check_output(['git','show',REV+':'+SOURCE],cwd=ROOT)).hexdigest(),'object_sha256':sha(frozen/'baseline.o'),'config_sha256':sha(frozen/'.build-config')}
    if (frozen/'manifest.json').exists():assert json.loads((frozen/'manifest.json').read_text())==manifest,'frozen inputs changed'
    else:(frozen/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    config=(frozen/'.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1];flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')));flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags];flags+=['-v','-save-temps','-da','-fdump-tree-original','-fdump-tree-inlined']
    report={'revision':REV,'config':config,'baseline_sha256':sha(frozen/'baseline.o'),'headers':{str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'include').rglob('*.h')},'prediction':__doc__,'cells':{}}
    def run(cmd,folder,label):
        with (folder/(label+'.log')).open('w') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    identity=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; gcc --version; p=$(gcc -print-prog-name=as); "$p" --version; p=$(gcc -print-prog-name=ld); "$p" --version'];run(identity,out,'identity');report['identity_command']=identity
    base=None
    for label,sources in generate().items():
        folder=out/label;folder.mkdir(exist_ok=True);rec={'commands':[],'source_hashes':{}};report['cells'][label]=rec
        for name,text in sources.items():
            (folder/name).write_text(text);rec['source_hashes'][name]=sha(folder/name);dst='/work/'+label
            cmd=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH,flags,dst+'/'+name+'.o',dst+'/'+name)];rec['commands'].append(cmd);run(cmd,folder,name+'.compile')
            if label=='baseline':assert (folder/(name+'.o')).read_bytes()==(frozen/'baseline.o').read_bytes(),'raw baseline mismatch'
        cmd=d.tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; exec gcc -nostdlib -r -o /work/'+label+'/union.o '+' '.join('/work/'+label+'/'+n+'.o' for n in sources)];run(cmd,folder,'partial-link');rec['commands'].append(cmd)
        obj=folder/'union.o';current=Object(obj).canonical();sizes=d.b.sizes(str(obj))
        if base is None:base=(obj,current,sizes)
        bp,bc,bs=base;assert set(sizes)==set(bs),'function surface';assert current[2]==bc[2],'public binding surface'
        rec.update({'object_sha256':sha(obj),'function_count':len(sizes),'sizes':sizes,'verdicts':{n:list(d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n))) for n in sizes},'changed_bodies':[n for n in sizes if d.b.body(str(obj),n)!=d.b.body(str(bp),n)],'changed_data_sections':[n for n in set(current[1])|set(bc[1]) if current[1].get(n)!=bc[1].get(n)],'changed_relocation_consumers':[n for n in set(current[3])|set(bc[3]) if current[3].get(n)!=bc[3].get(n)],'bindings_equal':True,'target_edges':[(op,operand) for op,operand in d.b.insns(str(obj),TARGET) if op in ('call','jmp')],'helper_consumers':[n for n in sizes if any(HELPER in operands and op in ('call','jmp') for op,operands in d.b.insns(str(obj),n))]})
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(label,rec['verdicts'][TARGET],rec['verdicts'][HELPER],len(rec['changed_bodies']),flush=True)
if __name__=='__main__':main()
