#!/usr/bin/env python3
"""Complete the bounded V34 source/GCSE/CSE2 cube with four new diagnostic cells."""
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
    parser.add_argument('--baseline-object', type=Path, help='Explicit unchanged full-TU control for replay')
    args = parser.parse_args()
    revision = 'fc46a6a0'
    out = ROOT / 'build/v34-metric-pre'
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
    for name, paths, narrow in [('baseline', False, False), ('production-no-gcse', False, False), ('production-no-gcse-no-rerun', False, False), ('graph-no-gcse', True, True), ('graph-no-gcse-no-rerun', True, True)]:
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
        generated[name] = source[:start] + fn + source[end:]
    assert len(generated) == 5 and len(set(generated.values())) == 2
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    retained = args.baseline_object or (out / 'baseline/V34hshak.o' if (out / 'baseline/V34hshak.o').exists() else ROOT / 'build/tc_out/src_pump_v34_V34hshak.c.o')
    retained_bytes = retained.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'revision': revision, 'config': config, 'domain': args.domain, 'headers': headers, 'retained_object': str(retained), 'retained_object_hash': hashlib.sha256(retained_bytes).hexdigest(), 'cells': {}}
    historical_path = ROOT / 'build/v34-metric-cse/results.json'
    historical = json.loads(historical_path.read_text())
    assert historical['config'] == config and historical['headers'] == headers
    for old_name, old in historical['cells'].items():
        source_name = 'graph-no-gcse' if old_name.startswith('graph') else 'baseline'
        assert old['source_hash'] == hashlib.sha256(generated[source_name].encode()).hexdigest()
        old_object = historical_path.parent / old_name / 'V34hshak.o'
        assert old['object_hash'] == hashlib.sha256(old_object.read_bytes()).hexdigest()
    results['historical_controls'] = {'path': str(historical_path), 'json_hash': hashlib.sha256(historical_path.read_bytes()).hexdigest(), 'cells': historical['cells']}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'V34hshak.c').write_text(text)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + (['-fno-gcse'] if name != 'baseline' else []) + (['-fno-rerun-cse-after-loop'] if name.endswith('no-rerun') else []) + ['-da'], target + '/V34hshak.o', target + '/V34hshak.c')]
        entry = {'gcse_disabled': name != 'baseline', 'post_loop_cse_disabled': name.endswith('no-rerun'), 'command': command, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
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
            assert Path(obj).read_bytes() == (historical_path.parent / 'baseline/V34hshak.o').read_bytes(), 'historical control drift'
            entry['baseline_reproduced'] = True
        else:
            base = results['cells']['baseline']
            assert entry['globals'] == base['globals']
            assert entry['function_symbols'] == base['function_symbols']
            baseline = str(out / 'baseline/V34hshak.o')
            entry['changed_bodies'] = [symbol for symbol in entry['function_symbols'] if b.body(obj, symbol) != b.body(baseline, symbol)]
            entry['exact_gains'] = [symbol for symbol, verdict in entry['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            entry['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and entry['verdicts'][symbol][0] != 'EXACT']
        assert len(entry['function_symbols']) == 63 and len(entry['globals']) == 55
        print(name, entry['verdicts']['setInitialPhase'], 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    (out / 'blob-setInitialPhase.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, 'setInitialPhase'], text=True))


if __name__ == '__main__':
    main()
