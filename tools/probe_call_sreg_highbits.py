#!/usr/bin/env python3
"""Run the explicitly synthetic high-bit host S-register return probe.

First make period T=t_call. No automatic freshness claim: supply complete fresh
period objects. Compile/link/run failures remain nonzero, including red controls.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import experiment_toolchain as tc

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--period-dir', type=Path, required=True)
    ap.add_argument('--out-dir', type=Path, default=ROOT/'build/call-sreg-highbits-probe')
    ap.add_argument('--image', default='ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest')
    args = ap.parse_args()
    period, out = args.period_dir.resolve(), args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    inner = (ROOT/'tools/toolchain/period_inner.sh').read_text()
    flags = shlex.split(re.search(r'^FLAGS="([^"]+)"', inner, re.M).group(1))
    flags = ['-I/src/include' if f=='-Iinclude' else '-I/src/test/harness' if f=='-Itest/harness' else
             '/src/tools/toolchain/period_compat.h' if f=='tools/toolchain/period_compat.h' else f for f in flags]
    objects = []
    for line in (period/'objs.list').read_text().splitlines():
        path = period/'testhost'/Path(line).name
        if not path.exists():
            path = period/Path(line).name
        assert path.is_file(), path
        objects.append(path)
    assert len(objects)>300
    reference = ROOT/'build/dsplibs_ref.o'
    (out/'dsplibs_ref.o').write_bytes(reference.read_bytes())
    prefix = tc.docker_prefix(args.image, ROOT, out, True, output=period)
    tc.print_identity(args.image, tc.GENTOO_COMPILER_PATH, True)
    fixture = ROOT/'test/explore/t_call_sreg_highbits.c'
    compile_command = prefix + ['/bin/sh','-c',tc.compile_shell(tc.GENTOO_COMPILER_PATH,flags,'/work/probe.o','/src/test/explore/t_call_sreg_highbits.c')]
    linked = ['/out/'+str(path.relative_to(period)) for path in objects]
    link_command = prefix + ['/bin/sh','-c','export PATH='+shlex.quote(tc.GENTOO_COMPILER_PATH)+':$PATH; exec gcc -static -Wl,-N -o /work/probe /work/probe.o '+shlex.join(linked)+' /work/dsplibs_ref.o -lm']
    commands = {'compile':compile_command,'link':link_command,'run':prefix+['/work/probe']}
    report = {'boundary':'synthetic host callback returns high-bit S16/S56; constructor/public process; no modem host reachability evidence',
              'fixture_sha256':digest(fixture),'included_fixture_sha256':digest(ROOT/'test/unit/t_call.c'),
              'source_sha256':digest(ROOT/'src/call/call.c'),'header_sha256':digest(ROOT/'include/dsplib/call.h'),
              'reference_sha256':digest(reference),'period_objects':{str(p):digest(p) for p in objects},
              'flags':tc.add_reproduce_bugs(flags),'commands':commands}
    for label,command in commands.items():
        with (out/(label+'.log')).open('w') as log:
            code = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
        report[label+'_exit']=code
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        if code:
            print((out/(label+'.log')).read_text(),end='')
            return code
    print((out/'run.log').read_text(),end='')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
