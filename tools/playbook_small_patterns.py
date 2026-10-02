#!/usr/bin/env python3
"""Replay three finite small-function playbook domains on complete period TUs."""
import argparse, hashlib, json, shlex, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
# Keep the analysis script dis.py from shadowing stdlib exception reporting.
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT/'tools']
import dis
sys.path[:] = _script_path
sys.path.insert(0, str(ROOT/'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b
REV='555036b4'
SOURCE_PATHS=('src/call/call.c','src/pump/v32/V32int.c','src/service/Beepgen.c')
OUT_NAME='playbook-parent-patterns'

def function(source,name):
    start=source.index('\n'+name+'(')+1
    end=source.index('\n}\n',start)+2
    return start,end,source[start:end]

def variants(path,source):
    cells={'baseline':source}
    if path=='src/call/call.c':
        for label,block in [('owner-local',False),('owner-block',True)]:
            text=source
            for name,field in [('SetPulseBreakTime','pulse_break'),('SetPulseMakeTime','pulse_make')]:
                start,end,fn=function(text,name)
                assert fn.count('c->self->'+field)==1
                if block:
                    fn=fn.replace('\n\tif (DSPLIB_DEBUG_ON())','\n\t{\n\t\tstruct call_dp *st = c->self;\n\n\tif (DSPLIB_DEBUG_ON())',1)
                    fn=fn[:-1]+'\t}\n}'
                else:
                    fn=fn.replace('\tstruct call *c = call_of(modem);','\tstruct call *c = call_of(modem);\n\tstruct call_dp *st;')
                    fn=fn.replace('\n\tif (DSPLIB_DEBUG_ON())','\n\tst = c->self;\n\n\tif (DSPLIB_DEBUG_ON())',1)
                fn=fn.replace('c->self->'+field,'st->'+field)
                text=text[:start]+fn+text[end:]
            cells[label]=text
    elif path=='src/pump/v32/V32int.c':
        for label,early in [('result-early',True),('result-after-loads',False)]:
            text=source
            for name,mask in [('RetrainDetectV32','V32_DEC_RETRAIN_REQ'),('RenegotiateDetectV32','V32_DEC_RENEG_REQ')]:
                start,end,fn=function(text,name)
                marker='\tstruct v32_dec *dec = DEC(FP(modem));'
                if early: fn=fn.replace(marker,'\tint detected = 0;\n'+marker)
                else: fn=fn.replace('\tunsigned short req = (unsigned short)dec->retrain;','\tunsigned short req = (unsigned short)dec->retrain;\n\tint detected = 0;')
                old='\tif (!(req & '+mask+'))\n\t\treturn 0;\n\n'
                assert fn.count(old)==1
                fn=fn.replace(old,'\tif (req & '+mask+') {\n')
                fn=fn.replace('\treturn 1;','\t\tdetected = 1;\n\t}\n\treturn detected;')
                text=text[:start]+fn+text[end:]
            cells[label]=text
    else:
        start,end,fn=function(source,'check_for_valid')
        head=fn[:fn.index('\n\tif (')]
        cond='v == w[1] && v == w[2] && v != w[3] &&\n\t\t  v != w[4] && v != w[5] && v != w[6]'
        forms={'conjunction':head+'\n\tint valid = '+cond+';\n\n\treturn valid;\n}',
               'result-branch':head+'\n\tint valid = 0;\n\n\tif ('+cond+')\n\t\tvalid = 1;\n\treturn valid;\n}'}
        for label,body in forms.items():cells[label]=source[:start]+body+source[end:]
    assert len(cells)==len(set(cells.values()))==3
    return cells

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--domain',required=True);args=ap.parse_args()
    out=ROOT/'build'/OUT_NAME;out.mkdir(exist_ok=True)
    config=(ROOT/'build/tc_out/.build-config').read_text();image=config.splitlines()[0].split(' ',1)[1]
    flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if f=='-Iinclude' else '/src/'+f if f=='tools/toolchain/period_compat.h' else f for f in flags]
    hpaths=subprocess.check_output(['git','ls-tree','-r','--name-only',REV,'--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).splitlines()
    assert not subprocess.check_output(['git','diff','--name-only',REV,'--','include','tools/toolchain/period_compat.h'],cwd=ROOT,text=True).strip()
    local_paths=subprocess.check_output(['git','ls-tree','-r','--name-only',REV,'--','src/pump/v32'],cwd=ROOT,text=True).splitlines()
    hpaths += [p for p in local_paths if p.endswith('.h')]
    headers={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in hpaths}
    result={'revision':REV,'domain':args.domain,'config':config,'headers':headers,'families':{}}
    tc.print_identity(image,tc.GENTOO_COMPILER_PATH,True)
    if any(Path(path).suffix == '.cpp' for path in SOURCE_PATHS):
        identity_shell = ('set -e; export PATH=' + shlex.quote(tc.GENTOO_COMPILER_PATH) +
                          ':$PATH; command -v g++; g++ --version; g++ -dumpmachine; '
                          'replay_as_path=$(g++ -print-prog-name=as); "$replay_as_path" --version')
        identity_command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', identity_shell]
        (out/'cxx-identity-command.json').write_text(json.dumps(identity_command, indent=2) + '\n')
        with (out/'cxx-identity.log').open('w') as log:
            subprocess.run(identity_command, stdout=log, stderr=subprocess.STDOUT, check=True)
    for path in SOURCE_PATHS:
        source=subprocess.check_output(['git','show',REV+':'+path],cwd=ROOT,text=True)
        family=Path(path).stem;fo=out/family;fo.mkdir(exist_ok=True)
        retained=ROOT/'build/tc_out'/ (path.replace('/','_')+'.o')
        saved=fo/'retained.o'
        if not saved.exists():saved.write_bytes(retained.read_bytes())
        cells=variants(path,source);fr={'source_path':path,'retained_hash':hashlib.sha256(saved.read_bytes()).hexdigest(),'cells':{}};result['families'][family]=fr
        for label,text in cells.items():
            cd=fo/label;cd.mkdir(exist_ok=True);file=cd/Path(path).name;file.write_text(text)
            for local in (ROOT/Path(path).parent).glob('*.h'):
                rel=str(local.relative_to(ROOT))
                contents=subprocess.check_output(['git','show',REV+':'+rel],cwd=ROOT)
                assert hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()==headers[rel]
                (cd/local.name).write_bytes(contents)
            dst='/work/'+family+'/'+label
            cell_flags = flags + ['-da']
            if file.suffix == '.cpp':
                cell_flags += shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
            shell = tc.compile_shell(tc.GENTOO_COMPILER_PATH, cell_flags, dst+'/candidate.o', dst+'/'+file.name)
            if file.suffix == '.cpp':
                assert shell.count('exec gcc -c ') == 1
                shell = shell.replace('exec gcc -c ', 'exec g++ -c ', 1)
            command=tc.docker_prefix(image,ROOT,out,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+shell]
            entry={'source_hash':hashlib.sha256(text.encode()).hexdigest(),'command':command};fr['cells'][label]=entry
            assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==d for p,d in headers.items())
            with (cd/'compile.log').open('w') as log:entry['compile_exit']=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
            (out/'results.json').write_text(json.dumps(result,indent=2)+'\n');assert entry['compile_exit']==0,label
            obj=str(cd/'candidate.o');entry['object_hash']=hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals']={x.split()[-1]:x.split()[-2] for x in subprocess.check_output(['nm','-g','--defined-only',obj],text=True).splitlines()}
            entry['functions']=sorted(b.sizes(obj));entry['verdicts']={n:b.verdict(*b.body(b.BLOB,n),*b.body(obj,n)) for n in sorted(set(b.sizes(obj))&set(b.sizes(b.BLOB)))}
            if label=='baseline':assert Path(obj).read_bytes()==saved.read_bytes(),'raw baseline mismatch';entry['baseline_reproduced']=True
            else:
                base=fr['cells']['baseline'];baseobj=str(fo/'baseline/candidate.o')
                assert entry['globals']==base['globals'] and entry['functions']==base['functions']
                entry['changed_bodies']=[n for n in entry['functions'] if b.body(obj,n)!=b.body(baseobj,n)]
                entry['gains']=[n for n,v in entry['verdicts'].items() if v[0]=='EXACT' and base['verdicts'][n][0]!='EXACT']
                entry['losses']=[n for n,v in base['verdicts'].items() if v[0]=='EXACT' and entry['verdicts'][n][0]!='EXACT']
                for n in entry['changed_bodies']:
                    (cd/(n+'.dis')).write_text(subprocess.check_output(['python3',str(ROOT/'tools/dis.py'),obj,n],text=True,stderr=subprocess.DEVNULL))
            print(family,label,'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']),'gains',entry.get('gains',[]),'losses',entry.get('losses',[]),flush=True)
            (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
