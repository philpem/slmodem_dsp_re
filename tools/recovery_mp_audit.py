#!/usr/bin/env python3
"""Audit the MP iteration against the frozen integrated canonical objects."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    out = Path('build/recovery-mp-next-integration')
    out.mkdir(exist_ok=True)
    baseline = Path('build/recovery-mp-next-baseline')
    before = baseline / 'src_pump_v90_V90MP.cpp.o'
    selected = Path('build/recovery-mp-next-padding/V90MP/full-padding-bound/candidate.o')
    native = Path('build/tc_repro/src_pump_v90_V90MP.cpp.o')
    period = Path('build/period/src_pump_v90_V90MP.o')
    assert selected.read_bytes() == native.read_bytes() == period.read_bytes()
    assert period.stat().st_mtime_ns > Path('src/pump/v90/V90MP.cpp').stat().st_mtime_ns
    assert inspect(before)['records'] == inspect(selected)['records']
    assert inspect(before)['objects'] == inspect(selected)['objects']
    old = json.loads((baseline / 'objects.json').read_text())
    new = {p.name: sha(p) for p in Path('build/tc_repro').glob('*.o')}
    assert old.keys() == new.keys() and len(old) == 300
    changed = {name: {'before': old[name], 'after': value}
               for name, value in new.items() if old[name] != value}
    assert set(changed) == {'src_pump_v90_V90MP.cpp.o'}
    bodies = {}
    names = d.b.sizes(str(before))
    assert names.keys() == d.b.sizes(str(selected)).keys() and len(names) == 16
    targets = {'_ZN5V90MP10infoToBitsEv', '_ZN5V90MP10bitsToInfoEi'}
    for name in names:
        a, b = d.b.body(str(before), name), d.b.body(str(selected), name)
        bodies[name] = {'verdict': list(d.b.verdict(*a, *b)),
                        'raw_body_and_relocations_equal': a == b}
        if name not in targets:
            assert a == b, name
    domains = {}
    count = grades = 0
    for domain in ('trace', 'predicate', 'decoder', 'print-owner', 'quantizer', 'padding', 'crc-owner'):
        folder = Path('build/recovery-mp-next-' + domain)
        audit = json.loads((folder / 'audit.json').read_text())
        ledger = json.loads((folder / 'results.json').read_text())
        denominator = audit['denominator']
        count += denominator['compiled_cells']
        grades += denominator['shared_body_verdicts']
        domains[domain] = {'denominator': denominator, 'cells': {
            label: {'object_sha256': cell['object_hash'],
                    'gains': cell.get('gains', []), 'losses': cell.get('losses', [])}
            for label, cell in ledger['families']['V90MP']['cells'].items()}}
    assert (count, grades) == (25, 400)
    report = {'baseline_revision': 'd9995470', 'compiled_cells': count,
              'shared_body_verdicts': grades, 'baseline_objects': 300,
              'unchanged_objects': 299, 'changed_objects': changed,
              'selected_sha256': sha(selected), 'raw_candidate_native_period_equal': True,
              'source_unchanged_bodies_raw_equal': 14, 'body_before_after': bodies,
              'domains': domains}
    census = Path('build/recovery-mp-next-census.json')
    if census.exists():
        current = json.loads(census.read_text())
        frozen = Path('/mnt/zfs/dev/slmodem/claude_re/build/worktrees/source-recovery-next/build/recovery-packers-next/baseline/byteident.json')
        prior = json.loads(frozen.read_text())
        assert current['exact_symbols'] == prior['exact_symbols']
        assert current['exact'] == 1091 and current['exact_bytes'] == 122715
        report['census'] = {key: current[key] for key in ('exact', 'compared', 'exact_bytes', 'compared_bytes')}
        report['exact_gains'] = []
        report['exact_losses'] = []
    (out / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('25 cells/400 body grades; 299/300 objects and 14/16 MP bodies unchanged raw')
    print('selected candidate/native/period raw equal', sha(selected))


if __name__ == '__main__':
    main()
