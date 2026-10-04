#!/usr/bin/env python3
"""Conservative ELF32 absolute jump-table proof, separate from byteident grades.

Only adjacent CMP/JA/JMP dispatches with an unmodified 32-bit index are
supported. Refusal is evidence of an unsupported proof, not unequal code.
"""
import argparse
import io
import json
import re
import subprocess
from functools import lru_cache
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

PREFIXES = frozenset(('rep', 'repz', 'repnz', 'bnd', 'notrack', 'addr16',
                      'data16', 'cs', 'ds', 'es', 'fs', 'gs', 'ss', 'lock',
                      'xacquire', 'xrelease'))


class Refused(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise Refused(reason)


@lru_cache(maxsize=None)
def instructions(path, section, start, end):
    output = subprocess.check_output([
        'objdump', '-dw', '--section=' + section,
        '--start-address=' + str(start), '--stop-address=' + str(end), path],
        text=True)
    rows = []
    for line in output.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*\t(\S+)\s*(.*)$', line)
        if match:
            address, raw, mnemonic, operands = match.groups()
            rows.append((int(address, 16), bytes.fromhex(raw), mnemonic, operands))
    require(rows and rows[0][0] == start, 'incomplete function decoding')
    cursor = start
    for address, raw, mnemonic, _ in rows:
        require(address == cursor and mnemonic not in ('(bad)', '.byte', '.word'),
                'invalid or incomplete instruction decoding')
        cursor += len(raw)
    require(cursor == end, 'function decoding extent')
    return rows


@lru_cache(maxsize=None)
def section_direct_edges(path, section):
    output = subprocess.check_output(['objdump', '-dw', '--section=' + section, path], text=True)
    edges = []
    for line in output.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t(.*)$', line)
        if match:
            tokens = match[2].split()
            while tokens and tokens[0] in PREFIXES:
                tokens.pop(0)
            if len(tokens) >= 2 and tokens[0].startswith(('j', 'loop', 'call')):
                if re.fullmatch('[0-9a-f]+', tokens[1]):
                    edges.append((int(match[1], 16), int(tokens[1], 16)))
    return edges


def prove_fixed_stack_frame(rows, dispatch, destinations):
    """Track one entry-allocated frame and untouched callee-save slots in CFG.

    This accepts ordinary register saves, not arbitrary stack writes. All
    paths start at the sized ABI entry, keep the same frame depth at joins,
    restore each saved register from its own unchanged slot, and deallocate
    the frame before RET. Argument loads cannot alias a saved slot.
    """
    first = rows[0]
    frame_match = re.fullmatch(r'\$0x([0-9a-f]+),%esp', first[3])
    require(first[2] == 'sub' and frame_match is not None,
            'fixed frame must be allocated at entry')
    frame = int(frame_match[1], 16)
    require(0 < frame <= 256 and frame % 4 == 0, 'fixed frame extent')
    callee = frozenset(('ebx', 'esi', 'edi', 'ebp'))
    supported = frozenset(('mov', 'movswl', 'movzwl', 'movsbl', 'movzbl',
                           'lea', 'inc', 'dec', 'add', 'sub', 'neg', 'not',
                           'and', 'or', 'xor', 'shl', 'shr', 'sar', 'mul',
                           'imul', 'cmp', 'test', 'ret', 'nop'))
    aliases = {'ebx':'ebx', 'bx':'ebx', 'bl':'ebx', 'bh':'ebx',
               'esi':'esi', 'si':'esi', 'edi':'edi', 'di':'edi',
               'ebp':'ebp', 'bp':'ebp'}
    def written_callee(mnemonic, operands):
        if mnemonic in ('cmp', 'test', 'ret', 'nop') or mnemonic.startswith('j'):
            return None
        target = re.search(r'(?:^|,)%(\w+)$', operands)
        return aliases.get(target[1]) if target else None
    stack_ops = {}; slots = {}; saved = set(); written = set()
    for address, raw, mnemonic, operands in rows:
        require(mnemonic in supported or mnemonic.startswith('j'),
                'unsupported fixed-frame instruction')
        require(re.search(r'%sp\b', operands) is None, 'partial stack-pointer access')
        if '%esp' not in operands:
            register = written_callee(mnemonic, operands)
            if register: written.add(register)
            continue
        adjust = re.fullmatch(r'\$0x([0-9a-f]+),%esp', operands)
        if adjust:
            require(mnemonic in ('add', 'sub') and int(adjust[1], 16) == frame,
                    'unsupported fixed-frame adjustment')
            require(mnemonic != 'sub' or address == first[0],
                    'reallocated fixed frame')
            stack_ops[address] = ('allocate' if mnemonic == 'sub' else 'release',)
            continue
        store = re.fullmatch(r'%(ebx|esi|edi|ebp),(?:(0x[0-9a-f]+))?\(%esp\)', operands)
        if store:
            offset = int(store[2] or '0', 16); register = store[1]
            require(mnemonic == 'mov' and offset % 4 == 0 and offset + 4 <= frame,
                    'saved slot extent/form')
            require(address < dispatch and offset not in slots and register not in saved,
                    'saved slot duplicate/clobber')
            require(register not in written, 'saved register modified before save')
            slots[offset] = register; saved.add(register)
            stack_ops[address] = ('save', offset, register)
            continue
        load = re.fullmatch(r'(?:(0x[0-9a-f]+))?\(%esp\),%(e(?:ax|bx|cx|dx|si|di|bp))', operands)
        require(load is not None and mnemonic == 'mov', 'unsupported fixed-frame stack access')
        offset = int(load[1] or '0', 16); register = load[2]
        if offset < frame:
            require(slots.get(offset) == register, 'saved slot wrong-owner read')
            stack_ops[address] = ('restore', offset, register)
        else:
            require(offset >= frame + 4 and offset % 4 == 0,
                    'return address/unaligned argument read')
            stack_ops[address] = ('argument', register)
            if register in callee: written.add(register)
    require(saved, 'fixed frame without supported saved registers')
    by_address = {row[0]: i for i, row in enumerate(rows)}
    pending = [(first[0], (False, frozenset(), frozenset()))]; seen = {}
    returns = 0
    while pending:
        address, state = pending.pop()
        if address in seen:
            require(seen[address] == state, 'inconsistent fixed-frame join')
            continue
        seen[address] = state
        allocated, available, restored = state
        i = by_address[address]; _, raw, mnemonic, operands = rows[i]
        available = set(available); restored = set(restored)
        operation = stack_ops.get(address)
        if operation:
            kind = operation[0]
            if kind == 'allocate':
                require(not allocated and not available, 'fixed-frame allocation state')
                allocated = True
            elif kind == 'release':
                require(allocated and restored == saved, 'frame release before register restoration')
                allocated = False
            else:
                require(allocated, 'stack access outside allocated frame')
                if kind == 'save': available.add(operation[1])
                elif kind == 'restore':
                    require(operation[1] in available, 'restore without saved slot')
                    restored.add(operation[2])
                elif kind == 'argument': restored.discard(operation[1])
        else:
            register = written_callee(mnemonic, operands)
            if register: restored.discard(register)
        if mnemonic == 'ret':
            require(not allocated and restored == saved, 'unbalanced fixed-frame return')
            returns += 1
            continue
        if mnemonic == 'jmp' and operands.startswith('*'):
            successors = destinations
        elif mnemonic.startswith('j'):
            target = re.match(r'^([0-9a-f]+)\s', operands)
            require(target is not None, 'fixed-frame branch target')
            successors = [int(target[1], 16)]
            if mnemonic != 'jmp':
                require(i + 1 < len(rows), 'fixed-frame branch fallthrough extent')
                successors.append(rows[i + 1][0])
        else:
            require(i + 1 < len(rows), 'fixed-frame fallthrough extent')
            successors = [rows[i + 1][0]]
        next_state = (allocated, frozenset(available), frozenset(restored))
        for successor in successors:
            require(successor in by_address, 'fixed-frame successor extent')
            pending.append((successor, next_state))
    require(returns > 0, 'fixed frame without reachable return')
    return {'bytes': frame, 'saved_slots': sorted(slots.items()),
            'reachable_instructions': len(seen), 'returns': returns}


def prove(path, name):
    """Return one fully proved table's ordered destinations and provenance.

    The ABI boundary is entry at the unique sized function symbol. Arbitrary
    jumps from unrelated code into its interior are outside this proof. Known
    direct/table edges and ELF relocations into the guarded sequence refuse it.
    """
    elf = ELFFile(io.BytesIO(Path(path).read_bytes()))
    require(elf.elfclass == 32 and elf.little_endian and
            elf['e_machine'] == 'EM_386' and elf['e_type'] == 'ET_REL', 'ELF format')
    sections = list(elf.iter_sections())
    symtab = elf.get_section_by_name('.symtab')
    require(symtab is not None, 'missing symbols')
    symbols = list(symtab.iter_symbols())
    functions = [s for s in symbols if s['st_info']['type'] == 'STT_FUNC'
                 and isinstance(s['st_shndx'], int) and s['st_size']]
    owners = [s for s in functions if s.name == name]
    require(len(owners) == 1, 'ambiguous owner')
    owner = owners[0]
    section_id = owner['st_shndx']
    section = sections[section_id]
    start, end = owner['st_value'], owner['st_value'] + owner['st_size']
    require(section['sh_type'] == 'SHT_PROGBITS' and section['sh_flags'] & 4,
            'non-executable owner')
    require(sum(s['st_shndx'] == section_id and s['st_value'] < end and
                s['st_value'] + s['st_size'] > start for s in functions) == 1,
            'overlapping function ownership')
    require(not any(s['st_info']['type'] == 'STT_FUNC' and not s['st_size'] and
                    s['st_shndx'] == section_id and start <= s['st_value'] < end
                    for s in symbols), 'zero-sized function alias')
    require(sum(s.name == section.name for s in sections) == 1, 'ambiguous code section')
    rows = instructions(str(path), section.name, start, end)
    require(not any(r[2] in ('ljmp', 'lcall', 'iret', 'iretd', 'retf', 'syscall',
                            'sysenter', 'int', 'int3', 'into', 'enter', 'leave',
                            'push', 'pop', 'pusha', 'popa') or r[2] in PREFIXES
                    for r in rows),
            'unsupported control/stack instruction')
    require(not any(r[2] == 'ret' and r[3] for r in rows), 'unsupported return form')
    stack_writes = any(r[2] not in ('cmp', 'test') and
                       (re.search(r'(?:^|,)%e?sp$', r[3]) or
                        r[3].endswith(')') and '%esp' in r[3]) for r in rows)
    boundaries = {r[0] for r in rows}
    indirect = [i for i, r in enumerate(rows) if r[2].startswith(('j', 'call'))
                and r[3].startswith('*')]
    require(len(indirect) == 1, 'additional or missing indirect control transfer')
    index = indirect[0]
    require(index >= 2, 'missing adjacent guard')
    cmp_row, guard, jump = rows[index - 2:index + 1]
    match = re.fullmatch(r'\$0x([0-9a-f]+),%(e(?:ax|bx|cx|dx|si|di|bp|sp))', cmp_row[3])
    require(cmp_row[2] == 'cmp' and match is not None, 'unsigned immediate/register guard')
    count = int(match[1], 16) + 1
    register = match[2]
    require(1 <= count <= 256, 'table entry bound')
    require(guard[2] == 'ja', 'unsigned JA guard')
    match = re.fullmatch(r'\*0x([0-9a-f]+)\(,%' + register + r',4\)', jump[3])
    require(jump[2] == 'jmp' and match is not None, 'absolute index/scale dispatch')
    protected = set(range(cmp_row[0] + 1, jump[0] + len(jump[1])))
    for address, _, mnemonic, operands in rows:
        if mnemonic.startswith(('j', 'loop', 'call')) and not operands.startswith('*'):
            target = re.match(r'^([0-9a-f]+)\s', operands)
            require(target is not None, 'undecoded direct branch')
            destination = int(target[1], 16)
            require(destination in boundaries, 'external/interior direct branch')
            require(destination not in protected, 'direct guard bypass')
    require(not any(s['st_shndx'] == section_id and s['st_value'] in protected
                    and s['st_info']['type'] != 'STT_SECTION' for s in symbols),
            'named guard bypass entry')
    require(not any(target in protected for _, target in
                    section_direct_edges(str(path), section.name)),
            'section direct guard bypass')
    relocations = []
    for relsec in sections:
        if isinstance(relsec, RelocationSection):
            require(not relsec.is_RELA(), 'RELA unsupported')
            require(0 <= relsec['sh_info'] < len(sections) and
                    0 <= relsec['sh_link'] < len(sections), 'relocation section indices')
            linked = sections[relsec['sh_link']]
            require(linked['sh_type'] == 'SHT_SYMTAB', 'relocation symbol table')
            for relocation in relsec.iter_relocations():
                require(relocation['r_info_sym'] < linked.num_symbols(), 'relocation symbol index')
                symbol = linked.get_symbol(relocation['r_info_sym'])
                if symbol['st_shndx'] == section_id:
                    require(relocation['r_info_type'] in (1, 2),
                            'unsupported incoming relocation kind')
                if relocation['r_info_type'] in (1, 2):
                    require(relocation['r_offset'] + 4 <=
                            sections[relsec['sh_info']]['sh_size'] and
                            sections[relsec['sh_info']]['sh_type'] == 'SHT_PROGBITS',
                            'relocation field extent')
                relocations.append((relsec['sh_info'], relocation, symbol))
    code_relocs = [(r, s) for sec, r, s in relocations
                  if sec == section_id and jump[0] <= r['r_offset'] < jump[0] + len(jump[1])]
    require(len(code_relocs) == 1, 'dispatch relocation count')
    require(sum(sec == section_id and start <= r['r_offset'] < end
                for sec, r, _ in relocations) == 1, 'additional owner relocations')
    relocation, table_symbol = code_relocs[0]
    require(relocation['r_info_type'] == 1 and
            relocation['r_offset'] == jump[0] + len(jump[1]) - 4 and
            table_symbol['st_info']['type'] == 'STT_SECTION' and
            isinstance(table_symbol['st_shndx'], int), 'dispatch relocation form')
    table_id = table_symbol['st_shndx']
    table_section = sections[table_id]
    require(sum(s.name == table_section.name for s in sections) == 1,
            'ambiguous table section')
    require(table_section['sh_type'] == 'SHT_PROGBITS' and
            table_section['sh_flags'] & 2 and not table_section['sh_flags'] & (1 | 4 | 16),
            'table storage flags')
    table_start = int.from_bytes(jump[1][-4:], 'little') + table_symbol['st_value']
    table_end = table_start + 4 * count
    require(table_start % 4 == 0 and table_end <= table_section['sh_size'], 'table extent/alignment')
    require(not any(s['st_shndx'] == table_id and
                    s['st_info']['type'] in ('STT_OBJECT', 'STT_FUNC') and
                    s['st_size'] and s['st_value'] < table_end and
                    s['st_value'] + s['st_size'] > table_start for s in symbols),
            'named object table overlap')
    entries = [(r, s) for sec, r, s in relocations if sec == table_id
               and r['r_offset'] < table_end and r['r_offset'] + 4 > table_start]
    require(len(entries) == count and sorted(r['r_offset'] for r, _ in entries) ==
            list(range(table_start, table_end, 4)), 'entry relocation extent/count')
    destinations = []
    for entry, symbol in sorted(entries, key=lambda item: item[0]['r_offset']):
        require(entry['r_info_type'] == 1 and symbol['st_shndx'] == section_id and
                symbol['st_info']['type'] == 'STT_SECTION', 'entry relocation form')
        offset = entry['r_offset']
        target = int.from_bytes(table_section.data()[offset:offset + 4], 'little') + symbol['st_value']
        require(target in boundaries and start <= target < end, 'entry instruction boundary/owner')
        require(target not in protected, 'table guard bypass')
        destinations.append(target - start)
    stack_proof = prove_fixed_stack_frame(rows, jump[0],
                                          [start + d for d in destinations]) if stack_writes else None
    for sec, relocation, symbol in relocations:
        if relocation['r_info_type'] in (1, 2) and symbol['st_shndx'] == section_id:
            raw = sections[sec].data()[relocation['r_offset']:relocation['r_offset'] + 4]
            target = int.from_bytes(raw, 'little', signed=True) + symbol['st_value']
            # PC32 in a direct branch uses a -4 inline addend. Conservatively
            # reject either interpretation for other PC-relative consumers.
            targets = {target} if relocation['r_info_type'] == 1 else {target, target + 4}
            require(not targets & protected, 'relocated guard bypass')
    return {'function': name, 'count': count, 'targets': destinations,
            'fixed_stack_frame': stack_proof,
            'table_section': table_section.name, 'table_offset': table_start,
            'relocation_offset': relocation_offset(code_relocs, start),
            'guard_offset': cmp_row[0] - start, 'index': register,
            'identity': ('jump-table', 'R_386_32', count,
                         tuple((name, target) for target in destinations))}


def relocation_offset(code_relocs, start):
    return code_relocs[0][0]['r_offset'] - start


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object')
    parser.add_argument('function')
    args = parser.parse_args()
    try:
        result = prove(args.object, args.function)
    except Refused as error:
        parser.exit(1, 'UNPROVED: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
