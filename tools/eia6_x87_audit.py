#!/usr/bin/env python3
"""Rescore all EIA6 controls and record full floating RTL roots and death notes."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect, function_chunk
from gcc3_reload_trace import expressions, instructions
from gcc_x87_eia6_mechanism import deaths

TARGET = '_ZN12V90PreFilter12setParamEia6Ev'
PACKAGES = ('eia6-x87-trace', 'eia6-x87-type', 'eia6-x87-default-math')
STAGES = ('01.rtl', '19.life', '20.combine', '22.regmove', '24.lreg',
          '25.greg', '26.postreload', '27.flow2', '33.sched2', '34.stack')
OPERATIONS = ('fix:', 'mult:', 'minus:', 'abs:', 'float_truncate:', 'compare:CCFP')


def unused(node):
    if not isinstance(node, list):
        return []
    if node and node[0] == 'expr_list:REG_UNUSED':
        return [node[1]]+unused(node[2])
    return sum((unused(child) for child in node), [])


def assembly_packets(path):
    text = path.read_text()
    start = text.index(TARGET+':\n')+len(TARGET)+2
    end = text.index('\t.size\t'+TARGET, start)
    packets, uid = {}, None
    for line in text[start:end].splitlines():
        match = re.match(r'^\t([a-z][a-z0-9]*)\s*(.*?)\s*(?:# (\d+)\t.*)?$', line)
        if not match:
            continue
        if match[3]:
            uid = int(match[3])
        if uid is not None:
            packets.setdefault(uid, []).append([match[1], match[2]])
    return packets


def stream(path):
    text = function_chunk(path, 'V90PreFilter::setParamEia6(')
    instructions(text)  # Validate unique IDs and balanced instruction patterns.
    start = re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ', text, re.M)
    return [n for n in expressions(text[start.start():]) if n and
            re.fullmatch(r'(?:insn|jump_insn|call_insn)(?::\S+)?', n[0])]


def floating(path):
    nodes = stream(path)
    result = []
    for index, node in enumerate(nodes):
        pattern = next(child for child in node[2:] if isinstance(child, list))
        if any(op in str(pattern) for op in OPERATIONS):
            result.append({'uid': int(node[1]), 'pattern': pattern, 'REG_DEAD': deaths(node), 'REG_UNUSED': unused(node),
                           'previous_uid': int(nodes[index-1][1]) if index else None})
    assert result and any('fix:SI' in str(r['pattern']) for r in result)
    return result


def tree_declaration(path):
    text = path.read_text()
    nodes = {int(m[1]): (m[2], m[3]) for m in re.finditer(
        r'^@(\d+)\s+(\w+)\s+(.*?)(?=^@\d+|\Z)', text, re.M | re.S)}
    ids = [key for key, (kind, body) in nodes.items() if kind == 'identifier_node' and
           re.search(r'\bstrg:\s*'+re.escape(TARGET)+r'\s', body)]
    assert len(ids) == 1
    matches = [(key, body) for key, (kind, body) in nodes.items() if kind == 'function_decl' and
               re.search(r'\bmngl:\s*@'+str(ids[0])+r'\b', body)]
    assert len(matches) == 1
    key, body = matches[0]
    return {'function_decl': key, 'body_present': 'body:' in body, 'declaration': body.strip()}


def main():
    baseline = d.ROOT/'build/production-before/src_pump_v90_V90PreFilter.cpp.o'
    metadata = inspect(baseline)
    report = {'cells': [], 'gains': [], 'losses': []}
    for package in PACKAGES:
        root = d.ROOT/'build'/package
        ledger = json.loads((root/'results.json').read_text())
        assert ledger['revision'] == '2ca02aec'
        family = ledger['families']['V90PreFilter']
        assert (root/'V90PreFilter/baseline/candidate.o').read_bytes() == baseline.read_bytes()
        for label, cell in family['cells'].items():
            obj = root/'V90PreFilter'/label/'candidate.o'
            source = obj.parent/'V90PreFilter.cpp'
            assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
            assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
            assert sorted(d.b.sizes(str(obj))) == cell['functions']
            for name, expected in cell['verdicts'].items():
                assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
            changed = sorted(n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(baseline), n))
            assert changed == ([] if label == 'baseline' else [TARGET])
            assert not cell.get('gains') and not cell.get('losses')
            current = inspect(obj)
            for key in ('records', 'nobits', 'relocations'):
                assert current[key] == metadata[key], (package, label, key)
            assert set(current['allocated']) == set(metadata['allocated'])
            data = {name: value for name, value in current['allocated'].items() if value != metadata['allocated'][name]}
            assert not data or data == {'.rodata.cst4': metadata['allocated']['.rodata.cst4']+'00000000'}
            if package == 'eia6-x87-trace':
                prior = d.ROOT/'build/eia6-prior-controls/V90PreFilter'/label/'candidate.o'
                assert obj.read_bytes() == prior.read_bytes(), 'tree diagnostics changed object'
            trace = {stage: floating(obj.parent/('V90PreFilter.cpp.'+stage)) for stage in STAGES}
            packets = assembly_packets(obj.parent/'V90PreFilter.s')
            unmatched = []
            for operation in trace['34.stack']:
                operation['assembly'] = packets.get(operation['uid'], [])
                if not operation['assembly']:
                    unmatched.append(operation['uid'])
            assert not unmatched, ('unmapped final floating operations', package, label, unmatched)
            before = next(r for r in trace['33.sched2'] if 'fix:SI' in str(r['pattern']))
            after = next(r for r in trace['34.stack'] if 'fix:SI' in str(r['pattern']))
            assert before['uid'] == after['uid'] and not before['REG_DEAD']
            xf = 'reg/v:XF' in str(before['pattern'])
            assert bool(after['REG_DEAD']) == xf, (package, label)
            opcodes = [m for m, operands in d.b.insns(str(obj), TARGET) if m.startswith(('fist', 'fmul', 'fcom'))]
            assert next(m for m in opcodes if m.startswith('fist')) == ('fistpl' if xf else 'fistl')
            tree = tree_declaration(obj.parent/'V90PreFilter.cpp.tu')
            assert not tree['body_present'], 'revisit tree-body capability'
            report['cells'].append({'package': package, 'cell': label, 'body_grades': len(cell['verdicts']),
                'changed_bodies': changed, 'anonymous_constant_delta': data, 'tree': tree,
                'target_bytes': d.b.sizes(str(obj))[TARGET], 'grade': cell['verdicts'][TARGET],
                'conversion_source_xf': xf, 'opcodes': opcodes, 'unmapped_final_ops': unmatched, 'floating_trace': trace})
    report['cell_count'] = len(report['cells'])
    report['body_grades'] = sum(r['body_grades'] for r in report['cells'])
    report['stage_streams'] = report['cell_count']*len(STAGES)
    assert report['cell_count'] == 15 and report['body_grades'] == 240
    (d.ROOT/'build/eia6-x87-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('15 full-TU controls (including 4 diagnostic replays) /240 live grades /150 RTL streams')
    print('Raw baselines/tree controls, all bystanders/bindings/BSS/nontext relocations pass; only anonymous +0 pool deltas')
    print('Live XF gains physical death/pop at stack; live SF/DF retains nonpopping first conversion. Gains 0, losses 0')
    print('15 translation-unit trees identify declaration but omit target body; no tree-body proof claimed')


if __name__ == '__main__':
    main()
