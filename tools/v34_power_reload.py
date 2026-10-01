#!/usr/bin/env python3
"""Two full-TU power-input provenance controls; no fuzzing or mutation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import experiment_toolchain as tc
sys.path.insert(0,str(ROOT/'tools/toolchain'))
import byteident as b

def replacement(original):
    source = original
    old = '\tunsigned w = (unsigned short)mp[0];\n'
    assert source.count(old) == 1
    source = source.replace(old, '')
    for shift in (5, 2):
        old = '(w >> %d)' % shift
        assert source.count(old) == 1
        source = source.replace(old, '((unsigned short)mp[0] >> %d)' % shift)
    return source

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--revision',default='b57597e5');ap.add_argument('--baseline-object',type=Path);args=ap.parse_args()
    out=ROOT/'build/v34-power-reload';out.mkdir(parents=True,exist_ok=True)
    source=subprocess.check_output(['git','show',args.revision+':src/pump/v34/V34hshak.c'],cwd=ROOT,text=True)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    control=args.baseline_object or (out/'baseline/V34hshak.o' if (out/'baseline/V34hshak.o').exists() else ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o')
    prior=control.read_bytes();blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    results={'domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5936082208','revision':args.revision,'config':config,'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    variants = ('baseline','direct')
    for variant in variants:
        cell=out/variant;cell.mkdir(parents=True,exist_ok=True);text=source
        if variant!='baseline':
            start=text.index('void\nsettxlevel(');end=text.index('\n}\n',start)+2
            text=text[:start]+replacement(text[start:end])+text[end:]
        (cell/'V34hshak.c').write_text(text);directory='/work/'+variant
        cmd=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(directory)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+['-da'],directory+'/V34hshak.o',directory+'/V34hshak.c')]
        entry={'command':cmd,'source_hash':hashlib.sha256(text.encode()).hexdigest()};results['cells'][variant]=entry
        with (cell/'compile.log').open('w') as log: entry['compile_exit']=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert entry['compile_exit']==0,variant
        obj=str(cell/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals']={l.split()[-1]:l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        (cell/'settxlevel.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=settxlevel',obj],text=True))
        if variant=='baseline':
            assert Path(obj).read_bytes()==prior,'unchanged fullTU drift';entry['baseline_reproduced']=True
        else:
            base=str(out/'baseline/V34hshak.o')
            entry['changed_bodies']=[sym for sym in sorted(set(b.sizes(obj))&set(b.sizes(base))) if b.body(obj,sym)!=b.body(base,sym)]
            entry['added_symbols']=sorted(set(b.sizes(obj))-set(b.sizes(base)));entry['removed_symbols']=sorted(set(b.sizes(base))-set(b.sizes(obj)))
            assert entry['globals']==results['cells']['baseline']['globals']
        print(variant,'power',entry['verdicts']['settxlevel'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
