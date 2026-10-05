#!/usr/bin/env python3
"""Audit V8 index/cursor full-TU controls and explicit pointer formations."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
from gcc3_reload_trace import function, instructions

TARGET = 'v8_fskdemodulate'


def dump(path):
    return subprocess.check_output(['objdump', '-d', '--no-show-raw-insn',
                                    '--disassemble='+TARGET, str(path)], text=True)


def convolution_branches(text):
    rows = text.splitlines()
    start = next(i for i, r in enumerate(rows) if '0xdd8(' in r)
    end = next(i for i in range(start, len(rows)) if re.search(r'\bsar[l]?\s', rows[i]))
    return [m.group(1) for r in rows[start:end]
            if (m := re.search(r'\b(jle|jbe|jae)\s', r))]


def input_countdown(text):
    rows = text.splitlines()
    end = next(i for i, r in enumerate(rows) if re.search(r'\bcmpw\s+\$0xc,', r))
    return any(re.search(r'\bjns\s', r) for r in rows[:end])


def formations(path):
    text = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', str(path)], text=True)
    owner = None
    result = []
    instructions = 0
    for row in text.splitlines():
        if m := re.match(r'^[0-9a-f]+ <(.+)>:', row):
            owner = m.group(1)
        instructions += bool(re.match(r'^\s*[0-9a-f]+:\s', row))
        if re.search(r'\b(?:lea|add)\s+.*(?:0xc20\(|\$0xc20|0xc2c\(|\$0xc2c)', row):
            result.append({'owner': owner, 'instruction': row.strip()})
    return {'instructions_screened': instructions, 'formations': result}


def walk(node):
    if isinstance(node, list):
        yield node
        for child in node[1:]:
            yield from walk(child)


def tap_conditions(initial):
    j = re.search(r'reg/v:SI (\d+) \[ j \]', initial).group(1)
    stream = list(instructions(initial).items())
    found = []
    for i, (uid, pattern) in enumerate(stream[:-1]):
        comparisons = [n for n in walk(pattern) if n[0].startswith('compare:')]
        for comparison in comparisons:
            if not any(n[0].startswith('reg') and len(n) > 1 and n[1] == j
                       for n in walk(comparison)):
                continue
            following = stream[i+1][1]
            conditions = [n[1][0] for n in walk(following)
                          if n[0] == 'if_then_else']
            assert len(conditions) == 1, (uid, conditions)
            found.append({'uid': uid, 'comparison_mode': comparison[0],
                          'exit_condition': conditions[0]})
    assert len(found) == 2, found
    return found


def main():
    root = d.ROOT/'build/v8-index-cursor'
    cells = json.loads((root/'results.json').read_text())['families']['V8Dpsk']['cells']
    assert len(cells) == 5 and cells['baseline']['baseline_reproduced']
    baseline = root/'V8Dpsk/baseline/candidate.o'
    original = formations(d.b.BLOB)
    assert len(original['formations']) == 2
    assert {r['owner'] for r in original['formations']} == {TARGET, 'v8_fskmodulate'}
    original_text = dump(d.b.BLOB)
    assert convolution_branches(original_text) == ['jbe', 'jbe']
    assert input_countdown(original_text)
    base = inspect(baseline)
    bodies = {n: d.b.body(str(baseline), n) for n in base['text_positions']}
    assert len(bodies) == 4
    reports = []
    for label, cell in cells.items():
        path = root/'V8Dpsk'/label/'candidate.o'
        assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
        current = inspect(path)
        for key in ('records', 'allocated', 'nobits', 'relocations'):
            assert current[key] == base[key], (label, key)
        assert set(current['text_positions']) == set(bodies)
        changed = [n for n, body in bodies.items() if d.b.body(str(path), n) != body]
        assert changed in ([], [TARGET]), (label, changed)
        assert not cell.get('gains') and not cell.get('losses')
        for n in bodies:
            assert list(d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(path), n))) == cell['verdicts'][n]
        text = dump(path)
        branches = convolution_branches(text)
        unsigned = label in ('unsigned-tap-index', 'unsigned-index-and-cursor')
        assert branches == (['jae', 'jbe'] if unsigned else ['jle', 'jle']), (label, branches)
        cursor = label in ('countdown-input-cursor', 'unsigned-index-and-cursor')
        assert input_countdown(text) == cursor, label
        # Initial expansion already records unsigned flags, before allocation.
        initial = function((path.parent/'V8Dpsk.c.01.rtl').read_text(), TARGET)
        conditions = tap_conditions(initial)
        assert [r['exit_condition'] for r in conditions] == (['ltu', 'gtu'] if unsigned else ['gt', 'gt']), (label, conditions)
        reports.append({'label': label, 'size': current['text_positions'][TARGET][1],
                        'verdict': cell['verdicts'][TARGET], 'changed': changed,
                        'tap_branches': branches, 'initial_tap_conditions': conditions, 'input_countdown': cursor})
    control = root/'V8Dpsk/decision-control/candidate.o'
    old = d.ROOT.parent/'v8-remainder-owner/build/v8-unsigned-decision/V8Dpsk/unsigned-switch-zero-bit/candidate.o'
    # Sibling package is optional for portable replay; cell hash pins the repeat.
    assert cells['decision-control']['object_hash'] == '912d3a101d984ae4d5c204e9f6812f75d452db95e69577fde17fa8e03d3a1ee5', 'repeat hash changed'
    if old.exists():
        assert control.read_bytes() == old.read_bytes()
    output = {'cells': 5, 'emitted_body_comparisons': 20, 'common_verdicts': 20,
              'controls': {'unsigned_taps': True, 'signed_taps_refusal': True,
                           'input_cursor': True, 'indexed_input_refusal': True},
              'original_pointer_screen': original, 'reports': reports,
              'exact_gains': [], 'exact_losses': []}
    (root/'audit.json').write_text(json.dumps(output, indent=2)+'\n')
    print('V8 index/cursor: 5 cells, 20 bodies, 20 common verdicts; '
          '4 witness controls; metadata/nontext/bystanders equal; 0 gains/losses')
    print(f"Pointer screen: {original['instructions_screened']} instructions, "
          '2 explicit formations; receive pointer only in target')


if __name__ == '__main__':
    main()
