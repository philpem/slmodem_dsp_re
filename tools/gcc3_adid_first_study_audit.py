#!/usr/bin/env python3
"""Audit first-study factoring/literal crosses and the recovered inner block."""
import hashlib
import json
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated

TARGET = '_ZN25V90AutoDigitalImpDetector17porcessFirstStudyEv'
ROOTS = ('gcc3-adid-first-study', 'gcc3-adid-first-study-opening',
         'gcc3-adid-first-study-literals', 'gcc3-adid-first-study-combined',
         'gcc3-adid-first-study-selection')
FAMILY = 'V90AutoDigitalImpDetector'


def masked(path, name):
    body, relocs = d.b.body(str(path), name)
    data = bytearray(body)
    for offset in relocs:
        data[offset:offset + 4] = b'\0' * 4
    return bytes(data), relocs


def main():
    reports = {}
    total = verdicts = 0
    for rootname in ROOTS:
        root = d.ROOT / 'build' / rootname
        cells = json.loads((root / 'results.json').read_text())['families'][FAMILY]['cells']
        basepath = root / FAMILY / 'baseline/candidate.o'
        baseline = inspect(basepath)
        rows = {}
        for label, cell in cells.items():
            path = root / FAMILY / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            assert current['records'] == baseline['records']
            assert current['objects'] == baseline['objects']
            changed = [n for n in cell['functions'] if masked(path, n) != masked(basepath, n)]
            assert set(changed) <= {TARGET}
            assert not cell.get('gains') and not cell.get('losses')
            assert len(cell['verdicts']) == 34
            assert sum(v[0] == 'EXACT' for v in cell['verdicts'].values()) == 17
            datachanges = [s for s, data in allocated(path).items() if allocated(basepath).get(s) != data]
            assert set(datachanges) <= {'.rodata.cst4', '.rodata.cst8'}
            rows[label] = {'target_verdict': cell['verdicts'][TARGET],
                           'canonical_body_changes': changed,
                           'constant_sections_changed': datachanges,
                           'object_hash': cell['object_hash']}
            total += 1
            verdicts += 34
        reports[rootname] = rows
    assert (total, verdicts) == (22, 748)
    first = reports[ROOTS[0]]
    assert first['baseline']['object_hash'] == first['explicit-float-late-1']['object_hash']
    chosen = first['explicit-double-late-0']['object_hash']
    for label in ('explicit-double-late-1', 'helper-late-0', 'helper-late-1'):
        assert first[label]['object_hash'] == chosen
    opening = reports[ROOTS[1]]
    literals = reports[ROOTS[2]]
    for mode in (0, 1):
        assert opening['threshold-double-%d-upper-guard-0' % mode]['object_hash'] == chosen
        assert literals['threshold-double-%d-diagnostic-double-0' % mode]['object_hash'] == chosen
    assert opening['threshold-double-0-upper-guard-1']['object_hash'] == opening['threshold-double-1-upper-guard-1']['object_hash'] != chosen
    assert literals['threshold-double-0-diagnostic-double-1']['object_hash'] == literals['threshold-double-1-diagnostic-double-1']['object_hash'] != chosen
    selection = reports[ROOTS[4]]
    assert selection['combined-count-inside']['object_hash'] == selection['combined-nested-selection']['object_hash'] != selection['combined']['object_hash']
    original = d.b.insns(d.b.BLOB, TARGET)
    candidate = d.ROOT / 'build' / ROOTS[0] / FAMILY / 'explicit-double-late-0/candidate.o'
    retained = d.b.insns(str(d.ROOT / 'build' / ROOTS[0] / FAMILY / 'baseline/candidate.o'), TARGET)
    proposed = d.b.insns(str(candidate), TARGET)
    block = original[70:94]
    assert len(block) == 24 and block[0] == ('mov', '0x1c00(%esi,%ecx,4),%edi')
    assert block[-1] == ('fldcw', '0x4e(%esp)')
    hits = lambda instructions: [i for i in range(len(instructions)-len(block)+1)
                                 if instructions[i:i+len(block)] == block]
    assert hits(proposed) == [71] and not hits(retained)
    candidate = d.ROOT / 'build' / ROOTS[4] / FAMILY / 'combined-count-inside/candidate.o'
    proposed = d.b.insns(str(candidate), TARGET)
    assert proposed[:66] == original[:66]
    # Distinct guards recover the opening; preserve literal jump-offset limits.
    assert hits(proposed) == [70]
    source = (d.ROOT / 'src/pump/v90/V90AutoDigitalImpDetector.cpp').read_text()
    _, _, fn = d.function(source, 'V90AutoDigitalImpDetector::porcessFirstStudy')
    locators = json.loads((d.ROOT / 'test/mutations/v90adid.json').read_text())
    for index in (166, 167, 168, 177, 178, 187):
        assert source.count(locators[index]['find']) == 1 and locators[index]['find'] in fn
    production = d.ROOT / 'build/tc_out/src_pump_v90_V90AutoDigitalImpDetector.cpp.o'
    assert production.read_bytes() == candidate.read_bytes()
    reports['denominator'] = {'source_cells': total, 'common_body_verdicts': verdicts,
                              'recovered_inner_instructions': 24, 'recovered_opening_instructions': 66, 'exact_gains': 0,
                              'exact_losses': 0, 'production_raw_equal': True}
    (d.ROOT / 'build/adid-first-study-audit.json').write_text(json.dumps(reports, indent=2) + '\n')
    print('22 cells /748 verdicts; recovered66-row opening and24-row inner block fire; 33 canonical bystanders unchanged; no exact gains/losses')


if __name__ == '__main__':
    main()
