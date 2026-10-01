#!/usr/bin/env python3
"""Locate V8's charFlip duplication and replay one block-reordering control."""
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
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b

TARGET = 'rebuildJMSequence'
CALLEE = 'charFlip'


def census(directory):
    stages = []
    for path in sorted(directory.glob('V8.c.*')):
        if not re.fullmatch(r'V8\.c\.\d+\.\w+', path.name):
            continue
        text = path.read_text()
        match = re.search(r'^;; Function ' + TARGET + r'\s*$', text, re.M)
        if match is None:
            continue
        section = text[match.end():]
        following = re.search(r'^;; Function ', section, re.M)
        if following:
            section = section[:following.start()]
        calls = {}
        for block in re.split(r'(?=^\((?:insn|jump_insn|note|code_label|barrier|call_insn)(?::[A-Z]+)? )', section, flags=re.M):
            uid = re.match(r'\(call_insn(?::[A-Z]+)? (\d+) ', block)
            if uid and re.search(r'symbol_ref[^\n]*\("' + CALLEE + r'"\)', block):
                calls[uid.group(1)] = block
        stages.append({'stage': path.name, 'calls': len(calls), 'call_uids': sorted(calls, key=int),
                       'symbol_refs': len(re.findall(r'symbol_ref[^\n]*\("' + CALLEE + r'"\)', section)),
                       'dump_hash': hashlib.sha256(path.read_bytes()).hexdigest()})
        (directory / (path.name + '.function')).write_text(section)
    assert stages, 'empty stage census'
    return stages


def globals_of(obj):
    return {row.split()[-1]: row.split()[-2] for row in subprocess.check_output(['nm', '-g', '--defined-only', str(obj)], text=True).splitlines()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True)
    parser.add_argument('--baseline-object', type=Path)
    args = parser.parse_args()
    revision = '4873c184'
    out = ROOT / 'build/v8-call-layout'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/v8/V8.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'src/v8', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'src/v8', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'input baseline drift'
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    retained = args.baseline_object or (out / 'baseline/V8.o' if (out / 'baseline/V8.o').exists() else ROOT / 'build/tc_out/src_v8_V8.c.o')
    retained_bytes = retained.read_bytes()
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(row[6:] for row in config.splitlines() if row.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    # Quoted local headers resolve beside V8.c in the normal build.
    flags += ['-I/src/src/v8']
    results = {'revision': revision, 'domain': args.domain, 'config': config, 'inputs': headers, 'cells': {}}
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, options in [('baseline', []), ('no-reorder', ['-fno-reorder-blocks'])]:
        directory = out / name
        directory.mkdir(exist_ok=True)
        (directory / 'V8.c').write_text(source)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + options + ['-da'], target + '/V8.o', target + '/V8.c')]
        cell = {'command': command, 'source_hash': hashlib.sha256(source.encode()).hexdigest()}
        results['cells'][name] = cell
        assert all(hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest for path, digest in headers.items()), 'input changed during compilation'
        with (directory / 'compile.log').open('w') as log:
            cell['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert cell['compile_exit'] == 0
        obj = directory / 'V8.o'
        cell['object_hash'] = hashlib.sha256(obj.read_bytes()).hexdigest()
        cell['globals'] = globals_of(obj)
        cell['function_symbols'] = sorted(b.sizes(str(obj)))
        cell['verdicts'] = {symbol: b.verdict(*b.body(blob, symbol), *b.body(str(obj), symbol)) for symbol in sorted(set(b.sizes(str(obj))) & set(b.sizes(blob)))}
        cell['stages'] = census(directory)
        cell['final_calls'] = sum(target[:2] == ('symbol', CALLEE) for kind, target in b.body(str(obj), TARGET)[1].values() if kind == 'R_386_PC32')
        if name == 'baseline':
            assert obj.read_bytes() == retained_bytes, 'raw unchanged full-TU baseline drift'
            cell['baseline_reproduced'] = True
            stage_counts = {row['stage']: row['calls'] for row in cell['stages']}
            assert stage_counts['V8.c.01.rtl'] == stage_counts['V8.c.30.rnreg'] == 7
            assert stage_counts['V8.c.31.bbro'] == cell['final_calls'] == 8
        else:
            base = results['cells']['baseline']
            assert cell['globals'] == base['globals'] and cell['function_symbols'] == base['function_symbols']
            cell['changed_bodies'] = [symbol for symbol in cell['function_symbols'] if b.body(str(obj), symbol) != b.body(str(out / 'baseline/V8.o'), symbol)]
            cell['exact_gains'] = [symbol for symbol, verdict in cell['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            cell['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and cell['verdicts'][symbol][0] != 'EXACT']
            assert cell['final_calls'] == 7, 'mechanism prediction refuted'
        (directory / (TARGET + '.dis')).write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=' + TARGET, str(obj)], text=True))
        print(name, cell['verdicts'][TARGET], 'calls', cell['final_calls'], 'exact', sum(v[0] == 'EXACT' for v in cell['verdicts'].values()), '/', len(cell['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
