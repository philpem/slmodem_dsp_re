#!/usr/bin/env python3
"""Four full-TU pre-emphasis dispatch/default controls; no fuzzing or mutation."""
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

def replacement(variant, original):
    source = original
    if variant.endswith('uninitialized'):
        source = source.replace('\tx = 0;\n\tratio = 0;\n', '')
    if variant.startswith('if-'):
        start = source.index('\tswitch (baudrate) {')
        end = source.index('\n\t}', start) + 3
        arms = []
        for baud, member, ratio in ((3429,'PREEMP_M3429','0x6626'), (3200,'PREEMP_M3200','0x639f'), (3000,'PREEMP_M3000','0x656f'), (2800,'PREEMP_M2400','0x6789'), (2400,'PREEMP_M2400','0x7da7')):
            arms += ['\t' + ('if' if not arms else '} else if') + ' (baudrate == ' + str(baud) + ') {', '\t\tx = preemp_get(p, ' + member + ');', '\t\tratio = ' + ratio + ';']
        arms += ['\t}']
        source = source[:start] + '\n'.join(arms) + source[end:]
    return source

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--revision',default='d884e844');ap.add_argument('--baseline-object',type=Path);args=ap.parse_args()
    out=ROOT/'build/v34-preemp-dispatch';out.mkdir(parents=True,exist_ok=True)
    source=subprocess.check_output(['git','show',args.revision+':src/pump/v34/V34hshak.c'],cwd=ROOT,text=True)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    control=args.baseline_object or (out/'baseline/V34hshak.o' if (out/'baseline/V34hshak.o').exists() else ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o')
    prior=control.read_bytes();blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    results={'domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5935262209','revision':args.revision,'config':config,'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    variants = ('baseline','switch-uninitialized','if-initialized','if-uninitialized')
    for variant in variants:
        cell=out/variant;cell.mkdir(parents=True,exist_ok=True);text=source
        if variant!='baseline':
            start=text.index('short\npreempindex(');end=text.index('\n}\n',start)+2
            text=text[:start]+replacement(variant,text[start:end])+text[end:]
        (cell/'V34hshak.c').write_text(text);directory='/work/'+variant
        cmd=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(directory)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+['-da'],directory+'/V34hshak.o',directory+'/V34hshak.c')]
        entry={'command':cmd,'source_hash':hashlib.sha256(text.encode()).hexdigest()};results['cells'][variant]=entry
        with (cell/'compile.log').open('w') as log: entry['compile_exit']=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert entry['compile_exit']==0,variant
        obj=str(cell/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals']={l.split()[-1]:l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        (cell/'preempindex.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=preempindex',obj],text=True))
        if variant=='baseline':
            assert Path(obj).read_bytes()==prior,'unchanged fullTU drift';entry['baseline_reproduced']=True
        else:
            base=str(out/'baseline/V34hshak.o')
            entry['changed_bodies']=[sym for sym in sorted(set(b.sizes(obj))&set(b.sizes(base))) if b.body(obj,sym)!=b.body(base,sym)]
            entry['added_symbols']=sorted(set(b.sizes(obj))-set(b.sizes(base)));entry['removed_symbols']=sorted(set(b.sizes(base))-set(b.sizes(obj)))
            assert entry['globals']==results['cells']['baseline']['globals']
        print(variant,'preemp',entry['verdicts']['preempindex'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
