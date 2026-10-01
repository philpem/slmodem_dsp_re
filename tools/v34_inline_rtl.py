#!/usr/bin/env python3
"""Trace four renaming/scheduling controls of a saved inlined V34 snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config', type=Path, required=True)
    ap.add_argument('--blob', type=Path, required=True)
    ap.add_argument('--snapshot', type=Path, required=True)
    ap.add_argument('--mount-root', type=Path, required=True)
    ap.add_argument('--common-pointer', action='store_true', help='Diagnostic only: address the one post-init flag statement through obj')
    args = ap.parse_args()
    config = args.config.read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else
             '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    out = ROOT / ('build/v34-inline-common-pointer' if args.common_pointer else 'build/v34-inline-rtl')
    out.mkdir(parents=True, exist_ok=True)
    source = args.snapshot / 'src/pump/v34/V34hshak.c'
    header = args.snapshot / 'include/dsplib/v34hstx1_arms.h'
    profile = ['--param', 'inline-unit-growth=100000', '--param', 'max-inline-insns-auto=100', '--param', 'max-inline-insns-single=1000000', '--param', 'large-function-insns=10000000', '--param', 'large-function-growth=100000']
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929138077',
              'common_pointer': args.common_pointer, 'config': config, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
              'snapshot': str(args.snapshot), 'cells': {}}
    if args.common_pointer:
        result['domain'] = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929231084'
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for rename in ((True,) if args.common_pointer else (True, False)):
        for schedule in (True, False):
            name = 'rename-%d-sched-%d' % (rename, schedule)
            cell = out / name
            cell.mkdir(exist_ok=True)
            extra = ([] if rename else ['-fno-rename-registers']) + ([] if schedule else ['-fno-schedule-insns2'])
            directory = '/work/' + name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            source_text = source.read_text()
            if args.common_pointer:
                old = 'T3M_I16(&frame, T3M_F3588) =\n\t\t\t(short)(T3M_U16(&frame, T3M_F3588) | 2);'
                new = '*(short *)((unsigned char *)obj + T3M_F3588) =\n\t\t\t(short)(*(unsigned short *)((unsigned char *)obj + T3M_F3588) | 2);'
                assert source_text.count(old) == 1
                source_text = source_text.replace(old, new)
            (cell / 'V34hshak.c').write_text(source_text)
            shutil.copyfile(header, cell / 'include/dsplib/v34hstx1_arms.h')
            cmd = tc.docker_prefix(image, args.mount_root, out, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH,
                ['-I' + directory + '/include'] + flags + profile + extra + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
            entry = {'command': cmd, 'source_sha256': hashlib.sha256((cell / 'V34hshak.c').read_bytes()).hexdigest()}
            result['cells'][name] = entry
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            assert entry['compile_exit'] == 0, name
            obj = str(cell / 'V34hshak.o')
            entry['object_sha256'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals'] = {line.split()[-1]: line.split()[-2] for line in
                subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
            entry['verdicts'] = {}
            for symbol in sorted(set(b.sizes(obj)) & set(b.sizes(str(args.blob)))):
                a, ar = b.body(str(args.blob), symbol)
                c, cr = b.body(obj, symbol)
                entry['verdicts'][symbol] = b.verdict(a, ar, c, cr)
            for symbol in ('dftRetrainDetInit', 'v34handshak'):
                (cell / (symbol + '.dis')).write_text(subprocess.check_output(
                    ['objdump', '-dr', '--disassemble=' + symbol, obj], text=True))
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    globals0 = result['cells']['rename-1-sched-1']['globals']
    assert all(c['globals'] == globals0 for c in result['cells'].values())
    baseline = args.snapshot / 'V34hshak.o'
    if baseline.exists() and not args.common_pointer:
        assert baseline.read_bytes() == (out / 'rename-1-sched-1/V34hshak.o').read_bytes()
        result['prior_baseline_byte_identical'] = True
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
