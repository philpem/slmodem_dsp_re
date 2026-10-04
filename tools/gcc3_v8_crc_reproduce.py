#!/usr/bin/env python3
"""Replay issue250's six diagnostic controls for the rejected V8 CRC lead."""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path
import experiment_toolchain as tc
import playbook_small_patterns as driver

ROOT = driver.ROOT
b = driver.b
REV = '9c0be89f'
OUT = ROOT/'build/gcc3-v8-crc-rtl'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regmove-cross",action="store_true")
    parser.add_argument("--source-order",action="store_true")
    parser.add_argument("--baseline-dir",type=Path,default=ROOT/"build/production-before")
    options=parser.parse_args()
    cross=options.regmove_cross
    order=options.source_order
    assert not (cross and order)
    prior=OUT
    if cross:
        OUT=ROOT/"build/gcc3-v8-crc-regmove"
    if order:
        OUT=ROOT/"build/gcc3-v8-crc-order"
    OUT.mkdir(parents=True, exist_ok=True)
    config = (options.baseline_dir/'.build-config').read_text()
    image = config.splitlines()[0].split(' ',1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f=='-Iinclude' else '/src/'+f if f=='tools/toolchain/period_compat.h' else f for f in flags]
    source = subprocess.check_output(['git','show',REV+':src/v8/V8global.c'],cwd=ROOT,text=True)
    assert not subprocess.check_output(['git','diff','--name-only',REV,'--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).strip()
    start,end,fn = driver.function(source,'v8_crc')
    old = '\tint msb = ((int)(short)crc) < 0 ? 1 : 0;'
    assert source.count(old)==1
    inputs = {'full':source,'signed-field':source.replace(old,'\tint msb = hs->crc < 0 ? 1 : 0;'),
              'extracted':'#include "dsplib/v8.h"\n\nvoid\n'+fn+'\n'}
    if cross or order:
        inputs.pop('extracted')
    if order:
        block='\tunsigned int crc = (unsigned short)hs->crc;\n\tint msb = hs->crc < 0 ? 1 : 0;'
        assert inputs['signed-field'].count(block)==1
        inputs['signed-first']=inputs['signed-field'].replace(block,'\tint msb = hs->crc < 0 ? 1 : 0;\n\tunsigned int crc = (unsigned short)hs->crc;')
    headers = subprocess.check_output(['git','ls-tree','-r','--name-only',REV,'--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).splitlines()
    domain='https://github.com/philpem/slmodem_dsp_re/issues/250'+('#issuecomment-5975084108' if order else '#issuecomment-5975070457' if cross else '')
    report = {'revision':REV,'domain':domain,
              'config':config,'headers':{p:digest(ROOT/p) for p in headers},'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    for label,text in inputs.items():
        for dump in ((True,) if order else (False,True)):
            name=label+(('-off' if dump else '-on') if cross else ('-rtl' if dump else '-plain'))
            folder=OUT/name;folder.mkdir(exist_ok=True)
            src=folder/'V8global.c';src.write_text(text)
            target='/work/'+name
            shell='cd '+shlex.quote(target)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+(['-da']+(['-fno-regmove'] if dump else []) if cross else ['-da'] if dump else []),target+'/candidate.o',target+'/V8global.c')
            command=tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c',shell]
            with (folder/'compile.log').open('w') as log:
                status=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
            entry={'source_sha256':digest(src),'command':command,'compile_exit':status}
            report['cells'][name]=entry
            (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
            assert status==0,(name,status)
            obj=folder/'candidate.o'
            entry.update(object_sha256=digest(obj),verdict=b.verdict(*b.body(b.BLOB,'v8_crc'),*b.body(str(obj),'v8_crc')))
            (folder/'v8_crc.dis').write_text(subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),str(obj),'v8_crc'],text=True))
            if dump and not (cross or order):
                plain=OUT/(label+'-plain')/'candidate.o'
                assert obj.read_bytes()==plain.read_bytes(),name+' diagnostic drift'
            if dump or cross:
                for path in folder.glob('V8global.c.*'):
                    if path.suffix in ('.c','.o'):continue
                    content=path.read_text()
                    marker=';; Function v8_crc'
                    if marker in content:
                        part=content.split(marker,1)[1].split(';; Function ',1)[0]
                        (folder/('v8_crc'+path.name[len('V8global.c'):])).write_text(marker+part)
            print(name,entry['verdict'],flush=True)
    if order:
        for label in ('full','signed-field'):
            assert (OUT/(label+'-rtl')/'candidate.o').read_bytes()==(prior/(label+'-rtl')/'candidate.o').read_bytes(),label+' order control drift'
        base=str(OUT/'full-rtl/candidate.o');candidate=str(OUT/'signed-first-rtl/candidate.o')
        functions=sorted(b.sizes(base))
        entry=report['cells']['signed-first-rtl']
        entry['changed_bodies']=[n for n in functions if b.body(base,n)!=b.body(candidate,n)]
        entry['gains']=[n for n in functions if b.verdict(*b.body(b.BLOB,n),*b.body(candidate,n))[0]=='EXACT' and b.verdict(*b.body(b.BLOB,n),*b.body(base,n))[0]!='EXACT']
        entry['losses']=[n for n in functions if b.verdict(*b.body(b.BLOB,n),*b.body(base,n))[0]=='EXACT' and b.verdict(*b.body(b.BLOB,n),*b.body(candidate,n))[0]!='EXACT']
        report['domain']='https://github.com/philpem/slmodem_dsp_re/issues/250#issuecomment-5975084108'
        report['controls']={'valid_compiles':3,'retained_raw_controls':2}
        (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print('3/3 source-order cells, 2/2 raw retained controls pass',entry['gains'],entry['losses'])
        return
    if cross:
        for label in inputs:
            assert (OUT/(label+'-on')/'candidate.o').read_bytes()==(prior/(label+'-rtl')/'candidate.o').read_bytes(),label+' retained control drift'
            base=str(OUT/(label+'-on')/'candidate.o'); candidate=str(OUT/(label+'-off')/'candidate.o')
            functions=sorted(b.sizes(base))
            assert functions==sorted(b.sizes(candidate))
            report['cells'][label+'-off']['changed_bodies']=[n for n in functions if b.body(base,n)!=b.body(candidate,n)]
            report['cells'][label+'-off']['gains']=[n for n in functions if b.verdict(*b.body(b.BLOB,n),*b.body(candidate,n))[0]=='EXACT' and b.verdict(*b.body(b.BLOB,n),*b.body(base,n))[0]!='EXACT']
            report['cells'][label+'-off']['losses']=[n for n in functions if b.verdict(*b.body(b.BLOB,n),*b.body(base,n))[0]=='EXACT' and b.verdict(*b.body(b.BLOB,n),*b.body(candidate,n))[0]!='EXACT']
        report['domain']='https://github.com/philpem/slmodem_dsp_re/issues/250#issuecomment-5975070457'
        report['controls']={'valid_compiles':4,'retained_raw_controls':2}
        (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print('4/4 cross cells, 2/2 raw retained controls pass')
        return
    full=OUT/'full-plain/candidate.o'
    assert full.read_bytes()==(options.baseline_dir/'src_v8_V8global.c.o').read_bytes(),'production drift'
    assert b.body(str(full),'v8_crc')==b.body(str(OUT/'extracted-plain/candidate.o'),'v8_crc'),'extraction body drift'
    report['controls']={'valid_compiles':6,'diagnostic_pairs':3,'production_raw_match':True,'extracted_body_match':True}
    (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('6/6 valid compiles; 3/3 raw diagnostic pairs; production and extracted body controls pass')

if __name__=='__main__':
    main()
