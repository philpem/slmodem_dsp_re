#!/usr/bin/env python3
"""Four bounded TimingV34 counter/halving controls for the complete V34 TU."""
import argparse
import hashlib
import json
import re
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True, help='Predeclared issue-comment URL')
    parser.add_argument('--baseline-object', type=Path, help='Explicit unchanged full-TU control for replay')
    args = parser.parse_args()
    revision = '9b7aeacc'
    out = ROOT / 'build/v34-timing-carriers'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    start = source.index('void\nTimingV34(')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    halves = {'(a + b) / 2': '((a + b) >> 1)', 'n / 2': '(n >> 1)', '(short)rx->symbol_period / 2': '((short)rx->symbol_period >> 1)'}
    assert all(function.count(old) == 1 for old in halves)
    assert function.count('\tint n;') == function.count('n = (unsigned short)(rx->ppm_count + 1);') == 1
    generated = {}
    for name, narrow, shifts in [('baseline', False, False), ('short-counter', True, False), ('arithmetic-halves', False, True), ('both', True, True)]:
        fn = function
        if narrow:
            fn = fn.replace('\tint n;', '\tshort n;')
            fn = fn.replace('n = (unsigned short)(rx->ppm_count + 1);', 'n = (short)(rx->ppm_count + 1);')
        if shifts:
            for old, new in halves.items():
                fn = fn.replace(old, new)
        generated[name] = source[:start] + fn + source[end:]
    assert len(generated) == len(set(generated.values())) == 4
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    retained = args.baseline_object or (out / 'baseline/V34hshak.o' if (out / 'baseline/V34hshak.o').exists() else ROOT / 'build/tc_out/src_pump_v34_V34hshak.c.o')
    retained_bytes = retained.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'revision': revision, 'config': config, 'domain': args.domain, 'headers': headers, 'retained_object': str(retained), 'retained_object_hash': hashlib.sha256(retained_bytes).hexdigest(), 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'V34hshak.c').write_text(text)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ['-da'], target + '/V34hshak.o', target + '/V34hshak.c')]
        entry = {'command': command, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
        results['cells'][name] = entry
        assert all(hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest for path, digest in headers.items()), 'header changed during experiment'
        with (directory / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert entry['compile_exit'] == 0, name
        obj = str(directory / 'V34hshak.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {line.split()[-1]: line.split()[-2] for line in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
        entry['function_symbols'] = sorted(b.sizes(obj))
        entry['verdicts'] = {symbol: b.verdict(*b.body(blob, symbol), *b.body(obj, symbol)) for symbol in sorted(set(b.sizes(obj)) & set(b.sizes(blob)))}
        dis = subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, 'TimingV34'], text=True)
        (directory / 'TimingV34.dis').write_text(dis)
        rtl = (directory / 'V34hshak.c.01.rtl').read_text().split(';; Function TimingV34')[1].split(';; Function')[0]
        entry['division_equal_notes'] = len(re.findall(r'expr_list:REG_EQUAL \(div:', rtl))
        entry['sar_one'] = re.findall(r'.*\bsar\s+\$1,.*', dis)
        entry['word_report_compare'] = re.findall(r'.*66 .*\bcmp\s+.*0x1d2.*', dis)
        expected_notes = 0 if name in ('arithmetic-halves', 'both') else 3
        assert entry['division_equal_notes'] == expected_notes, 'arithmetic generator did not fire'
        if name in ('short-counter', 'both'):
            assert len(entry['word_report_compare']) == 1, 'signed-short carrier did not fire'
        if name in ('arithmetic-halves', 'both'):
            assert len(entry['sar_one']) == 3, 'three explicit arithmetic halves did not fire'
        if name == 'baseline':
            assert Path(obj).read_bytes() == retained_bytes, 'raw unchanged baseline drift'
            entry['baseline_reproduced'] = True
        else:
            base = results['cells']['baseline']
            assert entry['globals'] == base['globals']
            assert entry['function_symbols'] == base['function_symbols']
            baseline = str(out / 'baseline/V34hshak.o')
            entry['changed_bodies'] = [symbol for symbol in entry['function_symbols'] if b.body(obj, symbol) != b.body(baseline, symbol)]
            entry['exact_gains'] = [symbol for symbol, verdict in entry['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            entry['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and entry['verdicts'][symbol][0] != 'EXACT']
        print(name, entry['verdicts']['TimingV34'], 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    (out / 'blob-TimingV34.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, 'TimingV34'], text=True))


if __name__ == '__main__':
    main()
