#!/usr/bin/env python3
"""Crossed filter coefficient clamp/equality-carrier controls on complete TUs."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True)
    parser.add_argument('--count-carrier', action='store_true', help='Run predeclared unsigned tap-count cache extension')
    parser.add_argument('--nested', action='store_true', help='Run single-return nested geometry domain')
    args = parser.parse_args()
    assert not (args.nested and args.count_carrier)
    revision = '555036b4'
    out = ROOT / ('build/playbook-filter-nested' if args.nested else 'build/playbook-filter-count-carrier' if args.count_carrier else 'build/playbook-filter-coefficients')
    out.mkdir(parents=True, exist_ok=True)
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags += shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('cxx   ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    headers = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'include').rglob('*') if p.is_file()}
    headers['tools/toolchain/period_compat.h'] = hashlib.sha256((ROOT/'tools/toolchain/period_compat.h').read_bytes()).hexdigest()
    results = {'revision':revision,'domain':args.domain,'config':config,'headers':headers,'cells':{}}
    # Capture original controls before any replay overwrites their objects.
    retained_controls = {}
    for cls in ('FloatFIR', 'FloatIIR'):
        saved = out/cls/'baseline'/(cls+'.o')
        control = saved if saved.exists() else ROOT/'build/tc_out'/('src_dsp_'+cls+'.cpp.o')
        retained_controls[cls] = (control, control.read_bytes())
    results['retained_controls'] = {cls: {'path': str(path), 'hash': hashlib.sha256(data).hexdigest()} for cls, (path, data) in retained_controls.items()}
    blob = str(ROOT/'ref/slmodemd/dsplibs.o')
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for cls, taps, coeff, index, length, arg in [('FloatFIR','taps','coefficients','index','bufferLength','coef'),('FloatIIR','m_ncoeff','m_coeff','m_pos','m_len','coeff')]:
        source = subprocess.check_output(['git','show',revision+':src/dsp/'+cls+'.cpp'],cwd=ROOT,text=True)
        start = source.index('int\n'+cls+'::setCoefficients(')
        end = source.index('\n}\n',start)+2
        fn = source[start:end]
        generated={}
        want = 'want' if cls=='FloatFIR' else 'n'
        domain = [('baseline',False,False),('count-cached',False,True),('count-minimum',True,True)] if args.count_carrier else [('baseline',False,False),('minimum',True,False),('cached',False,True),('both',True,True)]
        if args.nested:
            domain=[('baseline',False,False),('nested-conditional',False,False),('nested-minimum',True,False)]
        for name, ternary, cached in domain:
            text=fn
            if cached:
                old=coeff+' = '+arg+';\n\tif ('+taps+' == '+want+')'
                assert text.count(old)==1
                carrier = ('unsigned previousTaps = '+taps+';\n\t'+coeff+' = '+arg+';\n\tif (previousTaps == '+want+')') if args.count_carrier else ('bool sameGeometry = ('+taps+' == '+want+');\n\t'+coeff+' = '+arg+';\n\tif (sameGeometry)')
                text=text.replace(old,carrier)
            if ternary:
                old=('if (index > room)\n\t\tindex = room;' if cls=='FloatFIR' else 'if (m_pos > (int)(m_len - n))\n\t\tm_pos = (int)(m_len - n);')
                assert text.count(old)==1
                new=('index = index > room ? room : index;' if cls=='FloatFIR' else 'm_pos = m_pos > (int)(m_len - n) ? (int)(m_len - n) : m_pos;')
                text=text.replace(old,new)
            if args.nested and name != 'baseline':
                # Rewrite only the geometry arm; preserve all preceding behavior.
                branch_start=text.index('\tif ('+taps+' == '+want+')')
                arm_start=text.index('\t'+taps+' = '+want+';',branch_start)
                ret=text.rindex('\treturn 0;')
                arm=text[arm_start:ret]
                # FIR has a blank separator before its return; IIR does not.
                arm=''.join('\t'+line if line.strip() else line for line in arm.splitlines(keepends=True))
                text=text[:branch_start]+'\tif ('+taps+' != '+want+') {\n'+arm+'\t}\n'+text[ret:]
            generated[name]=source[:start]+text+source[end:]
        assert len(set(generated.values()))==len(domain)
        retained = retained_controls[cls][1]
        for name,text in generated.items():
            key=cls+'/'+name
            directory=out/cls/name
            directory.mkdir(parents=True,exist_ok=True)
            (directory/(cls+'.cpp')).write_text(text)
            target='/work/'+key
            command=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(target)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags+['-da'],target+'/'+cls+'.o',target+'/'+cls+'.cpp')]
            entry={'command':command,'source_hash':hashlib.sha256(text.encode()).hexdigest()}
            results['cells'][key]=entry
            assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in headers.items())
            with (directory/'compile.log').open('w') as log:
                entry['compile_exit']=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
            assert entry['compile_exit']==0,key
            obj=str(directory/(cls+'.o'))
            entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals']={l.split()[-1]:l.split()[-2] for l in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
            entry['function_symbols']=sorted(b.sizes(obj))
            entry['verdicts']={sym:b.verdict(*b.body(blob,sym),*b.body(obj,sym)) for sym in sorted(set(b.sizes(obj))&set(b.sizes(blob)))}
            sym='_ZN8'+cls+'15setCoefficientsEPfj'
            dis = subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),obj,sym],text=True)
            (directory/'setCoefficients.dis').write_text(dis)
            rtl = (directory/(cls+'.cpp.01.rtl')).read_text().split(';; Function int '+cls+'::setCoefficients')[1].split(';; Function')[0]
            (directory/'setCoefficients-initial.rtl').write_text(rtl)
            entry['sete_instructions'] = len(re.findall(r'\bsete\b', dis))
            entry['zero_definitions'] = len(re.findall(r'xor\s+(%e[a-z]{2}),\1', dis))
            if name in ('cached', 'both'):
                assert entry['sete_instructions'] == 1, 'bool generator failed known SETE control'
            else:
                assert entry['sete_instructions'] == 0, 'unexpected boolean materialization'
            if name=='baseline':
                assert Path(obj).read_bytes()==retained,'raw baseline drift '+cls
                entry['baseline_reproduced']=True
            else:
                base=results['cells'][cls+'/baseline']
                assert entry['globals']==base['globals'] and entry['function_symbols']==base['function_symbols']
                baseobj=str(out/cls/'baseline'/(cls+'.o'))
                entry['changed_bodies']=[s for s in entry['function_symbols'] if b.body(obj,s)!=b.body(baseobj,s)]
                entry['exact_gains']=[s for s,v in entry['verdicts'].items() if v[0]=='EXACT' and base['verdicts'][s][0]!='EXACT']
                entry['exact_losses']=[s for s,v in base['verdicts'].items() if v[0]=='EXACT' and entry['verdicts'][s][0]!='EXACT']
            print(key,entry['verdicts'][sym], 'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),flush=True)
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')

if __name__ == '__main__':
    main()
