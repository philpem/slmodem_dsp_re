#!/usr/bin/env python3
"""Bounded, conditional effects of an actual ELF32 leaf callee.

This is a footprint/bit-dependency proof, not a source-level purity assertion.
External loads must be valid and disjoint from the supplied private record.
Unsupported instructions, unresolved bindings and escaping record pointers
are refusals. No function names or source summaries grant trust.
"""
import argparse
import hashlib
import json
import os
from functools import reduce
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from jumptable import Refused, require, instructions, section_direct_edges

REGS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')
SAVED = frozenset((3, 5, 6, 7))
# Bit = (constant or None, dependency bitset, original record bit or None).
ZERO = (0, 0, None)
ONE = (1, 0, None)
UNKNOWN = (None, 0, None)


def constants(value, width):
    return tuple(ONE if value >> bit & 1 else ZERO for bit in range(width))


def unknown(values, width):
    deps = 0
    for value in values:
        for bit in value:
            deps |= bit[1]
    return ((None, deps, None),) * width


def boolean(left, right, op):
    result = []
    for a, b in zip(left, right):
        absorbing = 0 if op == 'and' else 1
        identity = 1 - absorbing
        if a[0] == absorbing or b[0] == absorbing:
            result.append(ZERO if absorbing == 0 else ONE)
        elif a[0] == identity:
            result.append(b)
        elif b[0] == identity:
            result.append(a)
        else:
            result.append((None, a[1] | b[1], None))
    return tuple(result)


def decode(address, raw):
    """Decode a deliberately small i386 subset, including implicit effects.

    Operands are register(bit offset,width), memory(base,index,scale,disp,width),
    or immediate(value,width). The consumed byte count must equal the row size.
    """
    pos = 0
    def take(n=1, signed=False):
        nonlocal pos
        require(pos + n <= len(raw), 'truncated callee instruction')
        value = int.from_bytes(raw[pos:pos+n], 'little', signed=signed)
        pos += n
        return value
    word = raw[:1] == b'\x66'
    if word: pos += 1
    width = 16 if word else 32
    op = take()
    def reg(number, bits):
        if bits == 8:
            return ('r', number & 3, 8 if number >= 4 else 0, 8)
        return ('r', number, 0, bits)
    def modrm(bits):
        byte = take(); mode, number, rm = byte >> 6, byte >> 3 & 7, byte & 7
        if mode == 3: return reg(rm, bits), reg(number, bits), number
        base, index, scale, disp = rm, None, 1, 0
        if rm == 4:
            sib = take(); base, index, scale = sib & 7, sib >> 3 & 7, 1 << (sib >> 6)
            if index == 4: index = None
        displacement = None
        if mode == 0 and base == 5:
            base = None; displacement = pos; disp = take(4, True)
        elif mode == 1:
            displacement = pos; disp = take(1, True)
        elif mode == 2:
            displacement = pos; disp = take(4, True)
        return ('m', base, index, scale, disp, bits, displacement), reg(number, bits), number
    args = (); target = None
    if 0x50 <= op <= 0x5f and not word:
        kind = 'push' if op < 0x58 else 'pop'; args = (reg(op & 7, 32),)
    elif op == 0xc3 and not word:
        kind = 'ret'
    elif op == 0x90 and not word:
        kind = 'nop'
    elif 0xb8 <= op <= 0xbf:
        kind = 'mov'; args = (('i', take(width // 8), width), reg(op & 7, width))
    elif op in (0x88, 0x89, 0x8a, 0x8b):
        bits = 8 if op in (0x88, 0x8a) else width
        require(not word or bits == 16, 'unsupported byte prefix')
        rm, rr, _ = modrm(bits); kind = 'mov'
        args = (rm, rr) if op & 2 else (rr, rm)
    elif op in (0xc6, 0xc7):
        bits = 8 if op == 0xc6 else width
        require(not word or bits == 16, 'unsupported byte prefix')
        rm, _, group = modrm(bits); require(group == 0, 'unknown MOV group')
        kind = 'mov'; args = (('i', take(bits // 8), bits), rm)
    elif op in (0x80, 0x81, 0x83):
        bits = 8 if op == 0x80 else width
        require(not word or bits == 16, 'unsupported byte prefix')
        rm, _, group = modrm(bits)
        require(group in (0, 1, 4, 5, 7), 'unsupported immediate ALU')
        kind = {0:'add', 1:'or', 4:'and', 5:'sub', 7:'cmp'}[group]
        args = (('i', take(1 if op in (0x80, 0x83) else bits // 8, op == 0x83), bits), rm)
    elif op in (0x00, 0x01, 0x08, 0x09, 0x20, 0x21, 0x28, 0x29, 0x84, 0x85):
        bits = width if op & 1 else 8
        require(not word or bits == 16, 'unsupported byte prefix')
        rm, rr, _ = modrm(bits)
        kind = {0:'add', 8:'or', 0x20:'and', 0x28:'sub', 0x84:'test'}[op & 0xfe]
        args = (rr, rm)
    elif op in (0x24, 0x0c, 0x25, 0x0d):
        bits=width if op&1 else 8
        require(not word or bits==16, 'unsupported accumulator byte prefix')
        kind = 'and' if op in (0x24,0x25) else 'or'
        args = (('i', take(bits//8), bits), reg(0,bits))
    elif op==0x8d and not word:
        rm,rr,_=modrm(32)
        require(rm[0]=='m' and rm[1]==rr[1] and rm[2] is None and rm[4]==0,
                'callee LEA is not exact identity padding')
        kind='nop'
    elif op in (0xc0, 0xc1, 0xd0, 0xd1):
        bits = width if op & 1 else 8
        require(not word or bits == 16, 'unsupported byte prefix')
        rm, _, group = modrm(bits)
        require(group in (4, 5, 7), 'unsupported shift group')
        kind = {4:'shl', 5:'shr', 7:'sar'}[group]
        args = (('i', take() if op in (0xc0, 0xc1) else 1, bits), rm)
    elif op == 0x69:
        rm, rr, _ = modrm(width); kind = 'imul'
        args = (('i', take(width // 8), width), rm, rr)
    elif (0x70 <= op <= 0x7f or op in (0xeb, 0xe9)) and not word:
        kind = 'jmp' if op in (0xeb, 0xe9) else 'branch'
        delta = take(4 if op == 0xe9 else 1, True); target = address + pos + delta
    elif op == 0x0f and not word:
        second = take()
        if second in (0xb6, 0xb7, 0xbe, 0xbf):
            bits = 8 if second in (0xb6, 0xbe) else 16
            rm, rr, number = modrm(bits); kind = 'movsx' if second in (0xbe, 0xbf) else 'movzx'
            args = (rm, reg(number, 32))
        elif 0x90 <= second <= 0x9f:
            rm, _, group = modrm(8)
            require(group == 0 and rm[0] == 'r', 'unsupported SETcc encoding/destination')
            kind = 'set'; args = (rm,)
        elif 0x80 <= second <= 0x8f:
            kind = 'branch'; delta = take(4, True); target = address + pos + delta
        else:
            raise Refused('unsupported callee two-byte opcode')
    else:
        raise Refused('unsupported callee opcode (including calls/implicit writes)')
    require(pos == len(raw), 'callee instruction extent/prefix')
    return kind, args, target


def artifact(path, name, check_entry=True):
    """Load one strong function and its binding facts without resolver fallback."""
    data = Path(path).read_bytes()
    import io
    elf = ELFFile(io.BytesIO(data))
    require(elf.elfclass == 32 and elf.little_endian and elf['e_type'] == 'ET_REL'
            and elf['e_machine'] == 'EM_386', 'unsupported callee ELF/ABI')
    tab = elf.get_section_by_name('.symtab')
    require(tab is not None, 'missing callee symbol table')
    symbols = list(tab.iter_symbols())
    matches = [(i,s) for i,s in enumerate(symbols) if s.name == name]
    require(len(matches) == 1, 'ambiguous callee name')
    index, fn = matches[0]
    require(fn['st_info']['bind'] in ('STB_GLOBAL', 'STB_LOCAL') and
            fn['st_info']['type'] == 'STT_FUNC' and
            isinstance(fn['st_shndx'], int) and fn['st_size'] > 0,
            'callee must be a defined strong sized function in supplied ELF')
    section = elf.get_section(fn['st_shndx']); start = fn['st_value']; end = start + fn['st_size']
    require(section['sh_flags'] & 4 and end <= section['sh_size'], 'callee function extent')
    require(sum(s.name == section.name for s in elf.iter_sections()) == 1, 'ambiguous callee section')
    for i,s in enumerate(symbols):
        if i == index or s['st_shndx'] != fn['st_shndx'] or s['st_info']['type'] == 'STT_SECTION': continue
        require(not start <= s['st_value'] < end, 'callee alias/interior entry')
        if s['st_info']['type'] == 'STT_FUNC' and s['st_size']:
            require(s['st_value'] + s['st_size'] <= start or s['st_value'] >= end,
                    'overlapping callee owner')
    relocs = []
    for sec in elf.iter_sections():
        if not isinstance(sec, RelocationSection): continue
        require(sec['sh_type'] == 'SHT_REL' and sec['sh_entsize'] == 8 and
                sec['sh_link'] == elf.get_section_index('.symtab'), 'unsupported callee relocations')
        for r in sec.iter_relocations():
            symbol = symbols[r['r_info_sym']]
            if sec['sh_info'] == fn['st_shndx'] and r['r_offset'] < end and r['r_offset'] + 4 > start:
                require(start <= r['r_offset'] and r['r_offset'] + 4 <= end, 'cross-boundary callee relocation')
                relocs.append((r, symbol))
            if check_entry and symbol['st_shndx'] == fn['st_shndx']:
                require(r['r_info_type'] in (1,2), 'unknown incoming callee relocation')
                owner = elf.get_section(sec['sh_info'])
                raw = owner.data()[r['r_offset']:r['r_offset']+4]
                require(len(raw) == 4, 'incoming callee relocation extent')
                target = symbol['st_value'] + int.from_bytes(raw, 'little', signed=True)
                targets = (target,) if r['r_info_type'] == 1 else (target, target+4)
                require(not any(start < t < end for t in targets), 'relocated interior callee entry')
    for source,target in (section_direct_edges.__wrapped__(str(path), section.name) if check_entry else []):
        require(not (start < target < end and not start <= source < end), 'direct interior callee entry')
    return elf, symbols, index, fn, section, relocs, hashlib.sha256(data).hexdigest()


def _align_up(value, alignment):
    alignment = alignment or 1
    return (value + alignment - 1) // alignment * alignment


def _function_identity(path, name):
    """Masked body bytes and relocation shape for one sized function.

    A `ld -r` artifact rebases section-relative addends, so the raw four-byte
    fields are not expected to equal the input object's. Bytes outside those
    fields must match, the relocation offsets/types/targets must match, and
    every relocation to one target must move by the same link rebase. Each
    record also carries the target's section and symbol value so the caller can
    compare that rebase with the actual section placement.

    The four-byte i386 addend is a two's-complement value, not an unsigned
    offset: an indexed table can legitimately address `table-12`, which `ld -r`
    rewrites to `placement-12`. Reading it unsigned turns that correct rebase
    into a delta off by 2**32, so the field is sign-extended here and every
    comparison below is made between signed addends.
    """
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        require(elf.elfclass == 32 and elf.little_endian and
                elf['e_type'] == 'ET_REL', 'unsupported cross-TU body ELF/ABI')
        table = elf.get_section_by_name('.symtab')
        require(table is not None, 'missing cross-TU body symbol table')
        matches = [s for s in table.iter_symbols()
                   if s.name == name and s['st_info']['type'] == 'STT_FUNC' and
                   isinstance(s['st_shndx'], int) and s['st_size'] > 0]
        require(len(matches) == 1, 'ambiguous cross-TU body')
        fn = matches[0]
        section = elf.get_section(fn['st_shndx'])
        data = section.data()
        start, end = fn['st_value'], fn['st_value'] + fn['st_size']
        require(section['sh_flags'] & 4 and 0 <= start and end <= len(data),
                'cross-TU body extent')
        raw = bytearray(data[start:end])
        relocations = []
        for sec in elf.iter_sections():
            if not isinstance(sec, RelocationSection) or sec['sh_info'] != fn['st_shndx']:
                continue
            symbols = elf.get_section(sec['sh_link'])
            require(symbols is not None and symbols['sh_type'] == 'SHT_SYMTAB',
                    'cross-TU relocation symbol table')
            for relocation in sec.iter_relocations():
                if not start <= relocation['r_offset'] < end:
                    continue
                require(relocation['r_info_type'] in (1, 2),
                        'unsupported cross-TU callee relocation')
                offset = relocation['r_offset'] - start
                require(offset + 4 <= len(raw), 'cross-TU relocation extent')
                target = symbols.get_symbol(relocation['r_info_sym'])
                if (target['st_info']['type'] == 'STT_SECTION' and
                        isinstance(target['st_shndx'], int)):
                    key = ('section', elf.get_section(target['st_shndx']).name)
                    target_section = key[1]
                else:
                    key = ('symbol', target.name)
                    target_section = (elf.get_section(target['st_shndx']).name
                                      if isinstance(target['st_shndx'], int) else None)
                addend = int.from_bytes(raw[offset:offset + 4], 'little',
                                        signed=True)
                relocations.append((offset, relocation['r_info_type'], key, addend,
                                    target['st_value'], target_section))
                raw[offset:offset + 4] = b'\0\0\0\0'
        return bytes(raw), tuple(sorted(relocations))


def _manifest_inputs(manifest):
    """The ordered link inputs, exactly as `ld -r` received them."""
    directory = Path(manifest).parent
    paths = []
    for number, line in enumerate(Path(manifest).read_text().splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split()
        require(len(fields) == 2, 'recovered-order manifest record')
        object_path = directory / fields[0]
        require(object_path.is_file(), 'recovered-order manifest object missing')
        paths.append(object_path)
    return paths


def _unique_section(elf, name, what):
    sections = [s for s in elf.iter_sections() if s.name == name]
    require(len(sections) == 1, what + ' section is missing or ambiguous: ' + name)
    return sections[0]


def _elf_section_identity(elf, section_name):
    """Masked bytes and relocation shape of one named section.

    `ld -r` rewrites a relocation's four-byte addend field and leaves the rest
    of a section unchanged. Masking those fields makes a COMDAT input copy
    directly comparable with the linked output section, so the resolver can
    observe which input copy the linker kept instead of assuming one.
    """
    require(elf.elfclass == 32 and elf.little_endian and
            elf['e_type'] == 'ET_REL', 'unsupported COMDAT section ELF/ABI')
    section = _unique_section(elf, section_name, 'COMDAT')
    index = elf.get_section_index(section_name)
    raw = bytearray(section.data())
    relocations = []
    for sec in elf.iter_sections():
        if not isinstance(sec, RelocationSection) or sec['sh_info'] != index:
            continue
        table = elf.get_section(sec['sh_link'])
        require(table is not None and table['sh_type'] == 'SHT_SYMTAB',
                'COMDAT relocation symbol table')
        for relocation in sec.iter_relocations():
            require(relocation['r_info_type'] in (1, 2),
                    'unsupported COMDAT relocation')
            offset = relocation['r_offset']
            require(0 <= offset and offset + 4 <= len(raw),
                    'cross-boundary COMDAT relocation')
            target = table.get_symbol(relocation['r_info_sym'])
            if (target['st_info']['type'] == 'STT_SECTION' and
                    isinstance(target['st_shndx'], int)):
                key = ('section', elf.get_section(target['st_shndx']).name)
            else:
                key = ('symbol', target.name,
                       elf.get_section(target['st_shndx']).name
                       if isinstance(target['st_shndx'], int)
                       else str(target['st_shndx']))
            relocations.append((offset, relocation['r_info_type'], key))
            raw[offset:offset + 4] = b'\0\0\0\0'
    return (bytes(raw), tuple(sorted(relocations)),
            section['sh_size'], section['sh_addralign'])


def _section_identity(path, section_name):
    with open(path, 'rb') as stream:
        return _elf_section_identity(ELFFile(stream), section_name)


def _comdat_section_placement(inputs, selected, section_name, linked_elf):
    """Placement of the kept name-based COMDAT group in its linked section.

    GNU `ld -r` keeps the first `.gnu.linkonce.*` section of a given name in
    input order and discards every later copy. The output section therefore
    contains the kept copy alone at offset 0. The kept copy is not guessed: the
    manifest's first contributor is compared with the linked section's masked
    bytes and relocation shape, and any disagreement refuses. Copies may
    legitimately differ outside relocated fields (different register
    allocation), so only the first contributor's identity is required; every
    contributor must still agree on size and alignment, otherwise the group is
    ambiguous.
    """
    contributors = []
    kept_size = kept_align = None
    for index, path in enumerate(inputs):
        with open(path, 'rb') as stream:
            elf = ELFFile(stream)
            require(elf.elfclass == 32 and elf.little_endian and
                    elf['e_type'] == 'ET_REL',
                    'unsupported manifest object ELF/ABI')
            matching = [s for s in elf.iter_sections() if s.name == section_name]
        require(len(matching) <= 1, 'ambiguous input section: ' + section_name)
        if not matching:
            continue
        section = matching[0]
        if kept_size is None:
            kept_size = section['sh_size']
            kept_align = section['sh_addralign']
        else:
            require(section['sh_size'] == kept_size and
                    section['sh_addralign'] == kept_align,
                    'COMDAT group copies disagree on size/alignment: ' + section_name)
        contributors.append((index, path))
    require(contributors,
            'manifest-selected copy has no target section: ' + section_name)
    require(any(index == selected for index, _ in contributors),
            'manifest-selected copy has no target section: ' + section_name)
    kept = _section_identity(str(contributors[0][1]), section_name)
    linked_section = _unique_section(linked_elf, section_name, 'linked')
    require(linked_section['sh_size'] == kept[2] and
            linked_section['sh_addralign'] == kept[3],
            'linked COMDAT section size/alignment differs from the kept group: '
            + section_name)
    linked = _elf_section_identity(linked_elf, section_name)
    require(linked[:2] == kept[:2],
            'linked COMDAT section is not the first contributing input: '
            + section_name)
    return 0


def _section_placement(manifest, source, section_name, linked_elf):
    """Where `source`'s named input section landed in the linked output section.

    The partial link is `ld -r` over the ordered manifest, so each same-named
    input section is concatenated at its own alignment. Reproducing that layout
    from the manifest's section headers and requiring it to explain the linked
    section's whole size turns the expected relocation delta into an observed
    placement rather than a guess.

    A `.gnu.linkonce.t.*` name is name-based COMDAT rather than a concatenated
    section, so it takes the kept-group path above. Every other section name
    keeps the ordinary concatenation checks unchanged.
    """
    inputs = _manifest_inputs(manifest)
    source = os.path.realpath(str(source))
    indices = [i for i, path in enumerate(inputs)
               if os.path.realpath(str(path)) == source]
    require(len(indices) == 1,
            'manifest-selected copy is not a unique ordered link input')
    selected = indices[0]
    if section_name.startswith('.gnu.linkonce.t.'):
        return _comdat_section_placement(inputs, selected, section_name,
                                         linked_elf)
    cursor = 0
    placement = None
    for index, path in enumerate(inputs):
        with open(path, 'rb') as stream:
            elf = ELFFile(stream)
            require(elf.elfclass == 32 and elf.little_endian and
                    elf['e_type'] == 'ET_REL', 'unsupported manifest object ELF/ABI')
            matching = [s for s in elf.iter_sections() if s.name == section_name]
        require(len(matching) <= 1, 'ambiguous input section: ' + section_name)
        if matching:
            section = matching[0]
            cursor = _align_up(cursor, section['sh_addralign'])
            if index == selected:
                placement = cursor
            cursor += section['sh_size']
    require(placement is not None,
            'manifest-selected copy has no target section: ' + section_name)
    linked_section = _unique_section(linked_elf, section_name, 'linked')
    require(cursor == linked_section['sh_size'],
            'manifest layout does not explain linked section: ' + section_name)
    return placement


def _same_linked_body(source, linked, name, manifest):
    """Is `linked` the same coded body as `source`, rebased by `ld -r`?

    The masked body and relocation shape must match. Each relocation's addend
    is signed, and its expected linked value is implied by the actual output
    section placement: a section-symbol target's addend becomes
    `placement + source_addend` (so a negative table base stays negative
    relative to its section), while a named-symbol target keeps its addend
    while the symbol itself moves by that offset. A uniform but stale delta is
    therefore a refusal.
    """
    source_raw, source_relocations = _function_identity(source, name)
    linked_raw, linked_relocations = _function_identity(linked, name)
    require(len(source_raw) == len(linked_raw),
            'linked callee body size differs from the manifest-selected copy')
    require([r[:3] for r in source_relocations] == [r[:3] for r in linked_relocations],
            'linked callee relocation shape differs from the manifest-selected copy')
    require(source_raw == linked_raw,
            'linked callee body bytes differ from the manifest-selected copy')
    with open(linked, 'rb') as stream:
        linked_elf = ELFFile(stream)
        placements = {}
        def placement(section):
            if section not in placements:
                placements[section] = _section_placement(
                    manifest, source, section, linked_elf)
            return placements[section]
        rebase = {}
        section_rebases = {}
        symbol_rebases = {}
        for source_record, linked_record in zip(source_relocations, linked_relocations):
            (_, kind, key, source_addend, source_value,
             source_section) = source_record
            (_, other_kind, other_key, linked_addend, linked_value,
             linked_section) = linked_record
            require((kind, key) == (other_kind, other_key),
                    'linked callee relocation target differs from the '
                    'manifest-selected copy')
            if key[0] == 'section':
                require(source_section == linked_section == key[1],
                        'linked callee section target differs from the '
                        'manifest-selected copy')
                offset = placement(key[1])
                expected_addend = offset + source_addend
                section_rebases[key[1]] = {
                    'placement': offset,
                    'observed_addend_delta': linked_addend - source_addend}
            else:
                # A named-symbol relocation keeps its addend: `ld -r` rebases
                # the symbol, not the four-byte field. An undefined reference
                # in the input may simply become defined in the link, which is
                # symbol resolution and not a section rebase.
                expected_addend = source_addend
                if source_section is not None:
                    require(linked_section == source_section,
                            'linked callee symbol target section differs from '
                            'the manifest-selected copy')
                    moved = linked_value - source_value
                    require(moved == placement(source_section),
                            'linked callee symbol is not rebased by its section '
                            'placement')
                symbol_rebases[key[1]] = {
                    'section': source_section,
                    'resolved_section': linked_section,
                    'observed_value_delta': (linked_value - source_value
                                             if source_section is not None else None),
                    'observed_addend_delta': linked_addend - source_addend}
            delta = linked_addend - source_addend
            require(linked_addend == expected_addend,
                    'linked callee relocation addend does not match its '
                    'section placement')
            require(rebase.setdefault((kind, key), delta) == delta,
                    'linked callee relocation is not a uniform section rebase')
    return {'section_rebases': section_rebases, 'symbol_rebases': symbol_rebases}


def _selected_manifest_copy(manifest, name):
    """The unique strong definition of `name` among the link's input objects."""
    copies = []
    for object_path in _manifest_inputs(manifest):
        with open(object_path, 'rb') as stream:
            elf = ELFFile(stream)
            table = elf.get_section_by_name('.symtab')
            require(table is not None, 'manifest object has no symbol table')
            for symbol in table.iter_symbols():
                if (symbol.name == name and
                        symbol['st_info']['type'] == 'STT_FUNC' and
                        symbol['st_info']['bind'] == 'STB_GLOBAL' and
                        isinstance(symbol['st_shndx'], int) and
                        symbol['st_size'] > 0):
                    copies.append(object_path)
    copies = list(dict.fromkeys(copies))
    require(len(copies) == 1,
            'callee is not a unique strong definition in the recovered-order manifest')
    return copies[0]


def _linked_callee(path, name, link, manifest):
    """Prove an undefined callee through the recovered-order partial link."""
    require(link is not None and manifest is not None,
            'undefined callee requires the recovered-order partial link')
    require(os.path.realpath(str(link)) != os.path.realpath(str(path)),
            'recovered-order link cannot resolve itself')
    require(Path(link).is_file() and Path(link).stat().st_size > 0,
            'recovered-order link artifact missing')
    require(Path(manifest).is_file() and Path(manifest).stat().st_size > 0,
            'recovered-order link manifest missing')
    source = _selected_manifest_copy(str(manifest), name)
    rebase = _same_linked_body(str(source), str(link), name, str(manifest))
    return source, rebase


def prove(path, name, argument=1, capacity=64):
    """All terminating paths of a bounded acyclic leaf, conditional on separation.

    argument is a zero-based cdecl argument index. capacity is a caller-proved
    bound, not a declaration of a source object size. Results cannot by
    themselves discharge external-read separation or a caller's ABI proof.
    """
    require(0 <= argument < 8 and 0 < capacity <= 256, 'callee argument/capacity bound')
    elf, symbols, index, fn, section, relocs, digest = artifact(path, name)
    start = fn['st_value']; end = start + fn['st_size']
    rows = instructions(str(path), section.name, start, end)
    require(b''.join(row[1] for row in rows)==section.data()[start:end],
            'callee bytes changed since disassembly (stale cache)')
    require(len(rows) <= 256, 'callee instruction budget')
    decoded = {a:decode(a,raw) for a,raw,_,_ in rows}
    nexts = {}; by_address = {a:(raw,m,o) for a,raw,m,o in rows}
    for a,raw,_,_ in rows:
        kind,args,target = decoded[a]
        successors = [] if kind == 'ret' else [a+len(raw)]
        if kind == 'jmp': successors = [target]
        if kind == 'branch': successors.append(target)
        if kind=='nop' and successors==[end]: successors=[]
        require(all(s in decoded for s in successors), 'callee branch/fallthrough outside body')
        nexts[a] = successors
    active = set(); visited = set()
    def visit(a):
        require(a not in active, 'callee loop unsupported')
        if a in visited: return
        active.add(a)
        for s in nexts[a]: visit(s)
        active.remove(a); visited.add(a)
    visit(start)
    require(all(nexts[a] or decoded[a][0]=='ret' for a in visited),
            'callee reachable fallthrough without return')
    require(all(a in visited or decoded[a][0]=='nop' for a in decoded),
            'unreachable non-padding callee instructions')
    reloc_by_address = {}
    relocation_facts = []
    for relocation,symbol in relocs:
        offset = relocation['r_offset']
        owner = next((a for a,raw,_,_ in rows if a <= offset < a+len(raw)), None)
        require(owner is not None, 'callee relocation owner')
        kind,args,_ = decoded[owner]
        memory = args[0] if kind in ('mov','movzx','movsx') and args else None
        require(memory and memory[0] == 'm' and memory[6] is not None and
                offset == owner+memory[6] and memory[6]+4 == len(by_address[owner][0]) and
                relocation['r_info_type'] == 1 and isinstance(symbol['st_shndx'],int),
                'unsupported callee relocation operand')
        require(memory[1]!=4 and memory[2]!=4, 'relocated stack argument/address')
        target_section = elf.get_section(symbol['st_shndx'])
        require(target_section['sh_flags'] & 2 and not target_section['sh_flags'] & (1|4),
                'callee relocated read is not readonly data')
        require(owner not in reloc_by_address, 'overlapping callee relocations')
        reloc_by_address[owner] = offset
        relocation_facts.append([offset-start, 1, symbol.name, symbol['st_shndx'], symbol['st_value']])
    original = tuple((None, 1 << bit, bit) for bit in range(capacity*8))
    initial = {'regs': {i:(UNKNOWN,)*32 for i in range(8)}, 'defined':{i:0 for i in range(8)}, 'memory':original,
               'stack':(), 'dirty':frozenset(), 'pointer':None, 'loaded':False,
               'flags':None, 'reads':frozenset(), 'writes':frozenset()}
    pending = [(start,initial)]; finals=[]; executed=0; stores=set(); external_reads=set()
    def copy(state):
        return dict(state, regs=dict(state['regs']), defined=dict(state['defined']))
    while pending:
        a,state = pending.pop(); state=copy(state); executed += 1
        require(executed <= 4096, 'callee path budget')
        kind,args,target = decoded[a]
        def record_operand(operand):
            _,base,index,scale,disp,bits,_ = operand
            if base == state['pointer'] and base is not None:
                require(index is None and 0 <= disp and disp+bits//8 <= capacity,
                        'record access outside caller capacity/indexed')
                return disp,bits
            require(index != state['pointer'] or index is None, 'record pointer used as index')
            return None
        def read(operand):
            if operand[0] == 'i': return constants(operand[1],operand[2])
            if operand[0] == 'r':
                _,r,lo,bits=operand
                require(r not in (4,state['pointer']), 'stack/record pointer read or escape')
                require(state['defined'][r] & (((1<<bits)-1)<<lo) == (((1<<bits)-1)<<lo),
                        'incoming/undefined register value consumed')
                return state['regs'][r][lo:lo+bits]
            _,base,index,scale,disp,bits,_ = operand
            record = record_operand(operand)
            if record:
                require(a not in reloc_by_address, 'relocated record read/address')
                state['reads'] |= frozenset(range(disp,disp+bits//8))
                return state['memory'][disp*8:disp*8+bits]
            if base == 4:
                require(index is None and bits == 32 and disp % 4 == 0 and
                        disp >= 4*len(state['stack'])+4, 'callee stack read overlaps saves/return')
                slot = disp-4*len(state['stack'])
                require(slot < 4*(argument+1), 'record argument copied or unbounded incoming argument read')
            else:
                require(index != 4, 'stack used as index')
                external_reads.add(a)
                address_values=[read(('r',r,0,32)) for r in (base,index) if r is not None]
                require(not any(bit[1] for value in address_values for bit in value),
                        'record-dependent external address')
                return unknown(address_values,bits)
            return (UNKNOWN,)*bits
        def write(operand,value):
            if operand[0] == 'r':
                _,r,lo,bits=operand
                require(r not in (4,state['pointer']), 'stack/record pointer mutation')
                require(r not in SAVED or r in [x[0] for x in state['stack']],
                        'unsaved callee register mutation')
                old=state['regs'][r]; state['regs'][r]=old[:lo]+tuple(value)+old[lo+bits:]
                state['dirty'] |= frozenset((r,))
                state['defined'][r] |= ((1<<bits)-1)<<lo
            else:
                require(operand[0] == 'm', 'invalid callee destination')
                record=record_operand(operand)
                require(record is not None, 'non-record memory write')
                disp,bits=record
                old=state['memory']; state['memory']=old[:disp*8]+tuple(value)+old[disp*8+bits:]
                state['writes'] |= frozenset(range(disp,disp+bits//8)); stores.add(a)
        if kind == 'push':
            r=args[0][1]
            require(r in SAVED and not state['loaded'] and r not in [x[0] for x in state['stack']]
                    and r not in state['dirty'], 'nonentry/unsafe callee save')
            state['stack'] += ((r,state['regs'][r],state['defined'][r]),)
        elif kind == 'pop':
            r=args[0][1]
            require(state['stack'] and state['stack'][-1][0] == r, 'callee restore order/owner')
            state['regs'][r]=state['stack'][-1][1]; state['defined'][r]=state['stack'][-1][2]
            state['stack']=state['stack'][:-1]
            state['dirty'] -= frozenset((r,))
        elif kind == 'ret':
            require(not state['stack'] and not (state['dirty'] & SAVED) and state['loaded'],
                    'unbalanced callee return/unloaded argument')
            require(state['pointer'] != 0, 'record pointer returned')
            finals.append(state); continue
        elif kind in ('branch','jmp','nop'):
            if kind=='branch':
                require(state['flags'] is not None and not any(bit[1] for bit in state['flags']), 'undefined/record-dependent control flow')
        elif kind == 'set':
            require(state['flags'] is not None, 'undefined flags consumed by SETcc')
            write(args[0],unknown([state['flags']],1)+(ZERO,)*7)
        elif kind in ('mov','movzx','movsx'):
            src,dst=args
            pointer_load = (src[0]=='m' and src[1:4]==(4,None,1) and src[4] ==
                            4*(len(state['stack'])+argument+1))
            if pointer_load:
                require(kind=='mov' and src[5]==32 and dst[0]=='r' and dst[2:]==(0,32)
                        and not state['loaded'] and dst[1] not in SAVED and dst[1]!=4,
                        'unsupported record pointer argument load')
                state['pointer']=dst[1]; state['loaded']=True
                state['defined'][dst[1]]=(1<<32)-1
            else:
                value=read(src)
                if kind in ('movzx','movsx'):
                    value += ((ZERO if kind=='movzx' else value[-1]),)*(32-len(value))
                write(dst,value)
        else:
            values=[read(x) for x in (args[:-1] if kind=='imul' else args)]
            width=args[-1][-1] if args[-1][0]=='r' else len(values[-1])
            if kind in ('cmp','test'):
                state['flags']=unknown(values,1)
            else:
                require(args[-1][0]=='r', 'memory ALU/implicit write unsupported')
                if kind in ('and','or'): value=boolean(values[0],values[1],kind)
                elif kind in ('shl','shr','sar'):
                    require(args[0][0]=='i', 'variable callee shift')
                    count=args[0][1]&31; source=values[1]
                    require(0 < count < width, 'zero/oversized callee shift')
                    if kind=='shl': value=(ZERO,)*count+source[:width-count]
                    else: value=source[count:]+((source[-1] if kind=='sar' else ZERO),)*count
                elif kind=='add' and args[0]==args[1]: value=(ZERO,)+values[0][:-1]
                else: value=unknown(values,width)
                write(args[-1],value); state['flags']=unknown(values,1)
        for successor in nexts[a]: pending.append((successor,state))
    require(finals, 'callee has no normal return')
    def masks(predicate):
        return {str(byte):sum(1<<bit for bit in range(8)
                             if all(predicate(s['memory'][byte*8+bit],byte*8+bit) for s in finals))
                for byte in range(capacity)}
    initialized=masks(lambda value,bit:value[1]==0)
    preserved=masks(lambda value,bit:value[2]==bit)
    reads=sorted(set.union(*(set(s['reads']) for s in finals)))
    writes=sorted(set.union(*(set(s['writes']) for s in finals)))
    return {'function':name, 'artifact_sha256':digest, 'symbol_index':index,
            'binding':fn['st_info']['bind'], 'visibility':fn['st_other']['visibility'],
            'section':section.name, 'entry':start, 'size':end-start,
            'body_sha256':hashlib.sha256(section.data()[start:end]).hexdigest(),
            'argument':argument, 'capacity':capacity, 'instructions':len(rows),
            'reachable_instructions':len(visited),
            'paths':len(finals), 'store_instructions':len(stores),
            'record_read_masks':{str(x):255 for x in reads},
            'record_write_masks':{str(x):255 for x in writes},
            'initialized_masks':{k:v for k,v in initialized.items() if v},
            'preserved_masks':{k:v for k,v in preserved.items() if v},
            'external_read_instructions':len(external_reads), 'relocations':relocation_facts,
            'retained_record_registers':sorted({REGS[s['pointer']] for s in finals}),
            'defined_return_register_masks':{REGS[r]:reduce(
                int.__and__,(s['defined'][r] for s in finals)) for r in range(8)},
            'obligations':['normal cdecl entry with valid arguments',
                           'all non-record reads valid and disjoint from private record']}


def prove_call(path, caller, address, argument=1, capacity=64, link=None, manifest=None):
    """Resolve this exact R_386_PC32/-4 CALL in this ELF, then prove its body.

    A target defined in this ELF is proved from this ELF. Only an undefined
    global is resolved through the caller-supplied recovered-order partial
    link, and only after the linked body matches the unique strong definition
    in that link's input manifest.
    """
    elf,symbols,_,fn,section,_,digest=artifact(path,caller,check_entry=False)
    start=fn['st_value']; end=start+fn['st_size']
    rows=instructions(str(path),section.name,start,end)
    require(b''.join(row[1] for row in rows)==section.data()[start:end],
            'caller bytes changed since disassembly (stale cache)')
    row=next((r for r in rows if r[0]==address),None)
    require(row is not None and row[1]==b'\xe8\xfc\xff\xff\xff', 'callee call encoding/addend')
    matches=[]
    for sec in elf.iter_sections():
        if isinstance(sec,RelocationSection) and sec['sh_info']==fn['st_shndx']:
            for r in sec.iter_relocations():
                if address <= r['r_offset'] < address+5: matches.append(r)
    require(len(matches)==1 and matches[0]['r_offset']==address+1 and
            matches[0]['r_info_type']==2, 'callee call relocation binding')
    target=symbols[matches[0]['r_info_sym']]
    require(target.name, 'callee call target name')
    if target['st_shndx'] != 'SHN_UNDEF':
        result=prove(path,target.name,argument,capacity)
        require(result['symbol_index']==matches[0]['r_info_sym'], 'callee symbol identity mismatch')
        resolution='per-tu'
        selected=None
        callee_rebase=None
    else:
        require(target['st_info']['bind']=='STB_GLOBAL',
                'undefined callee is not a global definition')
        selected,callee_rebase=_linked_callee(path,target.name,link,manifest)
        result=prove(str(link),target.name,argument,capacity)
        require(result['binding']=='STB_GLOBAL',
                'linked callee is not a strong global definition')
        resolution='recovered-order-link'
    result['call']={'caller':caller,'address':address,'relocation_offset':address+1,
                    'type':'R_386_PC32','addend':-4,'target_symbol_index':result['symbol_index'],
                    'caller_target_symbol_index':matches[0]['r_info_sym'],
                    'resolution':resolution,'caller_artifact_sha256':digest,
                    'link_manifest':str(manifest) if selected is not None else None,
                    'selected_object':str(selected) if selected is not None else None,
                    'callee_rebase':callee_rebase}
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object'); parser.add_argument('function')
    parser.add_argument('--argument',type=int,default=1)
    parser.add_argument('--capacity',type=int,default=64)
    args=parser.parse_args()
    try: result=prove(args.object,args.function,args.argument,args.capacity)
    except Refused as error: parser.exit(1,'UNPROVED: '+str(error)+'\n')
    print(json.dumps(result,indent=2))


def require_initialized(contract, offset, mask):
    """Check a caller's actual read mask, not merely the callee's store union."""
    require(0 <= offset < contract['capacity'] and 0 < mask <= 255,
            'private record read extent/mask')
    available = contract['initialized_masks'].get(str(offset), 0)
    require(available & mask == mask, 'private record read includes uninitialized bits')


def private_frame_effects(path, name, rows, frame, saves, outgoing, calls, successors, table_entries=(), link=None, manifest=None):
    """Prove one record's address corridor and masked reads in an ABI frame.

    The enclosing frame prover must validate every other stack access, all
    scalar instructions and normal returns. This routine does not substitute
    for that proof. Only one address-taken region and one leaf call are admitted.
    """
    import re
    elf,symbols,_,fn,section,_,_=artifact(path,name,check_entry=False)
    start=fn['st_value']; end=start+fn['st_size']
    require(b''.join(row[1] for row in rows)==section.data()[start:end],
            'private caller changed since disassembly')
    for source,target in section_direct_edges.__wrapped__(str(path),section.name):
        require(not (start < target < end and not start <= source < end),
                'external direct private-frame entry')
    allowed=set(table_entries)
    for sec in elf.iter_sections():
        if not isinstance(sec,RelocationSection): continue
        for relocation in sec.iter_relocations():
            symbol=symbols[relocation['r_info_sym']]
            if symbol['st_shndx']!=fn['st_shndx']: continue
            require(relocation['r_info_type'] in (1,2),'unknown private-frame entry relocation')
            raw=elf.get_section(sec['sh_info']).data()[relocation['r_offset']:relocation['r_offset']+4]
            require(len(raw)==4,'private-frame entry relocation extent')
            target=symbol['st_value']+int.from_bytes(raw,'little',signed=True)
            targets=(target,) if relocation['r_info_type']==1 else (target,target+4)
            if any(start < t < end for t in targets):
                require((sec['sh_info'],relocation['r_offset']) in allowed and
                        relocation['r_info_type']==1 and symbol['st_info']['type']=='STT_SECTION',
                        'unowned relocated private-frame entry')
    origins=[]; reads=[]
    for i,(a,raw,m,operands) in enumerate(rows):
        if m=='lea' and '%esp' in operands:
            match=re.fullmatch(r'(?:0x([0-9a-f]+))?\(%esp\),%(eax|ecx|edx)',operands)
            require(match is not None and len(raw) in (3,4,7) and raw[0]==0x8d and
                    raw[1]&0xc7 in (0x04,0x44,0x84) and raw[2]==0x24,
                    'unsupported private-frame address encoding')
            require(REGS[raw[1]>>3&7]==match[2], 'private-frame address register')
            displacement=int.from_bytes(raw[3:],'little',signed=True) if len(raw)>3 else 0
            require(displacement==int(match[1] or '0',16), 'private-frame address displacement')
            origins.append((i,displacement,match[2]))
        if m=='testb' and '%esp' in operands:
            match=re.fullmatch(r'\$0x([0-9a-f]+),(?:0x([0-9a-f]+))?\(%esp\)',operands)
            require(match is not None and len(raw) in (4,5,8) and raw[0]==0xf6 and
                    raw[1] in (0x04,0x44,0x84) and raw[2]==0x24,
                    'unsupported private record read encoding')
            offset=int.from_bytes(raw[3:-1],'little',signed=True) if len(raw)>4 else 0
            mask=int(match[1],16)
            require(offset==int(match[2] or '0',16) and mask==raw[-1] and mask,
                    'private record read displacement/mask')
            reads.append((a,offset,mask))
    require(len(origins)==1, 'one private frame address required')
    origin_index,offset,register=origins[0]; origin=rows[origin_index][0]
    protected=set(saves)|set(outgoing)|{frame}
    require(offset%4==0 and 0 <= offset < frame and
            not any(p <= offset < p+4 for p in protected if p != frame),
            'private record aliases outgoing/save/return slots')
    limit=min(p for p in protected if p>offset)
    capacity=limit-offset
    call_index=next((i for i in range(origin_index+1,len(rows)) if rows[i][0] in calls),None)
    require(call_index is not None, 'private record has no bound call')
    call=rows[call_index][0]; argument=None; argument_store=None; supplied=set(); stack_writes=[]
    corridor={row[0] for row in rows[origin_index+1:call_index+1]}
    for a,edges in successors.items():
        require(not any(t in corridor and not origin <= a < call for t in edges),
                'private argument corridor has bypass entry')
    for a,raw,m,operands in rows[origin_index+1:call_index]:
        operation,operands_decoded,_=decode(a,raw)
        require(operation not in ('branch','jmp','ret','push','pop'),
                'private argument corridor control flow/implicit effects')
        if operation=='mov' and operands_decoded[-1][0]=='m':
            dst=operands_decoded[-1]
            if dst[1]==4:
                require(dst[2] is None, 'indexed private argument store')
                stack_writes.append((a,dst[4],dst[5]//8))
        supplied_slot=re.fullmatch(r'(?:\$0x[0-9a-f]+|%(?:eax|ebx|ecx|edx|esi|edi|ebp)),(?:0x([0-9a-f]+))?\(%esp\)',operands)
        if supplied_slot and m in ('mov','movl'):
            supplied.add(int(supplied_slot[1] or '0',16)//4)
        # Aliases include low/high partial registers of the address owner.
        aliases={'eax':('eax','ax','al','ah'),'ecx':('ecx','cx','cl','ch'),
                 'edx':('edx','dx','dl','dh')}[register]
        if any('%'+r in operands for r in aliases):
            match=re.fullmatch(r'%'+register+r',(?:0x([0-9a-f]+))?\(%esp\)',operands)
            require(argument is None and match is not None and m=='mov' and raw[0]==0x89,
                    'private pointer mutation/copy/escape before call')
            slot=int(match[1] or '0',16)
            require(slot in outgoing and slot%4==0, 'private pointer argument slot')
            argument=slot//4; argument_store=a
    require(argument is not None, 'private pointer not passed in matched argument')
    require(set(range(argument+1)) <= supplied, 'callee arguments not established in private corridor')
    require(not any(a>argument_store and lo<argument*4+4 and lo+width>argument*4
                    for a,lo,width in stack_writes), 'private pointer argument overwritten before call')
    contract=prove_call(path,name,call,argument,capacity,link=link,manifest=manifest)
    # Nothing else in the enclosing function may materialize a frame address.
    # Other stores/loads are checked by the ordinary fixed-frame proof. Thus
    # non-stack pointer arguments and the callee's external reads cannot reach
    # this live, previously unescaped private region under the existing ABI.
    by_address={r[0]:r for r in rows}
    predecessors={a:set() for a in successors}
    for a,edges in successors.items():
        for t in edges: predecessors[t].add(a)
    reachable=set(); pending=[rows[0][0]]
    while pending:
        a=pending.pop()
        if a in reachable: continue
        reachable.add(a); pending.extend(successors[a])
    dominators={a:({a} if a==rows[0][0] else set(reachable)) for a in reachable}
    changed=True
    while changed:
        changed=False
        for a in reachable-{rows[0][0]}:
            parents=predecessors[a]&reachable
            new={a}|set.intersection(*(dominators[p] for p in parents))
            if new!=dominators[a]: dominators[a]=new;changed=True
    require(call in reachable, 'unreachable private call')
    for a,read_offset,mask in reads:
        require(a in reachable and call in dominators[a], 'private read not dominated by checked call')
        require_initialized(contract,read_offset-offset,mask)
    # Caller-save is permission to clobber, not evidence of a clobber. Track
    # the actual retained argument pointer and any incoming address register
    # that this body did not definitely overwrite, plus the outgoing slot.
    caps=set(contract['retained_record_registers'])
    if contract['defined_return_register_masks'][register] != 0xffffffff:
        caps.add(register)
    pending=[(a,frozenset(caps),True) for a in successors[call]]
    seen=set(); cleared=set()
    aliases={'eax':('eax','ax','al','ah'),'ecx':('ecx','cx','cl','ch'),
             'edx':('edx','dx','dl','dh')}
    while pending:
        a,caps,stale=pending.pop()
        state=(a,caps,stale)
        if state in seen: continue
        seen.add(state); _,raw,m,operands=by_address[a]
        require(a not in calls and m!='ret', 'retained private pointer reaches call/return')
        caps=set(caps)
        for cap in tuple(caps):
            used=any(re.search(r'%'+r+r'\b',operands) for r in aliases[cap])
            if used:
                # Only a full-width MOV definition from a non-capability
                # source kills this address. Partial writes do not suffice.
                match=re.fullmatch(r'(.+),%'+cap,operands)
                require(m=='mov' and match is not None and
                        not any(re.search(r'%'+r+r'\b',match[1])
                                for x in caps for r in aliases[x]) and
                        ((raw[0]==0x8b and raw[1]>>3&7==REGS.index(cap)) or
                         (raw[0]==0x89 and raw[1]&0xc7==0xc0|REGS.index(cap)) or
                         raw[0]==0xb8+REGS.index(cap)),
                        'retained private pointer use/partial kill')
                caps.remove(cap)
        match=re.fullmatch(r'(?:\$0x[0-9a-f]+|%(?:eax|ebx|ecx|edx|esi|edi|ebp)),(?:0x([0-9a-f]+))?\(%esp\)',operands)
        if match and m in ('mov','movl') and int(match[1] or '0',16)==argument*4:
            stale=False
        if not caps and not stale:
            cleared.add(a); continue
        pending.extend((s,frozenset(caps),stale) for s in successors[a])
    require(cleared, 'private capabilities are never cleared')
    return {'origin':origin,'offset':offset,'capacity':capacity,'argument_store':argument_store,
            'argument_clear_sites':sorted(cleared),'masked_reads':reads,'callee':contract}


if __name__=='__main__': main()
