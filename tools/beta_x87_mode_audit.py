#!/usr/bin/env python3
"""Rescore complete beta mode controls, preserving the separate MMX computation."""
import hashlib
import json
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc_x87_eia6_mechanism import deaths

TARGETS = ('_ZN12V90Equalizer10setDfeBetaEf', '_ZN12V90Equalizer16setLinearEquBetaEf')


def first_fix(path, method):
    from gcc3_value_carriers_audit import function_chunk
    from gcc3_reload_trace import expressions, instructions
    import re
    text = function_chunk(path, 'V90Equalizer::'+method+'(')
    instructions(text)
    start = re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ', text, re.M)
    for node in expressions(text[start.start():]):
        if node and str(node[0]).startswith('insn'):
            pattern = next(child for child in node[2:] if isinstance(child, list))
            if 'fix:SI' in str(pattern):
                return {'uid': int(node[1]), 'pattern': pattern, 'REG_DEAD': deaths(node)}
    raise ValueError('no SI conversion')


def main():
    root = d.ROOT/'build/beta-x87-mode'
    ledger = json.loads((root/'results.json').read_text())
    cells = ledger['families']['V90Equalizer']['cells']
    base = root/'V90Equalizer/baseline/candidate.o'
    assert base.read_bytes() == (d.ROOT/'build/production-before/src_pump_v90_V90Equalizer.cpp.o').read_bytes()
    metadata = inspect(base)
    source_base = (base.parent/'V90Equalizer.cpp').read_text()
    result = []
    for label, cell in cells.items():
        obj = root/'V90Equalizer'/label/'candidate.o'
        source = obj.parent/'V90Equalizer.cpp'
        assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
        assert sorted(d.b.sizes(str(obj))) == cell['functions']
        for name, expected in cell['verdicts'].items():
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
        current = inspect(obj)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == metadata[key], (label, key)
        changed = sorted(n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(base), n))
        assert changed == ([] if label == 'baseline' else sorted(TARGETS))
        traces = {}
        for method, name, marker in (('setDfeBeta', TARGETS[0], '\tdfeBeta = beta;'),
                                    ('setLinearEquBeta', TARGETS[1], '\tlinearEquBeta = beta;')):
            before_fn = d.function(source_base, 'V90Equalizer::'+method)[2]
            after_fn = d.function(source.read_text(), 'V90Equalizer::'+method)[2]
            assert before_fn[before_fn.index(marker):] == after_fn[after_fn.index(marker):], 'MMX source drift'
            before = first_fix(obj.parent/'V90Equalizer.cpp.33.sched2', method)
            after = first_fix(obj.parent/'V90Equalizer.cpp.34.stack', method)
            assert before['uid'] == after['uid'] and not before['REG_DEAD']
            assert bool(after['REG_DEAD']) == (label == 'baseline')
            codes = [m for m, o in d.b.insns(str(obj), name) if m.startswith(('fist', 'fmul', 'fcom'))]
            assert next(m for m in codes if m.startswith('fist')) == ('fistpl' if label == 'baseline' else 'fistl')
            traces[name] = {'sched2': before, 'stack': after, 'opcodes': codes,
                            'size': d.b.sizes(str(obj))[name], 'verdict': cell['verdicts'][name]}
        assert not cell.get('gains') and not cell.get('losses')
        result.append({'cell': label, 'body_grades': len(cell['verdicts']), 'changed': changed, 'traces': traces})
    assert sum(c['body_grades'] for c in result) == 105
    (root/'audit.json').write_text(json.dumps({'cells': result, 'body_grades': 105, 'gains': [], 'losses': []}, indent=2)+'\n')
    print('3 complete TUs /105 live grades; 33 bystanders, metadata/data/BSS/nontext and MMX source unchanged')
    print('Both SF/DF diagnostics recover live FISTL; both setters remain351vs350B,0gains/losses')


if __name__ == '__main__':
    main()
