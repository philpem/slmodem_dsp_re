#!/usr/bin/env python3
"""Replay the local-buffer/relocation-supported V32 ownership recovery."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import playbook_small_patterns as driver

REV = '2feb98d8'
FILES = ('V32.c', 'V32int.c', 'V32mod.c', 'v32fptab.c')


def variants(sources):
    candidate = dict(sources)
    first = sources['V32int.c'].index('/*\n * V32FP_modem --')
    function = sources['V32int.c'][first:]
    assert function.rstrip().endswith('return modem->status_word;\n}')
    buffers = ('static unsigned short tx_in_internal[V32_BIT_BUFFER];\n'
               'static unsigned short rx_out_internal[V32_BIT_BUFFER];\n')
    assert sources['V32int.c'].count(buffers) == 1
    candidate['V32int.c'] = sources['V32int.c'][:first].replace(buffers, '')
    include = '#include "v32fpdisp-common.h"\n'
    candidate['V32mod.c'] = sources['V32mod.c'].replace(include, include + '\n' + buffers) + '\n' + function
    owner_only = dict(candidate)
    candidate['V32mod.c'] = candidate['V32mod.c'].replace(include, include + '\nint Control_Flag = 0;\n')
    candidate['v32fptab.c'] = candidate['v32fptab.c'].replace('int Control_Flag = 0;\n', '')
    control_owner = dict(candidate)
    smc = 'struct v32_smc_cfg SMCv32_CFG = { 0 };\n'
    assert sources['v32fptab.c'].count(smc) == 1
    marker = '#include "dsplib/sysdep.h"\n'
    candidate['V32.c'] = candidate['V32.c'].replace(marker, marker + '\n' + smc)
    candidate['v32fptab.c'] = candidate['v32fptab.c'].replace(smc, '')
    return {'baseline': sources, 'buffer-function-owner': owner_only,
            'control-owner': control_owner, 'consumer-storage-owner': candidate}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-dir', type=Path, required=True)
    ap.add_argument('--partial-baseline', type=Path)
    args = ap.parse_args()
    out = driver.ROOT / 'build/recovery-v32-storage-owner'
    out.mkdir(exist_ok=True)
    config = (args.baseline_dir / '.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/' + f if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    flags += ['-I/src/src/pump/v32', '-v', '-save-temps']
    paths = ['src/pump/v32/' + name for name in FILES]
    sources = {name: subprocess.check_output(['git', 'show', REV + ':' + path], cwd=driver.ROOT, text=True)
               for name, path in zip(FILES, paths)}
    headers = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', REV, '--', 'include', 'src/pump/v32'], cwd=driver.ROOT, text=True).splitlines()
    headers = [p for p in headers if p.endswith('.h')] + ['tools/toolchain/period_compat.h']
    assert not subprocess.check_output(['git', 'diff', '--name-only', REV, '--'] + headers, cwd=driver.ROOT, text=True).strip()
    report = {'revision': REV, 'config': config, 'headers': {p: hashlib.sha256((driver.ROOT/p).read_bytes()).hexdigest() for p in headers}, 'cells': {}}
    driver.tc.print_identity(image, driver.tc.GENTOO_COMPILER_PATH, True)
    for label, cell in variants(sources).items():
        folder = out / label
        folder.mkdir(exist_ok=True)
        results = report['cells'][label] = {}
        for name, path in zip(FILES, paths):
            source = folder / name
            source.write_text(cell[name])
            obj = folder / (name + '.o')
            shell = driver.tc.compile_shell(driver.tc.GENTOO_COMPILER_PATH, flags,
                                             '/work/' + label + '/' + obj.name,
                                             '/work/' + label + '/' + name)
            command = driver.tc.docker_prefix(image, driver.ROOT, out, True) + ['/bin/sh', '-c', shell]
            with (folder / (name + '.log')).open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
            if label == 'baseline':
                retained = args.baseline_dir / (path.replace('/', '_') + '.o')
                assert obj.read_bytes() == retained.read_bytes(), name
            results[name] = {'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                             'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'command': command}
        if args.partial_baseline:
            # The canonical partial link uses recovered TU order, not the
            # compilation manifest's lexical source order.
            manifest_path = args.baseline_dir / 'tc_link_manifest.txt'
            manifest = [line.split() for line in manifest_path.read_text().splitlines() if line.strip()]
            replacements = {path: '/work/' + label + '/' + name + '.o' for name, path in zip(FILES, paths)}
            inputs = [replacements.get(path, '/frozen/' + obj) for obj, path in manifest]
            prefix = driver.tc.docker_prefix(image, driver.ROOT, out, True)
            prefix[-1:-1] = ['-v', str(args.baseline_dir.resolve()) + ':/frozen:ro']
            command = prefix + ['/bin/sh', '-c', 'exec ld -r -o /work/' + label + '/partial.o ' + shlex.join(inputs)]
            subprocess.run(command, check=True)
            if label == 'baseline':
                assert (folder / 'partial.o').read_bytes() == args.partial_baseline.read_bytes(), 'partial raw baseline drift'
            results['partial'] = {'command': command, 'manifest_objects': len(manifest),
                                   'manifest_sha256': hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
                                   'sha256': hashlib.sha256((folder/'partial.o').read_bytes()).hexdigest()}
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(label, '4 complete TUs compiled; baseline raw equality enforced', flush=True)


if __name__ == '__main__':
    main()
