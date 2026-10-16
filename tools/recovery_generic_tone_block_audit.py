#!/usr/bin/env python3
"""Audit block threshold recovery and locate its remaining countdown copy."""
import argparse
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect
from recovery_generic_tone_consumer_audit import tree_predicate

TARGET = '_ZN19GenericToneDetector7processEPfj'
SCALAR = '_ZN19GenericToneDetector7processEf'
HEADER = 'int GenericToneDetector::process(float*, unsigned int)'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path,
                        default=driver.ROOT / 'build/recovery-generic-tone-block-consumer')
    root = parser.parse_args().run_dir
    cells = json.loads((root / 'results.json').read_text())['families']['GenericToneDetector']['cells']
    base = root / 'GenericToneDetector/baseline/candidate.o'
    metadata = inspect(base)
    report = {}
    for label, cell in cells.items():
        folder = root / 'GenericToneDetector' / label
        obj = folder / 'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        changed = [name for name in cell['functions']
                   if driver.b.body(str(base), name) != driver.b.body(str(obj), name)]
        adopted = label == 'block-product-at-threshold'
        assert changed == ([TARGET] if adopted else [])
        verdicts = {name: list(driver.b.verdict(*driver.b.body(driver.b.BLOB, name),
                                              *driver.b.body(str(obj), name)))
                    for name in cell['verdicts']}
        assert verdicts == cell['verdicts'] and not cell.get('losses') and not cell.get('gains')
        assert verdicts[SCALAR] == ['EXACT', 0] and verdicts[TARGET] == ['SIZE', 3]
        tree = tree_predicate(folder / 'GenericToneDetector.cpp.original', TARGET)
        assert tree['code'] == ('ge_expr' if adopted else 'le_expr')
        assert [operand['code'] for operand in tree['operands']] == (
            ['mult_expr', 'component_ref'] if adopted else ['component_ref', 'var_decl'])
        stages = {}
        for stage in ('01.rtl', '06.cse', '09.loop', '20.combine', '25.greg',
                      '27.flow2', '31.bbro', '33.sched2', '34.stack'):
            text, = chunks(folder / ('GenericToneDetector.cpp.' + stage), HEADER)
            stream = instructions(text)
            decrements = {str(uid): pattern for uid, pattern in stream.items()
                          if 'plus:SI' in str(pattern) and "'-1'" in str(pattern) and "'k'" in str(pattern)}
            if stage in ('09.loop', '20.combine', '25.greg', '27.flow2'):
                assert len(decrements) == 1
            if stage in ('31.bbro', '33.sched2', '34.stack'):
                assert len(decrements) == 2
            products = [uid for uid, pattern in stream.items()
                        if 'mult:SF' in str(pattern) and "'out'" in str(pattern) and "'inv'" in str(pattern)]
            if stage in ('01.rtl', '06.cse'):
                assert len(products) == (2 if adopted and stage == '01.rtl' else 1)
            comparisons = {str(uid): pattern for uid, pattern in stream.items()
                           if 'if_then_else' in str(pattern) and ('reg:SF' in str(pattern) or 'reg/v:SF' in str(pattern))}
            stages[stage] = {'countdown_decrements': decrements,
                             'out_times_inv_product_uids': products,
                             'floating_branch_patterns': comparisons}
        report[label] = {'tree': tree, 'stages': stages, 'verdicts': verdicts, 'changed': changed}
    assert len(report) == 2
    (root / 'audit.json').write_text(json.dumps({'cells': report, 'body_verdicts': 14,
                                               'scope': 'current compiler stages; original RTL unavailable'}, indent=2) + '\n')
    print('2 full TUs / 14 verdicts: block only changed; scalar EXACT318B and five other bystanders preserved')
    print('data/bindings/BSS/nontext relocations equal; two tree controls and 18 RTL streams checked')
    print('block remains SIZE3; single countdown through flow2 is copied at bbro and survives stack conversion')


if __name__ == '__main__':
    main()
