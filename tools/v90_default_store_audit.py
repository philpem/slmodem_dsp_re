#!/usr/bin/env python3
"""Map V90Parameters defaults from decoded stores, including immediate bits.

This bounded i386 CFG constant-propagation diagnostic reports static stores,
not execution order or reachable-input behavior. Unknowns never compare equal.
Only annotated scalar field names/types are read from the header; stored bytes
and widths come independently from the objects. No source compilation occurs.
"""
import argparse
from collections import Counter, deque
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import sys
_saved = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
from dataclasses import dataclass
sys.path[:] = _saved
from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_AC_WRITE
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG
from elftools.elf.elffile import ELFFile
import numeric_consumer_audit as numeric

TARGET = '_ZN13V90Parameters12setToDefaultEv'
ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Value:
    kind: str
    data: object
    origins: frozenset = frozenset()


def constant(kind, data, origin):
    return Value(kind, data, frozenset([json.dumps(origin, sort_keys=True)]))


def meet(a, b):
    if a is None or b is None or (a.kind, a.data) != (b.kind, b.data):
        return None
    return Value(a.kind, a.data, a.origins | b.origins)


def join(a, b):
    regs = {r: meet(v, b[0].get(r)) for r, v in a[0].items()}
    regs = {r: v for r, v in regs.items() if v is not None}
    if len(a[1]) == len(b[1]):
        fp = tuple(meet(x, y) for x, y in zip(a[1], b[1]))
    else:
        fp = ()  # Unsupported stack shape is explicitly reported at its use.
    return regs, fp


def ieee_bytes(key, width):
    """Round an exact finite value once, to the decoded destination width."""
    if key.startswith('zero:'):
        return (int(key[-1]) << (width * 8 - 1)).to_bytes(width, 'little')
    number = Fraction(key)
    sign = number < 0
    number = abs(number)
    frac, bias, maxexp = {4: (23, 127, 255), 8: (52, 1023, 2047)}[width]
    power = number.numerator.bit_length() - number.denominator.bit_length()
    if number < Fraction(2) ** power:
        power -= 1
    exponent = max(power + bias, 0)
    scaled = number / Fraction(2) ** ((exponent or 1) - bias - frac)
    q, rem = divmod(scaled.numerator, scaled.denominator)
    q += rem * 2 > scaled.denominator or (rem * 2 == scaled.denominator and q & 1)
    if q >= 1 << (frac + 1):
        q >>= 1
        exponent += 1
    if not exponent and q >= 1 << frac:
        exponent = 1
    assert exponent < maxexp, 'non-finite result unsupported'
    bits = (int(sign) << (width * 8 - 1)) | (exponent << frac) | (q & ((1 << frac) - 1))
    return bits.to_bytes(width, 'little')


def load(path, symbol):
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    with Path(path).open('rb') as f:
        elf = ELFFile(f)
        assert elf.elfclass == 32 and elf.little_endian and elf['e_machine'] == 'EM_386' and elf['e_type'] == 'ET_REL'
        assert not any(s['sh_type'] == 'SHT_RELA' for s in elf.iter_sections())
        tab = elf.get_section_by_name('.symtab')
        matches = [s for s in tab.iter_symbols() if s.name == symbol and s['st_size'] and isinstance(s['st_shndx'], int)]
        assert len(matches) == 1, 'missing or duplicate function'
        sym = matches[0]
        base, size = sym['st_value'], sym['st_size']
        section = elf.get_section(sym['st_shndx'])
        raw = section.data()
        instructions = list(decoder.disasm(raw[base:base + size], base))
        assert sum(i.size for i in instructions) == size, 'undecoded bytes'
        relocations = {}
        for relsec in elf.iter_sections():
            if relsec['sh_type'] != 'SHT_REL' or relsec['sh_info'] != sym['st_shndx']:
                continue
            names = elf.get_section(relsec['sh_link'])
            for reloc in relsec.iter_relocations():
                off = reloc['r_offset']
                if not base <= off < base + size:
                    continue
                target = names.get_symbol(reloc['r_info_sym'])
                record = {'type': reloc['r_info_type'], 'symbol': target.name}
                if isinstance(target['st_shndx'], int):
                    data = elf.get_section(target['st_shndx'])
                    addend = int.from_bytes(raw[off:off + 4], 'little', signed=True)
                    start = target['st_value'] + addend
                    record.update(section=data.name, offset=start)
                    if data['sh_type'] != 'SHT_NOBITS' and 0 <= start < data['sh_size']:
                        record['bytes'] = data.data()[start:start + 10].hex()
                        if data.name.startswith('.rodata.str'):
                            text = data.data()[start:start + 1024].split(b'\0', 1)[0]
                            record['text'] = text.decode('ascii')
                relocations[off] = record
    return base, instructions, relocations


def trace(path, symbol=TARGET):
    base, instructions, relocations = load(path, symbol)
    lookup = {i.address: i for i in instructions}
    initial = ({'esp': Value('pointer', ('stack', 0))}, ())

    def transfer(inst, state):
        regs, stack = dict(state[0]), list(state[1])
        writes, unsupported = [], []
        name = inst.mnemonic
        ops = inst.operands
        origin = {'at': inst.address - base, 'kind': 'immediate'}
        rels = [r for off, r in relocations.items() if inst.address <= off < inst.address + inst.size]

        def address(op):
            if op.type != X86_OP_MEM or op.mem.index:
                return None
            root = regs.get(inst.reg_name(op.mem.base))
            if root and root.kind == 'pointer':
                return (root.data[0], root.data[1] + op.mem.disp)
            if not op.mem.base:
                return ('global', op.mem.disp)
            return None

        def read(op):
            if op.type == X86_OP_IMM:
                if rels:
                    return None  # A relocated address is not a numeric immediate.
                return constant('bits', op.imm & ((1 << (op.size * 8)) - 1), origin)
            if op.type == X86_OP_REG:
                return regs.get(inst.reg_name(op.reg))
            addr = address(op)
            if addr == ('stack', 4):
                return Value('pointer', ('this', 0))
            if addr == ('this', 0):
                return Value('pointer', ('modemParams', 0))
            return None

        def write(op, val):
            if op.type == X86_OP_REG:
                reg = inst.reg_name(op.reg)
                if val is None:
                    regs.pop(reg, None)
                else:
                    regs[reg] = val
                # Partial writes invalidate the containing register.
                for full, parts in {'eax': ('ax', 'al', 'ah'), 'ebx': ('bx', 'bl', 'bh'),
                                    'ecx': ('cx', 'cl', 'ch'), 'edx': ('dx', 'dl', 'dh')}.items():
                    if reg in parts:
                        regs.pop(full, None)
            elif op.type == X86_OP_MEM:
                writes.append((address(op), op.size, val))

        if name == 'mov':
            write(ops[0], read(ops[1]))
        elif name == 'lea':
            addr = address(ops[1])
            write(ops[0], Value('pointer', addr) if addr else None)
        elif name in ('push', 'pop'):
            esp = regs.get('esp')
            assert esp and esp.kind == 'pointer'
            if name == 'pop':
                write(ops[0], None)
            regs['esp'] = Value('pointer', ('stack', esp.data[1] + (-4 if name == 'push' else 4)))
        elif name in ('add', 'sub') and ops[0].type == X86_OP_REG and inst.reg_name(ops[0].reg) == 'esp':
            esp, amount = regs['esp'], read(ops[1])
            assert amount and amount.kind == 'bits'
            regs['esp'] = Value('pointer', ('stack', esp.data[1] + (amount.data if name == 'add' else -amount.data)))
        elif name == 'xor' and ops[0].type == ops[1].type == X86_OP_REG and ops[0].reg == ops[1].reg:
            write(ops[0], constant('bits', 0, dict(origin, kind='integer-zero')))
        elif name in ('fldz', 'fld1'):
            stack.insert(0, constant('float', 'zero:0' if name == 'fldz' else '1/1', dict(origin, kind=name)))
        elif name == 'fld':
            if ops[0].type == X86_OP_REG:
                index = int(inst.op_str.strip('st()') or 0)
                stack.insert(0, stack[index] if index < len(stack) else None)
            elif len(rels) == 1 and rels[0].get('section', '').startswith('.rodata.cst'):
                rel = rels[0]
                payload = bytes.fromhex(rel.get('bytes', ''))
                if len(payload) >= ops[0].size and ops[0].size in (4, 8, 10):
                    key = numeric.value(payload, ops[0].size)
                    stack.insert(0, constant('float', key, dict(origin, kind='numeric-pool', pool=rel['section'], pool_offset=rel['offset'], load_width=ops[0].size, exact_value=key)))
                else:
                    stack.insert(0, None)
                    unsupported.append('out-of-bounds or unsupported fld')
            else:
                stack.insert(0, None)
                unsupported.append('unclassified fld')
        elif name == 'fxch':
            index = int(inst.op_str.strip('st()') or 1)
            if index >= len(stack):
                unsupported.append('unknown x87 stack shape')
                stack = []
            else:
                stack[0], stack[index] = stack[index], stack[0]
        elif name in ('fst', 'fstp'):
            if ops[0].type != X86_OP_MEM:
                unsupported.append('unsupported x87 register store')
                stack = []
            else:
                write(ops[0], stack[0] if stack else None)
                if name == 'fstp' and stack:
                    stack.pop(0)
        elif name == 'call':
            for reg in ('eax', 'ecx', 'edx'):
                regs.pop(reg, None)
            if stack:
                unsupported.append('x87 values cross call boundary')
            stack = []
        elif name in ('cmp', 'test', 'ret', 'nop') or name.startswith('j'):
            pass
        else:
            # The bounded routine's remaining integer arithmetic depends on
            # inputs. Discard every written register; never reuse stale values.
            supported_unknown = {'mul', 'shr', 'movzx', 'sbb', 'and', 'add', 'sub', 'imul', 'or', 'dec', 'setbe', 'setae'}
            if name not in supported_unknown:
                unsupported.append('unsupported opcode ' + name)
                if name.startswith('f'):
                    stack = []
            _, written = inst.regs_access()
            for reg_id in written:
                reg = inst.reg_name(reg_id)
                regs.pop(reg, None)
                for full, parts in {'eax': ('ax', 'al', 'ah'), 'ebx': ('bx', 'bl', 'bh'),
                                    'ecx': ('cx', 'cl', 'ch'), 'edx': ('dx', 'dl', 'dh')}.items():
                    if reg in parts:
                        regs.pop(full, None)
            for op in ops:
                if op.type == X86_OP_MEM and op.access & CS_AC_WRITE:
                    write(op, None)
        assert len(stack) <= 8
        return (regs, tuple(stack)), writes, unsupported

    edges = {}
    for inst in instructions:
        successors = []
        if inst.mnemonic.startswith('j'):
            assert inst.operands[0].type == X86_OP_IMM, 'indirect jump unsupported'
            successors.append(inst.operands[0].imm)
        if inst.mnemonic not in ('jmp', 'ret'):
            successors.append(inst.address + inst.size)
        assert all(at in lookup for at in successors), 'edge outside function'
        edges[inst.address] = successors

    states = {base: initial}
    queue = deque([base])
    while queue:
        at = queue.popleft()
        inst = lookup[at]
        state, _, _ = transfer(inst, states[at])
        for nxt in edges[at]:
            merged = join(states[nxt], state) if nxt in states else state
            if merged != states.get(nxt):
                states[nxt] = merged
                queue.append(nxt)
    rows, other_writes, unsupported = [], [], []
    for inst in instructions:
        if inst.address not in states:
            continue
        _, writes, errors = transfer(inst, states[inst.address])
        unsupported += [{'at': inst.address - base, 'reason': e} for e in errors]
        for addr, width, val in writes:
            row = {'at': inst.address - base, 'instruction': inst.mnemonic + ' ' + inst.op_str, 'width': width}
            if addr is None or addr[0] != 'this':
                other_writes.append(dict(row, destination=addr, unresolved_address=addr is None))
                continue
            row['offset'] = addr[1]
            if val is not None and val.kind in ('bits', 'float'):
                try:
                    payload = (val.data & ((1 << (width * 8)) - 1)).to_bytes(width, 'little') if val.kind == 'bits' else ieee_bytes(val.data, width)
                    row.update(bytes=payload.hex(), origins=[json.loads(o) for o in sorted(val.origins)])
                except (ValueError, KeyError, AssertionError):
                    row['unresolved'] = 'unsupported floating store conversion'
            else:
                row['unresolved'] = 'input-dependent or unknown source value'
            rows.append(row)
    # Capstone 5 marks FST/FSTP memory operands READ, despite their
    # store semantics. Include those decoded opcodes explicitly.
    decoded_memory_writes = sum(op.type == X86_OP_MEM and bool(op.access & CS_AC_WRITE or inst.mnemonic in ('fst', 'fstp')) for inst in instructions for op in inst.operands)
    assert len(rows) + len(other_writes) == decoded_memory_writes, 'unvisited or unaccounted memory writes'
    assert len(states) == len(instructions), 'unvisited decoded instruction'
    # CFG direction, rather than code-address order: cold diagnostic blocks
    # sit after RET in both objects and jump back into the main body.
    reachable = {}
    for at in lookup:
        todo, seen = list(edges[at]), set()
        while todo:
            nxt = todo.pop()
            if nxt not in seen:
                seen.add(nxt)
                todo.extend(edges[nxt])
        reachable[at] = seen
    for row in rows:
        row['reachable_store_site_mask'] = format(sum(1 << index for index, other in enumerate(rows) if other['at'] + base in reachable[row['at'] + base]), 'x')
    calls = []
    for index, inst in enumerate(instructions):
        if inst.mnemonic != 'call':
            continue
        targets = [r['symbol'] for off, r in relocations.items() if inst.address <= off < inst.address + inst.size]
        formats = []
        for previous in reversed(instructions[max(0, index - 16):index]):
            if previous.mnemonic.startswith('j') or previous.mnemonic in ('call', 'ret'):
                break
            formats += [r['text'] for off, r in relocations.items() if previous.address <= off < previous.address + previous.size and 'text' in r]
        label = formats[0] if len(formats) == 1 else 'call@%x:%s' % (inst.address - base, ','.join(targets))
        calls.append({'at': inst.address - base, 'targets': targets, 'format': label})
        for row in rows:
            store = row['at'] + base
            before, after = inst.address in reachable[store], store in reachable[inst.address]
            relation = 'before' if before and not after else 'after' if after and not before else 'both' if before and after else 'disjoint'
            row.setdefault('call_relations', {})[label] = relation
    for call in calls:
        call['may_precede_calls'] = [other['format'] for other in calls if other['at'] + base in reachable[call['at'] + base]]
    return {'stores': rows, 'calls': calls, 'other_memory_writes': other_writes, 'unsupported': unsupported,
            'denominator': {'decoded_instructions': len(instructions), 'visited_instructions': len(states),
                            'decoded_memory_writes': decoded_memory_writes, 'object_stores': len(rows),
                            'mapped_values': sum('bytes' in r for r in rows), 'unresolved_values': sum('bytes' not in r for r in rows),
                            'other_memory_writes': len(other_writes), 'unresolved_addresses': sum(r['unresolved_address'] for r in other_writes)}}


def compare(a, b):
    def keyed(rows):
        counts, answer = Counter(), {}
        for row in rows:
            stem = (row['offset'], row['width'])
            key = (*stem, counts[stem])
            counts[stem] += 1
            answer[key] = row
        return answer
    left, right = keyed(a['stores']), keyed(b['stores'])
    common = left.keys() & right.keys()
    equal, different, unknown, materializations, call_order_changes = [], [], [], [], []
    for key in sorted(common):
        x, y = left[key], right[key]
        row = {'offset': key[0], 'width': key[1], 'occurrence': key[2], 'reference_at': x['at'], 'candidate_at': y['at']}
        row['field'] = x.get('field')
        if x.get('call_relations', {}) != y.get('call_relations', {}):
            call_order_changes.append(dict(row, reference=x.get('call_relations', {}), candidate=y.get('call_relations', {})))
        if 'bytes' not in x or 'bytes' not in y:
            unknown.append(row)
        elif x['bytes'] == y['bytes']:
            equal.append(row)
            left_kinds = sorted({o['kind'] for o in x.get('origins', [])})
            right_kinds = sorted({o['kind'] for o in y.get('origins', [])})
            if left_kinds != right_kinds:
                materializations.append(dict(row, bytes=x['bytes'], reference=left_kinds, candidate=right_kinds))
        else:
            different.append(dict(row, reference_bytes=x['bytes'], candidate_bytes=y['bytes']))
    left_positions, right_positions = {k: i for i, k in enumerate(left)}, {k: i for i, k in enumerate(right)}
    left_masks = {k: int(row['reachable_store_site_mask'], 16) for k, row in left.items()}
    right_masks = {k: int(row['reachable_store_site_mask'], 16) for k, row in right.items()}
    def direction(first, second, masks, positions):
        forward = bool(masks[first] & (1 << positions[second]))
        reverse = bool(masks[second] & (1 << positions[first]))
        return 'before' if forward and not reverse else 'after' if reverse and not forward else 'both' if forward else 'disjoint'
    ordered, changed_pairs, pair_examples = sorted(common), 0, []
    for index, first in enumerate(ordered):
        for second in ordered[index + 1:]:
            before = direction(first, second, left_masks, left_positions)
            after = direction(first, second, right_masks, right_positions)
            if before != after:
                changed_pairs += 1
                if len(pair_examples) < 20:
                    pair_examples.append({'first': first, 'second': second, 'reference': before, 'candidate': after})
    def call_inventory(result):
        return sorted((c['format'], tuple(c['targets']), tuple(sorted(c['may_precede_calls']))) for c in result.get('calls', []))
    return {'call_inventory_and_direction_equal': call_inventory(a) == call_inventory(b), 'shared_store_sites': len(common), 'equal_bytes': equal, 'different_bytes': different,
            'unresolved_values': unknown, 'reference_only_sites': sorted(left.keys() - right.keys()),
            'candidate_only_sites': sorted(right.keys() - left.keys()),
            'static_address_order_equal': list(left) == list(right),
            'static_site_positions_different': sum(left_positions[k] != right_positions[k] for k in common),
            'CFG_shared_site_pair_denominator': len(common) * (len(common) - 1) // 2,
            'CFG_site_pair_directions_different': changed_pairs, 'CFG_site_pair_change_examples': pair_examples,
            'materialization_changes': materializations, 'call_relative_order_changes': call_order_changes,
            'limits': 'Address order is static function layout, not dynamic write order; repeated visits to a loop store count once. CFG branches are conservatively explored without reachability proof.'}


def controls():
    with tempfile.TemporaryDirectory(prefix='v90-default-stores-') as tmp:
        root = Path(tmp)
        def build(label, change=''):
            body = ''' pushl %ebp
 movl 8(%esp),%ebp
 movl $0x3fc00000,%eax
 movl %eax,4(%ebp)
 fldz
 fsts 8(%ebp)
 fld1
 fstps 12(%ebp)
 flds pool
 fstps 16(%ebp)
 fstps 20(%ebp)
 popl %ebp
 ret
'''
            if change == 'offset': body = body.replace('%eax,4(%ebp)', '%eax,24(%ebp)')
            if change == 'width': body = body.replace('movl %eax,4(%ebp)', 'movw %ax,4(%ebp)')
            if change == 'value': body = body.replace('0x3fc00000', '0x3fc00001')
            if change == 'unsupported': body = body.replace(' flds pool', ' fcos\n flds pool')
            if change in ('equal-join', 'unknown-join'):
                alternate = '0x3fc00000' if change == 'equal-join' else '0x40000000'
                body = body.replace(' movl %eax,4(%ebp)', ' testl %ecx,%ecx\n jz alternate\n jmp joined\nalternate:\n movl $'+alternate+',%eax\njoined:\n movl %eax,4(%ebp)')
            if change == 'call-order': body = body.replace(' fldz', ' call observer\n fldz')
            if change == 'store-order':
                body = body.replace(' movl %eax,4(%ebp)\n', '').replace(' fsts 8(%ebp)\n', ' fsts 8(%ebp)\n movl %eax,4(%ebp)\n')
            asm, obj = root/(label+'.s'), root/(label+'.o')
            asm.write_text('.text\n.globl probe\n.type probe,@function\nprobe:\n'+body+'.size probe,.-probe\n.section .rodata.cst4,"aM",@progbits,4\npool: .float 1.5\n')
            subprocess.run(['as', '--32', str(asm), '-o', str(obj)], check=True)
            return trace(obj, 'probe')
        a = build('baseline')
        assert a['denominator']['object_stores'] == a['denominator']['mapped_values'] == 5
        assert [r['bytes'] for r in a['stores']] == ['0000c03f', '00000000', '0000803f', '0000c03f', '00000000']
        assert len(compare(a, build('value', 'value'))['different_bytes']) == 1
        assert len(compare(a, build('offset', 'offset'))['reference_only_sites']) == 1
        assert len(compare(a, build('width', 'width'))['reference_only_sites']) == 1
        assert build('unsupported', 'unsupported')['unsupported']
        assert build('equal-join', 'equal-join')['stores'][0]['bytes'] == '0000c03f'
        assert 'bytes' not in build('unknown-join', 'unknown-join')['stores'][0]
        ordered = build('call-order', 'call-order')
        assert [list(row['call_relations'].values()) for row in ordered['stores']] == [['before'], ['after'], ['after'], ['after'], ['after']]
        reordered = compare(a, build('store-order', 'store-order'))
        assert reordered['CFG_site_pair_directions_different'] == 1 and len(reordered['equal_bytes']) == 5
    return {'checks': 10, 'assembled_fixtures': 9,
            'CFG_store_reorder_detected_with_values_preserved': True,
            'CFG_equal_join_preserved': True, 'CFG_conflicting_join_invalidated': True,
            'CFG_call_before_after_distinguished': True, 'immediate_pool_zero_one_and_x87_stack_resolved': True,
            'changed_offset_detected': True, 'changed_width_detected': True,
            'changed_value_detected': True, 'unsupported_opcode_reported': True,
            'scope': 'host-assembled ELF32 measurement fixtures; no reconstruction compilation'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('reference', type=Path)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('--json', required=True, type=Path)
    ap.add_argument('--header', type=Path, default=ROOT/'include/dsplib/V90Parameters.h')
    args = ap.parse_args()
    checked = controls()
    a, b = trace(args.reference), trace(args.candidate)
    fields = {int(off, 16): {'name': name, 'type': kind} for kind, name, off in re.findall(r'^\s*(int|float)\s+(\w+);\s*/\*\s*\+0x([0-9a-f]+)', args.header.read_text(), re.M)}
    for result in (a, b):
        for row in result['stores']:
            row['field'] = fields.get(row['offset'])
            if row['field'] and 'bytes' in row and row['width'] == 4:
                raw = bytes.fromhex(row['bytes'])
                row['declared_value'] = numeric.value(raw, 4) if row['field']['type'] == 'float' else int.from_bytes(raw, 'little', signed=True)
        result['denominator']['named_field_stores'] = sum(r['field'] is not None for r in result['stores'])
        result['denominator']['unmapped_field_stores'] = sum(r['field'] is None for r in result['stores'])
        result['materializations'] = dict(Counter(o['kind'] for r in result['stores'] for o in r.get('origins', [])))
        result['x87_stores_to_declared_int_fields'] = [r for r in result['stores'] if r['instruction'].startswith(('fst ', 'fstp ')) and r['field'] and r['field']['type'] != 'float']
    report = {'inputs': {label: {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for label, path in [('reference', args.reference), ('candidate', args.candidate), ('field_annotations', args.header)]},
              'function': TARGET, 'reference': a, 'candidate': b, 'comparison': compare(a, b), 'controls': checked,
              'limits': 'Static byte/value/width mapping, not function equivalence. Integer field annotations are not proof that immediate bytes represent integer rather than float source literals. Dynamic stores and unsupported operations are retained explicitly. Field annotation offsets are display metadata, not independent compiled layout proof.'}
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, indent=2)+'\n')
    print('detector controls: %d checks passed over %d assembled fixtures (offset/value/width canaries included)' % (checked['checks'], checked['assembled_fixtures']))
    for label, result in [('reference', a), ('candidate', b)]:
        print(label, json.dumps(result['denominator'], sort_keys=True))
    comparison = report['comparison']
    print('%d shared store sites: %d equal bytes; %d differing; %d unresolved; %d/%d unmatched; static address order equal=%s' %
          (comparison['shared_store_sites'], len(comparison['equal_bytes']), len(comparison['different_bytes']), len(comparison['unresolved_values']), len(comparison['reference_only_sites']), len(comparison['candidate_only_sites']), comparison['static_address_order_equal']))
    print('%d/%d shared static positions and %d/%d conservative CFG site-pair directions differ' % (comparison['static_site_positions_different'], comparison['shared_store_sites'], comparison['CFG_site_pair_directions_different'], comparison['CFG_shared_site_pair_denominator']))
    print('%d materialization changes; %d store sites differ in CFG direction relative to diagnostic calls' % (len(comparison['materialization_changes']), len(comparison['call_relative_order_changes'])))
    assert not a['unsupported'] and not b['unsupported'], 'unsupported operations: inspect report'
    assert not a['denominator']['unresolved_addresses'] and not b['denominator']['unresolved_addresses'], 'unresolved store address'


if __name__ == '__main__':
    main()
