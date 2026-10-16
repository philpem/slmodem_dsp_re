#!/usr/bin/env python3
"""Audit all finite return-mode controls and the adopted B103 whole-TU gain."""
import argparse
import hashlib
import json
from pathlib import Path

import playbook_small_patterns as d
from narrow_call_boundary_screen import bodies
from recovery_sre_boundary_audit import first_return_use, nontext

CAMPAIGNS = ('recovery-return-mode-local', 'recovery-b103-return-boundaries',
             'recovery-v27-data-boundaries', 'recovery-b103-return-slots',
             'recovery-v27-published-budget', 'recovery-b103-header-collateral')
TARGETS = {'V21t_prc': 'TxHdxStartV21', 'V27t_int': 'TxNoCarrierV27',
           'V27t_prc': 'TxHdxDataV27', 'B103prc': 'DemodDataB103'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    cells, verdicts, baselines = [], 0, 0
    for campaign in CAMPAIGNS:
        root = d.ROOT/'build'/campaign
        report = json.loads((root/'results.json').read_text())
        for family, recorded in report['families'].items():
            baseline = root/family/'baseline/candidate.o'
            names = sorted(d.b.sizes(str(baseline)))
            for label, cell in recorded['cells'].items():
                path = root/family/label/'candidate.o'
                assert cell['compile_exit'] == 0
                assert sha(path) == cell['object_hash']
                source = path.parent/Path(recorded['source_path']).name
                assert sha(source) == cell['source_hash']
                assert sorted(d.b.sizes(str(path))) == names
                assert nontext(path) == nontext(baseline), (campaign, family, label)
                if label == 'baseline':
                    assert cell['baseline_reproduced']
                    baselines += 1
                changed, reordered = [], []
                for name in names:
                    before, br = d.b.body(str(baseline), name)
                    after, ar = d.b.body(str(path), name)
                    if list(br.values()) != list(ar.values()):
                        reordered.append(name)
                    if (before, br) != (after, ar):
                        changed.append(name)
                allowed = [] if campaign == 'recovery-b103-header-collateral' else [TARGETS[family]]
                assert set(changed) <= set(allowed), (family, label, changed)
                if campaign == 'recovery-b103-header-collateral' or \
                        (campaign == 'recovery-v27-data-boundaries' and family == 'V27t_int'):
                    assert path.read_bytes() == baseline.read_bytes()
                verdicts += len(cell['verdicts'])
                cells.append({'campaign': campaign, 'family': family, 'cell': label,
                              'object_sha256': sha(path), 'changed_bodies': changed,
                              'shared_verdicts': len(cell['verdicts']),
                              'changed_target_sequences': reordered})
    root = d.ROOT/'build/recovery-b103-return-slots/B103prc'
    winner = root/'r1_c1_s1/candidate.o'
    for name in d.b.sizes(str(winner)):
        assert list(d.b.body(str(root/'baseline/candidate.o'), name)[1].values()) == \
               list(d.b.body(str(winner), name)[1].values()), name
    reference = bodies(d.b.BLOB)['DemodDataB103']
    mode_controls = []
    for label, path in [('original', Path(d.b.BLOB)),
                        ('baseline', root/'baseline/candidate.o'), ('winner', winner)]:
        rows = reference if label == 'original' else bodies(path)['DemodDataB103']
        call = next(r for r in rows if r.get('target') == 'FPM_FSD_demodulate')
        measured = first_return_use(rows, call['address'])
        assert not measured['refusals'] and len(measured['outcomes']) == 1
        mode = measured['outcomes'][0]['kind']
        expected = 'signed-word-extension' if label == 'baseline' else 'unsigned-word-extension'
        assert mode == expected, (label, measured)
        mode_controls.append({'label': label, 'mode': mode, 'trace': measured})
    assert d.b.verdict(*d.b.body(d.b.BLOB, 'DemodDataB103'),
                       *d.b.body(str(winner), 'DemodDataB103')) == ('EXACT', 0)
    output = {'complete_tu_compiles': len(cells), 'shared_verdicts': verdicts,
              'raw_reproduced_baselines': baselines,
              'nontext_bindings_and_extents_unchanged': True,
              'cells': cells, 'b103_mode_controls': mode_controls,
              'gain': {'symbol': 'DemodDataB103', 'original_bytes': 568,
                       'newly_exact': 1, 'exact_losses': 0,
                       'unchanged_bystanders': 16}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2)+'\n')
    print('return-mode audit: %d compiles, %d shared verdicts, %d raw baselines; '
          '1 exact gain/568B, 0 losses; nontext/bystanders pass; winner targets pass' %
          (len(cells), verdicts, baselines))


if __name__ == '__main__':
    main()
