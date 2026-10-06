#!/usr/bin/env python3
"""Audit live controls and full production integration for dependency research."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from batch_cpp_constellation_expand_audit import cases, TARGET, CALLER

PACKAGES = ('fse-scheduler-dependencies', 'fse-getdiag-result',
            'fse-getdiag-selection', 'fse-getdiag-cap-exit',
            'batch-cpp-constellation-expand', 'batch-cpp-v92jd-crc',
            'data-state-reversal', 'data-state-reversal-width',
            'fax-epoch-pair-factoring', 'fax-epoch-index-boundary')
OBJECT = 'src_pump_v90_V90ConstellationPower.cpp.o'


def main():
    rows = []
    for package in PACKAGES:
        root = d.ROOT/'build'/package
        ledger = json.loads((root/'results.json').read_text())
        assert ledger['revision'] == '6b4509bd'
        for family, content in ledger['families'].items():
            for label, cell in content['cells'].items():
                obj = root/family/label/'candidate.o'
                assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
                source = obj.parent/content['source_path'].split('/')[-1]
                assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
                assert sorted(d.b.sizes(str(obj))) == cell['functions']
                for name, expected in cell['verdicts'].items():
                    assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
                rows.append({'package': package, 'label': label,
                             'emitted': len(cell['functions']), 'shared': len(cell['verdicts']),
                             'gains': cell.get('gains', []), 'losses': cell.get('losses', [])})
    assert len(rows) == 44 and sum(r['emitted'] for r in rows) == 396
    before, after = d.ROOT/'build/production-before', d.ROOT/'build/tc_out'
    names = {p.name for p in before.glob('*.o')}
    assert len(names) == 300 and names == {p.name for p in after.glob('*.o')}
    assert (before/'.build-config').read_bytes() == (after/'.build-config').read_bytes()
    changed = {n for n in names if (before/n).read_bytes() != (after/n).read_bytes()}
    assert changed == {OBJECT}, changed
    obj = after/OBJECT
    winner = d.ROOT/'build/batch-cpp-constellation-expand/V90ConstellationPower/expanded-sum-original-loads/candidate.o'
    assert obj.read_bytes() == winner.read_bytes(), 'production raw winner differs'
    a, b = inspect(before/OBJECT), inspect(obj)
    for key in ('records', 'allocated', 'nobits'):
        assert a[key] == b[key], key
    relocations = [(old, new) for old, new in zip(a['relocations'], b['relocations']) if old != new]
    assert len(a['relocations']) == len(b['relocations']) and len(relocations) == 6
    for old, new in relocations:
        assert old[:3] == new[:3] and old[0] == '.rodata'
        assert old[3][:2] == new[3][:2] == ('audited-code-destination', CALLER)
    assert cases(obj) == cases(winner) == cases(Path(d.b.BLOB))
    bodies = {n for n in a['text_positions'] if d.b.body(str(before/OBJECT), n) != d.b.body(str(obj), n)}
    assert bodies == {TARGET, CALLER}
    assert d.b.verdict(*d.b.body(d.b.BLOB, TARGET), *d.b.body(str(obj), TARGET)) == ('EXACT', 0)
    old = json.loads((d.ROOT/'build/baseline-byteident.json').read_text())
    new = json.loads((d.ROOT/'build/batch-final-byteident.json').read_text())
    gains = set(new['exact_symbols']) - set(old['exact_symbols'])
    losses = set(old['exact_symbols']) - set(new['exact_symbols'])
    assert gains == {TARGET} and not losses
    assert old['compared'] == new['compared'] == 1852
    assert old['exact'] == 1069 and new['exact'] == 1070
    assert new['exact_bytes'] == old['exact_bytes'] + 622 == 116851
    result = {'cells': len(rows), 'emitted_comparisons': sum(r['emitted'] for r in rows),
              'shared_comparisons': sum(r['shared'] for r in rows), 'rows': rows,
              'objects': 300, 'raw_unchanged': 299, 'raw_winner_repeats': 1,
              'changed_bodies': sorted(bodies), 'audited_table_destinations': cases(obj),
              'gains': sorted(gains), 'losses': [], 'exact': 1070, 'compared': 1852,
              'exact_bytes': 116851, 'gain_original_bytes': 622}
    (d.ROOT/'build/batch-production-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('Dependency batch:44valid controls/396live bodies;300objects/299raw unchanged/1raw winner;1070/1852 exact,+622bytes,0losses;6caller case destinations explicitly audited')


if __name__ == '__main__':
    main()
