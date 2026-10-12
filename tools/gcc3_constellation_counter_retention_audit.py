#!/usr/bin/env python3
"""Require only original-supported unsigned predicate changes across the TU."""
import hashlib
import json
import argparse
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from gcc3_adid_first_study_audit import masked

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--integrated', action='store_true')
    args = ap.parse_args()
    root = d.ROOT/'build/gcc3-constellation-counter-retention'
    family = 'V90MappingParamsInt'
    ledger = json.loads((root/'results.json').read_text())
    cells = ledger['families'][family]['cells']
    assert len(cells) == 4 and cells['baseline']['baseline_reproduced']
    basepath = root/family/'baseline/candidate.o'
    base = inspect(basepath)
    data = allocated(basepath)
    report = {}
    targets = ('getConstellationMask', 'getCodecConstellationMask')
    caller = 'setV92CPpckFromParamsInfo'
    for label, cell in cells.items():
        path = root/family/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        assert hashlib.sha256((path.parent/(family+'.cpp')).read_bytes()).hexdigest() == cell['source_hash']
        assert inspect(path) == base and allocated(path) == data
        assert not cell.get('gains') and not cell.get('losses')
        enabled = (label.startswith('ordinary-1'), label.endswith('codec-1'))
        changed = [n for n in cell['functions'] if masked(path, n) != masked(basepath, n)]
        expected = {fn for fn, yes in zip(targets, enabled) if yes}
        if any(enabled):
            expected.add(caller)
        assert set(changed) == expected
        changes = {}
        for fn in targets+(caller,):
            a = d.b.insns(str(basepath), fn)
            z = d.b.insns(str(path), fn)
            assert len(a) == len(z)
            diffs = [(i, old, new) for i, (old, new) in enumerate(zip(a, z)) if old != new]
            count = sum(enabled) if fn == caller else int(enabled[targets.index(fn)])
            assert len(diffs) == count
            assert all(old[0] == 'jle' and new[0] == 'jbe' and old[1] == new[1]
                       for _, old, new in diffs)
            if fn in targets and diffs:
                original = d.b.insns(d.b.BLOB, fn)
                # Both original loops use an unsigned predicate immediately
                # after the clearing-counter comparison against seven.
                original_clear = [original[i+1][0] for i, row in enumerate(original[:-1])
                                  if row[0] == 'cmp' and row[1].startswith('$0x7,')]
                assert original_clear == ['jbe']
                i = diffs[0][0]
                assert z[i-1][0] == 'cmp' and z[i-1][1].startswith('$0x7,')
            changes[fn] = diffs
        assert len(cell['verdicts']) == 11
        for fn, grade in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, fn), *d.b.body(str(path), fn))) == grade
        report[label] = {'predicate_changes': changes, 'canonical_body_changes': changed,
                         'exact_gains': 0, 'exact_losses': 0}
    chosen = root/family/'ordinary-1-codec-1/candidate.o'
    if args.integrated:
        assert chosen.read_bytes() == (d.ROOT/'build/tc_out/src_pump_v90_V90MappingParamsInt.cpp.o').read_bytes()
        assert (d.ROOT/'src/pump/v90/V90MappingParamsInt.cpp').read_bytes() == (chosen.parent/(family+'.cpp')).read_bytes()
    output = {'source_cells': 4, 'common_body_verdicts': 44, 'rows': report,
              'integrated_raw_equal': args.integrated, 'partial_function_improvements': 2,
              'inline_predicate_improvements': 2, 'exact_gains': 0, 'exact_losses': 0}
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('Constellation retention: 4 cells/44 verdicts; two original unsigned branches '
          'and two inline copies recovered; all other rows/data/metadata unchanged; integrated=%s' % args.integrated)

if __name__ == '__main__':
    main()
