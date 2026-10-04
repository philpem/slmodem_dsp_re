#!/usr/bin/env python3
"""Trace operand substitutions between GCC3 local allocation and global reload.

This reports dump evidence, not reconstructed liveness or allocator causality.
Only instruction patterns are compared; REG_EQUAL/dependency notes are excluded.
"""
import argparse
import json
import re
from pathlib import Path


def expressions(text):
    """Read balanced RTL, respecting quoted strings; refuse truncation."""
    tokens = re.findall(r'\(|\)|"(?:\\.|[^"\\])*"|[^\s()]+', text)
    roots, stack = [], []
    for token in tokens:
        if token == '(':
            node = []
            (stack[-1] if stack else roots).append(node)
            stack.append(node)
        elif token == ')':
            if not stack:
                raise ValueError('unmatched RTL close')
            stack.pop()
        elif stack:
            stack[-1].append(token)
    if stack:
        raise ValueError('truncated RTL expression')
    return roots


def function(text, name):
    blocks = re.split(r'^;; Function ', text, flags=re.M)[1:]
    matches = [block for block in blocks if block.splitlines()[0].split()[0] == name]
    if len(matches) != 1:
        raise ValueError(f'function {name}: {len(matches)} matching dump blocks')
    return matches[0]


def instructions(text):
    # Extract the instruction stream before parsing: prose reload diagnostics
    # also contain parenthesized operands, but are not instructions.
    start = re.search(r'^\((?:insn|jump_insn|call_insn)(?::\S+)? ', text, re.M)
    if not start:
        raise ValueError('no instruction stream')
    result = {}
    for node in expressions(text[start.start():]):
        if node and re.fullmatch(r'(?:insn|jump_insn|call_insn)(?::\S+)?', node[0]):
            uid = int(node[1])
            pattern = next((child for child in node[2:] if isinstance(child, list)), None)
            if pattern is None or uid in result:
                raise ValueError(f'missing pattern or duplicate UID {uid}')
            result[uid] = pattern
    return result


def register(node):
    if isinstance(node, list) and node and re.match(r'reg(?:/[^:]*)?:', node[0]):
        return int(node[1])
    return None


def stack_home(node):
    if not isinstance(node, list) or not node or not node[0].startswith('mem:'):
        return None
    address = node[1]
    if not isinstance(address, list):
        return None
    if register(address) == 7:
        return {'base': 'sp', 'offset': 0, 'mode': node[0].split(':')[1]}
    if address and address[0].startswith('plus:') and len(address) == 3:
        base, offset = address[1:]
        if register(base) == 7 and isinstance(offset, list) and offset[0] == 'const_int':
            return {'base': 'sp', 'offset': int(offset[1]), 'mode': node[0].split(':')[1]}
    return None


def compare(before, after, uid, path, records, unmatched):
    pseudo = register(before)
    if pseudo is None and isinstance(before, list) and before and before[0].startswith('subreg:'):
        pseudo = register(before[1])
    home = stack_home(after)
    if pseudo is not None and pseudo >= 53 and home is not None:
        records.append({'pseudo': pseudo, 'uid': uid, 'path': path, 'home': home})
        return
    if register(before) is not None and register(after) is not None:
        return
    if isinstance(before, list) and isinstance(after, list):
        if not before or not after or before[0] != after[0] or len(before) != len(after):
            unmatched.append({'uid': uid, 'path': path, 'before': before, 'after': after})
            return
        for index, (left, right) in enumerate(zip(before[1:], after[1:]), 1):
            if isinstance(left, list) or isinstance(right, list):
                compare(left, right, uid, path + [index], records, unmatched)


def trace(local, global_dump, name):
    before, after = function(local, name), function(global_dump, name)
    left, right = instructions(before), instructions(after)
    shared = sorted(left.keys() & right.keys())
    if not shared:
        raise ValueError('zero comparable instruction UIDs')
    records, unmatched = [], []
    for uid in shared:
        compare(left[uid], right[uid], uid, [], records, unmatched)
    assignments = {int(p): int(h) for p, h in re.findall(r'^;; Register (\d+) in (\d+)\.', before, re.M)}
    preferences = {int(p): choice for p, choice in re.findall(r'^Register (\d+) used .*?pref ([^.]+)\.', before, re.M)}
    conflicts = {int(p): [int(r) for r in regs.split()] for p, regs in re.findall(r'^;; (\d+) conflicts: ([0-9 ]+)$', after, re.M)}
    allocation = re.search(r'^;; \d+ regs to allocate: ([0-9 ]+)$', after, re.M)
    return {'function': name, 'local_instructions': len(left), 'global_instructions': len(right),
            'shared_instructions': len(shared), 'local_assignments': assignments,
            'local_preferences': preferences, 'global_conflicts': conflicts,
            'global_allocation_order': [int(r) for r in allocation[1].split()] if allocation else None,
            'stack_substitutions': records, 'inserted_patterns': {uid: right[uid] for uid in sorted(right.keys() - left.keys())},
            'unclassified_restructures': unmatched}


def self_test():
    local = ''';; Function probe
;; Register 73 in 1.
;; Register 77 in 1.
(insn 51 0 55 3 (set (reg:SI 73) (const_int 1)) 36 (nil)
 (expr_list:REG_EQUAL (mem:SI (reg:SI 7 sp)) (nil)))
(insn 55 51 56 3 (set (reg:SI 77) (div:SI (reg:SI 73) (reg:SI 74))) 184 (nil) (nil))
(insn 56 55 0 3 (set (reg/v:SI 67 [ r ]) (sign_extend:SI (subreg:HI (reg:SI 77) 0))) 84 (nil) (nil))
'''
    allocated = ''';; Function probe
;; 1 regs to allocate: 67
(insn 51 0 55 3 (set (mem:SI (plus:SI (reg:SI 7 sp) (const_int 28))) (const_int 1)) 36 (nil) (nil))
(insn 55 51 153 3 (set (reg:SI 0 ax) (div:SI (reg:SI 0 ax) (reg:SI 2 cx))) 184 (nil) (nil))
(insn 153 55 56 3 (set (mem:SI (plus:SI (reg:SI 7 sp) (const_int 24))) (reg:SI 0 ax)) 36 (nil) (nil))
(insn 56 153 0 3 (set (reg/v:SI 0 ax [orig:67 r ]) (sign_extend:SI (mem:HI (plus:SI (reg:SI 7 sp) (const_int 24))))) 84 (nil) (nil))
'''
    result = trace(local, allocated, 'probe')
    assert {(r['pseudo'], r['home']['offset']) for r in result['stack_substitutions']} == {(73, 28), (77, 24)}
    assert not any(r['pseudo'] == 67 for r in result['stack_substitutions'])
    assert result['shared_instructions'] == 3 and len(result['inserted_patterns']) == 1
    refusals = [lambda: expressions('('), lambda: expressions(')'),
                lambda: trace(local, allocated, 'absent'),
                lambda: instructions(local + '(insn 51 0 0 3 (set (reg:SI 73) (const_int 1)))'),
                lambda: trace(local, allocated.replace('(insn 51 ', '(insn 251 ').replace('(insn 55 ', '(insn 255 ').replace('(insn 56 ', '(insn 256 '), 'probe')]
    for refusal in refusals:
        try:
            refusal()
        except ValueError:
            pass
        else:
            raise AssertionError('invalid input accepted')
    print('reload trace self-test: 8/8 controls pass (spill/resident/metadata and 5 refusals)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('local', type=Path, nargs='?')
    parser.add_argument('global_dump', type=Path, nargs='?')
    parser.add_argument('--function')
    parser.add_argument('--json', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not all((args.local, args.global_dump, args.function, args.json)):
        parser.error('local/global dump paths, --function and --json are required')
    result = trace(args.local.read_text(), args.global_dump.read_text(), args.function)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(result, indent=2) + '\n')
    print(f"{args.function}: {result['shared_instructions']} shared instructions; "
          f"{len(result['stack_substitutions'])} stack substitutions; "
          f"{len(result['inserted_patterns'])} inserted instructions; "
          f"{len(result['unclassified_restructures'])} unclassified restructures")
    for pseudo in sorted({r['pseudo'] for r in result['stack_substitutions']}):
        homes = sorted({r['home']['offset'] for r in result['stack_substitutions'] if r['pseudo'] == pseudo})
        print(f"  pseudo {pseudo}: local hardreg {result['local_assignments'].get(pseudo, 'unassigned')}; sp offsets {homes}")


if __name__ == '__main__':
    main()
