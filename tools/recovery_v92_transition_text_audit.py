#!/usr/bin/env python3
"""Audit the corrected transition literal without hiding relocated bystanders."""
from collections import Counter
import json
from pathlib import Path
from recovery_fax_debug_audit import Object
from recovery_debug_boundary_audit import strings

ROOT = Path(__file__).resolve().parents[1]
TARGET = '_ZN20V90Phase3Demodulator14getV92DecisionEf'


def positioned_consumers(obj):
    rows = {}
    for index, at, symbol, kind in obj.relocations:
        owner = next((f for f in obj.functions if f['st_shndx'] == index and
                      f['st_value'] <= at < f['st_value'] + f['st_size']), None)
        if owner is None:
            continue
        data = obj.sections[index][1]
        addend = int.from_bytes(data[at:at+4], 'little', signed=True)
        rows[(owner.name, at-owner['st_value'], kind)] = obj.payload(symbol, addend)
    return rows


def main():
    root = ROOT/'build/recovery-v92-transition-text'
    cells = json.loads((root/'results.json').read_text())['families']['V90Phase3Demodulator']['cells']
    folder = root/'V90Phase3Demodulator'
    old, new = folder/'baseline/candidate.o', folder/'original-transition-text/candidate.o'
    old_obj, new_obj = Object(old), Object(new)
    a, b = old_obj.canonical(), new_obj.canonical()
    left, right = positioned_consumers(old_obj), positioned_consumers(new_obj)
    assert left.keys() == right.keys()
    changed_sites = [site for site in left if left[site] != right[site]]
    assert len(changed_sites) == 1 and changed_sites[0][0] == TARGET
    assert cells['baseline']['baseline_reproduced']
    assert a[:3] == b[:3], 'instruction, nonstring data or binding drift'
    for owner in a[3]:
        if owner != TARGET:
            assert a[3][owner] == b[3][owner], owner
    removed = a[3][TARGET] - b[3][TARGET]
    added = b[3][TARGET] - a[3][TARGET]
    assert removed == Counter({(1, ('string', b'V90Phase3Demodulator: JdNot detected @ %d\r\n')): 1})
    assert added == Counter({(1, ('string', b'V90Phase3Demodulator: enter Wait For V92Jd state @ %d\r\n')): 1})
    original = Counter(r['text'] for r in strings(ROOT/'ref/slmodemd/dsplibs.o', TARGET))
    before = Counter(r['text'] for r in strings(old, TARGET))
    after = Counter(r['text'] for r in strings(new, TARGET))
    assert original == after and before != original
    exact = [{n for n, v in c['verdicts'].items() if v[0] == 'EXACT'} for c in cells.values()]
    assert exact[0] == exact[1]
    report = {'full_TU_cells': 2, 'bodies_per_cell': len(a[0]), 'exact_names_retained': len(exact[0]),
              'all_masked_instructions_nonstring_data_bindings_equal': True,
              'bystander_canonical_consumers_equal': True,
              'raw_changed_bodies': cells['original-transition-text']['changed_bodies'],
              'original_string_consumer_count': sum(original.values()),
              'corrected_consumers': 1, 'changed_relocation_sites': changed_sites,
              'original_anchor': '233c8 sample count load;233ce literal;233d7 edprintf;then state7/count0/event5',
              'fixture_boundary': 'Synthetic ADID terminal state6; caller count4/5 becomes5/6; levels0..3. No full study history claimed.',
              'baseline_negative': '2/216 new checks fail at levels2/3; existing213241 checks pass'}
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('2 cells, %d bodies each; %d exact names retained; one original literal restored; all masked bodies and bystander consumers unchanged' % (len(a[0]), len(exact[0])))


if __name__ == '__main__':
    main()
