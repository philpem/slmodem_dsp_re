#!/usr/bin/env python3
"""Replay the bounded issue246 GCC3 allocation/reload diagnostic domain."""
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path
import experiment_toolchain as tc
from gcc3_reload_trace import trace

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', required=True, type=Path,
                        help='validated production tc_out containing saved .build-config')
    args = parser.parse_args()
    out = ROOT/'build/gcc3-reload-reproduction'
    out.mkdir(parents=True, exist_ok=True)
    config = (args.baseline_dir/'.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/'+f if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    source = (ROOT/'src/pump/v34/V34TX.c').read_text()
    start = source.index('\nvoid\nupdateAlpha(')+1
    end = source.index('\n}\n', start)+2
    small = '#include "dsplib/debug.h"\n'+source[start:end]+'\n'
    quiet = small.replace('if (DSPLIB_DEBUG_ON())', 'if (0)')
    assert quiet != small
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    cases = [('full-plain', source, []), ('full-rtl', source, ['-da']),
             ('tree-capability', source, ['-fdump-tree-all']),
             ('extracted-plain', small, []), ('extracted-rtl', small, ['-da']),
             ('quiet-plain', quiet, []), ('quiet-rtl', quiet, ['-da'])]
    results = {'config': config, 'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), 'cells': {}}
    for label, text, diagnostics in cases:
        folder = out/label
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(exist_ok=True)
        (folder/'V34TX.c').write_text(text)
        target = '/work/'+label
        shell = 'cd '+shlex.quote(target)+' && '+tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags+diagnostics, target+'/candidate.o', target+'/V34TX.c')
        command = tc.docker_prefix(image, ROOT, out, True)+['/bin/sh', '-c', shell]
        with (folder/'compile.log').open('w') as log:
            code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        row = {'command': command, 'compile_exit': code, 'source_sha256': hashlib.sha256(text.encode()).hexdigest()}
        results['cells'][label] = row
        if code == 0:
            row['object_sha256'] = hashlib.sha256((folder/'candidate.o').read_bytes()).hexdigest()
            if '-da' in diagnostics:
                row['trace'] = trace((folder/'V34TX.c.24.lreg').read_text(), (folder/'V34TX.c.25.greg').read_text(), 'updateAlpha')
            row['dumps'] = sorted(p.name for p in folder.glob('V34TX.c.*'))
        else:
            row['validity'] = 'capability refusal' if label == 'tree-capability' else 'INVALID compile'
        (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
        print(label, 'compile', code, 'stack substitutions', len(row.get('trace', {}).get('stack_substitutions', [])), flush=True)
        if label != 'tree-capability':
            assert code == 0, label
        else:
            assert code != 0 and 'unrecognized command line option "-fdump-tree-all"' in (folder/'compile.log').read_text(), 'unexpected tree capability result'
    retained = args.baseline_dir/'src_pump_v34_V34TX.c.o'
    assert (out/'full-plain/candidate.o').read_bytes() == retained.read_bytes(), 'production baseline drift'
    for prefix in ('full', 'extracted', 'quiet'):
        assert (out/(prefix+'-plain')/'candidate.o').read_bytes() == (out/(prefix+'-rtl')/'candidate.o').read_bytes(), prefix+' diagnostic drift'
        observed = results['cells'][prefix+'-rtl']['trace']
        offsets = (28, 24) if prefix != 'quiet' else (4, 0)
        assert {(r['pseudo'], r['home']['offset']) for r in observed['stack_substitutions']} == {(73, offsets[0]), (77, offsets[1])}, prefix+' spill graph drift'
        assert observed['local_assignments'][73] == observed['local_assignments'][77] == 1, prefix+' local assignment drift'
        assert not {73, 77} & set(observed['global_allocation_order']), prefix+' allocation stage drift'
        assert not any(r['pseudo'] == 67 for r in observed['stack_substitutions']), prefix+' named quotient negative control'
    results['controls'] = {'raw_production': True, 'diagnostic_pairs': 3,
                           'real_spill_graphs': 3, 'resident_quotient_negatives': 3,
                           'valid_compiles': 6, 'capability_refusals': 1}
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print('7 compile attempts: 6 valid, 1 capability refusal; production raw match; '
          '3/3 diagnostic pairs raw match; 3/3 spill graphs and 3/3 resident quotient controls pass')


if __name__ == '__main__':
    main()
