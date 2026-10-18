#!/usr/bin/env python3
"""Audit FPM_log10 neighbor reads reached by a constructed V32 status path.

The selected mantissa is a measured lifecycle result, not an injected state.
This ELF audit establishes layout, separately from the period lifecycle gate.
"""
import argparse,hashlib,json,shlex,shutil,subprocess
from pathlib import Path
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile

ROOT=d.ROOT

def negative_control(period,out):
    root=period.parents[1]
    image='ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
    files=[]
    for value in (period/'objs.list').read_text().split():
        path=period/Path(value).name;exposed=period/'testhost'/path.name
        files.append('/out/testhost/'+path.name if exposed.exists() else value)
    assert len(files)==305
    args=['gcc','-static','-Wl,-N','-o','/work/individual-negative.exe','/out/test_unit_t_v32fpdisp.o']+files+['build/dsplibs_ref.o','-lm']
    command=d.tc.docker_prefix(image,root,out,True,output=period)+['/bin/sh','-c','export PATH='+shlex.quote(d.tc.GENTOO_COMPILER_PATH)+':$PATH; exec '+shlex.join(args)]
    with (out/'negative-link.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    run=d.tc.docker_prefix(image,root,out,True,output=period)+['/work/individual-negative.exe']
    with (out/'negative-run.log').open('w') as log:result=subprocess.run(run,stdout=log,stderr=subprocess.STDOUT)
    text=(out/'negative-run.log').read_text()
    assert result.returncode!=0 and 'FAIL V32FP_status after timing component lifecycle' in text
    shutil.copy2(period/'t_v32fpdisp',out/'aggregate-positive.exe')
    shutil.copy2(period/'t_v32fpdisp.run.log',out/'aggregate-positive.run.log')
    (out/'negative-control.json').write_text(json.dumps({'command':command,'run_command':run,'exit':result.returncode,'link_objects':len(files),'source_objects':300,'input_sha256':{str(period/value.removeprefix('/out/')):hashlib.sha256((period/value.removeprefix('/out/')).read_bytes()).hexdigest() for value in files},'fixture_object_sha256':hashlib.sha256((period/'test_unit_t_v32fpdisp.o').read_bytes()).hexdigest(),'reference_object_sha256':hashlib.sha256((root/'build/dsplibs_ref.o').read_bytes()).hexdigest()},indent=2)+'\n')

def layout(path,symbol='FPM_log10_table'):
    with path.open('rb') as stream:
        elf=ELFFile(stream);symbols=list(elf.get_section_by_name('.symtab').iter_symbols())
        found=[s for s in symbols if s.name==symbol];assert len(found)==1,(path,symbol,len(found))
        table=found[0];section=elf.get_section(table['st_shndx']);start=table['st_value']-section['sh_addr'];data=section.data()
        window=data[start:start+772];assert len(window)==772
        at=table['st_value']+378*2
        owners=[s for s in symbols if s['st_shndx']==table['st_shndx'] and s['st_size'] and s['st_value']<=at<s['st_value']+s['st_size']]
        assert len(owners)==1
        owner=owners[0];value=int.from_bytes(window[756:758],'little',signed=True)
        return {'extent':table['st_size'],'window_386_words':window.hex(),'word378':value,'log_m64716_e0':value>>3,'status_scaled_log':(value>>3)>>3,'word378_owner':owner.name,'offset_in_owner':at-owner['st_value'],'pps_relative_offset':next(s['st_value']-table['st_value'] for s in symbols if s.name==('ref_FPM_PPS_CFG' if symbol.startswith('ref_') else 'FPM_PPS_CFG'))}

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--positive',type=Path,default=ROOT/'build/period/t_fpm_log10');ap.add_argument('--positive-log',type=Path);ap.add_argument('--make-negative-from',type=Path);ap.add_argument('--out',type=Path,default=ROOT/'build/v32-log-neighbor293');args=ap.parse_args();args.out.mkdir(exist_ok=True)
    if args.make_negative_from:
        negative_control(args.make_negative_from.resolve(),args.out.resolve())
        args.positive=args.out/'aggregate-positive.exe';args.positive_log=args.out/'aggregate-positive.run.log'
    negative=args.out/'individual-negative.exe';blob=Path(d.b.BLOB).resolve()
    canonical=layout(blob);good=layout(args.positive);goodref=layout(args.positive,'ref_FPM_log10_table');bad=layout(negative);badref=layout(negative,'ref_FPM_log10_table')
    for record in (good,goodref,badref):
        assert record['extent']==canonical['extent']==256
        assert record['window_386_words']==canonical['window_386_words']
        assert record['pps_relative_offset']==canonical['pps_relative_offset']==256
    assert bad['window_386_words']!=canonical['window_386_words']
    assert bad['word378']==26702 and canonical['word378']==32381
    assert bad['status_scaled_log']==417 and canonical['status_scaled_log']==505
    assert ((64716+64)>>7)-128==378
    report={'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (blob,negative,args.positive)},'index':378,'mantissa':64716,'exponent':0,'window_words_checked':386,'input_contract_window_words':385,'extra_guard_word':1,'reference':canonical,'aggregate_positive':good,'aggregate_reference':goodref,'individual_negative':bad,'individual_reference':badref,'conclusion':'Individual source linking changes neighboring-object placement. The reproduced log routine reads FPM_sqrt_table at a different relative offset and produces scaled417 rather than reference505. Canonical aggregate preserves every one of386 audited words including the385-word input-domain window. No arithmetic source discrepancy, ABI change, guard or tolerance is implicated. Fixture must measure the original relocatable-object boundary.'}
    if args.positive_log:
        text=args.positive_log.read_text();assert 'PASS V32FP_status after timing component lifecycle' in text and 'FAIL' not in text
        report['lifecycle_positive_log']=text
    (args.out/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('386/386 aggregate source/ref neighbor words match original;individual negative differs;idx378 source26702/ref32381 explains417/505')

if __name__=='__main__':main()
