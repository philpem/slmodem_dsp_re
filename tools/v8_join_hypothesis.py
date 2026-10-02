#!/usr/bin/env python3
"""Cross a single V8 source CFG hypothesis across the retained/no-reorder flag cells.

Reuses tools/v8_call_layout.py's census and byteident verdict logic, but reads a
candidate V8.c from the working tree (or a --source-path override) instead of a
fixed git revision, so a hypothesized acceptance join can be measured against the
retained baseline cell in build/v8-call-layout/baseline.
"""
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

TARGET = 'rebuildJMSequence'
CALLEE = 'charFlip'

sys.path.insert(0, str(ROOT / 'tools'))
import v8_call_layout as layout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source-path', required=True)
    ap.add_argument('--domain', required=True)
    ap.add_argument('--label', required=True)
    ap.add_argument('--out', default=None)
    args = ap.parse_args()

    source = Path(args.source_path).read_text()
    out = Path(args.out) if args.out else (ROOT / 'build/v8-call-layout' / args.label)
    out.mkdir(parents=True, exist_ok=True)

    retained = ROOT / 'build/tc_out/src_v8_V8.c.o'
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')

    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(row[6:] for row in config.splitlines() if row.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    flags += ['-I/src/src/v8']

    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    results = {'domain': args.domain, 'label': args.label, 'config': config,
               'source_hash': hashlib.sha256(source.encode()).hexdigest(), 'cells': {}}
    for name, options in [('retained', []), ('no-reorder', ['-fno-reorder-blocks'])]:
        directory = out / name
        directory.mkdir(exist_ok=True)
        (directory / 'V8.c').write_text(source)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                  'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + options + ['-da'], target + '/V8.o', target + '/V8.c')]
        with (directory / 'compile.log').open('w') as log:
            rc = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        assert rc == 0
        obj = directory / 'V8.o'
        cell = {
            'command': command,
            'compile_exit': rc,
            'object_hash': hashlib.sha256(obj.read_bytes()).hexdigest(),
            'verdicts': {symbol: b.verdict(*b.body(blob, symbol), *b.body(str(obj), symbol)) for symbol in sorted(set(b.sizes(str(obj))) & set(b.sizes(blob)))},
            'function_symbols': sorted(b.sizes(str(obj))),
            'stages': layout.census(directory),
        }
        # globals + sizes baseline for binding comparison
        cell['globals'] = {row.split()[-1]: row.split()[-2] for row in subprocess.check_output(['nm', '-g', '--defined-only', str(obj)], text=True).splitlines()}
        cell['final_calls'] = sum(t[0] == 'symbol' and t[1] == 'charFlip' for kind, t in b.body(str(obj), TARGET)[1].values())
        results['cells'][name] = cell
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(name, cell['verdicts'][TARGET], 'calls', cell['final_calls'],
              'exact', sum(v[0] == 'EXACT' for v in cell['verdicts'].values()), '/', len(cell['verdicts']), flush=True)

    # Compare retained cell against the canonical retained baseline cell.
    base = ROOT / 'build/v8-call-layout/baseline/V8.o'
    base_verdicts = {s: b.verdict(*b.body(blob, s), *b.body(str(base), s)) for s in sorted(set(b.sizes(str(base))) & set(b.sizes(blob)))}
    n = results['cells']['retained']
    print('--- retained vs baseline ---')
    print('same globals:', n['globals'] == {row.split()[-1]: row.split()[-2] for row in subprocess.check_output(['nm', '-g', '--defined-only', str(base)], text=True).splitlines()})
    print('same function_symbols:', n['function_symbols'] == sorted(b.sizes(str(base))))
    print('changed_bodies_globals(referencing blob):',
          [s for s in n['function_symbols'] if b.body(str(out / 'retained/V8.o'), s) != b.body(str(base), s)])
    print('exact_gains:', [s for s, v in n['verdicts'].items() if v[0] == 'EXACT' and (s not in base_verdicts or base_verdicts[s][0] != 'EXACT')])
    print('exact_losses:', [s for s, v in base_verdicts.items() if v[0] == 'EXACT' and s in n['verdicts'] and n['verdicts'][s][0] != 'EXACT'])
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
