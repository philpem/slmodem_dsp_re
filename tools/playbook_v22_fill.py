#!/usr/bin/env python3
"""Four bounded V22 fill-loop counter/cursor controls in the complete TU."""
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
    revision = '5632087b'
    out = ROOT / 'build/playbook-v22-fill'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/pump/v22/v22prc.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    generated = {}
    for name, countdown, cursor in [('baseline', False, False), ('countdown-index', True, False), ('ascending-cursor', False, True), ('countdown-cursor', True, True)]:
        text = source
        for symbol, block, value in [('TxNOP', 'V22_TX_BLOCK', '0'), ('RxClampV22', 'V22_CLAMP_BLOCK', 'V22_CLAMP_VALUE')]:
            start = text.index('void\n' + symbol + '(')
            end = text.index('\n}\n', start)+2
            fn = text[start:end]
            old = '\tfor (i = 0; i <= ' + block + ' - 1; i++)\n\t\tout[i] = ' + value + ';'
            assert fn.count(old) == 1
            if countdown:
                store = '*out++' if cursor else 'out[' + block + ' - 1 - i]'
                loop = '\ti = ' + block + ' - 1;\n\tdo {\n\t\t' + store + ' = ' + value + ';\n\t} while (i-- != 0);'
            elif cursor:
                loop = '\tfor (i = 0; i <= ' + block + ' - 1; i++)\n\t\t*out++ = ' + value + ';'
            else:
                loop = old
            fn = fn.replace(old, loop)
            text = text[:start] + fn + text[end:]
        generated[name] = text
    assert len(generated) == len(set(generated.values())) == 4
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    retained = args.baseline_object or (out / 'baseline/v22prc.o' if (out / 'baseline/v22prc.o').exists() else ROOT / 'build/tc_out/src_pump_v22_v22prc.c.o')
    retained_bytes = retained.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'revision': revision, 'config': config, 'domain': args.domain, 'headers': headers, 'retained_object': str(retained), 'retained_object_hash': hashlib.sha256(retained_bytes).hexdigest(), 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'v22prc.c').write_text(text)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ['-da'], target + '/v22prc.o', target + '/v22prc.c')]
        entry = {'command': command, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
        results['cells'][name] = entry
        assert all(hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest for path, digest in headers.items()), 'header changed during experiment'
        with (directory / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert entry['compile_exit'] == 0, name
        obj = str(directory / 'v22prc.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {line.split()[-1]: line.split()[-2] for line in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
        entry['function_symbols'] = sorted(b.sizes(obj))
        entry['verdicts'] = {symbol: b.verdict(*b.body(blob, symbol), *b.body(obj, symbol)) for symbol in sorted(set(b.sizes(obj)) & set(b.sizes(blob)))}
        for symbol in ('TxNOP', 'RxClampV22'):
            (directory / (symbol + '.dis')).write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, symbol], text=True))
        if name == 'baseline':
            assert Path(obj).read_bytes() == retained_bytes, 'raw unchanged baseline drift'
            entry['baseline_reproduced'] = True
        else:
            base = results['cells']['baseline']
            assert entry['globals'] == base['globals']
            assert entry['function_symbols'] == base['function_symbols']
            baseline = str(out / 'baseline/v22prc.o')
            entry['changed_bodies'] = [symbol for symbol in entry['function_symbols'] if b.body(obj, symbol) != b.body(baseline, symbol)]
            entry['exact_gains'] = [symbol for symbol, verdict in entry['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            entry['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and entry['verdicts'][symbol][0] != 'EXACT']
        print(name, {symbol: entry['verdicts'][symbol] for symbol in ('TxNOP', 'RxClampV22')}, 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    for symbol in ('TxNOP', 'RxClampV22'):
        (out / ('blob-' + symbol + '.dis')).write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, symbol], text=True))


if __name__ == '__main__':
    main()
