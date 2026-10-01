#!/usr/bin/env python3
"""Six-cell V34 DMA coefficient initializer source controls; no fuzz/mutation runs."""
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
    ap.add_argument('--retained-revision', default='cfd8e7c9', help='Source/header revision for the retained control')
    args = ap.parse_args()
    original = args.original_root
    config = (original / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    profile = ['--param', 'inline-unit-growth=100000', '--param', 'max-inline-insns-auto=100', '--param', 'max-inline-insns-single=1000000', '--param', 'large-function-insns=10000000', '--param', 'large-function-growth=100000']
    out = ROOT / 'build/v34-dma-stores'
    baseline_out = out
    out.mkdir(parents=True, exist_ok=True)
    blob = str(original / 'ref/slmodemd/dsplibs.o')
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5932245374', 'config': config, 'retained_revision': args.retained_revision, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for regime in ('retained', 'diagnostic'):
        snapshot = ROOT if regime == 'retained' else ROOT / 'build/v34-retrain-stores/diagnostic-direct-stores'
        mount = ROOT if regime == 'retained' else original
        prior = ROOT / ('build/v34-retrain-stores/' + regime + '-direct-stores/V34hshak.o')
        for variant in ('baseline', 'expanded-cached', 'direct-scalar'):
            name = regime + '-' + variant
            cell = out / name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            if regime == 'retained':
                source = subprocess.check_output(['git', 'show', args.retained_revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
                header = subprocess.check_output(['git', 'show', args.retained_revision + ':include/dsplib/v34hstx1_arms.h'], cwd=ROOT)
            else:
                source = (snapshot / 'V34hshak.c').read_text()
                header = (snapshot / 'include/dsplib/v34hstx1_arms.h').read_bytes()
            if variant != 'baseline':
                start = source.index('void\ntxrxdmainit(')
                end = source.index('\n}\n', start) + 3
                signature = 'void\ntxrxdmainit(short *dst, const short *src)\n{\n'
                lines = []
                for i in range(3):
                    if variant == 'expanded-cached':
                        lines += ['\t{', '\t\tint re = (unsigned short)src[%d];' % (2+i*2),
                                  '\t\tint im = (unsigned short)src[%d];' % (3+i*2),
                                  '\t\tdst[%d] = (short)re;' % (i*2),
                                  '\t\tdst[%d] = (short)-im;' % (i*2+1),
                                  '\t\tdst[%d] = (short)im;' % (6+i*2),
                                  '\t\tdst[%d] = (short)re;' % (7+i*2), '\t}']
                    else:
                        lines += ['\tdst[%d] = dst[%d] = src[%d];' % (i*2, 7+i*2, 2+i*2),
                                  '\tdst[%d] = (short)-src[%d];' % (i*2+1, 3+i*2),
                                  '\tdst[%d] = src[%d];' % (6+i*2, 3+i*2)]
                body = signature + '\n'.join(lines) + '\n}\n'
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
            (cell / 'txrxdmainit.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=txrxdmainit', obj], text=True))
            (cell / 'v34handshak.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=v34handshak', obj], text=True))
            if variant == 'baseline':
                assert prior.read_bytes() == Path(obj).read_bytes(), name + ' baseline drift'
                entry['prior_object_byte_identical'] = True
            else:
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
            entry['dma_grade1_rejection'] = b.alpha_why(b.insns(blob, 'txrxdmainit'), b.insns(obj, 'txrxdmainit'))

            if variant == 'direct-scalar':
                def start_address(path):
                    lines = subprocess.check_output(['nm', path], text=True).splitlines()
                    return int(next(line.split()[0] for line in lines
                                    if line.split()[-1] == 'v34handshak'), 16)
                base = str(out / (regime + '-baseline') / 'V34hshak.o')
                old_rows, new_rows = b.insns(base, 'v34handshak'), b.insns(obj, 'v34handshak')
                assert len(old_rows) == len(new_rows)
                changes = [(x, y) for x, y in zip(old_rows, new_rows) if x != y]
                old_start, new_start = start_address(base), start_address(obj)
                assert all(x[0] == y[0] == 'call' and x[1].startswith('.-')
                           and y[1].startswith('.-')
                           and old_start + int(x[1][1:])
                           == new_start + int(y[1][1:]) for x, y in changes)
                entry['handshake_layout_only_changes'] = {
                    'function_start_shift_bytes': new_start - old_start,
                    'changed_relative_calls_same_absolute_target': len(changes),
                    'other_instruction_operand_changes': 0,
                }

            print('dma', entry['verdicts']['txrxdmainit'], entry['dma_grade1_rejection'], flush=True)
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
