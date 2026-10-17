#!/usr/bin/env python3
"""Explain three large debug nominations with bounded same-TU call/tail expansion.

This inventories resolved strings, not paths or full behavioral equivalence.
Indirect callbacks and callees outside the supplied TU remain boundaries.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import recovery_debug_boundary_audit as strings
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
V90 = '_ZN14V90Demodulator8progressEPiRjPfj'


def graph(path, tails):
    text = subprocess.check_output(['objdump', '-dr', str(path)], text=True)
    result, owner, pending = {}, None, None
    for line in text.splitlines():
        header = re.match(r'^[0-9a-f]+ <([^>]+)>:', line)
        if header:
            owner = header[1]
            result.setdefault(owner, set())
            pending = None
            continue
        transfer = re.search(r'\b(call|jmp)\s+[0-9a-fx]+ <([^>]+)>', line)
        if transfer and (transfer[1] == 'call' or tails):
            destination = transfer[2].split('+')[0]
            result[owner].add(destination)
            pending = owner, destination
            continue
        relocation = re.search(r'R_386_PC32\s+(\S+)', line)
        if relocation and pending:
            result[pending[0]].discard(pending[1])
            result[pending[0]].add(relocation[1].split('-')[0])
            pending = None
        elif line.strip() and 'R_386_' not in line:
            pending = None
    return result


def closure(edges, root):
    seen, todo = set(), [root]
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        todo.extend(n for n in edges.get(name, []) if n in edges and n not in seen)
    return seen


def inventory(path, names):
    return {name: strings.strings(path, name) for name in sorted(names)}


def values(records):
    return {row['text'] for rows in records.values() for row in rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, default=ROOT/'build/large-diagnostics-next')
    parser.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    args = parser.parse_args()
    results = json.loads((args.run_dir/'results.json').read_text())['families']
    report = {'reference_sha256': hashlib.sha256(args.reference.read_bytes()).hexdigest(), 'families': {}}
    for name, family in [('rebuildJMSequence', 'V8'), (V90, 'V90Demodulator'), ('v34handshak', 'V34hshak')]:
        cell = results[family]['cells']['baseline']
        assert cell['baseline_reproduced']
        report['families'][family] = {'raw_reproduced': True, 'sha256': cell['object_hash'],
            'defined_bodies': len(cell['functions']), 'shared_body_verdicts': len(cell['verdicts'])}
    v8 = args.run_dir/'V8/baseline/candidate.o'
    a = Counter(r['text'] for r in strings.strings(args.reference, 'rebuildJMSequence'))
    b = Counter(r['text'] for r in strings.strings(v8, 'rebuildJMSequence'))
    assert a == b
    # Perturb a real referenced ELF string while retaining every call site.
    known = next(iter(b))
    raw = bytearray(v8.read_bytes())
    with v8.open('rb') as stream:
        elf = ELFFile(stream)
        for section in elf.iter_sections():
            if not section.name.startswith('.rodata.str'):
                continue
            at = section.data().find(known.encode('ascii') + b'\0')
            if at >= 0:
                raw[section['sh_offset'] + at] ^= 0x20
                break
        else:
            raise AssertionError('known referenced literal not found')
    wrong = args.run_dir/'known-wrong-v8-format.o'
    wrong.write_bytes(raw)
    negative = Counter(r['text'] for r in strings.strings(wrong, 'rebuildJMSequence'))
    assert negative != a and sum(negative.values()) == sum(a.values())
    report['rebuildJMSequence'] = {'resolved_string_consumers': sum(a.values()), 'wrong_format_negative_rejected': True,
        'mechanism': 'Original ProtocolMatch and DefaultProtocol messages share call76853 (Default format7686e jumps76875); candidate separate calls. Same12string consumers.',
        'selected_boundary': 'Source receives unsigned menu words and signed wordidx; final JM length diagnostic precedes sequence-state publications, CRCffff and ten-bit word count. No discrepancy demonstrated.'}
    current = args.run_dir/'V90Demodulator/baseline/candidate.o'
    before = {r['text'] for r in strings.strings(args.reference, V90)}
    after = {r['text'] for r in strings.strings(current, V90)}
    helpers = ['_ZN14V90Demodulator8enterRRNEv', '_ZN14V90Demodulator14enterDataPhaseEv']
    helper_values = values(inventory(args.reference, helpers))
    assert after - before == helper_values and not before - after
    assert after - before  # No-helper negative must expose the four unmatched extras.
    report['V90_progress'] = {'original_root_unique': len(before), 'current_root_unique': len(after),
        'added_formats_explained': sorted(helper_values), 'no_helper_negative_exposes': len(after-before),
        'original_call_anchors': ['1d4bb enterRRN', '1d8e7 enterDataPhase'],
        'selected_boundary': 'Original calls transition methods; candidate inlines their diagnostics, state publications, study reset/disable and rate report. No missing format; no all-state equivalence claim.'}
    path = args.run_dir/'V34hshak/baseline/candidate.o'
    original = {r['text'] for r in strings.strings(args.reference, 'v34handshak')}
    calls = closure(graph(path, False), 'v34handshak')
    expanded = closure(graph(path, True), 'v34handshak')
    call_records = inventory(path, calls)
    all_records = inventory(path, expanded)
    call_missing, missing = original - values(call_records), original - values(all_records)
    assert not missing
    assert len(call_missing) == 3 and 'tx1_moh_hold' in expanded - calls
    report['v34handshak'] = {'original_root_unique': len(original), 'root_unique': len(values(inventory(path, ['v34handshak']))),
        'call_only_bodies': len(calls), 'call_and_tail_bodies': len(expanded), 'remaining_original_formats': [],
        'omitting_sibling_tail_negative': sorted(call_missing), 'tail_anchors': ['3836→tx1_moh_hold', '388e→tx1_moh_hold'],
        'selected_boundary': 'MOH helper preserves vector threshold(rtd>>4)+4b0, path selector and unsigned(message-2)>1 branch to reinit; original guards6889f/6923d/65031 and source agree.',
        'callee_boundaries': 'Only direct same-TU calls and sibling jumps expanded. Indirect callbacks and external-TU callees not traversed.'}
    report['next_discriminator'] = 'An independent valid lifecycle transcript or ordinary argument/publication disagreement; no absent-format source edit justified by these counts.'
    (args.run_dir/'expanded-string-consumers.json').write_text(json.dumps(all_records, indent=2)+'\n')
    (args.run_dir/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Raw TUs3/3;V8 inventory12/12;V90 extras4/4 explained;V34 original formats%d/%d located;%d direct/tail bodies;three negatives fire' %
          (len(original), len(original), len(expanded)))


if __name__ == '__main__':
    main()
