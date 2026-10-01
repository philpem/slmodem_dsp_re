#!/usr/bin/env python3
"""Seven full-TU mode-loop/source-storage controls; no fuzzing or mutation."""
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

def replacement(variant):
    direct = variant.startswith('pointer')
    masked = variant.endswith('masked') and not variant.endswith('unmasked')
    split = 'two-loops' in variant
    value = '*sr' if direct else 'reg'
    shift = '((int)nbits & 31)' if masked else 'nbits'
    tail = '((0x1f - (int)nbits) & 31)' if masked else '(0x1f - (int)nbits)'
    lines = ['int', 'V34scrambler(unsigned *sr, short mode, short bits, short nbits)', '{',
             '\tint mask = (short)((int)(1u << '+shift+') - 1);', '\tshort i;']
    if not direct: lines += ['\tunsigned reg = *sr;']
    def loop(tap=None, indent='\t'):
        r = [indent+'for (i = 0; i < nbits; i = (short)(i + 1)) {',
             indent+'\tshort parity = 0;', indent+'\tif (bits & 1)', indent+'\t\tparity++;',
             indent+'\tbits = (short)(bits >> 1);']
        if tap:
            r += [indent+'\tif ('+value+' & '+tap+')', indent+'\t\tparity++;']
        else:
            r += [indent+'\tif (mode == 0) {', indent+'\t\tif ('+value+' & 0x04000000u)', indent+'\t\t\tparity++;',
                  indent+'\t} else {', indent+'\t\tif ('+value+' & 0x00002000u)', indent+'\t\t\tparity++;', indent+'\t}']
        r += [indent+'\tif ('+value+' & 0x00000100u)', indent+'\t\tparity++;',
              indent+'\tif (parity & 1)', indent+'\t\t'+value+' |= 0x80000000u;',
              indent+'\t'+value+' >>= 1;', indent+'}']
        return r
    if split:
        lines += ['\tif (mode == 0) {']+loop('0x04000000u','\t\t')+['\t} else {']+loop('0x00002000u','\t\t')+['\t}']
    else: lines += loop()
    if not direct: lines += ['\tif (nbits > 0)', '\t\t*sr = reg;']
    lines += ['\treturn (short)(('+value+' >> '+tail+') & (unsigned)mask);','}']
    text = '\n'.join(lines)
    if 'inverse' in variant:
        text = text.replace('if (mode == 0)', 'if (mode != 0)').replace('0x04000000u', 'TAP_PLACEHOLDER').replace('0x00002000u', '0x04000000u').replace('TAP_PLACEHOLDER', '0x00002000u')
    return text

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--revision',default='1f340221');ap.add_argument('--mode-order',action='store_true');args=ap.parse_args()
    out=ROOT/('build/v34-scrambler-mode-order' if args.mode_order else 'build/v34-scrambler-factoring');out.mkdir(parents=True,exist_ok=True)
    source=subprocess.check_output(['git','show',args.revision+':src/pump/v34/V34hshak.c'],cwd=ROOT,text=True)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    prior=(ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o').read_bytes();blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    results={'domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5934181387','revision':args.revision,'config':config,'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    variants = ('baseline','pointer-inside-unmasked','pointer-inverse-unmasked') if args.mode_order else ('baseline','cached-inside-masked','cached-inside-unmasked','pointer-inside-masked','pointer-inside-unmasked','pointer-two-loops-unmasked','cached-two-loops-unmasked')
    if args.mode_order:
        results['domain'] = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5934260776'
    for variant in variants:
        cell=out/variant;cell.mkdir(parents=True,exist_ok=True);text=source
        if variant!='baseline':
            start=text.index('int\nV34scrambler(');end=text.index('\n}\n',start)+2
            text=text[:start]+replacement(variant)+text[end:]
        (cell/'V34hshak.c').write_text(text);directory='/work/'+variant
        cmd=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(directory)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+['-da'],directory+'/V34hshak.o',directory+'/V34hshak.c')]
        entry={'command':cmd,'source_hash':hashlib.sha256(text.encode()).hexdigest()};results['cells'][variant]=entry
        with (cell/'compile.log').open('w') as log: entry['compile_exit']=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert entry['compile_exit']==0,variant
        obj=str(cell/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals']={l.split()[-1]:l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
        (cell/'V34scrambler.dis').write_text(subprocess.check_output(['objdump','-dr','--disassemble=V34scrambler',obj],text=True))
        if variant=='baseline':
            assert Path(obj).read_bytes()==prior,'unchanged fullTU drift';entry['baseline_reproduced']=True
        else:
            base=str(out/'baseline/V34hshak.o')
            entry['changed_bodies']=[sym for sym in sorted(set(b.sizes(obj))&set(b.sizes(base))) if b.body(obj,sym)!=b.body(base,sym)]
            entry['added_symbols']=sorted(set(b.sizes(obj))-set(b.sizes(base)));entry['removed_symbols']=sorted(set(b.sizes(base))-set(b.sizes(obj)))
            assert entry['globals']==results['cells']['baseline']['globals']
        print(variant,'scrambler',entry['verdicts']['V34scrambler'],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
