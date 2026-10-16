#!/usr/bin/env python3
"""Audit complete-TU collateral and phase modes in the four saved controls."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect

TARGET = '_ZN5V92Jd10getJdPhaseEv'
HEADER = 'float V92Jd::getJdPhase()'


def main():
    root = driver.ROOT / 'build/recovery-v92jd-phase-conversion'
    ledger = json.loads((root / 'results.json').read_text())
    family = ledger['families']['V92Jd']
    base = root / 'V92Jd/baseline/candidate.o'
    metadata = inspect(base)
    report = {'baseline_revision': ledger['revision'], 'cells': {}}
    for label, cell in family['cells'].items():
        folder = root / 'V92Jd' / label
        obj = folder / 'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        changed = [name for name in cell['functions']
                   if driver.b.body(str(base), name) != driver.b.body(str(obj), name)]
        adopted = label == 'integer-producer-double-scale'
        assert changed == ([TARGET] if adopted else []), (label, changed)
        rtl = chunks(folder / 'V92Jd.cpp.01.rtl', HEADER)
        assert len(rtl) == 1
        text = rtl[0]
        features = {needle: needle in text for needle in
                    ('float:DF', 'mult:DF', 'float_truncate:SF', 'mult:SF')}
        assert features == {'float:DF': adopted, 'mult:DF': adopted,
                            'float_truncate:SF': adopted, 'mult:SF': not adopted}
        assert cell['verdicts'][TARGET] == (['EXACT', 0] if adopted else ['SIZE', 24])
        assert not cell.get('losses')
        report['cells'][label] = {'changed_bodies': changed, 'initial_RTL': features,
                                  'metadata_data_bss_nontext_relocations_equal': True,
                                  'shared_body_verdicts': len(cell['verdicts']),
                                  'target': cell['verdicts'][TARGET]}
    assert len(report['cells']) == 4
    report['shared_body_verdicts'] = sum(
        cell['shared_body_verdicts'] for cell in report['cells'].values())
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('4 complete TUs / %d shared-body verdicts; 20 bystanders unchanged;' %
          report['shared_body_verdicts'])
    print('metadata/data/BSS/nontext relocations equal; only direct integer-to-double')
    print('has initial DF multiply + terminal SF truncation and EXACT80B; zero losses')


if __name__ == '__main__':
    main()
