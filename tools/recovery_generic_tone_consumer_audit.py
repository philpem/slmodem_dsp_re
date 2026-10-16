#!/usr/bin/env python3
"""Audit the threshold tree operand boundary, shared RTL product and full TU."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import playbook_small_patterns as driver
from gcc3_reload_trace import instructions
from gcc3_stage_divergence import chunks
from gcc3_value_carriers_audit import inspect

TARGET = '_ZN19GenericToneDetector7processEf'
HEADER = 'int GenericToneDetector::process(float)'
FOLD_SHA256 = 'a9ebc6ac7de8b53e8489c26ba6cea520fe261bb58ead338b86a8164c7d76277d'
FOLD_URL = 'https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-3.4.2/gcc/fold-const.c'


def tree_predicate(path, target=TARGET):
    sections = re.split(r'^;; Function ', path.read_text(), flags=re.M)[1:]
    text, = [section for section in sections if '(' + target + ')' in section.splitlines()[0]]
    nodes = {key: (code, body) for key, code, body in re.findall(
        r'^@(\d+)\s+(\w+)(.*?)(?=^@|\Z)', text, re.M | re.S)}

    def operands(key):
        return re.findall(r'op [01]:\s+@(\d+)', nodes[key][1])

    def name(key):
        code, body = nodes[key]
        if code == 'component_ref':
            key = operands(key)[1]
            code, body = nodes[key]
        if code in ('field_decl', 'var_decl'):
            identifier = re.search(r'name:\s+@(\d+)', body).group(1)
            return re.search(r'strg:\s+(\w+)', nodes[identifier][1]).group(1)
        return None

    found = []
    for key, (code, body) in nodes.items():
        if code not in ('le_expr', 'ge_expr'):
            continue
        args = operands(key)
        if len(args) == 2 and any(name(arg) == 'threshold' for arg in args):
            found.append({'node': key, 'code': code,
                          'operands': [{'code': nodes[arg][0], 'name': name(arg)} for arg in args]})
    result, = found
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path,
                        default=driver.ROOT / 'build/recovery-generic-tone-consumer-expression')
    parser.add_argument('--gcc-fold-source', type=Path,
                        default=driver.ROOT / 'build/tone-gcc-source/fold-const.c')
    args = parser.parse_args()
    raw = args.gcc_fold_source.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == FOLD_SHA256
    source = raw.decode("latin-1")
    assert 'if (DECL_P (arg1))\n    return 0;\n  if (DECL_P (arg0))\n    return 1;' in source
    assert 'return fold (build (swap_tree_comparison (code), type, arg1, arg0));' in source
    root = args.run_dir
    family = json.loads((root / 'results.json').read_text())['families']['GenericToneDetector']
    base = root / 'GenericToneDetector/baseline/candidate.o'
    metadata = inspect(base)
    report = {'stock_GCC_source': {'url': FOLD_URL, 'sha256': FOLD_SHA256,
                                 'scope': 'stock source rule; actual Gentoo tree/RTL controls below'}, 'cells': {}}
    for label, cell in family['cells'].items():
        folder = root / 'GenericToneDetector' / label
        obj = folder / 'candidate.o'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        actual = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert actual[key] == metadata[key], (label, key)
        changed = [name for name in cell['functions']
                   if driver.b.body(str(base), name) != driver.b.body(str(obj), name)]
        adopted = label == 'product-at-threshold'
        assert changed == ([TARGET] if adopted else [])
        verdicts = {name: list(driver.b.verdict(*driver.b.body(driver.b.BLOB, name),
                                              *driver.b.body(str(obj), name)))
                    for name in cell['verdicts']}
        assert verdicts == cell['verdicts'] and not cell.get('losses')
        predicate = tree_predicate(folder / 'GenericToneDetector.cpp.original')
        assert predicate['code'] == ('ge_expr' if adopted else 'le_expr')
        assert [operand['code'] for operand in predicate['operands']] == (
            ['mult_expr', 'component_ref'] if adopted else ['component_ref', 'var_decl'])
        trace = {}
        for stage in ('01.rtl', '06.cse', '20.combine', '25.greg', '33.sched2', '34.stack'):
            text, = chunks(folder / ('GenericToneDetector.cpp.' + stage), HEADER)
            stream = instructions(text)
            if stage in ('01.rtl', '06.cse'):
                products = [uid for uid, pattern in stream.items()
                            if 'mult:SF' in str(pattern) and "'out'" in str(pattern) and "'inv'" in str(pattern)]
                assert len(products) == (2 if adopted and stage == '01.rtl' else 1)
            else:
                products = None
            uid = (101 if adopted else 100) if stage <= '25.greg' else (281 if adopted else 280)
            assert uid in stream
            if adopted and stage == '06.cse':
                assert 'meanOut' in str(stream[uid])
            if adopted and stage == '20.combine':
                assert 'threshold' in str(stream[uid]) and 'mem/s:SF' in str(stream[uid]) and 100 not in stream
            trace[stage] = {'comparison_uid': uid, 'comparison': stream[uid],
                            'out_times_inv_product_uids': products}
        report['cells'][label] = {'tree_predicate': predicate, 'stage_trace': trace,
                                 'verdicts': verdicts, 'changed_bodies': changed}
    assert len(report['cells']) == 2
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('2 complete TUs / 14 verdicts: scalar EXACT318B; 6 bystanders and all nontext/bindings preserved')
    print('2 tree controls / 12 stage streams: declaration swap avoided, duplicate product CSE-shared, memory threshold folded')
    print('Pinned official GCC source rule verified; no original RTL or unique source spelling claimed')


if __name__ == '__main__':
    main()
