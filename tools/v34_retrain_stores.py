#!/usr/bin/env python3
"""Eight-cell V34 retrain quiet-store and return controls; no fuzz/mutation runs."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import shlex
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-root', type=Path, required=True)
    ap.add_argument('--retained-revision', default='3849cdfe', help='Source/header revision for the retained control')
    args = ap.parse_args()
    original = args.original_root
    config = (original / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    profile = ['--param', 'inline-unit-growth=100000', '--param', 'max-inline-insns-auto=100', '--param', 'max-inline-insns-single=1000000', '--param', 'large-function-insns=10000000', '--param', 'large-function-growth=100000']
    out = ROOT / 'build/v34-retrain-stores'
    baseline_out = out
    out.mkdir(parents=True, exist_ok=True)
    blob = str(original / 'ref/slmodemd/dsplibs.o')
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5930293342', 'config': config, 'retained_revision': args.retained_revision, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for regime in ('retained', 'diagnostic'):
        snapshot = ROOT if regime == 'retained' else ROOT / 'build/v34-retrain-switch/diagnostic-switch'
        mount = ROOT if regime == 'retained' else original
        prior = ROOT / ('build/v34-retrain-switch/' + regime + '-switch/V34hshak.o')
        for variant in ('baseline', 'explicit-if', 'direct-stores', 'direct-stores-if'):
            name = regime + '-' + variant
            cell = out / name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            if regime == 'retained':
                source = subprocess.check_output(['git', 'show', args.retained_revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
                header = subprocess.check_output(['git', 'show', args.retained_revision + ':include/dsplib/v34hstx1_arms.h'], cwd=ROOT)
            else:
                source = (snapshot / 'V34hshak.c').read_text()
                header = (snapshot / 'include/dsplib/v34hstx1_arms.h').read_bytes()
            if variant in ('explicit-if', 'direct-stores-if'):
                old = '\treturn obj->retrain_runs == obj->retrain_tone_runs;'
                assert source.count(old) == 1
                source = source.replace(old, '\tif (obj->retrain_runs == obj->retrain_tone_runs)\n\t\treturn 1;\n\treturn 0;')
            if variant in ('direct-stores', 'direct-stores-if'):
                start = source.index('int\ndetectRetrainReq(')
                end = source.index('\n}\n', start) + 3
                body = source[start:end]
                replacements = {
                    '\tshort runs;\n': '',
                    '\t\t\truns = (short)(obj->retrain_runs + 1);': '\t\t\tobj->retrain_runs = (short)(obj->retrain_runs + 1);',
                    '\t\t\truns = 0;': '\t\t\tobj->retrain_runs = 0;',
                    '\t\tobj->retrain_runs = runs;\n': '',
                }
                for old, new in replacements.items():
                    assert body.count(old) == 1, old
                    body = body.replace(old, new)
                source = source[:start] + body + source[end:]
            (cell / 'V34hshak.c').write_text(source)
            (cell / 'include/dsplib/v34hstx1_arms.h').write_bytes(header)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, mount, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + directory + '/include'] + flags + (profile if regime == 'diagnostic' else []) + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
            entry = {'command': cmd, 'source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'header_sha256': hashlib.sha256((cell / 'include/dsplib/v34hstx1_arms.h').read_bytes()).hexdigest()}
            result['cells'][name] = entry
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
            assert entry['compile_exit'] == 0, name
            obj = str(cell / 'V34hshak.o')
            entry['object_sha256'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals'] = {l.split()[-1]: l.split()[-2] for l in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
            entry['verdicts'] = {}
            for sym in sorted(set(b.sizes(obj)) & set(b.sizes(blob))):
                a, ar = b.body(blob, sym); c, cr = b.body(obj, sym)
                entry['verdicts'][sym] = b.verdict(a, ar, c, cr)
            (cell / 'detectRetrainReq.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=detectRetrainReq', obj], text=True))
            (cell / 'v34handshak.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=v34handshak', obj], text=True))
            if variant in ('baseline', 'explicit-if'):
                prior = ROOT / ('build/v34-retrain-return/' + name + '/V34hshak.o')
                assert prior.read_bytes() == Path(obj).read_bytes(), name + ' baseline drift'
                entry['prior_object_byte_identical'] = True
            if variant != 'baseline':
                base = str(baseline_out / (regime + '-baseline') / 'V34hshak.o')
                entry['changed_body_or_relocation_records'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj, sym) != b.body(base, sym)]
                control = result['cells'][regime + '-baseline']
                all_base = set(b.sizes(base))
                all_new = set(b.sizes(obj))
                entry['added_function_symbols'] = sorted(all_new - all_base)
                entry['removed_function_symbols'] = sorted(all_base - all_new)
                def pc32_counts(path):
                    dis = subprocess.check_output(['objdump', '-dr', '--disassemble=v34handshak', path], text=True)
                    return collections.Counter(re.findall(r'R_386_PC32\s+(\S+)', dis))
                base_calls, new_calls = pc32_counts(base), pc32_counts(obj)
                entry['handshake_pc32_target_count_changes'] = {k: [base_calls[k], new_calls[k]] for k in sorted(base_calls.keys() | new_calls.keys()) if base_calls[k] != new_calls[k]}
                assert entry['globals'] == control['globals']
            if variant in ('direct-stores', 'direct-stores-if'):
                old_variant = 'baseline' if variant == 'direct-stores' else 'explicit-if'
                old_obj = str(out / (regime + '-' + old_variant) / 'V34hshak.o')
                before = b.insns(old_obj, 'detectRetrainReq')
                after = b.insns(obj, 'detectRetrainReq')
                assert before[80][0] == 'cwtl'
                before = before[:80] + before[81:]
                assert len(before) == len(after)
                differences = [(x, y) for x, y in zip(before, after) if x != y]
                assert all(x[0] == y[0] and x[0].startswith('j')
                           and x[1].startswith('.+') and y[1].startswith('.+')
                           for x, y in differences), differences
                entry['detector_removed_instruction'] = 'cwtl at original instruction index 80 (zero-based)'
                entry['remaining_detector_differences_only_shifted_branches'] = len(differences)
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
