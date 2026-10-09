#!/usr/bin/env python3
"""Audit all full-TU controls in the bounded shared-value follow-up."""
import hashlib
import json
from gcc3_value_carriers_audit import inspect
import playbook_small_patterns as d


def main():
    compiles = grades = 0
    losses = []
    for name, expected in [('shared-counter-results', 8), ('cid-shared-results', 4),
                           ('guarded-quotient-results', 6), ('receiver-field-increment', 8)]:
        root = d.ROOT/'build'/name
        result = json.loads((root/'results.json').read_text())
        assert result['revision'] == '04eee73f'
        assert '-DDSPLIB_REPRODUCE_BUGS' in result['config']
        count = 0
        for family, record in result['families'].items():
            base = root/family/'baseline/candidate.o'
            assert base.read_bytes() == (root/family/'retained.o').read_bytes()
            metadata = inspect(base)
            baseline = record['cells']['baseline']
            for label, cell in record['cells'].items():
                obj = root/family/label/'candidate.o'
                assert cell['compile_exit'] == 0
                assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
                assert '-DDSPLIB_REPRODUCE_BUGS' in cell['command'][-1]
                assert cell['functions'] == baseline['functions']
                assert cell['globals'] == baseline['globals']
                if label != 'baseline':
                    changed = [n for n in cell['functions']
                               if d.b.body(str(obj), n) != d.b.body(str(base), n)]
                    assert changed == cell['changed_bodies']
                    allowed = ({family[:3]+'RX_eq_train', family[:3]+'RX_decision'}
                               if family in ('V27rx', 'V29rx') else
                               {'cid_modem', 'pack_next_bit'} if family == 'Rxcid' else
                               {'_ZN20V90SignBitsExtractor5resetEjj',
                                '_ZN17V90SpectralShaper5resetEjjffff',
                                '_ZN9V90Mapper5resetEP16V90MappingParams7PcmType'})
                    assert set(changed) <= allowed
                actual = inspect(obj)
                assert all(actual[key] == value for key, value in metadata.items()
                           if key != 'text_positions')
                verdicts = {n: list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n)))
                            for n in cell['verdicts']}
                assert verdicts == cell['verdicts']
                assert not cell.get('gains', [])
                losses.extend((name, family, label, n) for n in cell.get('losses', []))
                count += 1
                grades += len(verdicts)
        assert count == expected
        compiles += count
    assert compiles == 26 and grades == 166
    assert len(losses) == 1 and losses[0][1] == 'V90SignBitsExtractor'
    (d.ROOT/'build/shared-value-batch-audit.json').write_text(json.dumps(
        {'full_TU_compiles': compiles, 'body_verdicts': grades, 'gains': [],
         'unadopted_losses': losses, 'metadata': 'equal except expected text positions'}, indent=2)+'\n')
    print('26 full-TU controls / 166 body verdicts: raw baselines and metadata pass')
    print('0 strict gains; 1 loss in an unadopted negative control')


if __name__ == '__main__':
    main()
