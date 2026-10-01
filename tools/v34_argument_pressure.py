#!/usr/bin/env python3
"""Ten bounded period-compiler pressure examples; compile only, no harness."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
from v34_argument_homes import audit


def source(count):
    lines = ['struct pressure_record { unsigned input[4]; unsigned output; };',
             'extern void pressure_tick(void);',
             'extern void pressure_sink(unsigned, unsigned, unsigned, unsigned);',
             'void pressure(struct pressure_record *p, unsigned rounds)', '{']
    for k in range(count):
        lines.append('    unsigned a%d = p->input[%d];' % (k, k))
    lines += ['    while (rounds--) {', '        pressure_tick();']
    for k in range(count):
        lines.append('        a%d = a%d * 1664525U + 1013904223U;' % (k, k))
    if count:
        lines.append('        p->output = ' + ' ^ '.join('a%d' % k for k in range(count)) + ';')
    else:
        lines.append('        p->output += 1U;')
    lines += ['    }', '    pressure_sink(' + ', '.join('a%d' % k if k < count else '0U' for k in range(4)) + ');', '}']
    return '\n'.join(lines) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config', type=Path, required=True)
    args = ap.parse_args()
    config = args.config.read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    out = ROOT / 'build/v34-argument-pressure'
    out.mkdir(parents=True, exist_ok=True)
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929958161', 'config': config, 'kind': 'compiler mechanism examples, no blob-equivalence verdict', 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for count in range(5):
        for rename in (True, False):
            name = 'acc-%d-rename-%d' % (count, rename)
            cell = out / name
            cell.mkdir(exist_ok=True)
            text = source(count)
            (cell / 'pressure.c').write_text(text)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ([] if rename else ['-fno-rename-registers']) + ['-da'], directory + '/pressure.o', directory + '/pressure.c')]
            entry = {'command': cmd, 'source_sha256': hashlib.sha256(text.encode()).hexdigest()}
            result['cells'][name] = entry
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            assert entry['compile_exit'] == 0, name
            obj = cell / 'pressure.o'
            entry['object_sha256'] = hashlib.sha256(obj.read_bytes()).hexdigest()
            dis = subprocess.check_output(['objdump', '-dr', '--disassemble=pressure', str(obj)], text=True)
            (cell / 'pressure.dis').write_text(dis)
            greg = (cell / 'pressure.c.25.greg').read_text()
            root = re.search(r'\(set \(reg/v/f:SI (\d+) \[ p \]\)', (cell / 'pressure.c.01.rtl').read_text())
            assert root, name
            entry['pointer_pseudo'] = int(root[1])
            entry['pointer_allocated_registers'] = sorted(set(re.findall(r'\(reg[^\n]*?:SI (\d+) (\w+) \[orig:' + root[1] + r' p \]', greg)))
            entry['incoming_equivalence_retained'] = 'p+0 S4 A32' in greg
            entry['argument_home'] = audit(obj, 'pressure')
            print(name, 'pointer pseudo=%s registers=%s' % (root[1], entry['pointer_allocated_registers']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('compiler pressure cells: %d compiled; 0 executed; no reconstruction exactness denominator' % len(result['cells']), flush=True)


if __name__ == '__main__':
    main()
