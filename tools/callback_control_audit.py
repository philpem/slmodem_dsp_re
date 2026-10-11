#!/usr/bin/env python3
"""Audit indirect-callback and macro-loop source crosses, including all misses."""
import hashlib
import json
import re
import subprocess
import playbook_small_patterns as d
from helper_caller_audit import inspect, allocated
from elftools.elf.elffile import ELFFile

ROOTS = ('gcc3-getframe-callback-expansion', 'gcc3-dftenergy-loop-exit',
         'gcc3-dftenergy-count-exit', 'gcc3-fskdetect-phase-delay',
         'gcc3-fskdetect-phase-read-age')


def callback_reads(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbol = next(s for s in elf.get_section_by_name('.symtab').iter_symbols() if s.name == 'getFrame')
        # Displacement of the callback in this shell view; count exact instructions
        # through disassembly, not occurrences of byte sequences in instructions/data.
        raw = subprocess.check_output(['objdump','-d','--no-show-raw-insn',
            '--start-address='+str(symbol['st_value']), '--stop-address='+str(symbol['st_value']+symbol['st_size']),str(path)], text=True)
        lines = raw.splitlines()
        return {'owner_callback_mentions': [line.strip() for line in lines if '0xe48(' in line],
                'indirect_call_lines': [line.strip() for line in lines if '\tcall' in line and '*' in line]}


def coefficient_age(path):
    instructions = d.b.insns(str(path), 'fskdetect')
    coefficient = next(i for i,(m,o) in enumerate(instructions) if 'intcoef3' in o)
    operand = re.sub(r'@R_386_32:\([^)]*\)', '', instructions[coefficient][1])
    if '(' not in operand:
        register = re.findall(r'%e[a-z]{2}', operand)[-1]
        coefficient = next(i for i,(m,o) in enumerate(instructions) if i > coefficient
                           and m.startswith('mov') and '('+register+')' in o)
    stores = [i for i,(m,o) in enumerate(instructions) if m.startswith('mov')
              and re.search(r'%[a-z0-9]+,.*\(', o) and '%esp' not in o]
    previous = max(i for i in stores if i < coefficient)
    following = min(i for i in stores if i > coefficient)
    return 'before-transfer' if following-coefficient < coefficient-previous else 'after-transfer'


def main():
    assert coefficient_age(d.b.BLOB) == 'before-transfer'
    report, ncells, verdicts = {}, 0, 0
    for name in ROOTS:
        root = d.ROOT/'build'/name
        ledger = json.loads((root/'results.json').read_text())
        family = next(iter(ledger['families']))
        cells = ledger['families'][family]['cells']
        baseline = root/family/'baseline/candidate.o'
        base, data = inspect(baseline), allocated(baseline)
        rows = {}
        for label, cell in cells.items():
            path = root/family/label/'candidate.o'
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            current = inspect(path)
            assert current == base, (name,label,'metadata/named-data/sizes')
            assert allocated(path) == data, (name,label,'allocated nontext')
            assert not cell.get('gains') and not cell.get('losses')
            row = {'common_body_verdicts': len(cell['verdicts']),
                   'changed_bodies': cell.get('changed_bodies',[]),
                   'metadata_named_allocated_data_unchanged': True}
            if family == 'v34shell':
                calls = callback_reads(path)
                count = len(calls['indirect_call_lines'])
                assert count == (6 if label.startswith('expand-1-') else 3)
                captured = '-capture-1-' in label
                assert len(calls['owner_callback_mentions']) == (1 if captured else count)
                if captured:
                    assert all('(%esp)' in line for line in calls['indirect_call_lines'])
                row['callback_controls'] = calls
            if family == 'DPSK' and label.startswith('transfer-1-'):
                row['coefficient_read_age'] = coefficient_age(path)
                corrected = name.endswith('read-age')
                assert row['coefficient_read_age'] == ('before-transfer' if corrected else 'after-transfer')
                row['supported_original_read_age'] = corrected
            rows[label] = row
            ncells += 1
            verdicts += len(cell['verdicts'])
        report[name] = rows
        print(name,len(rows),'cells: metadata, data and complete-TU controls pass')
    assert (ncells,verdicts) == (33,158), (ncells,verdicts)
    report['denominator'] = {'cells':ncells,'common_body_verdicts':verdicts}
    (d.ROOT/'build/callback-control-audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(ncells,'cells /',verdicts,'common verdicts; zero exact gains/losses')


if __name__ == '__main__':
    main()
