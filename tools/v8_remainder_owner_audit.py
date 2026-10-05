#!/usr/bin/env python3
"""Audit four bounded V8 full-TU domains and original operand witnesses."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect

PACKAGES = ('v8-remainder-owner', 'v8-bit-extension', 'v8-decision-owner',
            'v8-unsigned-decision')
TARGET = 'v8_fskdemodulate'


def assembly(path):
    text = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn',
                                    '--disassemble='+TARGET, str(path)], text=True)
    return [m.group(1).strip() for line in text.splitlines()
            if (m := re.match(r'^\s*[0-9a-f]+:\s+(.+)$', line))]


def countdowns(rows):
    count = 0
    for i, row in enumerate(rows):
        if row.split()[0] != 'dec':
            continue
        j = i+1
        while j < len(rows) and rows[j].split()[0] == 'mov':
            j += 1
        count += j < len(rows) and rows[j].split()[0] == 'jne'
    return count


def witnesses(rows):
    dispatch = []
    for i, row in enumerate(rows):
        if re.match(r'cmp\s+\$0x1,', row):
            dispatch += [r.split()[0] for r in rows[i+1:i+4]
                         if r.split()[0] in ('jb', 'jle')]
    return {
        'dispatch_below_branches': dispatch,
        'setg_count': sum(bool(re.match(r'setg\s', r)) for r in rows),
        'bit_input_zero_extensions': sum(bool(re.match(
            r'movzwl\s+(?:0x(?:4|6|10|12))\(%\w+\),', r)) for r in rows),
        'bit_input_sign_extensions': sum(bool(re.match(
            r'movswl\s+(?:0x(?:4|6|10|12))\(%\w+\),', r)) for r in rows),
        'dec_jne_backedges': countdowns(rows),
    }


def main():
    reports = []
    objects = {}
    basepath = d.ROOT/'build/production-before/src_v8_V8Dpsk.c.o'
    base = inspect(basepath)
    basebodies = {n: d.b.body(str(basepath), n) for n in base['text_positions']}
    assert len(basebodies) == 4
    original = witnesses(assembly(d.b.BLOB))
    assert original['dispatch_below_branches'] == ['jb'], original
    assert original['bit_input_zero_extensions'] == 4, original
    assert original['dec_jne_backedges'] == 4, original
    for package in PACKAGES:
        root = d.ROOT/'build'/package
        result = json.loads((root/'results.json').read_text())
        assert result['revision'] == '38626248'
        cells = result['families']['V8Dpsk']['cells']
        baseline = cells['baseline']
        assert baseline['baseline_reproduced']
        for label, cell in cells.items():
            path = root/'V8Dpsk'/label/'candidate.o'
            assert cell['compile_exit'] == 0
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            for key in ('records', 'allocated', 'nobits', 'relocations'):
                assert current[key] == base[key], (package, label, key)
            assert set(current['text_positions']) == set(basebodies)
            changed = [n for n, body in basebodies.items()
                       if d.b.body(str(path), n) != body]
            assert changed in ([], [TARGET]), (package, label, changed)
            verdicts = {n: d.b.verdict(*d.b.body(d.b.BLOB, n),
                                      *d.b.body(str(path), n)) for n in basebodies}
            gains = [n for n, v in verdicts.items() if v[0] == 'EXACT'
                     and baseline['verdicts'][n][0] != 'EXACT']
            losses = [n for n, v in verdicts.items() if v[0] != 'EXACT'
                      and baseline['verdicts'][n][0] == 'EXACT']
            assert not gains and not losses, (package, label, gains, losses)
            objects[package, label] = (path, cell)
            reports.append({'package': package, 'label': label,
                            'size': current['text_positions'][TARGET][1],
                            'verdict': verdicts[TARGET], 'changed': changed,
                            'gains': gains, 'losses': losses,
                            'witnesses': witnesses(assembly(path))})
    repeat_pairs = [
        (('v8-remainder-owner', 'published-countdown'),
         ('v8-decision-owner', 'published-countdown-control')),
        (('v8-decision-owner', 'wrapped-captured-switch'),
         ('v8-unsigned-decision', 'signed-switch-control'))]
    for left, right in repeat_pairs:
        lp, lc = objects[left]; rp, rc = objects[right]
        assert lc['source_hash'] == rc['source_hash']
        assert lp.read_bytes() == rp.read_bytes()
    bykey = {(r['package'], r['label']): r['witnesses'] for r in reports}
    signed = bykey['v8-unsigned-decision', 'signed-switch-control']
    unsigned = bykey['v8-unsigned-decision', 'unsigned-switch']
    bits = bykey['v8-unsigned-decision', 'unsigned-switch-zero-bit']
    assert signed['dispatch_below_branches'] == ['jle'], signed
    assert unsigned['dispatch_below_branches'] == ['jb'], unsigned
    assert signed['setg_count'] == unsigned['setg_count'] == 1
    assert unsigned['bit_input_sign_extensions'] == 4, unsigned
    assert bits['bit_input_zero_extensions'] == 4, bits
    assert bits['bit_input_sign_extensions'] == 0, bits
    assert bits['dec_jne_backedges'] == 4, bits
    assert bykey['v8-remainder-owner', 'unsigned-zero-control']['dec_jne_backedges'] == 0
    output = {'cells': len(reports), 'emitted_body_comparisons': 4*len(reports),
              'common_verdicts': 4*len(reports), 'repeat_controls': 2,
              'original_size': d.b.sizes(d.b.BLOB)[TARGET],
              'original_witnesses': original, 'reports': reports,
              'controls': {'signed_dispatch': True, 'unsigned_dispatch': True,
                           'signed_bits': True, 'zero_extended_bits': True,
                           'postdecrement_refusal': True, 'guarded_countdown': True}}
    path = d.ROOT/'build/v8-remainder-owner-audit.json'
    path.write_text(json.dumps(output, indent=2)+'\n')
    print(f"V8 audit: {len(reports)} cells, {4*len(reports)} emitted bodies, "
          f"{4*len(reports)} common verdicts; 2 raw repeats; 6 witness controls; "
          "0 gains, 0 losses; metadata/nontext/bystanders preserved")


if __name__ == '__main__':
    main()
