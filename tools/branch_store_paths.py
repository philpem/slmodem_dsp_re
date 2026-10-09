#!/usr/bin/env python3
"""Follow two straight-line branch arms to their first explicit field store.

This classifies instruction sites, not arbitrary address aliasing or source
syntax. Calls, nested conditions, cycles and base-register writes are barriers.
"""
import argparse
import json
import re
from pathlib import Path
from gcc_x87_transfer_screen import assembly
from value_timing_tail_screen import operands
import playbook_small_patterns as d

ALIASES = {'eax': ('eax', 'ax', 'al', 'ah'), 'ebx': ('ebx', 'bx', 'bl', 'bh'),
           'ecx': ('ecx', 'cx', 'cl', 'ch'), 'edx': ('edx', 'dx', 'dl', 'dh'),
           'esi': ('esi', 'si'), 'edi': ('edi', 'di'), 'ebp': ('ebp', 'bp'), 'esp': ('esp', 'sp')}

def destination(arg):
    match = re.fullmatch(r'(0x[0-9a-f]+)?\(%([a-z]+)\)', arg)
    return (int(match[1] or '0', 16), match[2]) if match else None

def analyze(path, symbol, branch_offset, field_offset, base_register):
    rows = assembly(path)[symbol]
    size = d.b.sizes(str(path))[symbol]
    start = rows[0][0]
    rows = [r for r in rows if start <= r[0] < start+size]
    by_address = {r[0]: i for i, r in enumerate(rows)}
    branch = start+branch_offset
    assert branch in by_address, 'branch is not an instruction boundary'
    index = by_address[branch]
    _, op, args = rows[index]
    assert op.startswith('j') and op != 'jmp' and not args.startswith('*'), 'require direct conditional branch'
    target = int(args.split()[0], 16)
    assert target in by_address and index+1 < len(rows), 'arm outside function'
    def follow(address):
        visited = []
        while len(visited) < 64:
            assert address not in visited and address in by_address, 'cycle/outside function'
            visited.append(address)
            i = by_address[address]
            _, mnemonic, operand = rows[i]
            fields = operands(operand)
            if mnemonic.startswith('mov') and destination(fields[-1]) == (field_offset, base_register):
                return {'store_offset': address-start, 'instruction': [mnemonic, operand],
                        'path_offsets': [a-start for a in visited]}
            assert not mnemonic.startswith(('call', 'ret')), 'call/return before field store'
            if mnemonic == 'jmp':
                assert not operand.startswith('*'), 'indirect jump'
                address = int(operand.split()[0], 16)
                continue
            assert not mnemonic.startswith('j'), 'nested condition before field store'
            assert mnemonic in ('mov', 'movl', 'movw', 'movb', 'movzbl', 'movzwl', 'movsbl', 'movswl',
                                'lea', 'inc', 'dec', 'add', 'sub', 'and', 'or', 'xor', 'shl', 'shr',
                                'sar', 'sal', 'neg', 'not', 'cmp', 'cmpb', 'cmpw', 'test', 'testb', 'testw', 'nop'), 'instruction outside validated straight-line domain'
            if not mnemonic.startswith(('cmp', 'test', 'nop')):
                assert fields[-1] not in ['%'+r for r in ALIASES[base_register]], 'base register changes'
            assert i+1 < len(rows), 'fallthrough outside function'
            address = rows[i+1][0]
        raise AssertionError('arm exceeds 64 instructions')
    normal, taken = follow(rows[index+1][0]), follow(target)
    return {'symbol': symbol, 'branch_offset': branch_offset, 'field_offset': field_offset,
            'base_register': base_register, 'fallthrough': normal, 'taken': taken,
            'same_store_instruction': normal['store_offset'] == taken['store_offset']}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object', type=Path)
    parser.add_argument('symbol')
    parser.add_argument('--branch-offset', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--field-offset', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--base-register', choices=ALIASES, required=True)
    args = parser.parse_args()
    print(json.dumps(analyze(args.object, args.symbol, args.branch_offset,
                             args.field_offset, args.base_register), indent=2))

if __name__ == '__main__':
    main()
