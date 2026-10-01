#!/usr/bin/env python3
"""Four bounded signed-halving controls for the complete V34 handshake TU."""
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True, help='Predeclared issue-comment URL')
    parser.add_argument('--width-study', action='store_true', help='Hold both shifts and cross best/loop local widths')
    parser.add_argument('--baseline-object', type=Path, help='Explicit unchanged full-TU control for replay')
    args = parser.parse_args()
    revision = '27cfae0f'
    out = ROOT / ('build/v34-initialphase-width' if args.width_study else 'build/v34-initialphase-halving')
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    start = source.index('void\nsetInitialPhase(')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    input_half = '(a + b) / 2'
    polynomial_half = '(p1 + p0) / 2'
    assert function.count(input_half) == function.count(polynomial_half) == 1
    generated = {}
    for name, input_shift, polynomial_shift in [('baseline', False, False), ('input-shift', True, False), ('polynomial-shift', False, True), ('both-shift', True, True)]:
        fn = function
        if input_shift:
            fn = fn.replace(input_half, '((a + b) >> 1)')
        if polynomial_shift:
            fn = fn.replace(polynomial_half, '((p1 + p0) >> 1)')
        generated[name] = source[:start] + fn + source[end:]
    assert len(generated) == len(set(generated.values())) == 4
    if args.width_study:
        shifted = generated['both-shift']
        generated = {'baseline': source}
        for best_width in ('int', 'short'):
            for loop_width in ('int', 'short'):
                fn_start = shifted.index('void\nsetInitialPhase(')
                fn_end = shifted.index('\n}\n', fn_start) + 2
                fn = shifted[fn_start:fn_end]
                assert fn.count('\tint best = 0x7d00;') == fn.count('\tint i;') == 1
                fn = fn.replace('\tint best = 0x7d00;', '\t' + best_width + ' best = 0x7d00;')
                fn = fn.replace('\tint i;', '\t' + loop_width + ' i;')
                generated[best_width + '-best-' + loop_width + '-loop'] = shifted[:fn_start] + fn + shifted[fn_end:]
        assert len(generated) == len(set(generated.values())) == 5
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
        (directory / 'setInitialPhase.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, 'setInitialPhase'], text=True))
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
        print(name, entry['verdicts']['setInitialPhase'], 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    (out / 'blob-setInitialPhase.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, 'setInitialPhase'], text=True))


if __name__ == '__main__':
    main()
