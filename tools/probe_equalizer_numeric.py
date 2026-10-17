#!/usr/bin/env python3
"""Link the constructor-created equalizer probe against one supplied full TU.

First make period T=t_v90equnumeric. Supply fresh complete period objects; no
automatic freshness claim. The wrong-scale calibration must fail, and nonzero
link/run exits stay nonzero. Source and test flags remain those recorded by
period and recovery_equalizer_numeric.py, not an alternative compiler profile.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import experiment_toolchain as tc

ROOT=Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--period-dir',type=Path,required=True)
    ap.add_argument('--equalizer-object',type=Path,required=True)
    ap.add_argument('--out-dir',type=Path,required=True)
    ap.add_argument('--image',default='ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest')
    args=ap.parse_args();period=args.period_dir.resolve();out=args.out_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    selected=[];objects=[];replaced=0
    for line in (period/'objs.list').read_text().splitlines():
        name=Path(line).name;path=period/'testhost'/name
        if not path.exists():path=period/name
        assert path.is_file(),path
        selected.append(path)
        if name=='src_pump_v90_V90Equalizer.o':objects.append('/work/equalizer.o');replaced+=1
        else:objects.append('/out/'+str(path.relative_to(period)))
    assert replaced==1 and len(selected)>300
    (out/'equalizer.o').write_bytes(args.equalizer_object.read_bytes())
    (out/'fixture.o').write_bytes((period/'test_unit_t_v90equnumeric.o').read_bytes())
    reference=ROOT/'build/dsplibs_ref.o';(out/'reference.o').write_bytes(reference.read_bytes())
    prefix=tc.docker_prefix(args.image,ROOT,out,True,output=period)
    tc.print_identity(args.image,tc.GENTOO_COMPILER_PATH,True)
    shell='export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH; exec gcc -static -Wl,-N -o /work/probe /work/fixture.o '+shlex.join(objects)+' /work/reference.o -lm'
    link=prefix+['/bin/sh','-c',shell];run=prefix+['/work/probe']
    report={'scope':'valid constructed equalizer component lifecycle; no negotiated modem claim',
            'link_command':link,'run_command':run,'period_objects':{str(p):sha(p) for p in selected},
            'equalizer_object_sha256':sha(args.equalizer_object),'fixture_object_sha256':sha(out/'fixture.o'),
            'fixture_source_sha256':sha(ROOT/'test/unit/t_v90equnumeric.cpp'),'reference_sha256':sha(reference)}
    for label,command in (('link',link),('run',run)):
        with (out/(label+'.log')).open('w') as log:
            status=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
        report[label+'_exit']=status;(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        if status:
            print((out/(label+'.log')).read_text(),end='');return status
    print((out/'run.log').read_text(),end='');return 0

if __name__=='__main__':raise SystemExit(main())
