#!/usr/bin/env python3
"""Conservative ELF32 absolute jump-table proof, separate from byteident grades.

Supported dispatches use CMP/JA fallthrough or a uniquely guarded taken JBE
edge, with an unmodified 32-bit index. One narrowly decoded flag-preserving
MOV may intervene. A 16-bit CMP/JA followed by an exact same-register MOVZX
also bounds the resulting 32-bit index. SETcc is admitted only in its exact
unprefixed register-byte encoding, in both frame models, because it reads the
flags and writes one byte register; memory and prefixed forms still need an
alias proof. Refusal is an unsupported proof, not unequal code.
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


def prove_fixed_stack_frame(rows, dispatch, destinations, calls=frozenset(), context=None):
    """Track one entry-allocated frame and untouched callee-save slots in CFG.

    This accepts ordinary register saves, not arbitrary stack writes. All
    paths start at the sized ABI entry, keep the same frame depth at joins,
    restore each saved register from its own unchanged slot, and deallocate
    the frame before RET. Incoming loads cannot alias a saved slot. Outgoing
    words are disjoint from all saves; direct relocated calls obey the i386
    callee-save/stack-return ABI, as in the pushed-frame proof. No frame address
    may escape. CWTL modifies only EAX.
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
                           'imul', 'cmp', 'test', 'ret', 'nop', 'call', 'cwtl'))
    aliases = {'ebx':'ebx', 'bx':'ebx', 'bl':'ebx', 'bh':'ebx',
               'esi':'esi', 'si':'esi', 'edi':'edi', 'di':'edi',
               'ebp':'ebp', 'bp':'ebp'}
    def written_callee(mnemonic, operands):
        if mnemonic == 'lea' and re.fullmatch(r'0x0\(%(esi|edi|ebp)(?:,%eiz,1)?\),%\1', operands):
            return None  # GNU as alignment no-op, no register value changes.
        if mnemonic in ('cmp', 'test', 'ret', 'nop') or mnemonic.startswith('j'):
            return None
        target = re.search(r'(?:^|,)%(\w+)$', operands)
        return aliases.get(target[1]) if target else None
    def base(mnemonic):
        return mnemonic[:-1] if mnemonic[-1:] in ('b','w','l') and mnemonic[:-1] in supported else mnemonic
    stack_ops = {}; slots = {}; saved = set(); written = set(); outgoing = set()
    for address, raw, mnemonic, operands in rows:
        original_mnemonic = mnemonic
        mnemonic = base(mnemonic)
        if mnemonic == 'call':
            require(address in calls, 'unproved fixed-frame ABI call')
            stack_ops[address] = ('call',)
            continue
        # SETcc has no implicit register or flag write: it READS the flags and
        # writes one byte register.  Admit only the exact unprefixed i386
        # register-byte encoding, as the pushed-ABI prover already does; the
        # ModRM /0 form excludes the memory and prefixed encodings, which need
        # an alias/stack-extent proof this model does not carry.
        setcc = mnemonic.startswith('set')
        if setcc:
            byte_registers = ('al','cl','dl','bl','ah','ch','dh','bh')
            require(len(raw) == 3 and raw[0] == 0x0f and
                    0x90 <= raw[1] <= 0x9f and raw[2] & 0xf8 == 0xc0 and
                    operands == '%' + byte_registers[raw[2] & 7],
                    'unsupported fixed-frame SETcc destination/encoding')
        require(mnemonic in supported or mnemonic.startswith('j') or setcc,
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
        if context is not None and (mnemonic=='lea' or original_mnemonic=='testb'):
            # Exact decoding, bounds, dominance and capability cleanup are
            # checked by private_frame_effects before this frame can pass.
            stack_ops[address] = ('private',)
            continue
        store = re.fullmatch(r'%(ebx|esi|edi|ebp),(?:(0x[0-9a-f]+))?\(%esp\)', operands)
        if store and address < dispatch:
            offset = int(store[2] or '0', 16); register = store[1]
            require(mnemonic == 'mov' and offset % 4 == 0 and offset + 4 <= frame,
                    'saved slot extent/form')
            require(address < dispatch and offset not in slots and register not in saved,
                    'saved slot duplicate/clobber')
            require(register not in written, 'saved register modified before save')
            slots[offset] = register; saved.add(register)
            stack_ops[address] = ('save', offset, register)
            continue
        outgoing_store = re.fullmatch(r'(?:\$0x[0-9a-f]+|%(?:eax|ebx|ecx|edx|esi|edi|ebp)),(?:(0x[0-9a-f]+))?\(%esp\)', operands)
        if outgoing_store:
            offset = int(outgoing_store[1] or '0', 16)
            require(original_mnemonic in ('mov','movl') and offset % 4 == 0 and offset + 4 <= frame,
                    'fixed-frame outgoing extent/form')
            outgoing.add(offset)
            stack_ops[address] = ('outgoing',)
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
    require(not outgoing.intersection(slots), 'fixed-frame outgoing/save overlap')
    require(written <= saved, 'unsaved fixed-frame callee-register write')
    by_address = {row[0]: i for i, row in enumerate(rows)}
    private = None
    if any(op[0]=='private' for op in stack_ops.values()):
        from callee_effects import private_frame_effects
        graph = {}
        for i,(address,raw,mnemonic,operands) in enumerate(rows):
            if mnemonic=='ret': edges=[]
            elif mnemonic=='jmp' and operands.startswith('*'): edges=list(destinations)
            elif mnemonic.startswith('j'):
                target=re.match(r'^([0-9a-f]+)\s',operands)
                require(target is not None,'private-frame branch decoding')
                edges=[int(target[1],16)]
                if mnemonic!='jmp': edges.append(address+len(raw))
            else: edges=[address+len(raw)]
            require(all(edge in by_address for edge in edges),'private-frame successor extent')
            graph[address]=edges
        private=private_frame_effects(context[0],context[1],rows,frame,slots,outgoing,calls,graph,context[2])
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
            'reachable_instructions': len(seen), 'returns': returns,
            'direct_abi_calls': len(calls), 'outgoing_slots': sorted(outgoing),
            'private_record': private}


def prove_pushed_abi_frame(rows, dispatch, destinations, calls):
    """Prove a bounded cdecl push-save frame with disjoint outgoing arguments.

    Direct relocated callees obey i386 callee-save/stack-return ABI. No frame
    address may escape, and only the entry saves and balanced exit pops touch
    save slots. Argument stores stay in the separately allocated outgoing area.
    """
    callee = frozenset(('ebx', 'esi', 'edi', 'ebp'))
    pushed = []; prefix = 0
    while prefix < len(rows) and rows[prefix][2] == 'push':
        operand = rows[prefix][3]
        require(operand.startswith('%') and operand[1:] in callee,
                'unsupported ABI entry push')
        register = operand[1:]
        require(register not in pushed, 'duplicate ABI register save')
        pushed.append(register); prefix += 1
    require(pushed and prefix < len(rows), 'missing ABI saves/frame')
    allocation = rows[prefix]
    match = re.fullmatch(r'\$0x([0-9a-f]+),%esp', allocation[3])
    if allocation[2] == 'sub' and match is not None:
        frame = int(match[1], 16)
        require(0 < frame <= 256 and frame % 4 == 0 and allocation[0] < dispatch,
                'ABI outgoing frame extent')
    else:
        # Push-only leaf: incoming arguments are above saved registers and
        # the return word. There is no private/outgoing area or call proof.
        frame = 0
        require(not calls, 'push-only ABI calls unsupported')
    total = frame + 4 * len(pushed)
    basic = frozenset(('mov','movswl','movzwl','movsbl','movzbl','lea','inc',
                       'dec','add','sub','neg','not','and','or','xor','shl',
                       'shr','sar','mul','imul','cmp','test','sbb','adc',
                       'ret','nop','push','pop','call'))
    aliases = {'ebx':'ebx','bx':'ebx','bl':'ebx','bh':'ebx',
               'esi':'esi','si':'esi','edi':'edi','di':'edi',
               'ebp':'ebp','bp':'ebp'}
    def base(mnemonic):
        return mnemonic[:-1] if mnemonic[-1:] in ('b','w','l') and mnemonic[:-1] in basic else mnemonic
    def writes(mnemonic, operands):
        if mnemonic == 'lea' and re.fullmatch(r'0x0\(%(esi|edi|ebp)(?:,%eiz,1)?\),%\1', operands):
            return None  # GNU as alignment no-op, no register value changes.
        if base(mnemonic) in ('cmp','test','ret','nop','call','push') or mnemonic.startswith('j'):
            return None
        target = re.search(r'(?:^|,)%(\w+)$', operands)
        return aliases.get(target[1]) if target else None
    operations = {}
    for i,(address,raw,mnemonic,operands) in enumerate(rows):
        op = base(mnemonic)
        # SETcc has no implicit register or flag writes. Admit only its exact
        # unprefixed i386 register-byte encoding; memory forms need a separate
        # alias/stack-extent proof. ModRM /0 excludes unsupported encodings.
        setcc = mnemonic.startswith('set')
        if setcc:
            byte_registers = ('al','cl','dl','bl','ah','ch','dh','bh')
            require(len(raw) == 3 and raw[0] == 0x0f and
                    0x90 <= raw[1] <= 0x9f and raw[2] & 0xf8 == 0xc0 and
                    operands == '%' + byte_registers[raw[2] & 7],
                    'unsupported ABI SETcc destination/encoding')
        require(op in basic or mnemonic.startswith('j') or setcc,
                'unsupported ABI-frame instruction')
        require(re.search(r'%sp\b',operands) is None, 'partial ABI stack pointer')
        if op == 'push':
            require(i < prefix, 'nonentry ABI push')
            operations[address] = ('push',pushed[i]); continue
        if op == 'pop':
            require(re.fullmatch(r'%(ebx|esi|edi|ebp)',operands) is not None,
                    'unsupported ABI pop')
            operations[address] = ('pop',operands[1:]); continue
        if op == 'call':
            require(address in calls, 'unproved ABI call')
            operations[address] = ('call',); continue
        written = writes(mnemonic, operands)
        require(written is None or written in pushed, 'unsaved ABI callee-register write')
        if '%esp' not in operands: continue
        adjust = re.fullmatch(r'\$0x([0-9a-f]+),%esp',operands)
        if adjust:
            require(op in ('sub','add') and int(adjust[1],16)==frame,
                    'unsupported ABI frame adjustment')
            require(op != 'sub' or i == prefix, 'reallocated ABI frame')
            operations[address] = ('allocate' if op=='sub' else 'release',); continue
        load = re.fullmatch(r'(?:(0x[0-9a-f]+))?\(%esp\),%(e(?:ax|bx|cx|dx|si|di|bp))',operands)
        if load:
            offset = int(load[1] or '0',16)
            require(op in ('mov','movswl','movzwl','movsbl','movzbl') and
                    offset >= total+4 and offset%4==0, 'ABI saved-slot/return-address read')
            operations[address] = ('argument',load[2]); continue
        store = re.fullmatch(r'(?:\$0x[0-9a-f]+|%(?:eax|ebx|ecx|edx|esi|edi|ebp)),(?:(0x[0-9a-f]+))?\(%esp\)',operands)
        require(store is not None and op=='mov', 'ABI frame address escape/unsupported access')
        offset = int(store[1] or '0',16)
        require(offset%4==0 and offset+4<=frame, 'ABI argument store overlaps save/return slot')
        operations[address] = ('outgoing',)
    by_address = {r[0]:i for i,r in enumerate(rows)}
    pending = [(rows[0][0],(0,(),False,frozenset()))]; seen = {}; returns = 0
    while pending:
        address,state = pending.pop()
        if address in seen:
            require(seen[address]==state, 'inconsistent ABI-frame join'); continue
        seen[address]=state; depth,saves,allocated,restored=state
        restored=set(restored); saves=list(saves); i=by_address[address]
        _,raw,mnemonic,operands=rows[i]; operation=operations.get(address)
        if operation:
            kind=operation[0]
            if kind=='push':
                require(not allocated and len(saves)==i, 'ABI push state')
                saves.append(operation[1]); depth+=4
            elif kind=='allocate':
                require(not allocated and saves==pushed and depth==total-frame,
                        'ABI allocation state')
                allocated=True; depth+=frame
            elif kind=='release':
                require(allocated and depth==total and saves==pushed,
                        'ABI release state')
                allocated=False; depth-=frame
            elif kind=='pop':
                require(not allocated and saves and saves[-1]==operation[1] and depth==4*len(saves),
                        'wrong/unbalanced ABI restore')
                saves.pop(); depth-=4; restored.add(operation[1])
            else:
                require((allocated or frame==0) and depth==total and saves==pushed,
                        'ABI call/argument access outside frame')
                if kind=='argument':restored.discard(operation[1])
        else:
            register=writes(mnemonic,operands)
            if register:restored.discard(register)
        if mnemonic=='ret':
            require(depth==0 and not allocated and not saves and restored==set(pushed),
                    'unbalanced ABI-frame return')
            returns+=1; continue
        if mnemonic=='jmp' and operands.startswith('*'):successors=destinations
        elif mnemonic.startswith('j'):
            target=re.match(r'^([0-9a-f]+)\s',operands)
            require(target is not None,'ABI branch target');successors=[int(target[1],16)]
            if mnemonic!='jmp':
                require(i+1<len(rows),'ABI branch fallthrough');successors.append(rows[i+1][0])
        else:
            require(i+1<len(rows),'ABI fallthrough extent');successors=[rows[i+1][0]]
        next_state=(depth,tuple(saves),allocated,frozenset(restored))
        for successor in successors:
            require(successor in by_address,'ABI successor extent');pending.append((successor,next_state))
    require(returns>0,'ABI frame without return')
    return {'bytes':frame,'pushed_registers':pushed,'reachable_instructions':len(seen),
            'returns':returns,'direct_abi_calls':len(calls)}



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
    pushed_abi = rows[0][2] == 'push'
    require(not any(r[2] in ('ljmp', 'lcall', 'iret', 'iretd', 'retf', 'syscall',
                            'sysenter', 'int', 'int3', 'into', 'enter', 'leave',
                            'pusha', 'popa') or (r[2] in ('push','pop') and not pushed_abi) or r[2] in PREFIXES
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
    corridor = None
    taken_guard = False
    extension = None
    # A JBE taken edge may dispatch across a default/return block. The unique
    # direct edge into the dispatch must be this guard; nothing may enter the
    # comparison/guard interior. This establishes dominance without treating
    # the intervening default block as part of the protected corridor.
    if (index >= 3 and rows[index - 1][2] == 'movzwl' and
            rows[index - 2][2] == 'ja'):
        cmp_row, guard, extension, jump = rows[index - 3:index + 1]
    elif guard[2] != 'ja':
        candidates = [(i, row) for i, row in enumerate(rows[:index])
                      if row[2] == 'jbe' and
                      re.match(r'^([0-9a-f]+)\s', row[3]) and
                      int(row[3].split()[0], 16) == jump[0]]
        require(len(candidates) == 1, 'missing unique taken-edge guard')
        guard_index, guard = candidates[0]
        require(guard_index >= 2, 'missing taken-edge comparison')
        cmp_row, corridor = rows[guard_index - 2:guard_index]
        require(rows[0][2] == 'sub' and stack_writes, 'taken-edge fixed frame required')
        require(rows[index - 1][2] in ('ret','jmp') and
                not rows[index - 1][3].startswith('*'),
                'taken-edge dispatch has fallthrough predecessor')
        taken_guard = True
    elif cmp_row[2] != 'cmp':
        require(pushed_abi and index >= 3, 'missing adjacent guard')
        corridor = cmp_row
        cmp_row = rows[index - 3]
    if extension is not None:
        # The unsigned 16-bit comparison constrains only the low half. The
        # exact zero-extension below then erases every untested upper bit.
        # No intervening instruction or alternate entry may bypass either
        # operation: the existing protected corridor includes the extension.
        registers = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
        words = ('ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di')
        match = re.fullmatch(r'\$0x([0-9a-f]+),%(ax|cx|dx|bx|bp|si|di)', cmp_row[3])
        require(cmp_row[2] == 'cmp' and match is not None,
                'unsigned immediate/word-register guard')
        bound = int(match[1], 16)
        code = words.index(match[2])
        register = registers[code]
        raw = cmp_row[1]
        short_cmp = (len(raw) == 4 and raw[:3] == bytes((0x66, 0x83, 0xf8 + code))
                     and raw[3] < 0x80 and raw[3] == bound)
        word_cmp = (len(raw) == 5 and raw[:3] == bytes((0x66, 0x81, 0xf8 + code))
                    and int.from_bytes(raw[3:], 'little') == bound)
        accumulator_cmp = (code == 0 and len(raw) == 4 and raw[:2] == b'\x66\x3d'
                           and int.from_bytes(raw[2:], 'little') == bound)
        require(short_cmp or word_cmp or accumulator_cmp,
                'unsupported word-guard encoding')
        require(extension[1] == bytes((0x0f, 0xb7, 0xc0 + 9 * code)) and
                extension[3] == '%' + words[code] + ',%' + register,
                'word-guard index zero-extension')
        count = bound + 1
    else:
        match = re.fullmatch(r'\$0x([0-9a-f]+),%(e(?:ax|bx|cx|dx|si|di|bp|sp))', cmp_row[3])
        require(cmp_row[2] == 'cmp' and match is not None, 'unsigned immediate/register guard')
        count = int(match[1], 16) + 1
        register = match[2]
    if corridor is not None:
        raw = corridor[1]
        if taken_guard:
            # One unprefixed MOV r32,disp32(r32), with no ESP base, modifies
            # neither EFLAGS nor the selector. Decode the field exactly; a
            # relocation in this displacement is not admitted below.
            operand = re.fullmatch(r'%(e(?:ax|bx|cx|dx|si|di|bp)),0x[0-9a-f]+\(%(e(?:ax|bx|cx|dx|si|di|bp))\)', corridor[3])
            require(corridor[2] == 'mov' and operand is not None and
                    len(raw) == 6 and raw[0] == 0x89 and
                    raw[1] & 0xc0 == 0x80 and raw[1] & 7 != 4,
                    'unsupported taken-edge guard corridor')
        else:
            # One unprefixed MOV disp8/disp32(%esp),r32 preserves EFLAGS. The
            # frame proof below separately owns the incoming-argument offset.
            operand = re.fullmatch(r'(0x[0-9a-f]+)\(%esp\),%(e(?:ax|bx|cx|dx|si|di|bp))', corridor[3])
            require(corridor[2] == 'mov' and operand is not None and
                    operand[2] != register and len(raw) in (4,7) and raw[0] == 0x8b and
                    raw[1] & 0xc7 == (0x44 if len(raw)==4 else 0x84) and raw[2] == 0x24,
                    'unsupported guard corridor')
    require(1 <= count <= 256, 'table entry bound')
    require(guard[2] == ('jbe' if taken_guard else 'ja'), 'unsigned dispatch guard')
    match = re.fullmatch(r'\*0x([0-9a-f]+)\(,%' + register + r',4\)', jump[3])
    require(jump[2] == 'jmp' and match is not None, 'absolute index/scale dispatch')
    if taken_guard:
        protected = (set(range(cmp_row[0] + 1, guard[0] + len(guard[1]))) |
                     set(range(jump[0], jump[0] + len(jump[1]))))
    else:
        protected = set(range(cmp_row[0] + 1, jump[0] + len(jump[1])))
    require(not any(s['st_shndx'] == section_id and s['st_value'] in protected
                    and s['st_info']['type'] != 'STT_SECTION' for s in symbols),
            'named guard bypass entry')
    require(not any(target in protected and not
                    (taken_guard and source == guard[0] and target == jump[0])
                    for source, target in section_direct_edges(str(path), section.name)),
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
    owner_relocs = [(r,s) for sec,r,s in relocations
                    if sec==section_id and start<=r['r_offset']<end]
    calls = set()
    fixed_abi = stack_writes and rows[0][2] == 'sub'
    if pushed_abi or fixed_abi:
        occupied = set()
        for r,symbol in owner_relocs:
            field = r['r_offset']
            matching = [row for row in rows if row[0] <= field and field+4 <= row[0]+len(row[1])]
            require(len(matching)==1 and not occupied.intersection(range(field,field+4)),
                    'overlapping/undecoded ABI relocation')
            occupied.update(range(field,field+4));row=matching[0]
            if row[2]=='call':
                require(row[1][:1]==b'\xe8' and len(row[1])==5 and field==row[0]+1 and
                        r['r_info_type']==2 and row[1][1:]==b'\xfc\xff\xff\xff' and
                        symbol.name and symbol['st_info']['type'] in ('STT_FUNC','STT_NOTYPE') and
                        symbol['st_shndx']!='SHN_ABS', 'unsupported ABI call relocation')
                calls.add(row[0])
            elif row[2].startswith('j'):
                require(row[0]==jump[0] and r['r_info_type']==1,
                        'additional relocated ABI control transfer')
            else:
                # Only decoded data fields may relocate. Frame/stack offsets,
                # guard bounds and opcode bytes were proved at their literal
                # values and must never be changed by the linker.
                raw=row[1]; offset=field-row[0]
                immediate_store=(row[2] in ('mov','movl') and
                                 len(raw)>=6 and raw[0]==0xc7 and
                                 raw[1]&0x38==0 and offset==len(raw)-4 and
                                 row[3].startswith('$') and
                                 not row[3].endswith(',%esp'))
                absolute_compare=(row[2] in ('cmp','cmpl') and
                                  len(raw)==7 and raw[:2]==b'\x83\x3d' and
                                  offset==2 and row[0]!=cmp_row[0])
                indexed_word_load=(row[2] in ('movswl','movzwl') and len(raw)==8 and
                                   raw[:2] in (b'\x0f\xbf',b'\x0f\xb7') and raw[2]&0xc7==0x84 and
                                   raw[3]&7!=4 and raw[3]>>3&7!=4 and offset==4)
                require(r['r_info_type']==1 and
                        (immediate_store or absolute_compare or indexed_word_load),
                        'unsupported ABI data relocation field')
        require(all(row[0] in calls for row in rows if row[2]=='call'),
                'unrelocated ABI call')
    else:
        require(len(owner_relocs)==1, 'additional owner relocations')
    for address, _, mnemonic, operands in rows:
        if address in calls: continue
        if mnemonic.startswith(('j', 'loop', 'call')) and not operands.startswith('*'):
            target = re.match(r'^([0-9a-f]+)\s', operands)
            require(target is not None, 'undecoded direct branch')
            destination = int(target[1], 16)
            require(destination in boundaries, 'external/interior direct branch')
            require(destination not in protected or
                    (taken_guard and address == guard[0] and destination == jump[0]),
                    'direct guard bypass')
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
    if pushed_abi:
        stack_proof = prove_pushed_abi_frame(rows, jump[0], [start+d for d in destinations], calls)
    else:
        stack_proof = prove_fixed_stack_frame(rows, jump[0],
                                              [start+d for d in destinations], calls, (str(path),name,
                                              [(table_id,o) for o in range(table_start,table_end,4)])) if stack_writes else None
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
