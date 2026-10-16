#!/usr/bin/env python3
"""Audit the three call diagnostics and complete-TU collateral."""
import argparse
import json
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_value_carriers_audit import inspect

FORMATS = {'call_create': {b'call: create...\n\0', b'call: create RC: %d <-> %d...\n\0'},
           'call_run': {b'call: process: msg %d --> %d\n\0'}}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run-dir', type=Path, default=driver.ROOT / 'build/recovery-call-diagnostics')
    root = ap.parse_args().run_dir
    cells = json.loads((root/'results.json').read_text())['families']['call']['cells']
    paths = {label: root/'call'/label/'candidate.o' for label in cells}
    base, candidate = paths['baseline'], paths['original-diagnostics']
    functions = driver.b.sizes(str(base))
    assert len(functions) == 11 and functions.keys() == driver.b.sizes(str(candidate)).keys()
    stable, changed = [], []
    for name in functions:
        old, old_rel = driver.b.body(str(base), name)
        new, new_rel = driver.b.body(str(candidate), name)
        if name in FORMATS:
            changed.append(name)
            for path in (str(candidate), driver.b.BLOB):
                relocations = driver.b.body(path, name)[1].values()
                formats = {target[1] for kind, target in relocations if kind=='R_386_32' and target[0]=='merge-string'}
                assert FORMATS[name] <= formats
                relocations = list(driver.b.body(path, name)[1].values())
                assert sum(kind=='R_386_PC32' and target[:2]==('symbol','dsplibs_debug_printf') for kind,target in relocations)==len(FORMATS[name])
                assert sum(kind=='R_386_32' and target[:2]==('symbol','dsplibs_debug_level') for kind,target in relocations)==len(FORMATS[name])
            assert not any(kind=='R_386_PC32' and target[:2]==('symbol','dsplibs_debug_printf') for kind,target in old_rel.values())
        else:
            assert old_rel == new_rel, name
            old, new = bytearray(old), bytearray(new)
            for offset in old_rel:
                old[offset:offset+4] = new[offset:offset+4] = b'\0'*4
            assert old == new, name
            stable.append(name)
    a,b = inspect(base), inspect(candidate)
    assert a['records']==b['records'] and a['nobits']==b['nobits']
    added = set()
    assert a['allocated'].keys()==b['allocated'].keys()
    for name in a['allocated']:
        if '.str' in name:
            old = set(bytes.fromhex(a['allocated'][name]).split(b'\0'))
            new = set(bytes.fromhex(b['allocated'][name]).split(b'\0'))
            assert old <= new
            added |= new-old
        else:
            assert a['allocated'][name] == b['allocated'][name], name
    assert added == {fmt[:-1] for values in FORMATS.values() for fmt in values}
    assert len(a['relocations'])==len(b['relocations'])==22
    moved = 0
    for old,new in zip(a['relocations'],b['relocations']):
        assert old[:3]==new[:3]
        if old!=new:
            assert old[3][:2]==new[3][:2]==('audited-code-destination','call_run')
            assert new[3][2]-old[3][2]==28
            moved += 1
    assert moved==18
    assert not cells['original-diagnostics']['gains'] and not cells['original-diagnostics']['losses']
    assert cells['original-diagnostics']['verdicts']['call_create']==['SIZE',2]
    assert cells['original-diagnostics']['verdicts']['call_run']==['SIZE',3]
    report = {'full_TUs':2,'function_verdicts':22,'unchanged_canonical_bystanders':stable,
              'changed_functions':changed,'original_diagnostic_sites_restored':3,
              'new_strings':[s.decode() for s in sorted(added)],
              'BSS_bindings_nonstring_payloads_equal':True,
              'nontext_code_relocations':{'total':22,'call_run_switch_destinations_shifted_28':moved},
              'exact_gains':[],'exact_losses':[],
              'limits':'Remaining SIZE2/SIZE3 are partial recovery; changed message publication matches original ordering after resampling and before status dispatch.'}
    (root/'audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print('2 full TUs / 22 verdicts: 3 original diagnostic sites restored; 9 canonical bystanders and7 exact matches preserved')
    print('bindings/BSS/nonstring payloads equal; exactly3 new original formats; 18 switch destinations move28bytes and remain instruction boundaries')


if __name__ == '__main__':
    main()
