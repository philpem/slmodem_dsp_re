#!/usr/bin/env python3
"""Reproduce issue #25's seven-cell CID ownership/conditional control.

Sources are pinned to the pre-correction revision, not the current checkout.
Use an output directory outside the source tree; all generated inputs,
objects, compiler dumps and the result ledger are retained there.
"""
import sys
_script_path = sys.path.pop(0)
import dis  # Load stdlib dis before tools/dis.py can shadow it.
sys.path.insert(0, _script_path)
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as bi
import experiment_toolchain as tc

BASE = '286d7126d791addb4b1d503a723a006679297e5b'
IMAGE = 'ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest'
COMPILER = '/usr/i386-pc-linux-gnu/gcc-bin/3.4'
FLAGS = shlex.split('-O3 -frename-registers -march=i386 -mtune=i686 '
                    '-mfpmath=387 -mno-ieee-fp -fomit-frame-pointer '
                    '-maccumulate-outgoing-args -I/src/include '
                    '-D__SIZEOF_POINTER__=4 '
                    '-include /src/tools/toolchain/period_compat.h')


def original(path):
    return subprocess.check_output(['git', 'show', BASE + ':' + path],
                                   cwd=ROOT, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--dumps', action='store_true',
                        help='also retain GCC RTL dumps and assembly')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    print('Pinned sources:', BASE, flush=True)
    print('Header checkout:', subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), flush=True)
    rx, dt = (original('src/service/' + name + '.c')
              for name in ('Rxcid', 'Dtmf_Rx'))
    cut = dt.index('/*\n * The 9:10 interpolating prototype')
    block, prefix = dt[cut:], dt[:cut]
    insert = rx.index('/*\n * Construct one.')
    old = 'FPM_MRF_init(&cid->mrf, &cfg, cid->mrf.history == NULL);'
    assert block.count(old) == 1
    forms = {'boolean': old}
    for label, predicate, yes, no in (
            ('conditional', '==', 1, 0), ('nonnull', '!=', 0, 1)):
        forms[label] = ('if (cid->mrf.history ' + predicate + ' NULL)\n'
                        '\t\tFPM_MRF_init(&cid->mrf, &cfg, %d);\n'
                        '\telse\n\t\tFPM_MRF_init(&cid->mrf, &cfg, %d);'
                        ) % (yes, no)
    cells = [('split-baseline', rx, dt)]
    for position in ('before', 'after'):
        for form, replacement in forms.items():
            moved = block.replace(old, replacement)
            source = (rx[:insert] + moved + '\n' + rx[insert:]
                      if position == 'before' else rx + '\n' + moved)
            cells.append((position + '-' + form, source, prefix))
    tc.print_identity(IMAGE, COMPILER, True)
    reference = str(ROOT / 'ref/slmodemd/dsplibs.o')
    print('Reference SHA256:', hashlib.sha256(Path(reference).read_bytes()).hexdigest(),
          flush=True)
    flags = FLAGS + (['-da', '-save-temps'] if args.dumps else [])
    rows = []
    baseline_bodies = {}
    for name, r, d in cells:
        dest = out / name
        dest.mkdir(exist_ok=True)
        row = {'cell': name, 'objects': {}, 'functions': {}}
        for stem, text in (('Rxcid', r), ('Dtmf_Rx', d)):
            source, obj = dest / (stem + '.c'), dest / (stem + '.o')
            source.write_text(text)
            shell = tc.compile_shell(COMPILER, flags,
                                     '/work/' + str(obj.relative_to(out)),
                                     '/work/' + str(source.relative_to(out)))
            command = tc.docker_prefix(IMAGE, ROOT, out, True)
            command[-1:-1] = ['-w', '/work/' + name]
            command += ['/bin/sh', '-c', shell]
            print(shlex.join(command), flush=True)
            subprocess.run(command, check=True, cwd=ROOT)
            row['objects'][stem] = {
                'sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                'symbols': sorted(bi.sizes(str(obj)))}
            for symbol in sorted(bi.sizes(str(obj))):
                raw, rel = bi.body(str(obj), symbol)
                if name == 'split-baseline':
                    baseline_bodies[symbol] = (raw, rel)
                a, ar = bi.body(reference, symbol)
                grade, difference = bi.verdict(a, ar, raw, rel)
                if grade not in ('EXACT', 'UNRESOLVED', 'NODATA', 'RELOC'):
                    if bi.alpha_equal(bi.insns(reference, symbol),
                                      bi.insns(str(obj), symbol)):
                        grade = 'REGALLOC'
                row['functions'][symbol] = {
                    'grade': grade, 'bytes': len(raw),
                    'reference_bytes': len(a), 'difference': difference,
                    'baseline_body_same': (raw, rel) == baseline_bodies[symbol],
                    'body_sha256': bi.body_digest(raw, rel)}
        rows.append(row)
        print(json.dumps(row, sort_keys=True), flush=True)
    (out / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('CID ownership control: %d cells, %d objects, %d function verdicts'
          % (len(rows), sum(len(r['objects']) for r in rows),
             sum(len(r['functions']) for r in rows)))


if __name__ == '__main__':
    main()
