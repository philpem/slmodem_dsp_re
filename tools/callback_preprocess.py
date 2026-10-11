#!/usr/bin/env python3
"""Preprocess every retained C/C++ TU with its production Gentoo command."""
import hashlib
import json
import shlex
import subprocess
import playbook_small_patterns as d


def main():
    root = d.ROOT
    out = root / 'build/callback-preprocessed'
    out.mkdir(exist_ok=True)
    subprocess.run(['git', 'diff', '--exit-code', '80da7b0c', '--', 'src', 'include',
                    'tools/toolchain/period.mk', 'tools/toolchain/period_compat.h'], cwd=root, check=True)
    config = (root / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    cxx = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('cxx   ')))
    dcr = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('dcr   ')))
    headers = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in (root / 'include').rglob('*') if p.is_file()}
    headers.update({str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in (root / 'src').rglob('*') if p.is_file() and p.suffix in ('.h', '.inc')})
    headers['tools/toolchain/period_compat.h'] = hashlib.sha256((root / 'tools/toolchain/period_compat.h').read_bytes()).hexdigest()
    sources = sorted(p for p in (root / 'src').rglob('*') if p.suffix in ('.c', '.cpp'))
    report = {'source_baseline': '80da7b0c', 'config': config, 'commands': {},
              'source_hashes': {}, 'preprocessed_hashes': {}, 'header_hashes': headers}
    script = ['#!/bin/sh', 'set -e']
    for source in sources:
        relative = str(source.relative_to(root))
        destination = relative.replace('/', '_') + '.i'
        extra = cxx if source.suffix == '.cpp' else dcr if relative == 'src/service/dcr.c' else []
        shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, flags + extra,
                                   '/work/' + destination, '/src/' + relative)
        shell = shell.replace('exec gcc -c ', 'gcc -E ', 1)
        if source.suffix == '.cpp':
            shell = shell.replace('gcc -E ', 'g++ -E ', 1)
        report['commands'][relative] = shell
        report['source_hashes'][relative] = hashlib.sha256(source.read_bytes()).hexdigest()
        script.append(shell)
    assert len(sources) == 299
    (out / 'preprocess.sh').write_text('\n'.join(script) + '\n')
    d.tc.print_identity(image, d.tc.GENTOO_COMPILER_PATH, True)
    command = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '/work/preprocess.sh']
    report['docker_command'] = command
    with (out / 'preprocess.log').open('w') as log:
        report['exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
    assert report['exit'] == 0
    for source in sources:
        relative = str(source.relative_to(root))
        assert hashlib.sha256(source.read_bytes()).hexdigest() == report['source_hashes'][relative]
        text = out / (relative.replace('/', '_') + '.i')
        report['preprocessed_hashes'][relative] = hashlib.sha256(text.read_bytes()).hexdigest()
    assert all(hashlib.sha256((root / p).read_bytes()).hexdigest() == h for p, h in headers.items())
    (out / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print(len(sources), 'period TUs preprocessed; no source changed', flush=True)


if __name__ == '__main__':
    main()
