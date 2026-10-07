#!/usr/bin/env python3
"""Unchanged full-TU stock/Gentoo A/B for the historical constructor floor."""
import hashlib,json,shlex,subprocess
from pathlib import Path
import playbook_small_patterns as d
import experiment_toolchain as tc
from gcc3_value_carriers_audit import inspect
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/reload-baseline'
OUT=ROOT/'build/v90-parameters-ratchet-ab'
SOURCE='src/pump/v90/V90Parameters.cpp'
TARGET='_ZN13V90ParametersC2EP19_tagModemParameters'

def main():
    OUT.mkdir(exist_ok=True)
    config=(BASE/'.build-config').read_text();flags=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags=['-I/src/include' if x=='-Iinclude' else '/src/'+x if x=='tools/toolchain/period_compat.h' else x for x in flags]
    flags+=shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
    source=(ROOT/SOURCE).read_bytes();revision='75e7ef4b'
    assert source==subprocess.check_output(['git','show',revision+':'+SOURCE],cwd=ROOT)
    report={'revision':revision,'config':config,'source_sha256':hashlib.sha256(source).hexdigest(),'cells':{}}
    for label,image,path in [('gentoo',config.splitlines()[0].split(' ',1)[1],tc.GENTOO_COMPILER_PATH),('stock','dsplibs-tc342','/opt/gcc342/bin')]:
        cell=OUT/label;cell.mkdir(exist_ok=True);(cell/'V90Parameters.cpp').write_bytes(source)
        dst='/work/'+label
        identity=['docker','run','--rm','--platform','linux/386',image,'/bin/sh','-c','export PATH='+shlex.quote(path)+':$PATH; command -v g++; g++ --version; g++ -dumpmachine; replay_as_path=$(g++ -print-prog-name=as); "$replay_as_path" --version']
        (cell/'identity-command.json').write_text(json.dumps(identity,indent=2)+'\n')
        (cell/'identity.log').write_text(subprocess.check_output(identity,text=True,stderr=subprocess.STDOUT))
        shell=tc.compile_shell(path,flags+['-MMD','-MF',dst+'/dependencies.d'],dst+'/candidate.o',dst+'/V90Parameters.cpp').replace('exec gcc -c ','exec g++ -c ',1)
        command=tc.docker_prefix(image,ROOT,OUT,True)+['/bin/sh','-c','cd '+shlex.quote(dst)+' && '+shell]
        with (cell/'compile.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        obj=cell/'candidate.o'
        if label=='gentoo':assert obj.read_bytes()==(BASE/(SOURCE.replace('/','_')+'.o')).read_bytes(),'raw baseline mismatch'
        deps=(cell/'dependencies.d').read_text().replace('\\\n',' ').split(':',1)[1].split()
        headers={x[5:]:hashlib.sha256((ROOT/x[5:]).read_bytes()).hexdigest() for x in deps if x.startswith('/src/')}
        entry={'command':command,'source_and_header_hashes':headers,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),
               'functions':sorted(d.b.sizes(str(obj))),
               'verdicts':{n:d.b.verdict(*d.b.body(d.b.BLOB,n),*d.b.body(str(obj),n)) for n in d.b.sizes(str(obj)) if n in d.b.sizes(d.b.BLOB)}}
        if label=='gentoo':entry['raw_baseline_reproduced']=True
        else:
            baseline=OUT/'gentoo/candidate.o';entry['changed_bodies']=[n for n in entry['functions'] if d.b.body(str(obj),n)!=d.b.body(str(baseline),n)]
            a=inspect(baseline);b=inspect(obj);entry['metadata_equal']={key:a[key]==b[key] for key in a}
            entry['changed_data']={key:b['allocated'][key] for key in b['allocated'] if a['allocated'].get(key)!=b['allocated'][key]}
        report['cells'][label]=entry
        (OUT/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        print(label,'C2',entry['verdicts'][TARGET],'exact',sum(v[0]=='EXACT' for v in entry['verdicts'].values()),'/',len(entry['verdicts']))
    assert report['cells']['gentoo']['functions']==report['cells']['stock']['functions']
    for label in ('gentoo','stock'):
        for symbol in report['cells'][label]['functions']:
            (OUT/label/(symbol+'.dis')).write_text(subprocess.check_output(['objdump','-dr','--disassemble='+symbol,str(OUT/label/'candidate.o')],text=True))
if __name__=='__main__':main()
