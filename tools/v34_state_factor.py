#!/usr/bin/env python3
"""Six-cell V34 comparison/logging source-factor control; no fuzz/mutation runs."""
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
    ap.add_argument('--retained-revision', default='a35ed924', help='Source/header revision for the retained control')
    ap.add_argument('--direct-load-domain', action='store_true', help='Run the four direct comparison-load controls after the six-cell domain')
    args = ap.parse_args()
    original = args.original_root
    config = (original / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    profile = ['--param', 'inline-unit-growth=100000', '--param', 'max-inline-insns-auto=100', '--param', 'max-inline-insns-single=1000000', '--param', 'large-function-insns=10000000', '--param', 'large-function-growth=100000']
    baseline_out = ROOT / 'build/v34-state-factor'
    out = ROOT / ('build/v34-state-load' if args.direct_load_domain else 'build/v34-state-factor')
    out.mkdir(parents=True, exist_ok=True)
    blob = str(original / 'ref/slmodemd/dsplibs.o')
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929650860', 'config': config, 'retained_revision': args.retained_revision, 'cells': {}}
    if args.direct_load_domain:
        result['domain'] = 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5929689120'
        baseline_result = json.loads((baseline_out / 'results.json').read_text())
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for regime in ('retained', 'diagnostic'):
        snapshot = ROOT if regime == 'retained' else original / 'build/frontier-v34-audit/ternary/horner'
        mount = ROOT if regime == 'retained' else original
        prior = ROOT / ('build/v34-flag-field/' + regime + '-field/V34hshak.o')
        for variant in (('direct-signed', 'direct-unsigned') if args.direct_load_domain else ('baseline', 'reread', 'debug-local')):
            name = regime + '-' + variant
            cell = out / name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            if regime == 'retained':
                source = subprocess.check_output(['git', 'show', args.retained_revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
                header = subprocess.check_output(['git', 'show', args.retained_revision + ':include/dsplib/v34hstx1_arms.h'], cwd=ROOT)
            else:
                source = (snapshot / 'src/pump/v34/V34hshak.c').read_text()
                header = (snapshot / 'include/dsplib/v34hstx1_arms.h').read_bytes()
            if regime == 'diagnostic':
                old = 'T3M_I16(&frame, T3M_F3588) =\n\t\t\t(short)(T3M_U16(&frame, T3M_F3588) | 2);'
                assert source.count(old) == 1
                source = source.replace(old, 'obj->short_3588 |= 2;')
            header_text = header.decode()
            old = '\tshort now = hs_get(obj, off);\n\n\tif (now == next)'
            assert header_text.count(old) == 1
            if variant != 'baseline':
                header_text = header_text.replace(old, '\tif (hs_get(obj, off) == next)')
                if variant == 'reread':
                    assert header_text.count('StateName[now]') == 1
                    header_text = header_text.replace('StateName[now]', 'StateName[hs_get(obj, off)]')
                else:
                    start = header_text.index('hs_setstate(struct v34_object *obj')
                    position = header_text.index('\tif (DSPLIB_DEBUG_ON()) {', start) + len('\tif (DSPLIB_DEBUG_ON()) {')
                    header_text = header_text[:position] + '\n\t\tshort now = hs_get(obj, off);' + header_text[position:]
            if variant.startswith('direct-'):
                predicate = '*(const short *)((const char *)obj + off) == next' if variant == 'direct-signed' else '*(const unsigned short *)((const char *)obj + off) == (unsigned short)next'
                assert header_text.count('if (hs_get(obj, off) == next)') == 1
                header_text = header_text.replace('if (hs_get(obj, off) == next)', 'if (' + predicate + ')')
            header = header_text.encode()
            (cell / 'V34hshak.c').write_text(source)
            (cell / 'include/dsplib/v34hstx1_arms.h').write_bytes(header)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, mount, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + directory + '/include'] + flags + (profile if regime == 'diagnostic' else []) + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
            entry = {'command': cmd, 'source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'header_sha256': hashlib.sha256((cell / 'include/dsplib/v34hstx1_arms.h').read_bytes()).hexdigest()}
            result['cells'][name] = entry
            with (cell / 'compile.log').open('w') as log:
                entry['compile_exit'] = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
            assert entry['compile_exit'] == 0, name
            obj = str(cell / 'V34hshak.o')
            entry['object_sha256'] = hashlib.sha256(Path(obj).read_bytes()).hexdigest()
            entry['globals'] = {l.split()[-1]: l.split()[-2] for l in subprocess.check_output(['nm', '-g', '--defined-only', obj], text=True).splitlines()}
            entry['verdicts'] = {}
            for sym in sorted(set(b.sizes(obj)) & set(b.sizes(blob))):
                a, ar = b.body(blob, sym); c, cr = b.body(obj, sym)
                entry['verdicts'][sym] = b.verdict(a, ar, c, cr)
            (cell / 'v34handshak.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=v34handshak', obj], text=True))
            if variant == 'baseline':
                assert prior.read_bytes() == Path(obj).read_bytes(), name + ' baseline drift'
                entry['prior_object_byte_identical'] = True
            else:
                base = str(baseline_out / (regime + '-baseline') / 'V34hshak.o')
                entry['changed_body_or_relocation_records'] = [sym for sym in sorted(set(b.sizes(obj)) & set(b.sizes(base))) if b.body(obj, sym) != b.body(base, sym)]
                control = baseline_result['cells'][regime + '-baseline'] if args.direct_load_domain else result['cells'][regime + '-baseline']
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
                if args.direct_load_domain:
                    assert control['prior_object_byte_identical']
                    assert hashlib.sha256(Path(base).read_bytes()).hexdigest() == control['object_sha256']
                    entry['validated_baseline_sha256'] = control['object_sha256']
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
