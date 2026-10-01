#!/usr/bin/env python3
"""Bounded full-TU bit-reader source controls; no fuzzing or mutation execution."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import shlex
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));import experiment_toolchain as tc
sys.path.insert(0,str(ROOT/'tools/toolchain'));import byteident as b


def rewrite(body,counters,shifts,crc):
    if counters in ('short','unsigned-short'):
        for old,new in [('int n = (unsigned short)b->avail;', ('short' if counters=='short' else 'unsigned short')+' n = b->avail;'),('int left;','short left;')]:
            assert body.count(old)==1
            body=body.replace(old,new)
    if shifts=='direct':
        for old,new in [('((short)wb & 31)','(short)wb'),('(left & 31)','left'),('(folded & 31)','folded'),('(n & 31)','n')]:
            assert body.count(old)==1
            body=body.replace(old,new)
    if crc=='signed':
        old='unsigned int crc = (unsigned short)b->crc;\n\t\t\t\tint bit = (int)(crc >> 15);'
        assert body.count(old)==1
        body=body.replace(old,'short crc = b->crc;\n\t\t\t\tint bit = crc < 0;')
    if crc=='signed-top':
        old='int bit = (int)(crc >> 15);'
        assert body.count(old)==1
        body=body.replace(old,'int bit = (short)crc < 0;')
    return body


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--crc-top-study',action='store_true');args=parser.parse_args()
    out=ROOT/('build/v34-getbit-crc-top' if args.crc_top_study else 'build/v34-getbit-source');out.mkdir(parents=True,exist_ok=True)
    revision='d67e4042'
    def saved(name):return subprocess.check_output(['git','show',revision+':'+name],cwd=ROOT,text=True)
    source=saved('src/pump/v34/V34hshak.c');header=saved('include/dsplib/v34hstx1_arms.h')
    start=header.index('static short\ngetbit(');end=header.index('\n}\n',start)+2
    body=header[start:end]
    variants=[('baseline',None)]+[('-'.join(cell),cell) for cell in itertools.product(('int','short'),('masked','direct'),('unsigned','signed'))]
    if args.crc_top_study:
        variants=[('baseline',None)]+[('-'.join(cell),cell) for cell in itertools.product(('int','short','unsigned-short'),('masked','direct'),('signed-top',))]
    generated={name:header if cell is None else header[:start]+rewrite(body,*cell)+header[end:] for name,cell in variants}
    assert len(generated)==(7 if args.crc_top_study else 9) and len(set(generated.values()))==(7 if args.crc_top_study else 8)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    prior=(out/'baseline/V34hshak.o' if (out/'baseline/V34hshak.o').exists() else ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o').read_bytes()
    blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    results={'revision':revision,'config':config,'domain':('https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5937081347' if args.crc_top_study else 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5936993590'),'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    for name,cell in variants:
        directory=out/name;(directory/'include/dsplib').mkdir(parents=True,exist_ok=True)
        (directory/'V34hshak.c').write_text(source);(directory/'include/dsplib/v34hstx1_arms.h').write_text(generated[name])
        target='/work/'+name
        command=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(target)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,['-I'+target+'/include']+flags+['-da'],target+'/V34hshak.o',target+'/V34hshak.c')]
        entry={'command':command,'header_hash':hashlib.sha256(generated[name].encode()).hexdigest()};results['cells'][name]=entry
        with (directory/'compile.log').open('w') as log:entry['compile_exit']=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert entry['compile_exit']==0,name
        obj=str(directory/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals']={line.split()[-1]:line.split()[-2] for line in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['function_symbols']=sorted(b.sizes(obj))
        entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        (directory/'getbit.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=getbit',obj],text=True))
        if name=='baseline':
            assert Path(obj).read_bytes()==prior,'raw unchanged baseline drift';entry['baseline_reproduced']=True
        else:
            base=results['cells']['baseline'];assert entry['globals']==base['globals']
            assert entry['function_symbols']==base['function_symbols']
            baseline=str(out/'baseline/V34hshak.o')
            entry['changed_bodies']=[sym for sym in entry['function_symbols'] if b.body(obj,sym)!=b.body(baseline,sym)]
            entry['exact_gains']=[sym for sym,v in entry['verdicts'].items() if v[0]=='EXACT' and base['verdicts'][sym][0]!='EXACT']
            entry['exact_losses']=[sym for sym,v in base['verdicts'].items() if v[0]=='EXACT' and entry['verdicts'][sym][0]!='EXACT']
        print(name,entry['verdicts']['getbit'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
