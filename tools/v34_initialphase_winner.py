#!/usr/bin/env python3
"""Finite initial-phase winner certificate and three bounded compiler controls."""
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



def prove_winner():
    # Optional dependency: compiler controls do not import NumPy.
    import time
    # tools/dis.py must not shadow the standard-library module NumPy needs.
    saved_path = sys.path[:]
    sys.path = [entry for entry in sys.path if Path(entry).resolve() != ROOT / 'tools']
    try:
        import numpy as np
    finally:
        sys.path = saved_path

    def poly(k):
        return -21*k*k+837*k-354

    def truncdiv(n, d):
        return (abs(n)//abs(d)) * (-1 if (n < 0) != (d < 0) else 1)

    ratios = [truncdiv(((poly(i+20)-poly(i)) << 13) + ((poly(i+20)+poly(i)) >> 1), poly(i+20)+poly(i)) for i in range(20)]
    assert ratios[:4] == [8952, 7293, 5974, 4887]
    mask, lo, hi = (1 << 29)-1, 32000*8192, 32768*8192-1
    # SAR13 followed by signed16 narrowing depends only on bits 13..28.
    # Squaring modulo 2^29 is periodic in x with period 2^28:
    # (x+2^28)^2-x^2 = x*2^29 + 2^56.
    controls = []
    for x, expected in [(16190, 31997), (16191, 32001), (16383, 32764), (16384, -32768)]:
        signed32 = (x*x+4096) & 0xffffffff
        if signed32 >= 1 << 31:
            signed32 -= 1 << 32
        error = (signed32 >> 13) & 0xffff
        if error >= 1 << 15:
            error -= 1 << 16
        assert error == expected
        predicate = lo <= ((x*x+4096) & mask) <= hi
        assert predicate == (error >= 32000)
        controls.append({'x': x, 'signed16_error': error, 'unimproved': predicate})
    counts = [0]*20
    checked = 0
    started = time.monotonic()
    for begin in range(0, 1 << 28, 1 << 20):
        survivors = np.arange(begin, begin+(1 << 20), dtype=np.int64)
        checked += len(survivors)
        # |x-r_i| < 2^29, so x*x+4096 fits signed int64 exactly.
        for j, r in enumerate(ratios):
            x = survivors-r
            residue = (x*x+4096) & mask
            survivors = survivors[(residue >= lo) & (residue <= hi)]
            counts[j] += len(survivors)
            if not len(survivors):
                break
    assert checked == 1 << 28
    assert counts[:4] == [3142656, 37136, 431, 0]
    assert all(count == 0 for count in counts[3:])
    result = {'boundary': 'compiled i386 wrapped subtraction/multiply/add, SAR13 and signed16 narrowing; not C signed-overflow semantics or ratio reachability', 'proof_kind': 'exhaustive finite modular arithmetic, no blob oracle or sampling', 'polynomial_ratios': ratios, 'ratio_period': 1 << 28, 'checked_residues': checked, 'unimproved_residue_interval': [lo, hi], 'positive_controls': controls, 'survivor_counts_by_prefix': counts, 'first_guaranteed_improvement_by_index': 3, 'elapsed_seconds': time.monotonic()-started}
    out = ROOT / 'build/v34-initialphase-winner'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'proof.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', help='Predeclared issue-comment URL')
    parser.add_argument('--proof', action='store_true', help='Exhaustively prove the compiled arithmetic winner invariant; NumPy required only here')
    parser.add_argument('--baseline-object', type=Path, help='Explicit unchanged full-TU control for replay')
    args = parser.parse_args()
    if args.proof:
        prove_winner()
        return
    if not args.domain:
        parser.error('--domain is required for compiler controls')
    revision = 'ad63a915'
    out = ROOT / 'build/v34-initialphase-winner'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    start = source.index('void\nsetInitialPhase(')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    assert function.count('(a + b) >> 1') == function.count('(p1 + p0) >> 1') == 1
    generated = {}
    old_input = """	a = (short)rx->timing_out[k - 1];
	b = (short)rx->timing_out[k];
	if (a < 0) {
		a = (short)-a;
		b = (short)-b;
	}
"""
    new_input = """	if (rx->timing_out[k - 1] < 0) {
		a = (short)-rx->timing_out[k - 1];
		b = (short)-rx->timing_out[k];
	} else {
		a = (short)rx->timing_out[k - 1];
		b = (short)rx->timing_out[k];
	}
"""
    assert function.count(old_input) == 1
    for name, paths, narrow in [('baseline', False, False), ('graph-winner-zero', True, True), ('graph-winner-assigned', True, True)]:
        fn = function
        if paths:
            fn = fn.replace(old_input, new_input)
            fn = fn.replace('\tint ratio = 0;', '\tint ratio;')
            fn = fn.replace('\tint best = 0x7d00;', '\tint best;')
            fn = fn.replace('\tint besti = 0;', '\tint besti;')
            fn = fn.replace('\tif (a + b == 0) {', '\tif (a + b == 0) {\n\t\tratio = 0;')
            fn = fn.replace('\tfor (i = 0; i <= 0x13; i++) {', '\tbest = 0x7d00;\n\tbesti = 0;\n\tfor (i = 0; i <= 0x13; i++) {')
            fn = fn.replace('\t\tint r = 0;', '\t\tint r;')
            fn = fn.replace('\t\tif (p1 + p0 == 0) {', '\t\tif (p1 + p0 == 0) {\n\t\t\tr = 0;')
        if narrow:
            fn = fn.replace('\tint a, b;', '\tshort a, b;')
            fn = fn.replace('\tint best', '\tshort best', 1)
            fn = fn.replace('\tint i;', '\tshort i;')
            fn = fn.replace('\t\tint e;', '\t\tshort e;')
            fn = fn.replace('pos = (10 - besti) * 0x230 +', 'pos = (short)((10 - besti) * 0x230) +')
        if name == 'graph-winner-assigned':
            assert fn.count('\tbesti = 0;') == 1
            fn = fn.replace('\tbesti = 0;\n', '')
        generated[name] = source[:start] + fn + source[end:]
    assert len(generated) == len(set(generated.values())) == 3
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
