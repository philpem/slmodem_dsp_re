#!/usr/bin/env python3
"""Identify incoming argument homes from i386 prologues and count their accesses."""
import argparse
import json
import re
import subprocess


def audit(path, symbol):
    text = subprocess.check_output(['objdump', '-dr', '--disassemble=' + symbol, str(path)], text=True)
    instructions = []
    for line in text.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+\s*([a-z][a-z0-9]*)\s*(.*)', line)
        if m:
            instructions.append((int(m[1], 16), m[2], m[3]))
    assert instructions, (path, symbol)
    # Supported ordinary fixed i386 frame: pushes and one subtraction before
    # the first ESP-relative argument load. Reject layouts we cannot establish.
    pushed = allocated = 0
    found = False
    for address, op, args in instructions:
        if op == 'push' and re.fullmatch(r'%e[a-z]{2}', args):
            pushed += 4
        elif op == 'sub' and re.fullmatch(r'\$0x[0-9a-f]+,%esp', args):
            allocated += int(args.split(',')[0][3:], 16)
        elif op.startswith('mov') and re.search(r'0x[0-9a-f]+\(%esp\),%e[a-z]{2}', args):
            # Stack stores can precede the first argument load, as in V34.
            found = True
            break
        elif op in ('call', 'ret', 'jmp'):
            raise ValueError('unsupported prologue before argument load')
    assert found, (path, symbol)
    offset = 4 + pushed + allocated
    operand = '0x%x(%%esp)' % offset
    reads = writes = addresses = 0
    for address, op, args in instructions:
        if operand not in args:
            continue
        if op == 'lea':
            addresses += 1
        elif args.endswith(',' + operand) and op not in ('cmp', 'test'):
            writes += 1
            if not op.startswith('mov'):
                reads += 1
        else:
            reads += 1
    return {'object': str(path), 'symbol': symbol, 'saved_register_bytes': pushed,
            'frame_subtraction_bytes': allocated, 'first_argument_offset': offset,
            'argument_home_read_sites': reads, 'argument_home_write_sites': writes, 'argument_home_address_sites': addresses,
            'scope': 'static instruction sites, not dynamic execution counts'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('object')
    ap.add_argument('symbol')
    args = ap.parse_args()
    print(json.dumps(audit(args.object, args.symbol), indent=2))


if __name__ == '__main__':
    main()
