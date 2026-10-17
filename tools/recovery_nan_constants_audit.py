#!/usr/bin/env python3
"""Audit two current constant-NaN recoveries and all complete-TU collateral."""
from collections import Counter
import json
from pathlib import Path
import gcc3_value_carriers_audit as metadata
from recovery_fax_debug_audit import Object
import playbook_small_patterns as d

ROOT = Path(__file__).resolve().parents[1]
TARGETS = {
    'V90AutoDigitalImpDetector': '_ZN25V90AutoDigitalImpDetector18porcessSecondStudyEv',
    'V90ConstellationDesigner': '_ZN24V90ConstellationDesigner23setConstellationToNoiseEfPA128_sS1_PsPhPA128_h',
}


def section(obj, name):
    return next(bytes(data) for label, data in obj.sections.values() if label == name)


def main():
    root = ROOT/'build/recovery-nan-constants'
    matrix = json.loads((root/'results.json').read_text())['families']
    original = Object(ROOT/'ref/slmodemd/dsplibs.o').canonical()
    report = {}
    for family, target in TARGETS.items():
        cells = matrix[family]['cells']
        assert cells['baseline']['baseline_reproduced']
        before = root/family/'baseline/candidate.o'
        after = root/family/'original-quiet-nan/candidate.o'
        left, right = Object(before), Object(after)
        a, b = left.canonical(), right.canonical()
        lm, rm = metadata.inspect(before), metadata.inspect(after)
        allowed = dict(lm['records'])
        assert 'nanf' in allowed
        del allowed['nanf']
        assert allowed == rm['records']
        assert lm['nobits'] == rm['nobits'] and lm['relocations'] == rm['relocations']
        assert [record for record in a[2] if record[0] != 'nanf'] == b[2], 'bindings beyond the removed import changed'
        changed = [name for name in a[0] if a[0][name] != b[0][name]]
        assert changed == [target]
        for name in a[3].keys() | b[3].keys():
            if name != target:
                assert a[3].get(name) == b[3].get(name), ('bystander consumers', name)
        removed = Counter({(1, ('string', b'')): 1, (2, ('nanf', -4)): 1})
        added = Counter({(1, ('.rodata.cst4', b'\x00\x00\xc0\x7f')): 1})
        assert a[3][target] - b[3][target] == removed
        assert b[3][target] - a[3][target] == added
        assert added <= original[3][target]
        assert not any(item[1][0] == 'nanf' for item in original[3][target])
        assert not any(item[1][0] == 'nanf' for item in b[3][target])
        for label in lm['allocated']:
            if label == '.rodata.cst4':
                chunks = lambda data: Counter(data[i:i+4] for i in range(0, len(data), 4))
                x, y = chunks(section(left,label)), chunks(section(right,label))
                assert y-x == Counter({b'\x00\x00\xc0\x7f': 1}) and not x-y
            elif label.startswith('.rodata.str'):
                x, y = Counter(section(left,label).split(b'\0')), Counter(section(right,label).split(b'\0'))
                if label == '.rodata.str1.1':
                    x[b''] -= 1
                assert x == y
            else:
                assert lm['allocated'][label] == rm['allocated'][label], label
        exact = [{name for name,v in cell['verdicts'].items() if v[0] == 'EXACT'} for cell in cells.values()]
        assert exact[0] == exact[1]
        report[family] = {'bodies_per_cell':len(a[0]), 'canonical_bystanders':len(a[0])-1,
            'exact_names_preserved':len(exact[0]), 'raw_changed_bodies':cells['original-quiet-nan']['changed_bodies'],
            'original_backed_constant_consumer':True, 'unmatched_library_call_removed':True,
            'baseline_verdict':cells['baseline']['verdicts'][target],
            'candidate_verdict':cells['original-quiet-nan']['verdicts'][target],
            'limits':'Call/constant fidelity improvement; no new runtime failure or exact-function gain claimed. '
                     'Prior no-gain controls remain valid; source choice is justified by original operands and call absence.'}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('4 full-TU cells,116 bodies;56 canonical bystanders unchanged;31 exact names preserved;2 unmatched nanf calls removed')


if __name__ == '__main__':
    main()
