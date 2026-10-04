#!/usr/bin/env python3
"""Discover DiffCoder consumers with actual GCC dependencies, then replay both cells."""
import argparse
import json
import shlex
import subprocess
from pathlib import Path
import gcc3_value_carriers_decoder as decoder
import playbook_small_patterns as d

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--discover', action='store_true')
    ap.add_argument('--domain', required=True)
    ap.add_argument('--baseline-dir', type=Path, default=d.ROOT/'build/production-before')
    ap.add_argument('--historical-headers', action='store_true',
                    help='forward historical header snapshots to the compilation replayer')
    args = ap.parse_args()
    assert Path(args.domain).is_file()
    out = d.ROOT/'build/gcc3-value-carriers-consumers'
    out.mkdir(exist_ok=True)
    if args.discover:
        assert not subprocess.check_output(
            ['git', 'diff', '--name-only', decoder.REV, '--', 'src', 'include',
             'tools/toolchain/period_compat.h'], cwd=d.ROOT, text=True).strip(), 'dependency discovery requires the baseline source/header tree'
        config = (args.baseline_dir/'.build-config').read_text()
        image = config.splitlines()[0].split(' ', 1)[1]
        flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
        flags = ['-I/src/include' if flag == '-Iinclude' else '/src/'+flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
        cxx = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('cxx   ')))
        tracked = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', decoder.REV, '--', 'src'], cwd=d.ROOT, text=True).splitlines()
        sources = [p for p in tracked if p.endswith(('.c', '.cpp', '.S'))]
        assert len(sources) == 300, len(sources)
        commands = []
        for source in sources:
            compiler = 'g++' if source.endswith('.cpp') else 'gcc'
            cell_flags = d.tc.add_reproduce_bugs(flags + (cxx if compiler == 'g++' else []))
            commands.append(shlex.join([compiler, '-MM', '-MT', source]+cell_flags+['/src/'+source]))
        script = 'set -e\nexport PATH='+shlex.quote(d.tc.GENTOO_COMPILER_PATH)+':$PATH\n'+'\n'.join(commands)+'\n'
        (out/'dependencies.sh').write_text(script)
        command = d.tc.docker_prefix(image, d.ROOT, out, True)+['/bin/sh', '/work/dependencies.sh']
        with (out/'dependencies.log').open('w') as log, (out/'dependencies.err').open('w') as err:
            subprocess.run(command, stdout=log, stderr=err, check=True)
        lines = (out/'dependencies.log').read_text().replace('\\\n', ' ').splitlines()
        deps = {}
        for line in lines:
            source, names = line.split(':', 1)
            deps[source] = names.split()
        assert set(deps) == set(sources)
        consumers = sorted(source for source, names in deps.items() if '/src/include/dsplib/DiffCoder.h' in names)
        assert consumers, 'header detector did not fire'
        report = {'revision': decoder.REV, 'sources': len(sources), 'consumers': consumers,
                  'config': config, 'commands': commands, 'dependencies': deps}
        (out/'dependencies.json').write_text(json.dumps(report, indent=2)+'\n')
        print('dependency denominator:', len(sources), 'sources;', len(consumers), 'DiffCoder consumers', flush=True)
        return
    report = json.loads((out/'dependencies.json').read_text())
    assert report['revision'] == decoder.REV
    d.REV = decoder.REV
    d.SOURCE_PATHS = tuple(report['consumers'])
    d.OUT_NAME = out.name
    d.DUMP_FLAGS = ()
    d.variants = decoder.variants
    d.HEADER_OVERLAYS = decoder.overlays
    d.main()

if __name__ == '__main__':
    main()
