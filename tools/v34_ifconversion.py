#!/usr/bin/env python3
"""GCC if-conversion source/option controls: eight small and sixteen full-TU cells; no fuzz/mutation runs."""
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


OPTIONS = {
    'baseline': [],
    'no-ce1': ['-fno-if-conversion'],
    'no-ce2': ['-fno-if-conversion2'],
    'no-both': ['-fno-if-conversion', '-fno-if-conversion2'],
}


def rtl_counts(cell, source_name, symbol):
    counts = {}
    for stage in ('01.rtl', '12.bp', '14.ce1', '20.combine', '21.ce2', '29.ce3'):
        dump = cell / (source_name + '.' + stage)
        if not dump.exists():
            counts[stage] = None
            continue
        body = dump.read_text()
        start = body.index(';; Function ' + symbol + '\n')
        end = body.find(';; Function ', start + 15)
        body = body[start:end if end >= 0 else None]
        counts[stage] = len(re.findall(r'\(eq:\w+\s+\(reg:CC', body))
    return counts


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-root', type=Path, required=True)
    ap.add_argument('--retained-revision', default='464f3466', help='Source/header revision for the retained control')
    args = ap.parse_args()
    original = args.original_root
    config = (original / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    profile = ['--param', 'inline-unit-growth=100000', '--param', 'max-inline-insns-auto=100', '--param', 'max-inline-insns-single=1000000', '--param', 'large-function-insns=10000000', '--param', 'large-function-growth=100000']
    out = ROOT / 'build/v34-ifconversion'
    baseline_out = out
    out.mkdir(parents=True, exist_ok=True)
    blob = str(original / 'ref/slmodemd/dsplibs.o')
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5931550698', 'config': config, 'retained_revision': args.retained_revision, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)

    result['small_cells'] = {}
    for spelling in ('equality', 'explicit-if'):
        for option, extra in OPTIONS.items():
            name = 'small-' + spelling + '-' + option
            cell = out / name
            cell.mkdir(parents=True, exist_ok=True)
            tail = ('return p->runs == p->limit;' if spelling == 'equality'
                    else 'if (p->runs == p->limit) return 1; return 0;')
            source = 'struct state { short runs, limit; };\nint decision(const struct state *p) { ' + tail + ' }\n'
            (cell / 'small.c').write_text(source)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(directory) + ' && ' +
                tc.compile_shell(tc.GENTOO_COMPILER_PATH, flags + extra + ['-da'],
                                 directory + '/small.o', directory + '/small.c')]
            entry = {'command': cmd, 'source_sha256': hashlib.sha256(source.encode()).hexdigest()}
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            assert entry['compile_exit'] == 0
            obj = str(cell / 'small.o')
            entry['object_sha256'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['rtl_setcc_counts'] = rtl_counts(cell, 'small.c', 'decision')
            entry['final_sete_count'] = sum(row[0] == 'sete' for row in b.insns(obj, 'decision'))
            entry['instructions'] = b.insns(obj, 'decision')
            (cell / 'small.dis').write_text(subprocess.check_output(['objdump', '-dr', obj], text=True))
            result['small_cells'][name] = entry
            print(name, 'setcc=', entry['rtl_setcc_counts'], 'sete=', entry['final_sete_count'], flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    for regime in ('retained', 'diagnostic'):
        snapshot = ROOT if regime == 'retained' else ROOT / 'build/v34-retrain-stores/diagnostic-direct-stores'
        mount = ROOT if regime == 'retained' else original
        for spelling, option in [(s, o) for s in ('equality', 'explicit-if') for o in OPTIONS]:
            name = regime + '-' + spelling + '-' + option
            cell = out / name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            if regime == 'retained':
                source = subprocess.check_output(['git', 'show', args.retained_revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
                header = subprocess.check_output(['git', 'show', args.retained_revision + ':include/dsplib/v34hstx1_arms.h'], cwd=ROOT)
            else:
                source = (snapshot / 'V34hshak.c').read_text()
                header = (snapshot / 'include/dsplib/v34hstx1_arms.h').read_bytes()
            if spelling == 'explicit-if':
                old = '\treturn obj->retrain_runs == obj->retrain_tone_runs;'
                assert source.count(old) == 1
                source = source.replace(old, '\tif (obj->retrain_runs == obj->retrain_tone_runs)\n\t\treturn 1;\n\treturn 0;')
            (cell / 'V34hshak.c').write_text(source)
            (cell / 'include/dsplib/v34hstx1_arms.h').write_bytes(header)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, mount, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + directory + '/include'] + flags + (profile if regime == 'diagnostic' else []) + OPTIONS[option] + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
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
            if option == 'baseline':
                saved = 'direct-stores' if spelling == 'equality' else 'direct-stores-if'
                prior = ROOT / ('build/v34-retrain-stores/' + regime + '-' + saved + '/V34hshak.o')
                assert prior.read_bytes() == Path(obj).read_bytes(), name + ' baseline drift'
                entry['prior_object_byte_identical'] = True
            if name != regime + '-equality-baseline':
                base = str(baseline_out / (regime + '-equality-baseline') / 'V34hshak.o')
                entry['changed_body_or_relocation_records'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj, sym) != b.body(base, sym)]
                control = result['cells'][regime + '-equality-baseline']
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
            entry['rtl_setcc_counts'] = rtl_counts(cell, 'V34hshak.c', 'detectRetrainReq')
            entry['final_sete_count'] = sum(row[0] == 'sete' for row in b.insns(obj, 'detectRetrainReq'))
            entry['detector_grade1_rejection'] = b.alpha_why(b.insns(blob, 'detectRetrainReq'), b.insns(obj, 'detectRetrainReq'))
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


    result['interactions'] = {}
    for regime in ('retained', 'diagnostic'):
        first = str(out / (regime + '-equality-no-ce1') / 'V34hshak.o')
        both = str(out / (regime + '-equality-no-both') / 'V34hshak.o')
        result['interactions'][regime] = {
            'both_vs_no_if_conversion_changed_bodies':
                [sym for sym in sorted(set(b.sizes(first)) & set(b.sizes(both)))
                 if b.body(first, sym) != b.body(both, sym)]
        }
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
