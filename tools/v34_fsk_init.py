#!/usr/bin/env python3
"""Eight-cell V34 FSK clear/index and initialization-order controls; no fuzz/mutation runs."""
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
    ap.add_argument('--retained-revision', default='953bbb45', help='Source/header revision for the retained control')
    args = ap.parse_args()
    original = args.original_root
    config = (original / 'build/tc_out/.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if x == '-Iinclude' else '/src/' + x if x == 'tools/toolchain/period_compat.h' else x for x in flags]
    out = ROOT / 'build/v34-fsk-init'
    baseline_out = out
    out.mkdir(parents=True, exist_ok=True)
    blob = str(original / 'ref/slmodemd/dsplibs.o')
    result = {'domain': 'https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5932794082', 'config': config, 'retained_revision': args.retained_revision, 'cells': {}}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    for regime in ('retained',):
        mount = ROOT
        prior = ROOT / 'build/v34-dma-stores/retained-direct-scalar/V34hshak.o'
        for variant in ('baseline', 'retained-short-pointer', 'retained-short-byte-root', 'retained-short-word-root', 'blob-int-pointer', 'blob-short-pointer', 'blob-short-byte-root', 'blob-short-word-root'):
            name = regime + '-' + variant
            cell = out / name
            (cell / 'include/dsplib').mkdir(parents=True, exist_ok=True)
            source = subprocess.check_output(['git', 'show', args.retained_revision + ':src/pump/v34/V34hshak.c'], cwd=ROOT, text=True)
            header = subprocess.check_output(['git', 'show', args.retained_revision + ':include/dsplib/v34hstx1_arms.h'], cwd=ROOT)
            clear = '\tshort *p = (short *)((char *)obj + 0xaae6);\n\tint i;\n\n\tfor (i = 0; i <= 0x63; i++)\n\t\tp[i] = 0;'
            if variant != 'baseline':
                assert source.count(clear) == 1
                if variant.endswith('short-pointer'):
                    source = source.replace(clear, clear.replace('int i;', 'short i;'))
                elif variant.endswith('short-byte-root'):
                    source = source.replace(clear, '\tshort i;\n\n\tfor (i = 0; i <= 0x63; i++)\n\t\t*(short *)((char *)obj + 0xaae6 + 2 * i) = 0;')
                elif variant.endswith('short-word-root'):
                    source = source.replace(clear, '\tshort i;\n\n\tfor (i = 0; i <= 0x63; i++)\n\t\t((short *)obj)[0x5573 + i] = 0;')
                if variant.startswith('blob-'):
                    start = source.index('fsk_state_init(struct v34_object *obj)')
                    body_start = source.index('\n{', start) + 2
                    end = source.index('\n}', body_start)
                    order = ['delay', 'offset', 'phase', 'prev', 'sr', 'nbits', 'bit_lo', 'bit_hi', 'next', 'bit_len', 'resync_next']
                    lines = {re.search(r'obj->fsk\.(\w+)', line).group(1): line
                             for line in source[body_start:end].splitlines() if 'obj->fsk.' in line}
                    assert set(lines) == set(order)
                    source = source[:body_start] + '\n' + '\n'.join(lines[field] for field in order) + source[end:]
            (cell / 'V34hshak.c').write_text(source)
            (cell / 'include/dsplib/v34hstx1_arms.h').write_bytes(header)
            directory = '/work/' + name
            cmd = tc.docker_prefix(image, mount, out, True) + ['/bin/sh', '-c', 'cd ' + shlex.quote(directory) + ' && ' + tc.compile_shell(tc.GENTOO_COMPILER_PATH, ['-I' + directory + '/include'] + flags + ['-da'], directory + '/V34hshak.o', directory + '/V34hshak.c')]
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
            (cell / 'dpskDetectInfo1Init.dis').write_text(subprocess.check_output(['objdump', '-dr', '--disassemble=dpskDetectInfo1Init', obj], text=True))
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
            entry['fsk_grade1_rejection'] = b.alpha_why(b.insns(blob, 'dpskDetectInfo1Init'), b.insns(obj, 'dpskDetectInfo1Init'))
            print('fsk', entry['verdicts']['dpskDetectInfo1Init'], flush=True)
            print(name, 'globals=%d shared=%d exact=%d handshake=%s' % (len(entry['globals']), len(entry['verdicts']), sum(v[0] == 'EXACT' for v in entry['verdicts'].values()), entry['verdicts']['v34handshak']), flush=True)
            (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
