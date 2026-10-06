#!/usr/bin/env python3
"""Audit all CRC controls, changed table destinations and nested copy loops."""
import hashlib
import json
import re
from pathlib import Path
import playbook_small_patterns as d
from gcc3_value_carriers_audit import inspect
import jumptable
from elftools.elf.elffile import ELFFile

TARGETS = {'V90Jd': ('_ZN5V90Jd10unPackDataEi',), 'V92Jd': (
    '_ZN5V92Jd12unPackJdDataEi', '_ZN5V92Jd17unPackJdPhaseDataEi')}


def input_loop(path, name):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbol = elf.get_section_by_name('.symtab').get_symbol_by_name(name)[0]
        start = symbol['st_value']
        section = elf.get_section(symbol['st_shndx'])
        rows = jumptable.instructions(str(path), section.name, start, start+symbol['st_size'])
    edges = []
    for address, raw, mnemonic, operands in rows:
        match = re.match(r'([0-9a-f]+)\s+<', operands)
        if mnemonic.startswith('j') and mnemonic != 'jmp' and match:
            target = int(match[1], 16)
            if target < address:
                edges.append((target, address))
    loops = []
    for target, address in edges:
        body = [r for r in rows if target <= r[0] <= address]
        comparisons = [r for r in body if r[2].startswith(('cmp', 'test'))]
        if comparisons and comparisons[-1][2].startswith('cmp') and '$0x1f,' in comparisons[-1][3]:
            loops.append((target, address, body))
    assert len(loops) == 1, (path, name, len(loops))
    target, address, body = loops[0]
    nested = [(a-start, b-start) for a, b in edges if target <= a <= b <= address]
    return {'input_loop': [target-start, address-start], 'backedges_inside': nested}


def main():
    root = d.ROOT/'build/jd-unpack-fixed-shift'
    ledger = json.loads((root/'results.json').read_text())
    report = {'cells': [], 'gains': [], 'losses': []}
    for family, content in ledger['families'].items():
        baseline = root/family/'baseline/candidate.o'
        retained = d.ROOT/'build/production-before'/('src_pump_v90_'+family+'.cpp.o')
        assert baseline.read_bytes() == retained.read_bytes()
        metadata = inspect(baseline)
        for label, cell in content['cells'].items():
            obj = root/family/label/'candidate.o'
            source = obj.parent/(family+'.cpp')
            assert hashlib.sha256(obj.read_bytes()).hexdigest() == cell['object_hash']
            assert hashlib.sha256(source.read_bytes()).hexdigest() == cell['source_hash']
            assert sorted(d.b.sizes(str(obj))) == cell['functions']
            for name, expected in cell['verdicts'].items():
                assert list(d.b.verdict(*d.b.body(d.b.BLOB, name), *d.b.body(str(obj), name))) == expected
            current = inspect(obj)
            for key in ('records', 'allocated', 'nobits'):
                assert current[key] == metadata[key], (family, label, key)
            changed = sorted(n for n in cell['functions'] if d.b.body(str(obj), n) != d.b.body(str(baseline), n))
            assert changed == sorted(cell.get('changed_bodies', []))
            assert set(changed) <= set(TARGETS[family])
            assert len(current['relocations']) == len(metadata['relocations'])
            deltas = []
            for before, after in zip(metadata['relocations'], current['relocations']):
                if before != after:
                    assert before[:3] == after[:3] and before[0] == '.rodata'
                    assert before[3][:2] == after[3][:2]
                    assert before[3][0] == 'audited-code-destination' and before[3][1] in changed
                    deltas.append([before, after])
            traces = {}
            for name in TARGETS[family]:
                own = input_loop(obj, name)
                reference = input_loop(Path(d.b.BLOB), name)
                assert len(reference['backedges_inside']) == 1
                assert len(own['backedges_inside']) == (1 if name in changed else 2)
                traces[name] = {'ours': own, 'original': reference,
                                'size': d.b.sizes(str(obj))[name], 'verdict': cell['verdicts'][name]}
            assert not cell.get('gains') and not cell.get('losses')
            report['cells'].append({'family': family, 'label': label,
                'body_grades': len(cell['verdicts']), 'changed_bodies': changed,
                'relocation_deltas': deltas, 'loop_traces': traces})
    report['cell_count'] = len(report['cells'])
    report['live_body_grades'] = sum(r['body_grades'] for r in report['cells'])
    assert report['cell_count'] == 6 and report['live_body_grades'] == 114
    (root/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('6 full-TU controls / 114 live body grades; raw baselines, bindings/data/BSS and bystanders pass')
    print('All changed switch destinations remain in their changed owner at decoded instruction boundaries')
    print('Three expanded CRC consumers remove the nested copy backedge; original has none. Gains 0, losses 0')


if __name__ == '__main__':
    main()
