#!/usr/bin/env python3
"""Six bounded FIFO8 write return/wrap controls with temporary public-header overlays."""
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
    revision = '555036b4'
    out = ROOT / 'build/playbook-fifo-write'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/service/Fifo8.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    start = source.index('short\nFIFO8_write(')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    original_header = (ROOT / 'include/dsplib/fifo8.h').read_text()
    assert original_header.count('short FIFO8_write(') == 1
    old_wrap = """		wr++;
		if (wr >= size)
			wr = 0;"""
    prefix_wrap = """		if (++wr >= size)
			wr = 0;"""
    assert function.count(old_wrap) == 1
    generated, overlays = {}, {}
    for return_type in ('short', 'unsigned short', 'int'):
        for combined in (False, True):
            name = ('baseline' if return_type == 'short' and not combined else return_type.replace(' ', '-') + ('-prefix-if' if combined else '-separate-if'))
            fn = function
            if return_type != 'short':
                assert fn.startswith('short\nFIFO8_write(')
                fn = return_type + fn[len('short'):]
                fn = fn.replace('return (short)cnt;', 'return cnt;')
            if combined:
                fn = fn.replace(old_wrap, prefix_wrap)
            generated[name] = source[:start] + fn + source[end:]
            overlays[name] = original_header.replace('short FIFO8_write(', return_type + ' FIFO8_write(')
    assert len(generated) == len(set(generated.values())) == 6
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    retained = args.baseline_object or (out / 'baseline/Fifo8.o' if (out / 'baseline/Fifo8.o').exists() else ROOT / 'build/tc_out/src_service_Fifo8.c.o')
    retained_bytes = retained.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'revision': revision, 'config': config, 'domain': args.domain, 'headers': headers, 'retained_object': str(retained), 'retained_object_hash': hashlib.sha256(retained_bytes).hexdigest(), 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'Fifo8.c').write_text(text)
        overlay_path = directory / 'include/dsplib/fifo8.h'
        overlay_path.parent.mkdir(parents=True, exist_ok=True)
        overlay_path.write_text(overlays[name])
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + target + '/include'] + flags + ['-da'], target + '/Fifo8.o', target + '/Fifo8.c')]
        entry = {'consumer_boundary': 'Only complete Fifo8.c TU compiled; public callers and independently vendored reference declarations not rebuilt by this diagnostic', 'overlay_header_hash': hashlib.sha256(overlays[name].encode()).hexdigest(), 'command': command, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
        results['cells'][name] = entry
        assert all(hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest for path, digest in headers.items()), 'header changed during experiment'
        with (directory / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert entry['compile_exit'] == 0, name
        obj = str(directory / 'Fifo8.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {line.split()[-1]: line.split()[-2] for line in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
        entry['function_symbols'] = sorted(b.sizes(obj))
        entry['verdicts'] = {symbol: b.verdict(*b.body(blob, symbol), *b.body(obj, symbol)) for symbol in sorted(set(b.sizes(obj)) & set(b.sizes(blob)))}
        (directory / 'FIFO8_write.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, 'FIFO8_write'], text=True))
        if name == 'baseline':
            assert Path(obj).read_bytes() == retained_bytes, 'raw unchanged baseline drift'
            entry['baseline_reproduced'] = True
        else:
            base = results['cells']['baseline']
            assert entry['globals'] == base['globals']
            assert entry['function_symbols'] == base['function_symbols']
            baseline = str(out / 'baseline/Fifo8.o')
            entry['changed_bodies'] = [symbol for symbol in entry['function_symbols'] if b.body(obj, symbol) != b.body(baseline, symbol)]
            entry['exact_gains'] = [symbol for symbol, verdict in entry['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            entry['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and entry['verdicts'][symbol][0] != 'EXACT']
        print(name, entry['verdicts']['FIFO8_write'], 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    (out / 'blob-FIFO8_write.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, 'FIFO8_write'], text=True))


if __name__ == '__main__':
    main()
