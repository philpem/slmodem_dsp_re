#!/usr/bin/env python3
"""Audit all eight complete-TU controls and their selected sibling alternatives."""
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_stage_divergence import chunks
from small_call_result_screen import forms, compare


def main():
    root = d.ROOT/'build/small-call-result'
    result = json.loads((root/'results.json').read_text())
    output = {'revision': result['revision'], 'cells': [], 'verdict_count': 0}
    for family, record in result['families'].items():
        symbol = '_ZN8V90Modem14setSessionFlagEj' if family == 'V90Modem' else 'DialerAbort'
        header = 'void V90Modem::setSessionFlag(unsigned int)' if family == 'V90Modem' else 'DialerAbort'
        folder = root/family
        base = folder/'baseline/candidate.o'
        assert record['cells']['baseline']['baseline_reproduced']
        metadata = inspect(base)
        names = set(d.b.sizes(str(base)))
        for cell, entry in record['cells'].items():
            path = folder/cell/'candidate.o'
            actual = inspect(path)
            assert all(actual[key] == metadata[key] for key in metadata if key != 'text_positions')
            assert set(d.b.sizes(str(path))) == names
            changed = [name for name in sorted(names) if d.b.body(str(base), name) != d.b.body(str(path), name)]
            assert set(changed) <= {symbol}, (family, cell, changed)
            stages = {}
            for stage in ('01.rtl', '02.sibling'):
                dump = next((folder/cell).glob('*.'+stage))
                selected = chunks(dump, header)
                assert len(selected) == 1
                text = selected[0]
                stages[stage] = {'call_placeholders': text.count('call_placeholder'),
                                 'sibling_markers': text.count('(call_insn/j')}
            row = {'family': family, 'cell': cell, 'target_verdict': entry['verdicts'][symbol],
                   'changed_bodies': changed, 'unchanged_bystanders': len(names)-1,
                   'metadata_nontext_equal': True, 'stages': stages,
                   'remaining_call_to_sibling_targets': compare(forms(d.b.BLOB, symbol), forms(path, symbol))}
            output['cells'].append(row)
            output['verdict_count'] += len(names)
            print(family, cell, row['target_verdict'], stages['02.sibling'])
    rows = output['cells']
    assert [row['target_verdict'][0] for row in rows] == ['SIZE', 'SIZE', 'BYTES', 'EXACT', 'SIZE', 'SIZE', 'SIZE', 'SIZE']
    assert [row['stages']['01.rtl']['call_placeholders'] for row in rows] == [2, 2, 2, 2, 4, 4, 4, 4]
    assert all(row['stages']['02.sibling']['call_placeholders'] == 0 for row in rows)
    assert [row['stages']['02.sibling']['sibling_markers'] for row in rows] == [2, 2, 1, 1, 2, 2, 2, 2]
    assert rows[3]['remaining_call_to_sibling_targets'] == []
    assert all(rows[index]['remaining_call_to_sibling_targets'] for index in (0, 1, 4, 5, 6, 7))
    # Structured Dialer exits have no effect at all on either complete object.
    assert (root/'Dialer/baseline/candidate.o').read_bytes() == (root/'Dialer/unsigned-0-common-1/candidate.o').read_bytes()
    assert (root/'Dialer/unsigned-1-common-0/candidate.o').read_bytes() == (root/'Dialer/unsigned-1-common-1/candidate.o').read_bytes()
    assert output['verdict_count'] == 56
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('8 full-TU cells / 56 function verdicts; metadata/data and every bystander preserved')


if __name__ == '__main__':
    main()
