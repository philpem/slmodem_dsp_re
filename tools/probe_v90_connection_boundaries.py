#!/usr/bin/env python3
"""Run explicit, passing period ConnectionEvaluator component boundaries.

First run make period T=t_v90conneval. Then:
  python3 tools/probe_v90_connection_boundaries.py --period-dir build/period

Two constructor/public-method sequences each test 0.5, 1, 2 and an external
quiet NaN: 24 checks. These are outside default unit-test discovery, and
establish no modem-path reachability. The known modern comparison behavior
is not suppressed or tested here. Compiler/link/run failures remain nonzero.
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--period-dir', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=ROOT / 'build/dsplibs_ref.o')
    parser.add_argument('--out-dir', type=Path, default=ROOT / 'build/v90-connection-boundaries-probe')
    parser.add_argument('--image', default='ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest')
    args = parser.parse_args()
    period, out = args.period_dir.resolve(), args.out_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    script = (ROOT / 'tools/toolchain/period_inner.sh').read_text()
    flags = shlex.split(re.search(r'^FLAGS="([^"]+)"', script, re.M).group(1))
    flags += shlex.split(re.search(r'^CXXFLAGS="\$FLAGS ([^"]+)"', script, re.M).group(1))
    flags = ['-I/src/include' if flag == '-Iinclude' else
             '-I/src/test/harness' if flag == '-Itest/harness' else
             '/src/tools/toolchain/period_compat.h' if flag == 'tools/toolchain/period_compat.h'
             else flag for flag in flags]
    selected = []
    for line in (period / 'objs.list').read_text().splitlines():
        name = Path(line).name
        path = period / 'testhost' / name
        if not path.exists():
            path = period / name
        assert path.is_file(), path
        selected.append(path)
    assert len(selected) > 300, 'require complete source + harness period object list'
    (out / 'dsplibs_ref.o').write_bytes(args.reference.read_bytes())
    prefix = tc.docker_prefix(args.image, ROOT, out, True, output=period)
    tc.print_identity(args.image, tc.GENTOO_COMPILER_PATH, True)
    identity_command = prefix + ['/bin/sh', '-c',
                                 'export PATH=' + shlex.quote(tc.GENTOO_COMPILER_PATH)
                                 + ':$PATH; command -v g++; exec g++ --version']
    with (out / 'cxx-identity.log').open('w') as log:
        subprocess.run(identity_command, stdout=log, stderr=subprocess.STDOUT, check=True)
    print((out / 'cxx-identity.log').read_text(), end='')
    shell = tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags, '/work/probe.o',
                             '/src/test/explore/t_v90_connection_boundaries.cpp')
    shell = shell.replace('exec gcc -c ', 'exec g++ -c ', 1)
    compile_command = prefix + ['/bin/sh', '-c', shell]
    objects = ['/out/' + str(path.relative_to(period)) for path in selected]
    link_shell = ('export PATH=' + shlex.quote(tc.GENTOO_COMPILER_PATH) + ':$PATH; exec gcc '
                  '-static -Wl,-N -o /work/probe /work/probe.o ' + shlex.join(objects)
                  + ' /work/dsplibs_ref.o -lm')
    link_command = prefix + ['/bin/sh', '-c', link_shell]
    run_command = prefix + ['/work/probe']
    report = {'component_boundary': 'constructor + public updater calls + external finite/quiet-NaN floats; no modem reachability claim',
              'compile_command': compile_command, 'link_command': link_command,
              'run_command': run_command, 'cxx_identity_command': identity_command,
              'fixture_flags': tc.add_reproduce_bugs(flags),
              'period_objects': {str(path): digest(path) for path in selected},
              'reference_sha256': digest(args.reference),
              'probe_sha256': digest(ROOT / 'test/explore/t_v90_connection_boundaries.cpp'),
              'related_default_fixture_sha256': digest(ROOT / 'test/unit/t_v90conneval.cpp'),
              'reconstruction_source_sha256': digest(ROOT / 'src/pump/v90/V90ConnectionEvaluator.cpp'),
              'period_inner_sha256': digest(ROOT / 'tools/toolchain/period_inner.sh'),
              'image': args.image}
    for label, command in [('compile', compile_command), ('link', link_command), ('run', run_command)]:
        with (out / (label + '.log')).open('w') as log:
            code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        report[label + '_exit'] = code
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        if code:
            print((out / (label + '.log')).read_text(), end='')
            print('probe: %s failed; exit %d preserved; see %s' % (label, code, out))
            return code
    print((out / 'run.log').read_text(), end='')
    print('probe: all component checks passed; see', out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
