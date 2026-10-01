#!/usr/bin/env python3
"""Eight toy/full-TU coefficient propagation controls; compiler comparison only."""
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
FORMS = {
    'baseline': 'return (short)(-21 * (int)k * k + 837 * k - 354);',
    'variable-square': 'int a = -21, b = 837, c = -354; return (short)(a * ((int)k * k) + b * k + c);',
    'constant-square': 'const int a = -21, b = 837, c = -354; return (short)(a * ((int)k * k) + b * k + c);',
    'variable-left': 'int a = -21, b = 837, c = -354; return (short)(a * (int)k * k + b * k + c);',
}

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--revision', default='1f340221')
    ap.add_argument('--baseline-object', type=Path, default=ROOT / 'build/v34-agent-polycoeff/full-baseline/V34hshak.o')
    args = ap.parse_args()
    out = ROOT / 'build/v34-polynomial-coefficients'
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(['git', 'show', args.revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
    assert source.count(FORMS['baseline']) == 1
    config = (ROOT / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    prior = args.baseline_object.read_bytes()
    blob = str(ROOT / 'ref/slmodemd/dsplibs.o')
    results = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5933832353', 'revision': args.revision, 'config': config, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for context in ('toy', 'full'):
        for name, body in FORMS.items():
            cell = out / (context + '-' + name)
            cell.mkdir(parents=True, exist_ok=True)
            text = 'int polyValue(short k)\n{\n' + body + '\n}\n' if context == 'toy' else source.replace(FORMS['baseline'], body)
            (cell / 'V34hshak.c').write_text(text)
            directory = '/work/' + cell.name
            cmd = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
            entry = {'command': cmd, 'source_hash': hashlib.sha256(text.encode()).hexdigest()}
            results['cells'][cell.name] = entry
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
            assert entry['compile_exit'] == 0, cell.name
            obj = str(cell / 'V34hshak.o')
            entry['object_hash'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['polyValue'] = b.verdict(*b.body(blob, 'polyValue'), *b.body(obj, 'polyValue'))
            (cell / 'polyValue.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=polyValue', obj], text=True))
            if context == 'full':
                entry['verdicts'] = {sym: b.verdict(*b.body(blob, sym), *b.body(obj, sym)) for sym in sorted(set(b.sizes(blob)) & set(b.sizes(obj)))}
                entry['globals'] = {l.split()[-1]: l.split()[-2] for l in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
                if name == 'baseline':
                    assert Path(obj).read_bytes() == prior, 'unchanged control drift'
                    entry['baseline_reproduced'] = True
                else:
                    base = str(out / 'full-baseline/V34hshak.o')
                    entry['changed_bodies'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj, sym) != b.body(base, sym)]
                    entry['added_symbols'] = sorted(set(b.sizes(obj)) - set(b.sizes(base)))
                    entry['removed_symbols'] = sorted(set(b.sizes(base)) - set(b.sizes(obj)))
                    assert entry['globals'] == results['cells']['full-baseline']['globals']
            else:
                entry['header_hashes'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT / 'include/dsplib').glob('v34*.h')}
            print(cell.name, entry['polyValue'], flush=True)
            (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    for name in FORMS:
        assert b.body(str(out / ('toy-' + name) / 'V34hshak.o'), 'polyValue') == b.body(str(out / ('full-' + name) / 'V34hshak.o'), 'polyValue'), 'toy/full mismatch ' + name

if __name__ == '__main__':
    main()
