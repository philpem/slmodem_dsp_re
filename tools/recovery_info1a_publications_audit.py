#!/usr/bin/env python3
"""Check INFO1a's seven publications against the original, retaining the old negative."""
import argparse
import json
import re
import subprocess
from pathlib import Path

EXPECTED_FLAGS = [(0xabce, 1), (0xabce, 2), (0xabd0, 1),
                  (0xabd0, 2), (0xabd2, 1), (0xabd2, 2)]


def instructions(path):
    text = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn',
                                   '--disassemble=V34SetINFO1aBits', str(path)], text=True)
    rows = []
    for line in text.splitlines():
        match = re.match(r'\s*([0-9a-f]+):\s*([a-z][a-z0-9]*)\s*(.*)', line)
        if match:
            rows.append((int(match[1], 16), match[2], match[3]))
    return rows


def sequence(path):
    rows = instructions(path)
    starts = []
    for index, (_, op, arg) in enumerate(rows):
        match = re.fullmatch(r'\$0x0,\(%(\w+)\)', arg)
        if op == 'movw' and match:
            following = rows[index + 1:index + 5]
            if any(op2 == 'testb' and '0xabce(' in arg2 for _, op2, arg2 in following):
                starts.append((index, match[1]))
    if len(starts) != 1:
        return {'found': False, 'candidate_starts': len(starts), 'events': []}
    index, destination = starts[0]
    events = [{'kind': 'zero', 'address': rows[index][0]}]
    flag_base = None
    current_flag = None
    pending_skip = None
    completed_flags = []
    for at, op, arg in rows[index + 1:]:
        match = re.fullmatch(r'\$0x([12]),0x(abce|abd0|abd2)\(%(\w+)\)', arg)
        if op == 'testb' and match:
            current_flag = (int(match[2], 16), int(match[1], 16))
            if flag_base is None:
                flag_base = match[3]
            assert flag_base == match[3], 'flag owner changed'
            events.append({'kind': 'flag', 'field': current_flag[0],
                           'mask': current_flag[1], 'address': at})
        elif current_flag is not None and op == 'je':
            pending_skip = int(arg.split()[0], 16)
        elif op == 'mov' and re.fullmatch(r'%ax,\(%' + destination + r'\)', arg):
            assert current_flag is not None and pending_skip is not None
            # The false arm must join exactly at this unconditional publication.
            # The original's final join has independent Uinfo preparation first.
            assert pending_skip <= at
            completed_flags.append(current_flag)
            events.append({'kind': 'store', 'address': at})
            current_flag = None
            pending_skip = None
            if len(completed_flags) == 6:
                break
    assert completed_flags == EXPECTED_FLAGS, completed_flags
    assert [event['kind'] for event in events] == ['zero'] + ['flag', 'store'] * 6
    return {'found': True, 'publications': 7, 'flag_tests': 6, 'events': events}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    args = parser.parse_args()
    family = args.run_dir / 'v34info1a'
    reference = sequence(args.reference)
    old = sequence(family / 'baseline/candidate.o')
    new = sequence(family / 'seven-publications/candidate.o')
    assert reference['found'] and new['found']
    assert not old['found'], 'known collapsed old control unexpectedly passed'
    source_stages = {}
    for label in ('baseline', 'seven-publications'):
        path = family / label / 'v34info1a.cpp.01.rtl'
        text = path.read_text()
        # The last direct bits[0] store is the separate V.34 arm. The prior
        # direct stores are the PCM arm; helper-local aliases are distinct.
        uids = [int(uid) for uid in re.findall(
            r'\(insn (\d+) [^\n]*\(set \(mem:HI \(reg/v/f:SI 60 \[ bits \]\)', text)]
        expected = 2 if label == 'baseline' else 8
        assert len(uids) == expected, (label, uids)
        source_stages[label] = {'pcm_publication_uids': uids[:-1],
                                'separate_v34_publication_uid': uids[-1]}
    result = {'reference': reference, 'old_negative': old, 'candidate': new,
              'initial_rtl': source_stages,
              'scope': 'seven word publications interleaved with six low-byte flag tests; not full function identity'}
    (args.run_dir / 'publication-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    print('INFO1a: original 7/7 publications and 6/6 flag tests; candidate 7/7 and 6/6; old collapsed control rejected')


if __name__ == '__main__':
    main()
