#!/usr/bin/env python3
"""Two-cell composition of observed transmitter owner/source paths."""
import hashlib,json,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));import experiment_toolchain as tc
sys.path.insert(0,str(ROOT/'tools/toolchain'));import byteident as b

def main():
    out=ROOT/'build/v34-owner-combination';out.mkdir(parents=True,exist_ok=True)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    prior=(out/'baseline/V34hshak.o' if (out/'baseline/V34hshak.o').exists() else ROOT/'build/tc_out/src_pump_v34_V34hshak.c.o').read_bytes();blob=str(ROOT/'ref/slmodemd/dsplibs.o')
    results={'revision':'d884e844','domain':'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5935132899','config':config,'cells':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    for variant in ('baseline','owner-dibit-freeze'):
        cell=out/variant;(cell/'include/dsplib').mkdir(parents=True,exist_ok=True)
        if variant=='baseline':
            source=subprocess.check_output(['git','show','d884e844:src/pump/v34/V34hshak.c'],cwd=ROOT,text=True)
            headers={name:subprocess.check_output(['git','show','d884e844:include/dsplib/'+name],cwd=ROOT,text=True) for name in ('v34fsk.h','v34hstx1_arms.h')}
        else:
            snapshot=ROOT/'build/v34-dibit-width/d-int-q-short'
            source=(snapshot/'V34hshak.c').read_text()
            headers={name:(snapshot/'include/dsplib'/name).read_text() for name in ('v34fsk.h','v34hstx1_arms.h')}
            start=source.index('v34FreezeEcho(void *objp)');end=source.index('\n}\n',start)
            fn=source[start:end];anchor='\tstruct v34_object *obj = (struct v34_object *)objp;\n'
            assert fn.count(anchor)==1
            fn=fn.replace(anchor,anchor+'\tstruct v34_transmitter_prefix *tx = &obj->tx;\n',1).replace('obj->tx.flags','tx->flags')
            source=source[:start]+fn+source[end:]
        (cell/'V34hshak.c').write_text(source)
        for name,text in headers.items():(cell/'include/dsplib'/name).write_text(text)
        directory='/work/'+variant
        cmd=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(directory)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,['-I'+directory+'/include']+flags+['-da'],directory+'/V34hshak.o',directory+'/V34hshak.c')]
        entry={'command':cmd,'source_hash':hashlib.sha256(source.encode()).hexdigest(),'header_hashes':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in headers.items()}};results['cells'][variant]=entry
        with (cell/'compile.log').open('w') as log:entry['compile_exit']=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert entry['compile_exit']==0
        obj=str(cell/'V34hshak.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['verdicts']={s:b.verdict(*b.body(blob,s),*b.body(obj,s)) for s in sorted(set(b.sizes(blob))&set(b.sizes(obj)))}
        entry['globals']={l.split()[-1]:l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
        if variant=='baseline':assert Path(obj).read_bytes()==prior;entry['baseline_reproduced']=True
        else:
            base=str(out/'baseline/V34hshak.o');assert entry['globals']==results['cells']['baseline']['globals']
            assert set(b.sizes(base))==set(b.sizes(obj))
            entry['changed_bodies']=[s for s in sorted(b.sizes(base)) if b.body(base,s)!=b.body(obj,s)]
        print(variant,{s:entry['verdicts'][s] for s in ('txmitdibit','v34FreezeEcho')},'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),flush=True)
        (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
