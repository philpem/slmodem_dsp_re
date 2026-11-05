#!/usr/bin/env python3
"""Strict allocated-data observations, separate from function/CFG equivalence.

Relocation masking establishes payload equality only. Code targets retain their
function-relative offsets and must land on decoded instruction boundaries.
Unsupported or ambiguous destinations stay unresolved even in identical files.
"""
from collections import defaultdict
import hashlib
from pathlib import Path
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

sys.path.insert(0, str(Path(__file__).resolve().parent / 'toolchain'))
import jumptable


def observe(path):
    path = str(Path(path).resolve())
    with open(path, 'rb') as stream:
        elf = ELFFile(stream)
        if elf.elfclass != 32 or not elf.little_endian or elf['e_machine'] != 'EM_386' or elf['e_type'] != 'ET_REL':
            raise ValueError('data audit requires little-endian ELF32 i386 REL objects')
        table = elf.get_section_by_name('.symtab')
        if table is None:
            raise ValueError('data audit requires a symbol table')
        symbols = list(table.iter_symbols())
        rels = defaultdict(list)
        for section in elf.iter_sections():
            if isinstance(section, RelocationSection):
                rels[section['sh_info']].append(section)
        sections, fields = {}, {}
        for index, section in enumerate(elf.iter_sections()):
            if not section['sh_flags'] & 2 or section['sh_flags'] & 4:
                continue
            if section.name in sections:
                raise ValueError('ambiguous allocated section name: ' + section.name)
            raw = bytearray(section.data())
            if len(raw) != section['sh_size']:
                raise ValueError('truncated allocated section: ' + section.name)
            occupied = set()
            for rs in rels[index]:
                if rs['sh_type'] != 'SHT_REL' or rs['sh_link'] != elf.get_section_index('.symtab'):
                    raise ValueError('unsupported relocation table for ' + section.name)
                for rel in rs.iter_relocations():
                    offset, kind = rel['r_offset'], rel['r_info_type']
                    if offset < 0 or offset + 4 > len(raw) or occupied.intersection(range(offset, offset + 4)):
                        raise ValueError('overlapping or out-of-range data relocation')
                    occupied.update(range(offset, offset + 4))
                    addend = int.from_bytes(raw[offset:offset + 4], 'little')
                    target = table.get_symbol(rel['r_info_sym'])
                    record = {'section': section.name, 'offset': offset, 'type': kind,
                              'status': 'unresolved', 'symbol': target.name, 'addend': addend}
                    if kind == 1:
                        dest_index = target['st_shndx']
                        if isinstance(dest_index, int) and elf.get_section(dest_index)['sh_flags'] & 4:
                            address = (target['st_value'] + addend) & 0xffffffff
                            owners = [s for s in symbols if s['st_info']['type'] == 'STT_FUNC'
                                      and s['st_shndx'] == dest_index and s['st_size']
                                      and s['st_value'] <= address < s['st_value'] + s['st_size']]
                            if len(owners) == 1:
                                owner = owners[0]
                                if not owner.name or sum(s.name == owner.name for s in symbols) != 1:
                                    record['reason'] = 'ambiguous function owner name'
                                    fields[(section.name, offset)] = record
                                    raw[offset:offset + 4] = bytes(4)
                                    continue
                                try:
                                    starts = {r[0] for r in jumptable.instructions(
                                        path, elf.get_section(dest_index).name, owner['st_value'],
                                        owner['st_value'] + owner['st_size'])}
                                except jumptable.Refused as exc:
                                    record['reason'] = str(exc)
                                else:
                                    if address in starts:
                                        record.update(status='resolved', target=['code', owner.name,
                                                                               address - owner['st_value']])
                                    else:
                                        record['reason'] = 'code target is not an instruction boundary'
                            else:
                                record['reason'] = 'code target has %d function owners' % len(owners)
                        elif target.name and target['st_info']['type'] != 'STT_SECTION' and (
                                isinstance(dest_index, int) or dest_index == 'SHN_UNDEF'):
                            same = [s for s in symbols if s.name == target.name]
                            if len(same) == 1:
                                record.update(status='resolved', target=['symbol', target.name, addend])
                            else:
                                record['reason'] = 'ambiguous named destination'
                        else:
                            record['reason'] = 'unsupported section-relative or special-symbol destination'
                    else:
                        record['reason'] = 'unsupported relocation type'
                    fields[(section.name, offset)] = record
                    # Unsupported kinds have unknown widths: do not mask them.
                    if kind == 1:
                        raw[offset:offset + 4] = bytes(4)
            sections[section.name] = {
                'type': section['sh_type'], 'flags': section['sh_flags'],
                'size': section['sh_size'], 'alignment': section['sh_addralign'],
                'entry_size': section['sh_entsize'],
                'masked_sha256': hashlib.sha256(raw).hexdigest()}
        return sections, fields


def compare(baseline, candidate):
    before, old = observe(baseline)
    after, new = observe(candidate)
    changes, unresolved, code_rows = [], [], []
    for key in sorted(set(old) | set(new)):
        a, b = old.get(key), new.get(key)
        row = {'section': key[0], 'offset': key[1], 'baseline': a, 'candidate': b}
        if a is None or b is None:
            changes.append(row)
        elif a['status'] != 'resolved' or b['status'] != 'resolved':
            unresolved.append(row)
        elif (a['type'], a['target']) != (b['type'], b['target']):
            changes.append(row)
        if a and b and a.get('target', [None])[0] == b.get('target', [None])[0] == 'code':
            code_rows.append({'section': key[0], 'offset': key[1],
                              'baseline': a['target'], 'candidate': b['target'],
                              'owner_equal': a['target'][1] == b['target'][1],
                              'target_equal': a['target'] == b['target']})
    return {'allocated_sections': {'baseline': len(before), 'candidate': len(after)},
            'relocation_fields': {'baseline': len(old), 'candidate': len(new)},
            'masked_payload_and_layout_unchanged': before == after,
            'strict_relocation_targets_unchanged': False if changes else None if unresolved else True,
            'changed_relocation_fields': changes, 'unresolved_relocation_fields': unresolved,
            'code_targets': {'compared': len(code_rows),
                             'changed': sum(not r['target_equal'] for r in code_rows),
                             'owner_changes': sum(not r['owner_equal'] for r in code_rows),
                             'rows': code_rows},
            'scope': 'Allocated non-executable sections only. Masked payloads, code owners and '
                     'function-relative targets are separate observations. Unresolved is not equal; '
                     'no table bounds, CFG equivalence, string-pool normalization or adoption proof.'}
