"""TEST ONLY: bind one scoped 16-bit table read to an external memory fixture.

No table definition, other consumer, or comparison input is redirected. This
changes the test memory environment; it does not canonicalize grading inputs.
"""
import sys
from pathlib import Path
# tools/dis.py must not shadow stdlib dis imported by ELF dependencies.
sys.path = [p for p in sys.path if Path(p or '.').resolve() != Path(__file__).resolve().parent]
import dis
import hashlib
import io
import json
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
from callee_effects import decode
from jumptable import Refused, require, instructions
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection


def _read(path):
    raw = Path(path).read_bytes()
    elf = ELFFile(io.BytesIO(raw))
    require(elf.elfclass == 32 and elf.little_endian and
            elf['e_type'] == 'ET_REL' and elf['e_machine'] == 'EM_386',
            'requires little-endian ELF32 i386 REL')
    symtab = elf.get_section_by_name('.symtab')
    require(symtab is not None and symtab['sh_entsize'] == 16,
            'requires unique ordinary ELF32 symbol table')
    require(sum(s.name == '.symtab' for s in elf.iter_sections()) == 1,
            'ambiguous symbol table')
    return raw, elf, symtab, list(symtab.iter_symbols())


def _unique(symbols, name):
    found = [(i, s) for i, s in enumerate(symbols) if s.name == name]
    require(len(found) == 1, 'missing or ambiguous symbol: ' + name)
    return found[0]


def _locate(path, function, old_table, allowed_addends, table_object=None):
    raw, elf, tab, symbols = _read(path)
    _, table = _unique(symbols, old_table)
    context = None
    definition = table
    definition_elf = elf
    if table['st_shndx'] == 'SHN_UNDEF':
        require(table_object is not None and table['st_info']['bind'] == 'STB_GLOBAL'
                and table['st_value'] == 0 and table['st_size'] == 0,
                'undefined old table requires explicit authoritative table_object')
        context_raw, definition_elf, _, context_symbols = _read(table_object)
        _, definition = _unique(context_symbols, old_table)
        context = {'path': str(Path(table_object).resolve()),
                   'sha256': hashlib.sha256(context_raw).hexdigest()}
    require(isinstance(definition['st_shndx'], int) and definition['st_size'] == 6 and
            definition['st_info']['type'] == 'STT_OBJECT' and
            definition['st_info']['bind'] in ('STB_GLOBAL', 'STB_LOCAL'),
            'old table must have a defined six-byte object')
    table_sec = definition_elf.get_section(definition['st_shndx'])
    require(table_sec['sh_type'] != 'SHT_NOBITS' and
            definition['st_value'] + 6 <= table_sec['sh_size'], 'old table extent')
    table_bytes = table_sec.data()[definition['st_value']:definition['st_value']+6]
    require(table_bytes == struct.pack('<3h', 5792, 12953, 17378),
            'authoritative six-byte table values differ from reference')
    _, fn = _unique(symbols, function)
    require(isinstance(fn['st_shndx'], int) and fn['st_size'] > 0 and
            fn['st_info']['type'] == 'STT_FUNC' and
            fn['st_info']['bind'] in ('STB_GLOBAL', 'STB_LOCAL'),
            'function must be defined strong and sized')
    sec = elf.get_section(fn['st_shndx'])
    start, end = fn['st_value'], fn['st_value'] + fn['st_size']
    require(sec['sh_flags'] & 4 and end <= sec['sh_size'], 'function extent')
    owners = [s for s in symbols if s['st_shndx'] == fn['st_shndx'] and
              s['st_info']['type'] == 'STT_FUNC' and s['st_size'] and
              s['st_value'] < end and s['st_value'] + s['st_size'] > start]
    require(len(owners) == 1, 'ambiguous overlapping function owner')
    matches = []
    for rsec in elf.iter_sections():
        if not isinstance(rsec, RelocationSection) or rsec['sh_info'] != fn['st_shndx']:
            continue
        require(rsec['sh_type'] == 'SHT_REL' and rsec['sh_entsize'] == 8 and
                rsec['sh_link'] == elf.get_section_index('.symtab'), 'unsupported relocations')
        for index, rel in enumerate(rsec.iter_relocations()):
            offset = rel['r_offset']
            if not start <= offset < end:
                continue
            require(offset + 4 <= end, 'cross-boundary relocation')
            sym = symbols[rel['r_info_sym']]
            if rel['r_info_type'] != 1:
                continue
            if table['st_shndx'] == 'SHN_UNDEF':
                if sym.name != old_table or sym['st_shndx'] != 'SHN_UNDEF':
                    continue
            elif sym['st_shndx'] != table['st_shndx']:
                continue
            addend = struct.unpack_from('<i', sec.data(), offset)[0]
            effective = sym['st_value'] + addend
            if effective == table['st_value'] - 2:
                matches.append((rsec, index, rel, sym, addend))
    require(len(matches) == 1, 'requires exactly one matching R_386_32 table read')
    require(-2 in allowed_addends, 'table-relative addend -2 is not allowed')
    rsec, index, rel, target, addend = matches[0]
    rows = instructions(str(Path(path).resolve()), sec.name, start, end)
    rows = [row for row in rows if row[0] <= rel['r_offset'] < row[0] + len(row[1])]
    require(len(rows) == 1, 'relocation has no unique instruction')
    address, code, _, _ = rows[0]
    kind, args, _ = decode(address, code)
    require(kind in ('movsx', 'movzx') and len(args) == 2 and args[0][0] == 'm' and
            ((args[0][1] is None and args[0][2] is not None and args[0][3] == 2) or
             (args[0][1] is not None and args[0][1] == args[0][2] and args[0][3] == 1)) and
            args[0][5] == 16 and
            args[1][0] == 'r' and args[1][2:] == (0, 32) and
            args[0][6] is not None and address + args[0][6] == rel['r_offset'] and
            len(code) - args[0][6] == 4 and code[:2] in (b'\x0f\xbf', b'\x0f\xb7'),
            'requires 16-to-32 MOVSX/MOVZX memory read, SIB effective 2*index, GP destination')
    return raw, elf, tab, symbols, fn, sec, table, rsec, index, rel, target, addend, address, code, table_bytes, context


def _snapshot(elf):
    """All old symbol fields and relocation records, independent of file packing."""
    tab = elf.get_section_by_name('.symtab')
    symbols = [(s.name, s.entry) for s in tab.iter_symbols()]
    relocs = {s.name: [dict(r.entry) for r in s.iter_relocations()]
              for s in elf.iter_sections() if isinstance(s, RelocationSection)}
    return symbols, relocs


def bind_table_read(object, function, old_table, new_external_symbol, output,
                    allowed_addends=(-2,), table_object=None):
    """Clone and retarget the sole admitted read; return a JSON-ready record.

    Actual admitted reads are raw-decoded MOVSX/MOVZX 16-to-32 with SIB
    effective 2*register, either no-base scale2 or base==index scale1.
    Undefined old tables require an explicit defining object. Its six bytes
    must be the reference 3-short magnitude table; this binding never alters it.

    Output must differ from input. Failures never publish an output object.
    Existing destination symbols must already be global undefined NOTYPE/OBJECT.
    """
    source, output = Path(object).resolve(), Path(output).resolve()
    require(source != output, 'input and output must differ')
    facts = _locate(source, function, old_table, tuple(allowed_addends), table_object)
    raw, elf, tab, symbols, fn, sec, table, rsec, ri, rel, target, addend, address, code, table_bytes, context = facts
    require(new_external_symbol and new_external_symbol != old_table and
            '=' not in new_external_symbol and ',' not in new_external_symbol,
            'invalid external symbol name')
    existing = [s for s in symbols if s.name == new_external_symbol]
    require(len(existing) <= 1, 'ambiguous external symbol')
    if existing:
        s = existing[0]
        require(s['st_shndx'] == 'SHN_UNDEF' and s['st_value'] == 0 and
                s['st_info']['bind'] == 'STB_GLOBAL' and
                s['st_info']['type'] in ('STT_NOTYPE', 'STT_OBJECT') and
                s['st_size'] == 0, 'destination must be an undefined external symbol')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temp:
        clone = Path(temp) / 'bound.o'
        command = None
        if existing:
            shutil.copyfile(source, clone)
        else:
            command = ['objcopy', '--add-symbol', new_external_symbol + '=0,global', str(source), str(clone)]
            subprocess.run(command, check=True, capture_output=True)
        objcopy_data, objcopy_elf, objcopy_tab, objcopy_symbols = _read(clone)
        if existing:
            symbol_index, _ = _unique(symbols, new_external_symbol)
        else:
            _, added = _unique(objcopy_symbols, new_external_symbol)
            require(added['st_info']['bind'] == 'STB_GLOBAL', 'objcopy failed to add global')
            symbol_index = len(symbols)
        # Preserve original layout, strings and symbols verbatim. The host
        # objcopy result establishes the added symbol; normalize that cloned
        # symbol to GLOBAL/UNDEF and append storage to the original ELF layout.
        edited = elf
        mutable = bytearray(raw)
        metadata = []
        def patch(offset, new_bytes, role):
            old = bytes(mutable[offset:offset+len(new_bytes)])
            mutable[offset:offset+len(new_bytes)] = new_bytes
            metadata.append({'role': role, 'file_offset': offset,
                             'old_hex': old.hex(), 'new_hex': new_bytes.hex()})
        if not existing:
            original_strings = elf.get_section(tab['sh_link']).data()
            new_strings = original_strings + new_external_symbol.encode() + b'\0'
            extra_symbol = struct.pack('<IIIBBH', len(original_strings), 0, 0, 0x10, 0, 0)
            new_symbols = tab.data() + extra_symbol
            for section_index, payload in ((tab['sh_link'], new_strings),
                                           (elf.get_section_index('.symtab'), new_symbols)):
                mutable.extend(b'\0' * ((-len(mutable)) % 4))
                payload_offset = len(mutable)
                mutable.extend(payload)
                header = elf['e_shoff'] + section_index * elf['e_shentsize']
                patch(header+16, struct.pack('<II', payload_offset, len(payload)),
                      'original symbol/string storage plus appended undefined symbol')
                metadata.append({'role': 'appended section payload', 'file_offset': payload_offset,
                                 'size': len(payload), 'sha256': hashlib.sha256(payload).hexdigest()})
        ers = edited.get_section_by_name(rsec.name)
        ecs = edited.get_section_by_name(sec.name)
        require(ers is not None and ecs is not None, 'objcopy lost owning section')
        patch(ers['sh_offset'] + ri*8 + 4, struct.pack('<I', symbol_index << 8 | 1), 'relocation r_info')
        patch(ecs['sh_offset'] + rel['r_offset'], struct.pack('<i', -2), 'REL in-place addend')
        allowed_offsets = {i for change in metadata if 'new_hex' in change for i in range(change['file_offset'], change['file_offset'] + len(bytes.fromhex(change['new_hex'])))}
        require(all(a == b or i in allowed_offsets for i, (a, b) in enumerate(zip(raw, mutable))),
                'unrecorded original-file byte change')
        clone.write_bytes(mutable)
        _, final, ftab, fsymbols = _read(clone)
        before_symbols, before_relocs = _snapshot(elf)
        after_symbols, after_relocs = _snapshot(final)
        expected_symbols = list(before_symbols)
        if not existing:
            expected_symbols.append((new_external_symbol, fsymbols[symbol_index].entry))
        require(after_symbols == expected_symbols, 'unintended existing symbol change')
        require(fsymbols[symbol_index]['st_shndx'] == 'SHN_UNDEF' and
                fsymbols[symbol_index]['st_value'] == 0, 'external symbol is not undefined zero')
        expected_relocs = {k: [dict(v) for v in vs] for k, vs in before_relocs.items()}
        expected_relocs[rsec.name][ri]['r_info'] = symbol_index << 8 | 1
        expected_relocs[rsec.name][ri]['r_info_sym'] = symbol_index
        require(after_relocs == expected_relocs, 'unintended relocation change')
        require([s.name for s in elf.iter_sections()] == [s.name for s in final.iter_sections()],
                'objcopy changed section inventory')
        for oldsec in elf.iter_sections():
            newsec = final.get_section_by_name(oldsec.name)
            if oldsec.name in ('.symtab', elf.get_section(tab['sh_link']).name, '.shstrtab'):
                continue
            want = bytearray(oldsec.data())
            if oldsec.name == sec.name:
                struct.pack_into('<i', want, rel['r_offset'], -2)
            if oldsec.name == rsec.name:
                struct.pack_into('<I', want, ri*8+4, symbol_index << 8 | 1)
            require(bytes(want) == newsec.data(), 'unintended section bytes: ' + oldsec.name)
            for field in ('sh_type', 'sh_flags', 'sh_addr', 'sh_size', 'sh_link', 'sh_info', 'sh_addralign', 'sh_entsize'):
                require(oldsec[field] == newsec[field], 'unintended section metadata: ' + oldsec.name)
        record = {'purpose': 'test-only memory environment binding; never grading canonicalization',
                  'input': str(source), 'output': str(output),
                  'input_sha256': hashlib.sha256(raw).hexdigest(),
                  'output_sha256': hashlib.sha256(mutable).hexdigest(),
                  'owner': function, 'owner_section': sec.name,
                  'owner_start': fn['st_value'], 'owner_size': fn['st_size'],
                  'read_address': address, 'read_raw_before': code.hex(),
                  'relocation_offset': rel['r_offset'], 'old_table': old_table,
                  'old_relocation_symbol': target.name, 'old_addend': addend,
                  'old_effective_table_addend': -2, 'new_symbol': new_external_symbol,
                  'new_symbol_index': symbol_index, 'new_addend': -2,
                  'old_table_preserved_hex': table_bytes.hex(), 'authoritative_table_context': context,
                  'section_header_changes': {s.name: {k: {'old': s[k], 'new': final.get_section_by_name(s.name)[k]} for k in s.header if s[k] != final.get_section_by_name(s.name)[k]} for s in elf.iter_sections() if s.header != final.get_section_by_name(s.name).header},
                  'elf_header_changes': {k: {'old': elf[k], 'new': final[k]} for k in elf.header if elf[k] != final[k]},
                  'changed_metadata': metadata, 'objcopy_command': command,
                  'objcopy_version': subprocess.check_output(['objcopy','--version'], text=True).splitlines()[0],
                  'verified': 'all old symbols, every other relocation and all unrelated section bytes/metadata unchanged'}
        if not output.exists() or output.read_bytes() != mutable:
            clone.replace(output)
        return record


if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('object', 'function', 'old_table', 'new_external_symbol', 'output'):
        p.add_argument(name)
    p.add_argument('--table-object')
    p.add_argument('--allowed-addends', nargs='+', type=int, default=(-2,))
    a = p.parse_args()
    print(json.dumps(bind_table_read(**vars(a)), indent=2))
