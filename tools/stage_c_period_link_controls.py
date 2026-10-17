#!/usr/bin/env python3
"""Reject final-only wrapping and omitted test-visible locals in aggregate fixtures."""
import json
from pathlib import Path
import shlex
import subprocess
import playbook_small_patterns as d

root = d.ROOT / 'build/stage-c-period-link-controls'
root.mkdir(exist_ok=True)
image = (d.ROOT / 'build/stage-c-log10-baseline/.build-config').read_text().splitlines()[0].split(' ', 1)[1]
mounts = ['docker','run','--rm','--platform','linux/386','-v',str(d.ROOT)+':/src',
          '-v',str(d.ROOT/'build/period')+':/out','-v',str(root)+':/audit','-w','/src',image,'/bin/sh','-c']
harness = ' '.join('/out/test_harness_'+n+'.o' for n in ('harness','runtime','fakedp','v34hsstep','unwritten'))
# Config begins mode, objects, REF, flags, apparatus checksum.
config = Path('build/period/t_fpm_log10.link-config').read_text().splitlines()
ref = config[-3]
wrap = '-Wl,--wrap=Vparser_read_int -Wl,--wrap=Vparser_read_float'
steps = {
 'final-only-wrap': 'set -ex; gcc -nostdlib -r -o /audit/no-wrap.o $(cat /out/aggregate-order.list); '
    f'gcc -static -Wl,-N -o /audit/no-wrap.exe /out/test_unit_t_v90loadparams.o /audit/no-wrap.o {harness} {shlex.quote(ref)} {wrap} -lm',
 'unexposed-locals': 'set -ex; gcc -nostdlib -r -o /audit/local.o $(sed "s|/out/testhost/|/out/|" /out/aggregate-order.list); '
    f'gcc -static -Wl,-N -o /audit/local.exe /out/test_unit_t_fpm_log10.o /audit/local.o {harness} {shlex.quote(ref)} -lm',
}
report = {}
identity = mounts + ['command -v gcc; gcc --version; p=$(gcc -print-prog-name=ld); "$p" --version; p=$(gcc -print-prog-name=as); "$p" --version']
with (root/'identity.log').open('w') as log: subprocess.run(identity, stdout=log, stderr=subprocess.STDOUT, check=True)
report['identity_command'] = identity
for label, shell in steps.items():
    command = mounts + [shell]
    with (root/(label+'.link.log')).open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    report[label] = {'command':command,'link_exit':result.returncode}
    if label == 'unexposed-locals':
        assert result.returncode != 0
        assert 'undefined reference to `FPM_log10_table' in (root/(label+'.link.log')).read_text()
    else:
        assert result.returncode == 0
        command = mounts + ['/audit/no-wrap.exe']
        with (root/(label+'.run.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        assert result.returncode != 0
        report[label]['run_command'] = command
        report[label]['run_exit'] = result.returncode
report['negative_control_denominator'] = 2
(root/'results.json').write_text(json.dumps(report, indent=2)+'\n')
print('2/2 unsound link controls rejected: final-only wrappers, unexposed local table')
