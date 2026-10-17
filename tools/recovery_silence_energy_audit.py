#!/usr/bin/env python3
"""Audit silence operand ownership, all sibling bodies, data and exports."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
import playbook_small_patterns as d
from recovery_cid_terminal_audit import inspect
from elftools.elf.elffile import ELFFile


def decisions(path):
    text = subprocess.check_output(
        ['python3', str(d.ROOT/'tools/dis.py'), str(path), 'silence_progress'],
        text=True, stderr=subprocess.DEVNULL)
    lines = text.splitlines()
    indexed = [line for line in lines if 'fcomps' in line and ',4)' in line]
    branch_pairs = []
    for i, line in enumerate(lines):
        if 'fcom' not in line:
            continue
        following = next(s for s in lines[i+1:] if re.search(r'\bj[a-z]+\s', s))
        branch_pairs.append([line, following])
    return {'indexed_thresholds': indexed,
            'register_counter_compares': [line for line in lines if 'fcompp' in line],
            'float_compares': [line for line in lines if 'fcom' in line],
            'compare_branch_pairs': branch_pairs}


def rtl(path):
    text = path.read_text().split(';; Function silence_progress', 1)[1]
    post = text.split('dsplibs_debug_printf', 1)[1]
    before_decision = post.split('(jump_insn', 1)[0]
    # The first conversion is nsamp; the later DF conversion belongs to the
    # unsigned TIME callback result and is a different operation.
    return {'counter_float_mode': re.findall(r'\(float:(SF|DF) ', text)[0],
            'post_debug_owner_read': '.energy+0' in before_decision}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--production-object', type=Path)
    args = ap.parse_args()
    roots = [d.ROOT/'build/recovery-silence-energy',
             d.ROOT/'build/recovery-silence-precision']
    reports = {}
    original = decisions(Path(d.b.BLOB))
    assert len(original['indexed_thresholds']) == 2
    assert len(original['register_counter_compares']) == 1
    for root in roots:
        cells = json.loads((root/'results.json').read_text())['families']['silence']['cells']
        assert cells['baseline']['baseline_reproduced']
        base = root/'silence/baseline/candidate.o'
        inventory = inspect(base)
        report = {}
        for label, cell in cells.items():
            folder = root/'silence'/label
            path = folder/'candidate.o'
            assert inspect(path) == inventory, (label, 'data/symbol/metadata drift')
            assert not cell.get('gains') and not cell.get('losses')
            assert set(cell.get('changed_bodies', [])) <= {'silence_progress'}
            assert len(cell['functions']) == len(cell['verdicts']) == 5
            assert hashlib.sha256(path.read_bytes()).hexdigest() == cell['object_hash']
            report[label] = {'verdict': cell['verdicts']['silence_progress'],
                             'decisions': decisions(path),
                             'initial_rtl': rtl(folder/'silence.c.01.rtl'),
                             'changed_bodies': cell.get('changed_bodies', [])}
        # Corrupt a real data byte to demonstrate that the all-data audit fires.
        negative = root/'data-negative.o'
        raw = bytearray(base.read_bytes())
        with base.open('rb') as stream:
            section = ELFFile(stream).get_section_by_name('.data')
            assert section['sh_size'] == 16
            raw[section['sh_offset'] + 4] ^= 1
        negative.write_bytes(raw)
        assert inspect(negative) != inventory
        reports[root.name] = report
    energy = reports['recovery-silence-energy']
    precision = reports['recovery-silence-precision']
    assert not energy['baseline']['decisions']['indexed_thresholds']
    assert not energy['baseline']['initial_rtl']['post_debug_owner_read']
    assert energy['compare-field']['initial_rtl']['post_debug_owner_read']
    assert len(energy['compare-field']['decisions']['indexed_thresholds']) == 1
    assert len(energy['direct-field']['decisions']['indexed_thresholds']) == 2
    assert not precision['float-800.0f']['decisions']['register_counter_compares']
    for label in ('double-800.0f', 'double-800.0', 'native-double'):
        assert precision[label]['initial_rtl']['counter_float_mode'] == 'DF'
        assert len(precision[label]['decisions']['register_counter_compares']) == 1
        assert len(precision[label]['decisions']['indexed_thresholds']) == 2
    family = roots[1]/'silence'
    chosen = family/'native-double/candidate.o'
    assert chosen.read_bytes() == (family/'double-800.0/candidate.o').read_bytes()
    original_branches = [re.search(r'\b(j[a-z]+)\s', p[1]).group(1)
                         for p in original['compare_branch_pairs']]
    new_branches = [re.search(r'\b(j[a-z]+)\s', p[1]).group(1)
                   for p in precision['native-double']['decisions']['compare_branch_pairs']]
    assert original_branches == new_branches == ['jb', 'jbe', 'ja']
    with Path(d.b.BLOB).open('rb') as stream:
        old_table = ELFFile(stream).get_section_by_name('.data').data()[0x84d4:0x84e4]
    with chosen.open('rb') as stream:
        table = ELFFile(stream).get_section_by_name('.data').data()
    assert old_table == table and len(table) == 16
    if args.production_object:
        assert chosen.read_bytes() == args.production_object.read_bytes()
    result = {'cells': sum(map(len, reports.values())), 'function_verdicts': 50,
              'reports': reports, 'original_decisions': original,
              'data_negative_controls_detected': 2,
              'exact_gains': [], 'exact_losses': [],
              'production_raw_identical': bool(args.production_object),
              'candidate_sha256': hashlib.sha256(chosen.read_bytes()).hexdigest()}
    result['threshold_table_hex'] = table.hex()
    result['original_and_candidate_float_branches'] = original_branches
    (roots[1]/'audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print('10 full TUs / 50 function verdicts: 4 siblings, all data, bindings and imports stable;')
    print('2/2 corrupt-data controls rejected; SF/local controls distinguish both recovered graphs;')
    print('0 exact gains/losses; silence_progress remains nonexact (SIZE35).')


if __name__ == '__main__':
    main()
