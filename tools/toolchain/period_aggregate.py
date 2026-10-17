#!/usr/bin/env python3
"""Prepare canonical source-only input order for opt-in period aggregate fixtures."""
import argparse
import dis  # tools/dis.py must not shadow the standard module during imports
import hashlib
import json
from pathlib import Path
import sys
import recoverorder
import tuattrib


def mode(test, default):
    if default != 'fixture': return default
    path = Path('test/unit') / (test + '.ldmode')
    value = path.read_text().strip() if path.exists() else 'individual'
    if value not in ('individual', 'aggregate'): raise ValueError(f'{path}: invalid link mode {value!r}')
    return value


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--blob', type=Path, required=True)
    p.add_argument('--tests', required=True)
    p.add_argument('--mode', choices=('fixture', 'individual', 'aggregate'), default='fixture')
    a = p.parse_args()
    modes = {test:mode(test, a.mode) for test in a.tests.split()}
    if 'aggregate' not in modes.values(): return
    manifest = a.out / 'srcmanifest.list'
    rows = recoverorder.inputs(manifest)
    raw_attrib = tuattrib.build(str(a.blob))['attrib']
    attribution = {name:{'tu':value[0], 'how':value[1]} for name,value in raw_attrib.items()}
    order = recoverorder.recover(rows, a.out, recoverorder.reference(a.blob), attribution)
    assert len(order) == len(rows) and len({r['object'] for r in order}) == len(rows)
    paths, hashes = [], {}
    for row in order:
        obj = row['object']
        path = a.out / 'testhost' / obj
        if not path.exists(): path = a.out / obj
        assert path.is_file(), path
        paths.append('/out/' + path.relative_to(a.out).as_posix())
        hashes[paths[-1]] = hashlib.sha256(path.read_bytes()).hexdigest()
    payload = {'reference_sha256':hashlib.sha256(a.blob.read_bytes()).hexdigest(),
               'ordered_input_hashes':[(path, hashes[path]) for path in paths],
               'apparatus_hashes':{str(path):hashlib.sha256(path.read_bytes()).hexdigest()
                   for path in (Path(__file__), Path(recoverorder.__file__), Path(tuattrib.__file__))}}
    key = hashlib.sha256(json.dumps(payload, sort_keys=True).encode()).hexdigest()
    (a.out / 'aggregate-order.list').write_text('\n'.join(paths) + '\n')
    (a.out / 'aggregate-inputs.key').write_text(key + '\n')
    (a.out / 'aggregate-plan.json').write_text(json.dumps({'modes':modes, 'key':key, 'inputs':len(paths),
        'order':[{k:v for k,v in row.items() if k != 'key'} for row in order], **payload}, indent=2) + '\n')
    print(f'period aggregate: {len(paths)} canonical source inputs; {sum(m == "aggregate" for m in modes.values())} selected fixtures')


if __name__ == '__main__': main()
