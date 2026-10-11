#!/usr/bin/env python3
"""Audit all ADID grouping controls, including misses and complete-TU data."""
import hashlib
import json
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated

ROOTS = ('gcc3-adid-phase-grouping', 'gcc3-adid-phase-grouping-half',
         'gcc3-adid-phase-grouping-consumer', 'gcc3-adid-phase-definition-age')
TARGET = '_ZN25V90AutoDigitalImpDetector35uniteLinMappInfoOfUnsuspectedPhasesEh'


def main():
    report = {}
    count = verdicts = 0
    for name in ROOTS:
        root = d.ROOT / 'build' / name
        ledger = json.loads((root / 'results.json').read_text())
        cells = ledger['families']['V90AutoDigitalImpDetector']['cells']
        basepath = root / 'V90AutoDigitalImpDetector/baseline/candidate.o'
        base = inspect(basepath)
        rows = {}
        for label, cell in cells.items():
            path = root / 'V90AutoDigitalImpDetector' / label / 'candidate.o'
            assert cell['compile_exit'] == 0
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            assert current['records'] == base['records'], (name, label, 'metadata')
            assert current['allocated_sizes'] == base['allocated_sizes'], (name, label, 'sizes')
            assert current['objects'] == base['objects'], (name, label, 'named data')
            assert allocated(path) == allocated(basepath), (name, label, 'allocated data')
            assert len(cell['verdicts']) == 34
            assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 16
            assert not cell.get('gains') and not cell.get('losses')
            assert set(cell.get('changed_bodies', [])) <= {TARGET}
            if label == 'baseline':
                assert cell['baseline_reproduced']
            rows[label] = {'target_verdict': cell['verdicts'][TARGET],
                           'object_hash': cell['object_hash'],
                           'metadata_and_data_unchanged': True,
                           'common_body_verdicts': 34}
            count += 1
            verdicts += 34
        report[name] = rows
        print(name, len(rows), 'cells: 16/34 exact; 33 bystanders and metadata/data unchanged')
    assert (count, verdicts) == (22, 748)
    def target(root, cell):
        return d.b.body(str(d.ROOT / 'build' / root / 'V90AutoDigitalImpDetector' / cell / 'candidate.o'), TARGET)
    for half, prior_root, prior_cell in ((0, ROOTS[0], 'late-1-group-1-pool-1'),
                                         (1, ROOTS[1], 'late-1-group-1-pool-1-double-half')):
        assert d.b.verdict(*target(ROOTS[2], 'unsigned-carrier-double-%d' % half),
                           *target(prior_root, prior_cell)) == ('BYTES', 1)
    closest = d.ROOT / 'build' / ROOTS[3] / 'V90AutoDigitalImpDetector/best-first-0-float-first-1/candidate.o'
    original = d.b.insns(d.b.BLOB, TARGET)
    candidate = d.b.insns(str(closest), TARGET)
    # Detector fires on the eight remaining stack displacements, not a clean silence.
    assert len(original) == len(candidate)
    different = [(a, z) for a, z in zip(original, candidate) if a != z]
    assert len(different) == 8, different
    for a, z in different:
        assert a[0] == z[0]
        assert a[1].replace('0x20(%esp)', 'SLOT(%esp)').replace('0x24(%esp)', 'SLOT(%esp)') == z[1].replace('0x20(%esp)', 'SLOT(%esp)').replace('0x24(%esp)', 'SLOT(%esp)')
    report['closest_operand_differences'] = different
    report['denominator'] = {'cells': count, 'common_body_verdicts': verdicts,
                             'exact_gains': 0, 'exact_losses': 0}
    (d.ROOT / 'build/adid-phase-grouping-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(count, 'cells /', verdicts, 'common body verdicts; zero gains or losses')


if __name__ == '__main__':
    main()
