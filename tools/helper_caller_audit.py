#!/usr/bin/env python3
"""Audit every bounded helper/caller cell, including misses and bystanders."""
import hashlib
import json
import playbook_small_patterns as d
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ROOTS = ('gcc3-v34-putframe-expansion', 'gcc3-v34-putframe-capture',
         'gcc3-v34-putframe-owner', 'gcc3-v34-snapshot-width')
source = (d.ROOT / 'tools/playbook_bwch_unit_audit.py').read_text()
driver = d
b = d.b
exec(source[source.index('def inspect'):source.index('reports=')], globals())


def allocated(path):
    rows = {}
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        for index, section in enumerate(elf.iter_sections()):
            if not section['sh_flags'] & 2 or section.name == '.text':
                continue
            data = bytearray(section.data())
            for relsec in elf.iter_sections():
                if isinstance(relsec, RelocationSection) and relsec['sh_info'] == index:
                    for rel in relsec.iter_relocations():
                        data[rel['r_offset']:rel['r_offset'] + 4] = b'\0' * 4
            rows[section.name] = data.hex()
    return rows


def main():
    original = d.b.insns(d.b.BLOB, 'putFrame')
    assert sum(m == 'call' and o.startswith('*') for m, o in original) == 18
    report, cells, verdicts = {}, 0, 0
    for name in ROOTS:
        root = d.ROOT / 'build' / name
        ledger = json.loads((root / 'results.json').read_text())
        family = next(iter(ledger['families']))
        controls = ledger['families'][family]['cells']
        baseline = root / family / 'baseline/candidate.o'
        base = inspect(baseline)
        data = allocated(baseline)
        rows = {}
        for label, entry in controls.items():
            path = root / family / label / 'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry['object_hash']
            current = inspect(path)
            assert current['records'] == base['records'], (name, label, 'symbol metadata')
            assert current['allocated_sizes'] == base['allocated_sizes'], (name, label, 'nontext sizes')
            assert allocated(path) == data, (name, label, 'relocation-cleared allocated data')
            row = {'symbol_metadata_unchanged': True, 'allocated_data_unchanged': True,
                   'named_data_changes': [n for n in current['objects']
                                          if current['objects'][n] != base['objects'][n]],
                   'common_body_verdicts': len(entry['verdicts']),
                   'target_verdict': entry['verdicts']['putFrame' if family == 'v34shell' else 'v34handshak'],
                   'exact_gains': entry.get('gains', []), 'exact_losses': entry.get('losses', [])}
            assert not row['named_data_changes']
            assert not row['exact_gains'] and not row['exact_losses']
            if family == 'v34shell':
                instructions = d.b.insns(str(path), 'putFrame')
                row['indirect_call_sites'] = sum(m == 'call' and o.startswith('*') for m, o in instructions)
                rtl = (path.parent / 'v34shell.c.01.rtl').read_text().split(';; Function putFrame', 1)[1].split(';; Function ', 1)[0]
                row['initial_rtl_call_sites'] = rtl.count('(call_insn')
                expected = 6 if label == 'baseline' or label.startswith('expand-0-') else 18
                assert row['indirect_call_sites'] == expected, (name, label, row)
                assert row['initial_rtl_call_sites'] == expected + 1, (name, label, row)
            rows[label] = row
            cells += 1
            verdicts += len(entry['verdicts'])
        report[name] = rows
        print(name, len(rows), 'cells: metadata/data controls pass; named-data changes',
              {n: r['named_data_changes'] for n, r in rows.items() if r['named_data_changes']})
    assert (cells, verdicts) == (22, 332), (cells, verdicts)
    report['denominator'] = {'cells': cells, 'common_body_verdicts': verdicts}
    (d.ROOT / 'build/helper-caller-cell-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print(cells, 'cells /', verdicts, 'common body verdicts; zero gains or losses')


if __name__ == '__main__':
    main()
