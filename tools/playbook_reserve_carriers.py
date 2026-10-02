#!/usr/bin/env python3
"""Four bounded RxHdxSequenceE count/array carrier controls on its complete TU."""
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


def write_analysis(out, results):
    """Replay graph detectors on four known objects, with explicit controls."""
    expected = {'baseline': (True, False), 'chained': (True, True),
                'direct-array': (False, False), 'both': (False, True)}
    assert set(results['cells']) == set(expected), 'incomplete four-cell domain'
    analysis = {'domain': results['domain'], 'revision': results['revision'],
                'control_count': len(expected), 'controls_passed': 0, 'cells': {}}
    for name, (rebase, store_first) in expected.items():
        cell = results['cells'][name]
        obj = out/name/'V32rxhdx.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash'], 'object drift'
        dis = subprocess.check_output(['python3', str(ROOT/'tools/dis.py'),
                                       str(obj), 'RxHdxSequenceE'], text=True)
        region = dis[dis.index('R_386_PC32 DemodDataV32'):].split('R_386_PC32 DescrambleDataV32')[0]
        actual = (bool(re.search(r'add\s+\$0x3c', dis)),
                  region.index('mov    %ax,') < region.index('movzwl %ax,'))
        assert actual == (rebase, store_first), 'known graph control failed: '+name
        analysis['controls_passed'] += 1
        analysis['cells'][name] = {
            'verdict': cell['verdicts']['RxHdxSequenceE'],
            'functions': len(cell['function_symbols']), 'globals': len(cell['globals']),
            'exact': sum(v[0] == 'EXACT' for v in cell['verdicts'].values()),
            'compared': len(cell['verdicts']), 'gains': cell.get('exact_gains', []),
            'losses': cell.get('exact_losses', []), 'changed_bodies': cell.get('changed_bodies', []),
            'rate_store_rebased': actual[0], 'count_store_before_widen': actual[1],
            'expected_rate_store_rebased': rebase, 'expected_count_store_before_widen': store_first}
    (out/'analysis.json').write_text(json.dumps(analysis, indent=2)+'\n')
    print('graph controls:', analysis['controls_passed'], '/', analysis['control_count'], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domain', required=True, help='Predeclared issue-comment URL')
    parser.add_argument('--baseline-object', type=Path, help='Explicit unchanged full-TU control for replay')
    parser.add_argument('--analysis-only', action='store_true', help='Replay graph detectors on the existing four objects without compiling')
    args = parser.parse_args()
    if args.analysis_only:
        out = ROOT/'build/playbook-reserve-carriers'
        results = json.loads((out/'results.json').read_text())
        assert results['domain'] == args.domain, 'declared domain drift'
        write_analysis(out, results)
        return
    revision = '5632087b'
    out = ROOT / 'build/playbook-reserve-carriers'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', revision + ':src/pump/v32/V32rxhdx.c'], cwd=ROOT, text=True)
    header_paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).splitlines()
    assert not subprocess.check_output(['git', 'diff', '--name-only', revision, '--', 'include', 'tools/toolchain/period_compat.h'], cwd=ROOT, text=True).strip(), 'header baseline drift'
    header_paths += [str(p.relative_to(ROOT)) for p in (ROOT/'src/pump/v32').glob('*.h')]
    headers = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in header_paths}
    start = source.index('void\nRxHdxSequenceE(')
    end = source.index('\n}\n', start) + 2
    function = source[start:end]
    demod = '\tn = DemodDataV32(modem, in, out, *count);\n\t*count = n;'
    regs = '\t\tregs = hdx->regs;\n\t\tregs[V32HDX_REG_RATE_SEQ] = (short)GetSequence(modem);'
    assert function.count(demod)==function.count(regs)==1
    generated={}
    for name, chain, direct in [('baseline',False,False),('chained',True,False),('direct-array',False,True),('both',True,True)]:
        fn=function
        if chain: fn=fn.replace(demod,'\t*count = n = DemodDataV32(modem, in, out, *count);')
        if direct: fn=fn.replace(regs,'\t\thdx->regs[V32HDX_REG_RATE_SEQ] = (short)GetSequence(modem);')
        generated[name]=source[:start]+fn+source[end:]
    assert len(generated)==len(set(generated.values()))==4
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if flag == '-Iinclude' else '/src/' + flag if flag == 'tools/toolchain/period_compat.h' else flag for flag in flags]
    retained = args.baseline_object or (out / 'baseline/V32rxhdx.o' if (out / 'baseline/V32rxhdx.o').exists() else ROOT / 'build/tc_out/src_pump_v32_V32rxhdx.c.o')
    retained_bytes = retained.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'revision': revision, 'config': config, 'domain': args.domain, 'headers': headers, 'retained_object': str(retained), 'retained_object_hash': hashlib.sha256(retained_bytes).hexdigest(), 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for name, text in generated.items():
        directory = out / name
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'V32rxhdx.c').write_text(text)
        target = '/work/' + name
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(target) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ['-da'], target + '/V32rxhdx.o', target + '/V32rxhdx.c')]
        entry = {'command': command, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
        results['cells'][name] = entry
        assert all(hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest for path, digest in headers.items()), 'header changed during experiment'
        with (directory / 'compile.log').open('w') as log:
            entry['compile_exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        assert entry['compile_exit'] == 0, name
        obj = str(directory / 'V32rxhdx.o')
        entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
        entry['globals'] = {line.split()[-1]: line.split()[-2] for line in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
        entry['function_symbols'] = sorted(b.sizes(obj))
        entry['verdicts'] = {symbol: b.verdict(*b.body(blob, symbol), *b.body(obj, symbol)) for symbol in sorted(set(b.sizes(obj)) & set(b.sizes(blob)))}
        dis = subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), obj, 'RxHdxSequenceE'], text=True)
        (directory / 'RxHdxSequenceE.dis').write_text(dis)
        call_start = dis.index('R_386_PC32 DemodDataV32')
        call_region = dis[call_start:].split('R_386_PC32 DescrambleDataV32')[0]
        entry['count_store_before_widen'] = call_region.index('mov    %ax,') < call_region.index('movzwl %ax,')
        entry['rate_store_rebased'] = bool(re.search(r'add\s+\$0x3c', dis))
        assert entry['count_store_before_widen'] == (name in ('chained', 'both')), 'count-order known input detector did not fire'
        assert entry['rate_store_rebased'] == (name in ('baseline', 'chained')), 'array-base known input detector did not fire'
        rtl = (directory/'V32rxhdx.c.01.rtl').read_text().split(';; Function RxHdxSequenceE')[1].split(';; Function')[0]
        (directory/'RxHdxSequenceE-initial.rtl').write_text(rtl)
        if name == 'baseline':
            assert Path(obj).read_bytes() == retained_bytes, 'raw unchanged baseline drift'
            entry['baseline_reproduced'] = True
        else:
            base = results['cells']['baseline']
            assert entry['globals'] == base['globals']
            assert entry['function_symbols'] == base['function_symbols']
            baseline = str(out / 'baseline/V32rxhdx.o')
            entry['changed_bodies'] = [symbol for symbol in entry['function_symbols'] if b.body(obj, symbol) != b.body(baseline, symbol)]
            entry['exact_gains'] = [symbol for symbol, verdict in entry['verdicts'].items() if verdict[0] == 'EXACT' and base['verdicts'][symbol][0] != 'EXACT']
            entry['exact_losses'] = [symbol for symbol, verdict in base['verdicts'].items() if verdict[0] == 'EXACT' and entry['verdicts'][symbol][0] != 'EXACT']
        print(name, entry['verdicts']['RxHdxSequenceE'], 'exact', sum(verdict[0] == 'EXACT' for verdict in entry['verdicts'].values()), '/', len(entry['verdicts']), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    write_analysis(out, results)
    (out / 'blob-RxHdxSequenceE.dis').write_text(subprocess.check_output(['python3', str(ROOT / 'tools/dis.py'), blob, 'RxHdxSequenceE'], text=True))


if __name__ == '__main__':
    main()
