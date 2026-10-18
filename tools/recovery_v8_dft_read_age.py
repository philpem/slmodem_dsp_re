#!/usr/bin/env python3
"""Replay the V8 DFT sample cursor and post-phase read-age cross."""
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
SOURCE = 'src/v8/V8Dftc.c'
HEADERS = ('include/dsplib/v8.h', 'tools/toolchain/period_compat.h')
CELLS = [('baseline', False, False), ('cursor', True, False),
         ('post-phase', False, True), ('both', True, True)]


def pinned(path):
    return subprocess.check_output(['git', 'show', REV + ':' + path], cwd=ROOT, text=True)


def boundaries(path):
    instructions = b.insns(str(path), 'v8_dftupdate')
    samples = [i for i, (m, o) in enumerate(instructions)
               if m == 'movswl' and o.startswith('(') and '%esp' not in o]
    publications = [i for i, (m, o) in enumerate(instructions)
                    if m == 'mov' and re.fullmatch(r'%[a-z]{2},\(%e[a-z]{2}\)', o)]
    assert len(samples) == len(publications) == 1, (samples, publications)
    sample, publication = samples[0], publications[0]
    return {'phase_before_sample': publication < sample,
            'direct_sample': ',' not in instructions[sample][1].split(')')[0],
            'phase_index': publication, 'phase': instructions[publication],
            'sample_index': sample, 'sample': instructions[sample]}


def first_rtl(path):
    text = path.read_text()
    start = text.index(';; Function v8_dftupdate')
    end = text.find(';; Function ', start + 4)
    nodes = text[start:end if end >= 0 else None].split('\n\n')
    stores = [(i, n) for i, n in enumerate(nodes)
              if '(set (mem/s:HI' in n and '.phase+0' in n]
    samples = [(i, n) for i, n in enumerate(nodes)
               if '(mem:HI' in n and '[3 S2 A16]' in n]
    assert len(stores) == len(samples) == 1
    return {'phase_before_sample': stores[0][0] < samples[0][0],
            'phase_node': stores[0][0], 'sample_node': samples[0][0],
            'phase_RTL': stores[0][1], 'sample_RTL': samples[0][1]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=ROOT / 'build/v8-dft-boundary-baseline')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--check-adopted', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = pinned(SOURCE)
    headers = {p: pinned(p) for p in HEADERS}
    # Both period-visible headers are checked against the pinned inputs. v8.h
    # includes no further headers; the source has no local quoted dependency.
    for path, text in headers.items():
        assert text == (ROOT / path).read_text(), ('header drift', path)
    config = (args.objects / '.build-config').read_text()
    image = next(line[6:] for line in config.splitlines() if line.startswith('image '))
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/tools/toolchain/period_compat.h'
             if f == 'tools/toolchain/period_compat.h' else f for f in flags] + ['-da']
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    original = boundaries(b.BLOB)
    assert original['phase_before_sample'] and original['direct_sample']
    report = {'revision': REV, 'config': config, 'native': original,
              'header_hashes': {p: hashlib.sha256(t.encode()).hexdigest() for p, t in headers.items()},
              'pinned_source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'cells': {}}
    base = None
    base_rows = None
    for label, cursor, late in CELLS:
        text = source
        assert text.count('for (j = 0; j < nsamples; j++) {') == 1
        assert text.count('int x = samples[j];') == 1
        if cursor:
            text = text.replace('for (j = 0; j < nsamples; j++) {',
                                'for (j = 0; j < nsamples; j++, samples++) {')
            text = text.replace('int x = samples[j];', 'int x = *samples;')
        if late:
            expression = '*samples' if cursor else 'samples[j]'
            text = text.replace('int x = ' + expression + ';', 'int x;')
            store = '\t\t\tb->phase = (short)phase;'
            assert text.count(store) == 1
            text = text.replace(store, store + '\n\t\t\tx = ' + expression + ';')
        if args.check_adopted and label == 'both':
            assert text == (ROOT / SOURCE).read_text()
        directory = out / label
        directory.mkdir(exist_ok=True)
        (directory / 'V8Dftc.c').write_text(text)
        target = '/work/' + label
        command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                tc.GENTOO_COMPILER_PATH, flags, target + '/V8Dftc.o', target + '/V8Dftc.c')]
        with (directory / 'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        dependency_command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
            'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                tc.GENTOO_COMPILER_PATH, flags + ['-M'], target + '/dependencies.txt',
                target + '/V8Dftc.c')]
        with (directory / 'dependencies.log').open('w') as log:
            subprocess.run(dependency_command, stdout=log, stderr=subprocess.STDOUT, check=True)
        dependencies = (directory / 'dependencies.txt').read_text()
        resolved = dependencies.replace('\\\n', ' ').split(':', 1)[1].split()
        assert set(resolved) == {target + '/V8Dftc.c', '/src/include/dsplib/v8.h',
                                 '/src/tools/toolchain/period_compat.h'}, resolved
        obj = directory / 'V8Dftc.o'
        if label == 'baseline':
            assert obj.read_bytes() == (args.objects / 'src_v8_V8Dftc.c.o').read_bytes()
            base = obj
        names = b.sizes(str(obj))
        assert len(names) == 3 and names.keys() == b.sizes(str(base)).keys()
        metadata = pair_metadata({'V8Dftc': obj})
        assert metadata == pair_metadata({'V8Dftc': base})
        proof = boundaries(obj)
        rtl = first_rtl(directory / 'V8Dftc.c.01.rtl')
        assert proof['phase_before_sample'] == late and proof['direct_sample'] == cursor
        assert rtl['phase_before_sample'] == late
        rows = {n: {'bytes': len(b.body(str(obj), n)[0]),
                    'verdict': b.verdict(*b.body(b.BLOB, n), *b.body(str(obj), n)),
                    'relocations': b.body(str(obj), n)[1]}
                for n in names}
        if base_rows is None:
            base_rows = rows
        changes = [n for n in names if b.body(str(base), n) != b.body(str(obj), n)]
        assert set(changes) == (set() if label == 'baseline' else {'v8_dftupdate'})
        gains = [n for n in rows if rows[n]['verdict'][0] == 'EXACT'
                 and base_rows[n]['verdict'][0] != 'EXACT']
        losses = [n for n in rows if rows[n]['verdict'][0] != 'EXACT'
                  and base_rows[n]['verdict'][0] == 'EXACT']
        assert not gains and not losses
        report['cells'][label] = {'command': command, 'dependency_command': dependency_command,
                                 'resolved_inputs': resolved, 'dependency_text': dependencies,
                                 'rows': rows, 'changed_bodies': changes,
                                 'gains': gains, 'losses': losses, 'boundaries': proof,
                                 'first_RTL': rtl, 'metadata': metadata,
                                 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                                 'source_sha256': hashlib.sha256(text.encode()).hexdigest()}
        print(label, '3/3 audited; native read age', late, 'native direct sample', cursor, flush=True)
    report['adopted_source_checked'] = args.check_adopted
    (out / 'results.json').write_text(json.dumps(report, indent=2, default=json_value) + '\n')


if __name__ == '__main__':
    main()
