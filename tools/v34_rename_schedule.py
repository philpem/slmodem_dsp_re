#!/usr/bin/env python3
"""Four-cell period renaming/scheduling mechanism control; no harnesses."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
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
    args = ap.parse_args()
    config = args.config.read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else
             '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    out = ROOT / 'build/v34-rename-schedule'
    out.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'src/pump/v34/V34hshak.c'
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5928882513',
              'config': config, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for rename in (True, False):
        for schedule in (True, False):
            name = 'rename-%d-sched-%d' % (rename, schedule)
            cell = out / name
            cell.mkdir(exist_ok=True)
            extra = ([] if rename else ['-fno-rename-registers']) + ([] if schedule else ['-fno-schedule-insns2'])
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH,
                flags + extra + ['-da'], directory + '/V34hshak.o', '/src/src/pump/v34/V34hshak.c')]
            entry = {'command': cmd}
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
            print(name, entry['verdicts']['dftRetrainDetInit'], flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    globals0 = result['cells']['rename-1-sched-1']['globals']
    assert all(c['globals'] == globals0 for c in result['cells'].values())
    baseline = ROOT / 'build/v34-polyvalue-rtl/full-retained-on/V34hshak.o'
    if baseline.exists():
        assert baseline.read_bytes() == (out / 'rename-1-sched-1/V34hshak.o').read_bytes()
        result['prior_baseline_byte_identical'] = True
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
