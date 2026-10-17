#!/usr/bin/env python3
"""Audit the bounded timer narrowing recovery and complete-TU collateral."""
import hashlib
import json
from pathlib import Path
import gcc3_value_carriers_audit as metadata
from recovery_fax_debug_audit import Object

ROOT = Path(__file__).resolve().parents[1]


def main():
    root = ROOT / 'build/recovery-tone-timer'
    matrix = json.loads((root / 'results.json').read_text())
    family = matrix['families']['TONE']
    cells = family['cells']
    before = root / 'TONE/baseline/candidate.o'
    original = Object(before).canonical()
    meta = metadata.inspect(before)
    exact = {name for name, verdict in cells['baseline']['verdicts'].items()
             if verdict[0] == 'EXACT'}
    assert cells['baseline']['baseline_reproduced']
    assert len(original[0]) == 6
    report = {'cells': {}, 'source_revision': matrix['revision'],
              'limits': 'Narrowing fidelity and valid tone-component history; '
                        'no public modem-session or complete-function identity claim.'}
    for label, cell in cells.items():
        path = root / 'TONE' / label / 'candidate.o'
        current = Object(path).canonical()
        inspected = metadata.inspect(path)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert inspected[key] == meta[key], (label, key)
        assert current[2:] == original[2:], (label, 'bindings/consumers')
        changed = sorted(name for name in original[0]
                         if original[0][name] != current[0][name])
        assert changed == (['TONE_generate'] if label == 'double-literal' else [])
        assert {name for name, verdict in cell['verdicts'].items()
                if verdict[0] == 'EXACT'} == exact
        if label == 'explicit-float-cast':
            assert path.read_bytes() == before.read_bytes()
        rtl = (path.parent / 'TONE.c.01.rtl').read_text()
        body = rtl.split(';; Function TONE_generate\n', 1)[1].split(';; Function ', 1)[0]
        narrow = '(float_truncate:SF (reg:DF 79))'
        assert (narrow in body) == (label == 'double-literal')
        report['cells'][label] = {
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'bodies': len(current[0]), 'changed_canonical_bodies': changed,
            'exact_names': sorted(exact),
            'explicit_DF_to_SF_at_initial_expansion': narrow in body,
            'target_verdict': cell['verdicts']['TONE_generate'],
        }
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('3 full-TU cells,18 bodies;5 canonical bystanders,bindings,data and consumers stable;'
          'explicit float cast raw-identical;DF-to-SF conversion appears at initial RTL;'
          '1 exact name preserved,no gains or losses')


if __name__ == '__main__':
    main()
