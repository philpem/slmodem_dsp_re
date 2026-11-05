#!/usr/bin/env python3
"""Bounded i386 owner/value diagnostic; nominations are never semantic proof.

Facts describe machine-value origins, not C types. Entry argument slots assume
ordinary i386 cdecl. No external entry edges, signals, self-modification or
callee summaries are modeled. Unknown control flow refuses the entire symbol.
Store denominators cover decoded MOV/MOVb/MOVw/MOVl stores only. Unsupported
instructions have a separate count and may contain unclassified memory accesses.
Pointer-load origins identify a static load site, not a dynamic value epoch.
Only entry-argument roots (and copies/constant subobjects) can pair across
objects; pointer loads and call returns retain site-local provenance only.
"""
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile

TOOLS = Path(__file__).resolve().parent
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != TOOLS]
import dis  # avoid tools/dis.py shadowing the standard library
sys.path.insert(0, str(TOOLS))
import gcc3_search_width as width
byteident, jumptable = width.byteident, width.jumptable

UNKNOWN = ('unknown',)
AMBIGUOUS = ('ambiguous',)
DEPENDENCIES = tuple(dict.fromkeys(width.ANALYSIS_FILES + (Path(width.__file__), Path(__file__))))
LOADED_HASHES = {str(p.resolve()): width.sha256(p) for p in DEPENDENCIES}
REGS = ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp')
LANES = {r: (r, 32, 0) for r in REGS}
for r in REGS:
    LANES[r[1:]] = (r, 16, 0)
for r in ('eax', 'ebx', 'ecx', 'edx'):
    LANES[r[1] + 'l'] = (r, 8, 0)
    LANES[r[1] + 'h'] = (r, 8, 8)
MEM = re.compile(r'(?P<disp>-?(?:0x[0-9a-f]+|[0-9]+))?\(%(?P<base>e(?:ax|bx|cx|dx|si|di|bp|sp))\)$')


def parts(text):
    return [s.strip() for s in re.split(r',(?=[^()]*(?:\(|$))', text)]


def number(text):
    try:
        return int(text, 0)
    except ValueError:
        return None


def plus(value, delta):
    if value[0] in ('address', 'stack', 'constant'):
        return value[:-1] + (value[-1] + delta,)
    return value if value in (UNKNOWN, AMBIGUOUS) else UNKNOWN


def initial():
    return ({r: ('stack', 0) if r == 'esp' else UNKNOWN for r in REGS}, {})


def merge(a, b):
    def meet(x, y):
        return x if x == y else (UNKNOWN if UNKNOWN in (x, y) else AMBIGUOUS)
    return ({r: meet(a[0][r], b[0][r]) for r in REGS},
            {k: meet(a[1].get(k, UNKNOWN), b[1].get(k, UNKNOWN))
             for k in a[1].keys() | b[1].keys()})


def address(operand, regs):
    match = MEM.fullmatch(operand)
    if not match:
        return UNKNOWN
    return plus(regs[match['base']], number(match['disp'] or '0'))


def value(operand, regs, stack, site):
    if operand.startswith('$'):
        n = number(operand[1:])
        return ('constant', n) if n is not None else UNKNOWN
    if operand.startswith('%'):
        lane = LANES.get(operand[1:])
        if lane and lane[1:] == (32, 0):
            return regs[lane[0]]
        # Retain width and bit position; never promote a lane into a pointer.
        return ('lane', lane, regs[lane[0]]) if lane else UNKNOWN
    owner = address(operand, regs)
    if owner[0] == 'stack':
        offset = owner[1]
        if offset in stack:
            return stack[offset]
        if None not in stack and offset >= 4 and offset % 4 == 0:
            return ('address', ('entry-arg', offset // 4), 0)
        return UNKNOWN
    if owner[0] == 'address':
        return ('address', ('pointer-load', site, owner), 0)
    return owner


def width_bits(mnemonic, operands):
    if mnemonic in ('movb', 'movw', 'movl'):
        return {'b': 8, 'w': 16, 'l': 32}[mnemonic[-1]]
    if mnemonic == 'mov':
        lanes = [LANES.get(p[1:]) for p in operands if p.startswith('%')]
        return lanes[-1][1] if lanes and lanes[-1] else None
    return None


def invalidate_slot(stack, offset, bits):
    for key in list(stack):
        if key is not None and key < offset + bits // 8 and offset < key + 4:
            stack[key] = UNKNOWN
    # Incoming arg bytes can be overwritten before their first read.
    for key in range(max(4, offset // 4 * 4), offset + (bits + 7) // 8, 4):
        stack[key] = UNKNOWN


def transfer(row, state):
    at, raw, mnemonic, text = row
    regs, stack = state[0].copy(), state[1].copy()
    ops = parts(text)
    stores = []
    unsupported = False
    def put(target, val, bits=32):
        if target.startswith('%'):
            lane = LANES.get(target[1:])
            if lane:
                regs[lane[0]] = val if lane[1] == bits == 32 else UNKNOWN
        else:
            owner = address(target, regs)
            if owner[0] == 'stack':
                invalidate_slot(stack, owner[1], bits)
                if bits == 32:
                    stack[owner[1]] = val
            else:
                # An unproved store can alias any tracked stack spill.
                stack.clear()
                stack[None] = UNKNOWN
    if mnemonic in ('mov', 'movb', 'movw', 'movl') and len(ops) == 2:
        bits = width_bits(mnemonic, ops)
        if bits is None:
            unsupported = True
        else:
            if not ops[1].startswith('%'):
                owner = address(ops[1], regs)
                val = value(ops[0], regs, stack, at)
                stores.append({'address': at, 'instruction': mnemonic + ' ' + text,
                               'width': bits, 'owner': owner, 'value': val})
            put(ops[1], value(ops[0], regs, stack, at), bits)
    elif mnemonic in ('movsbl', 'movswl', 'movzbl', 'movzwl') and len(ops) == 2:
        put(ops[1], ('extended', mnemonic, value(ops[0], regs, stack, at)))
    elif mnemonic in ('xor', 'and', 'or', 'shl', 'shr', 'sar', 'imul') and len(ops) == 2:
        val = ('constant', 0) if mnemonic == 'xor' and ops[0] == ops[1] else UNKNOWN
        put(ops[1], val)
    elif mnemonic in ('inc', 'dec', 'neg', 'not') and len(ops) == 1:
        put(ops[0], UNKNOWN)
    elif mnemonic == 'lea' and len(ops) == 2:
        put(ops[1], address(ops[0], regs))
    elif mnemonic in ('add', 'sub') and len(ops) == 2 and ops[0].startswith('$') and number(ops[0][1:]) is not None:
        delta = number(ops[0][1:]) * (1 if mnemonic == 'add' else -1)
        put(ops[1], plus(value(ops[1], regs, stack, at), delta))
    elif mnemonic == 'push' and len(ops) == 1:
        val = value(ops[0], regs, stack, at)
        regs['esp'] = plus(regs['esp'], -4)
        put('(%esp)', val)
    elif mnemonic == 'pop' and len(ops) == 1:
        val = value('(%esp)', regs, stack, at)
        put(ops[0], val)
        regs['esp'] = plus(regs['esp'], 4)
    elif mnemonic == 'leave':
        regs['esp'] = regs['ebp']
        regs['ebp'] = value('(%esp)', regs, stack, at)
        regs['esp'] = plus(regs['esp'], 4)
    elif mnemonic == 'call':
        # Site identity is local, never equated merely by target/call ordinal.
        for reg in ('eax', 'ecx', 'edx'):
            regs[reg] = UNKNOWN
        regs['eax'] = ('address', ('call-return', at, text), 0)
        stack = {key: UNKNOWN for key in stack}
        stack[None] = UNKNOWN
        # Incoming arguments may have escaped; retain register copies only.
    elif mnemonic in ('cmp', 'test', 'nop', 'ret') or mnemonic.startswith('j'):
        pass
    else:
        # Unknown semantics can include implicit register/memory writes.
        unsupported = True
    if unsupported:
        regs = {r: UNKNOWN for r in REGS}
        stack = {key: UNKNOWN for key in stack}
        stack[None] = UNKNOWN
    return (regs, stack), stores, unsupported


def classify(owner):
    if owner == AMBIGUOUS:
        return 'ambiguous'
    if owner[0] == 'address' and owner[1][0] == 'entry-arg':
        return 'supported'
    return 'unknown'


def trace(path, symbol, max_steps=20000):
    owned = byteident.owned_function(str(path), symbol)
    if owned is None:
        raise ValueError('unavailable unique function body: ' + symbol)
    section, start, data, relocations = owned
    rows = jumptable.instructions(str(path), section, start, start + len(data))
    by_at = {row[0]: row for row in rows}
    successors = {}
    refusal = None
    for at, raw, mnemonic, text in rows:
        following = at + len(raw)
        edges = [] if mnemonic == 'ret' else ([following] if following in by_at else [])
        if mnemonic.startswith('j') or mnemonic.startswith('loop'):
            match = re.match(r'^([0-9a-f]+)(?:\s|$)', text)
            target = int(match[1], 16) if match else None
            if target not in by_at or any(at <= start + off < following for off in relocations):
                refusal = 'unsupported indirect/external/relocated control flow'
            else:
                edges = [target] if mnemonic == 'jmp' else edges + [target]
        if mnemonic != 'ret' and mnemonic != 'jmp' and following not in by_at:
            refusal = 'unmodeled fallthrough outside selected function'
        successors[at] = edges
    if refusal:
        return {'status': 'unknown', 'reason': refusal, 'instruction_count': len(rows),
                'stores': [], 'denominator': {'supported': 0, 'unknown': 0, 'ambiguous': 0},
                'unclassified_instruction_count': len(rows)}
    # Relocations can change immediate values or memory addresses. Refuse their
    # instruction semantics except ABI calls, whose result is already site-local.
    for at, raw, mnemonic, text in rows:
        if mnemonic != 'call' and any(at <= start + off < at + len(raw) for off in relocations):
            by_at[at] = (at, raw, 'unsupported-relocation', text)
    incoming = {start: initial()}
    pending = deque([start])
    steps = 0
    while pending:
        at = pending.popleft()
        steps += 1
        if steps > max_steps:
            raise ValueError('CFG convergence bound exceeded: ' + symbol)
        after, _, _ = transfer(by_at[at], incoming[at])
        for target in successors[at]:
            joined = merge(incoming[target], after) if target in incoming else after
            if incoming.get(target) != joined:
                incoming[target] = joined
                pending.append(target)
    stores, unsupported = [], []
    for at in sorted(incoming):
        _, observed, bad = transfer(by_at[at], incoming[at])
        if bad:
            unsupported.append(at)
        for store in observed:
            store['status'] = classify(store['owner'])
            stores.append(store)
    counts = Counter(s['status'] for s in stores)
    return {'status': 'bounded', 'instruction_count': len(rows),
            'reachable_instruction_count': len(incoming), 'worklist_steps': steps,
            'unsupported_instruction_addresses': unsupported,
            'unsupported_instruction_count': len(unsupported), 'stores': stores,
            'denominator': {k: counts[k] for k in ('supported', 'unknown', 'ambiguous')},
            'total_store_count': len(stores)}


def nominations(native, candidate):
    def census(report):
        return Counter((repr(s['owner']), repr(s['value']), s['width'])
                       for s in report['stores'] if s['status'] == 'supported'
                       and s['value'][0] == 'constant')
    a, b = census(native), census(candidate)
    for key in a.keys() | b.keys():
        canceled = min(a[key], b[key])
        a[key] -= canceled
        b[key] -= canceled
    found = []
    for owner, val in sorted({k[:2] for k in a.keys() | b.keys()}):
        left = [(bits, a[owner, val, bits]) for bits in (8, 16) if a[owner, val, bits]]
        right = [(bits, b[owner, val, bits]) for bits in (32,) if b[owner, val, bits]]
        if sum(n for _, n in left) == sum(n for _, n in right) == 1:
            found.append({'machine_origin': owner, 'value': val,
                          'native_width': left[0][0], 'candidate_width': right[0][0],
                          'semantic_proof': False})
    return found


def analyze(native, candidate, symbols):
    if any(width.sha256(p) != digest for p, digest in LOADED_HASHES.items()):
        raise ValueError('analysis dependency changed since import; restart Python')
    paths = [str(Path(p).resolve()) for p in (native, candidate)]
    files = list(dict.fromkeys(paths + list(LOADED_HASHES)))
    # Hash exactly the bytes supplied to every ELF reader. Hashing mutable
    # original paths before/after tracing misses a write-and-restore (ABA).
    contents = {p: Path(p).read_bytes() for p in dict.fromkeys(paths)}
    hashes = {p: hashlib.sha256(contents[p]).hexdigest() if p in contents
              else width.sha256(p) for p in files}
    width._clear_byteident_caches()
    if not symbols:
        raise ValueError('at least one explicit --symbol required')
    functions = []
    snapshot_root = TOOLS.parent / 'build'
    snapshot_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='gcc3-owner-', dir=snapshot_root) as directory:
        snapshots = {}
        for index, (path, content) in enumerate(contents.items()):
            snapshot = Path(directory) / ('%d.o' % index)
            with snapshot.open('xb') as stream:
                stream.write(content)
            snapshot.chmod(0o400)
            snapshots[path] = snapshot
        for symbol in sorted(set(symbols)):
            a, b = (trace(snapshots[p], symbol) for p in paths)
            functions.append({'symbol': symbol, 'native': a, 'candidate': b,
                              'nominations': nominations(a, b)})
    if any(width.sha256(p) != digest for p, digest in hashes.items()):
        raise ValueError('input/dependency changed during analysis')
    return {'schema': 'gcc3-owner-trace-v1', 'status': 'valid',
            'inputs': dict(zip(('native', 'candidate'), paths)), 'sha256': hashes,
            'selected_functions': len(functions), 'functions': functions,
            'nomination_count': sum(len(f['nominations']) for f in functions),
            'semantic_proof': False,
            'scope': __doc__}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', required=True, type=Path)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--symbol', required=True, action='append')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        with args.output.open('x') as stream:
            report = analyze(args.native, args.candidate, args.symbol)
            json.dump(report, stream, indent=2)
            stream.write('\n')
        print('%d functions, %d nominations; semantic proof: false' %
              (report['selected_functions'], report['nomination_count']))
        for f in report['functions']:
            print(f['symbol'], 'native', f['native']['denominator'],
                  'candidate', f['candidate']['denominator'])
        return 0
    except (OSError, ValueError, RuntimeError) as exc:
        print('invalid: denominator not established: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
