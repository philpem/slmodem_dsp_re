#!/usr/bin/env python3
"""Replay native tone-filter ring mask and old-count word tests."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
_paths = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT / 'tools']
import dis
sys.path[:] = _paths
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
from recovery_b103_helper_boundary import pair_metadata, json_value
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b

REV = '94caa17be'
SOURCE = 'src/dsp/fpm_tone.c'
HEADERS = ('include/dsplib/fpm_tone.h', 'include/dsplib/fpm_phasor.h',
           'include/dsplib/fpm_iir.h', 'include/dsplib/sysdep.h',
           'tools/toolchain/period_compat.h')
CELLS = [('baseline', False, False), ('ring', True, False),
         ('count', False, True), ('both', True, True)]


def pinned(path):
    return subprocess.check_output(['git', 'show', REV + ':' + path], cwd=ROOT, text=True)


def boundaries(path):
    instructions = b.insns(str(path), 'FPM_TONE_filter')
    branch = next(i for i, (m, o) in enumerate(instructions) if m.startswith('j'))
    word_inc = [row for row in instructions[:branch]
                if row[0] == 'inc' and re.fullmatch(r'%[a-z]{2}', row[1])]
    mask = [row for row in instructions if row[0] in ('setl', 'neg', 'and')]
    if mask:
        assert [row[0] for row in mask] == ['setl', 'neg', 'and']
    return {'word_old_count_test': bool(word_inc), 'word_increments': word_inc,
            'ring_mask': bool(mask), 'mask_operations': mask,
            'entry_branch': instructions[branch]}


def ce1(path):
    text = path.read_text()
    start = text.index(';; Function FPM_TONE_filter')
    end = text.find(';; Function ', start + 4)
    body = text[start:end if end >= 0 else None]
    converted = re.findall(r'(\d+) IF blocks converted\.', body)
    assert len(converted) == 1, converted
    return int(converted[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=ROOT / 'build/tone-ring-count-baseline')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--check-adopted', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = pinned(SOURCE)
    headers = {p: pinned(p) for p in HEADERS}
    # Check intended inputs, then independently prove actual compiler resolution.
    for path, text in headers.items():
        assert text == (ROOT / path).read_text(), ('header drift', path)
    config = (args.objects / '.build-config').read_text()
    image = next(line[6:] for line in config.splitlines() if line.startswith('image '))
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/tools/toolchain/period_compat.h'
             if f == 'tools/toolchain/period_compat.h' else f for f in flags] + ['-da']
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    original = boundaries(b.BLOB)
    assert original['word_old_count_test'] and original['ring_mask']
    report = {'revision': REV, 'config': config, 'native': original,
              'header_hashes': {p: hashlib.sha256(t.encode()).hexdigest() for p, t in headers.items()},
              'pinned_source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'cells': {}}
    base = None
    base_rows = None
    start = source.index('void\nFPM_TONE_filter(')
    end = source.index('\n}\n', start) + 2
    original_body = source[start:end]
    for label, ring, count in CELLS:
        body = original_body
        if ring:
            assert body.count('int taps = state->cfg.len;') == 1
            body = body.replace('int taps = state->cfg.len;', 'short taps = state->cfg.len;')
            old = 'idx = ((short)(idx + 1) < taps) ? (short)(idx + 1) : 0;'
            assert body.count(old) == 1
            body = body.replace(old, 'idx = (short)(idx + 1);\n\t\tif ((short)idx >= taps)\n\t\t\tidx = 0;')
        if count:
            assert body.count('\tint i;') == 1
            body = body.replace('\tint i;', '\tshort i;')
            old = 'for (i = (short)(count - 1); i != -1; i = (short)(i - 1))'
            assert body.count(old) == 1
            body = body.replace(old, 'for (i = count; i-- != 0;)')
        text = source[:start] + body + source[end:]
        if args.check_adopted and label == 'both':
            assert text == (ROOT / SOURCE).read_text()
        directory = out / label
        directory.mkdir(exist_ok=True)
        (directory / 'fpm_tone.c').write_text(text)
        target = '/work/' + label
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                tc.GENTOO_COMPILER_PATH, flags, target + '/fpm_tone.o', target + '/fpm_tone.c')]
        with (directory / 'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        dependency_command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                tc.GENTOO_COMPILER_PATH, flags + ['-M'], target + '/dependencies.txt',
                target + '/fpm_tone.c')]
        with (directory / 'dependencies.log').open('w') as log:
            subprocess.run(dependency_command, stdout=log, stderr=subprocess.STDOUT, check=True)
        dependencies = (directory / 'dependencies.txt').read_text()
        resolved = dependencies.replace('\\\n', ' ').split(':', 1)[1].split()
        repo_dependencies = {p for p in resolved if p.startswith('/src/')}
        assert repo_dependencies == {'/src/' + p for p in HEADERS}, resolved
        assert target + '/fpm_tone.c' in resolved
        assert all(p.startswith(('/src/', '/usr/lib/gcc/', target + '/')) for p in resolved)
        obj = directory / 'fpm_tone.o'
        if label == 'baseline':
            assert obj.read_bytes() == (args.objects / 'src_dsp_fpm_tone.c.o').read_bytes()
            base = obj
        names = b.sizes(str(obj))
        assert len(names) == 11 and names.keys() == b.sizes(str(base)).keys()
        metadata = pair_metadata({'fpm_tone': obj})
        assert metadata == pair_metadata({'fpm_tone': base})
        proof = boundaries(obj)
        converted = ce1(directory / 'fpm_tone.c.14.ce1')
        assert proof['word_old_count_test'] == count
        assert proof['ring_mask'] == ring and converted == int(ring)
        rows = {n: {'bytes': len(b.body(str(obj), n)[0]),
                    'verdict': b.verdict(*b.body(b.BLOB, n), *b.body(str(obj), n)),
                    'relocations': b.body(str(obj), n)[1]}
                for n in names}
        if base_rows is None:
            base_rows = rows
        changes = [n for n in names if b.body(str(base), n) != b.body(str(obj), n)]
        assert set(changes) == (set() if label == 'baseline' else {'FPM_TONE_filter'})
        gains = [n for n in rows if rows[n]['verdict'][0] == 'EXACT'
                 and base_rows[n]['verdict'][0] != 'EXACT']
        losses = [n for n in rows if rows[n]['verdict'][0] != 'EXACT'
                  and base_rows[n]['verdict'][0] == 'EXACT']
        assert not gains and not losses
        report['cells'][label] = {'command': command, 'dependency_command': dependency_command,
                                 'resolved_inputs': resolved, 'dependency_text': dependencies,
                                 'rows': rows, 'changed_bodies': changes,
                                 'gains': gains, 'losses': losses, 'boundaries': proof,
                                 'ce1_converted_blocks': converted, 'metadata': metadata,
                                 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                                 'source_sha256': hashlib.sha256(text.encode()).hexdigest()}
        print(label, '11/11 audited; native ring mask', ring, 'native word old-count', count, flush=True)
    report['adopted_source_checked'] = args.check_adopted
    (out / 'results.json').write_text(json.dumps(report, indent=2, default=json_value) + '\n')


if __name__ == '__main__':
    main()
