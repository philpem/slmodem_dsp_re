"""Exact-only identity for one narrowly guarded anonymous i386 switch table.

This proves relocation destination identity, not ABI, arbitrary pointer
arithmetic, or semantic equivalence. Entry is at the unique sized function.
All *observed* relocation and direct references are checked. The descriptor
must never participate in register-renaming comparison.
"""
import io
import re
import subprocess
from functools import lru_cache
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from jumptable import PREFIXES, Refused, require, instructions

TAG = 'exact-switch-v1'
# Deliberately bounded to ordinary instructions exercised by this proof's
# current domain. Unknown instructions must never acquire a fallthrough edge
# by default: objdump prints prefixes such as NOTRACK/BND as the mnemonic.
ORDINARY = frozenset(('add', 'cmp', 'cmpl', 'lea', 'mov', 'movl',
                      'pop', 'push', 'setne', 'sub', 'test', 'xor',
                      'and', 'andb', 'or', 'orb', 'movb', 'movw'))
CONDITIONAL = frozenset(('jo', 'jno', 'jb', 'jae', 'je', 'jne', 'jbe', 'ja',
                         'js', 'jns', 'jp', 'jnp', 'jl', 'jge', 'jle', 'jg'))


def modrm_end(raw, offset):
    """End of one unprefixed 32-bit-addressing ModRM/SIB operand."""
    require(len(raw) > offset, 'missing ModRM')
    mod, rm = raw[offset] >> 6, raw[offset] & 7
    length = offset + 1
    if mod != 3 and rm == 4:
        require(len(raw) > length, 'missing SIB')
        base = raw[length] & 7
        length += 1
    else:
        base = rm
    return length + (4 if mod == 0 and base == 5 else 1 if mod == 1 else 4 if mod == 2 else 0)


def control_kind(row):
    """Decode only unprefixed supported transfers; refuse every unknown row."""
    _, raw, mnemonic, operands = row
    if mnemonic in ('movswl', 'movzbl'):
        # Only unprefixed MOVSX r/m16,r32 and MOVZX r/m8,r32. Decode
        # ModRM/SIB displacement extent explicitly; 66/67/segment prefixes
        # change the widths/addressing contract and remain unsupported.
        require(raw[:2] == (b'\x0f\xbf' if mnemonic == 'movswl' else b'\x0f\xb6')
                and len(raw) >= 3, 'unsupported extension encoding')
        require(len(raw) == modrm_end(raw, 2) and
                re.search(r',%e(?:ax|cx|dx|bx|sp|bp|si|di)$', operands),
                'extension operand width/extent')
        return 'ordinary'
    if mnemonic == 'movw':
        # Native witnesses use exactly 66 C7 /0 imm16. No address-size,
        # segment, repeat or second operand-size prefix is admitted.
        require(raw[:2] == b'\x66\xc7' and len(raw) >= 5 and
                raw[2] & 0x38 == 0 and len(raw) == modrm_end(raw, 2) + 2,
                'unsupported word move encoding')
        return 'ordinary'
    if mnemonic in ORDINARY:
        require(raw and raw[0] not in
                (0xf0, 0xf2, 0xf3, 0x2e, 0x36, 0x3e, 0x26, 0x64, 0x65, 0x67)
                and raw[0] != 0x66,
                'unsupported ordinary prefix')
        return 'ordinary'
    if mnemonic == 'nop':
        require(raw == b'\x90' and not operands, 'unsupported nop encoding')
        return 'ordinary'
    if mnemonic == 'ret':
        require(raw == b'\xc3' and not operands, 'unsupported return encoding')
        return 'return'
    if mnemonic == 'call':
        require(len(raw) == 5 and raw[0] == 0xe8 and direct_target(row) is not None,
                'unsupported call encoding')
        return 'call'
    if mnemonic in CONDITIONAL:
        require((len(raw) == 2 and 0x70 <= raw[0] <= 0x7f) or
                (len(raw) == 6 and raw[0] == 0x0f and 0x80 <= raw[1] <= 0x8f),
                'unsupported conditional encoding')
        return 'conditional'
    if mnemonic == 'jmp':
        if len(raw) == 7 and raw[:3] == b'\xff\x24\x85':
            return 'dispatch'
        require((len(raw) == 2 and raw[0] == 0xeb) or
                (len(raw) == 5 and raw[0] == 0xe9), 'unsupported jump encoding')
        return 'jump'
    raise Refused('unsupported instruction/control: ' + mnemonic)


@lru_cache(maxsize=None)
def section_direct_edges(path, section):
    """Observed direct entries, including prefixed transfers and XBEGIN.

    This scan deliberately includes instructions outside the sized owner.
    Their ordinary instruction vocabulary need not match the owner's narrow
    allowlist. Prefixes must not hide a direct destination from this scan.
    """
    output = subprocess.check_output(['objdump', '-dw', '--section=' + section, path],
                                     text=True)
    edges = []
    for line in output.splitlines():
        match = re.match(r'^\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t(.*)$', line)
        if not match:
            continue
        tokens = match[2].split()
        while tokens and tokens[0] in PREFIXES:
            tokens.pop(0)
        require(not tokens or not tokens[0].startswith(('ljmp', 'lcall', 'jmpf', 'callf')),
                'section contains unsupported far transfer')
        if len(tokens) >= 2 and tokens[0].startswith(('j', 'loop', 'call', 'xbegin')):
            if re.fullmatch('[0-9a-f]+', tokens[1]):
                edges.append((int(match[1], 16), int(tokens[1], 16)))
    return edges


def prove(path, name):
    elf = ELFFile(io.BytesIO(Path(path).read_bytes()))
    require(elf.elfclass == 32 and elf.little_endian and
            elf['e_machine'] == 'EM_386' and elf['e_type'] == 'ET_REL', 'ELF format')
    sections = list(elf.iter_sections())
    require(sum(s['sh_type'] in ('SHT_SYMTAB', 'SHT_DYNSYM') for s in sections) == 1,
            'unique symbol table')
    tab = elf.get_section_by_name('.symtab')
    require(tab is not None, 'missing symbols')
    symbols = list(tab.iter_symbols())
    owners = [s for s in symbols if s.name == name and
              s['st_info']['type'] == 'STT_FUNC' and s['st_size'] and
              isinstance(s['st_shndx'], int)]
    require(len(owners) == 1, 'unique sized owner')
    owner = owners[0]
    sid, start = owner['st_shndx'], owner['st_value']
    end = start + owner['st_size']
    code = sections[sid]
    require(code['sh_type'] == 'SHT_PROGBITS' and code['sh_flags'] == 6 and
            code['sh_addr'] == 0 and
            0 <= start < end <= code['sh_size'], 'code extent')
    require(sum(s.name == code.name for s in sections) == 1, 'ambiguous code section')
    require(not any(s is not owner and s['st_shndx'] == sid and
                    s['st_info']['type'] == 'STT_FUNC' and
                    ((s['st_size'] and s['st_value'] < end and
                      s['st_value'] + s['st_size'] > start) or
                     start <= s['st_value'] < end) for s in symbols), 'owner alias')
    rows = instructions(str(path), code.name, start, end)
    require(b''.join(r[1] for r in rows) == code.data()[start:end], 'decoded bytes')
    kinds = {r[0]: control_kind(r) for r in rows}
    boundaries = {r[0] for r in rows}
    indirect = [r for r in rows if kinds[r[0]] == 'dispatch']
    require(len(indirect) == 1, 'one indirect transfer')
    jump = indirect[0]
    # Exact unprefixed JMP disp32(,EAX,4); relocation is precisely disp32.
    require(jump[2] == 'jmp' and len(jump[1]) == 7 and
            jump[1][:3] == b'\xff\x24\x85', 'dispatch encoding')
    ji = rows.index(jump)
    # Two deliberately disjoint grammars: the historical rebased taken-JBE
    # dispatch, or an adjacent CMP EAX / JA default / fallthrough dispatch.
    fallthrough = ji >= 2 and rows[ji - 1][2] == 'ja'
    if fallthrough:
        cmp, guard = rows[ji - 2:ji]
        sub = None
        require((len(guard[1]) == 2 and guard[1][0] == 0x77) or
                (len(guard[1]) == 6 and guard[1][:2] == b'\x0f\x87'), 'JA encoding')
        require(guard[0] + len(guard[1]) == jump[0] and
                direct_target(guard) != jump[0], 'adjacent default guard')
        sequence = (cmp, guard, jump)
    else:
        guards = [(i, r) for i, r in enumerate(rows) if r[2] == 'jbe' and
                  direct_target(r) == jump[0]]
        require(len(guards) == 1 and guards[0][0] >= 2, 'unique unsigned taken guard')
        gi, guard = guards[0]
        sub, cmp = rows[gi - 2:gi]
        require(sub[1][:2] == b'\x83\xe8' and len(sub[1]) == 3 and
                sub[1][2] < 128, 'full-width adjacent EAX rebase')
        require((len(guard[1]) == 2 and guard[1][0] == 0x76) or
                (len(guard[1]) == 6 and guard[1][:2] == b'\x0f\x86'), 'JBE encoding')
        require(sub[0] + len(sub[1]) == cmp[0], 'adjacent rebase')
        sequence = (sub, cmp, guard, jump)
    require(cmp[1][:2] == b'\x83\xf8' and len(cmp[1]) == 3 and
            cmp[1][2] < 128, 'full-width adjacent EAX bound')
    require(cmp[0] + len(cmp[1]) == guard[0], 'immediately consumed flags')
    count = cmp[1][2] + 1
    protected = set().union(*(set(range(r[0], r[0] + len(r[1])))
                              for r in sequence))
    require(not any(s['st_shndx'] == sid and s['st_info']['type'] not in
                    ('STT_SECTION', 'STT_FILE') and s['st_value'] in protected
                    for s in symbols), 'named protected entry')
    # Full instruction graph: dispatch has only the guard's selected edge
    # (taken JBE or JA fallthrough). No entry may bypass a protected byte.
    predecessors = {r[0]: set() for r in rows}
    for r in rows:
        at, raw, mnemonic, operands = r
        kind = kinds[at]
        if kind in ('dispatch', 'return'):
            edges = []
        elif kind in ('jump', 'conditional'):
            target = direct_target(r)
            require(target in boundaries, 'direct branch boundary')
            edges = [target] + ([] if kind == 'jump' else [at + len(raw)])
        else:
            edges = [at + len(raw)] if at + len(raw) < end else []
        for target in edges:
            require(target in boundaries, 'fallthrough boundary')
            predecessors[target].add(at)
    require(predecessors[jump[0]] == {guard[0]} and
            predecessors[guard[0]] == {cmp[0]} and
            (sub is None or predecessors[cmp[0]] == {sub[0]}), 'sole guarded predecessor')
    for source, target in section_direct_edges(str(path), code.name):
        require(target not in protected or
                (not fallthrough and source == guard[0] and target == jump[0]), 'direct protected entry')

    rels = []
    occupied = {}
    for rs in sections:
        if not isinstance(rs, RelocationSection):
            continue
        require(not rs.is_RELA(), 'RELA unsupported')
        require(0 <= rs['sh_info'] < len(sections) and
                0 <= rs['sh_link'] < len(sections), 'relocation section indices')
        data = sections[rs['sh_info']]
        st = sections[rs['sh_link']]
        require(st['sh_type'] == 'SHT_SYMTAB', 'relocation symbol table')
        for r in rs.iter_relocations():
            off, kind = r['r_offset'], r['r_info_type']
            require(kind in (1, 2), 'unsupported relocation kind')
            require(data['sh_type'] == 'SHT_PROGBITS' and
                    0 <= off and off + 4 <= data['sh_size'], 'relocation extent')
            fields = set(range(off, off + 4))
            used = occupied.setdefault(rs['sh_info'], set())
            require(not fields & used, 'overlapping relocation')
            used.update(fields)
            require(r['r_info_sym'] < st.num_symbols(), 'relocation symbol index')
            symbol = st.get_symbol(r['r_info_sym'])
            add = int.from_bytes(data.data()[off:off + 4], 'little', signed=True)
            rels.append((rs['sh_info'], off, kind, symbol, symbol['st_value'] + add))
    dispatch_offset = jump[0] + 3
    jr = [r for r in rels if r[0] == sid and
          set(range(r[1], r[1] + 4)) & set(range(jump[0], jump[0] + 7))]
    require(len(jr) == 1, 'dispatch relocation count')
    _, off, kind, symbol, base = jr[0]
    require(off == dispatch_offset and kind == 1 and
            symbol['st_info']['type'] == 'STT_SECTION' and
            isinstance(symbol['st_shndx'], int), 'exact dispatch relocation field')
    tid = symbol['st_shndx']
    table = sections[tid]
    require(sum(s.name == table.name for s in sections) == 1, 'ambiguous table section')
    limit = base + 4 * count
    require(table['sh_type'] == 'SHT_PROGBITS' and table['sh_flags'] == 2 and
            table['sh_addr'] == 0 and
            base >= 0 and base % 4 == 0 and limit <= table['sh_size'], 'table flags/extent')
    require(not any(s['st_shndx'] == tid and s['st_info']['type'] not in
                    ('STT_SECTION', 'STT_FILE') and
                    (base <= s['st_value'] < limit or
                     s['st_size'] and s['st_value'] < limit and
                     s['st_value'] + s['st_size'] > base) for s in symbols), 'named table overlap')
    entries = sorted((r for r in rels if r[0] == tid and
                      r[1] < limit and r[1] + 4 > base), key=lambda r: r[1])
    require([r[1] for r in entries] == list(range(base, limit, 4)), 'complete table entries')
    targets = []
    for _, _, kind, symbol, target in entries:
        require(kind == 1 and symbol['st_info']['type'] == 'STT_SECTION' and
                symbol['st_shndx'] == sid and target in boundaries and
                target not in protected, 'table target boundary/owner')
        targets.append(target - start)
    refs = []
    for sec, off, kind, symbol, value in rels:
        possible = {value} if kind == 1 else {value, value + 4}
        if symbol['st_shndx'] == tid and any(base <= v < limit for v in possible):
            refs.append((sec, off))
        if symbol['st_shndx'] == sid:
            require(not possible & protected, 'relocated protected entry')
        if sec == sid and set(range(off, off + 4)) & protected:
            require(off == dispatch_offset, 'relocated guard/opcode')
    require(refs == [(sid, dispatch_offset)], 'exclusive observed table reference')
    return {'relocation_offset': dispatch_offset - start,
            'table_section': table.name, 'table_offset': base,
            'identity': (TAG, name, jump[0] - start, 4, count, tuple(targets)),
            'relocations_audited': len(rels), 'observed_table_references': len(refs)}


def direct_target(row):
    match = re.match(r'^([0-9a-f]+)(?:\s|$)', row[3])
    return int(match[1], 16) if match else None
