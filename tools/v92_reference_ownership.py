#!/usr/bin/env python3
"""Audit V92 native references without inferring translation-unit ownership.

Reads the original ELF and a frozen canonical 300-object period build. No C/C++
is compiled. Includes named relocations, section-relative destinations and
unrelocated direct call/jump instructions. Indirect calls without a relocated
address remain outside this detector; external callers are not observable.

  python3 tools/v92_reference_ownership.py --baseline-dir /path/to/freeze \
      --output build/v92-reference-ownership/audit.json

Five copied-ELF controls expose an additional caller, an equivalent section
relocation, a non-call address reference, an incomplete function decode and
an overlapping function extent. The copies are never executed.
"""
import argparse
import bisect
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

# tools/dis.py must not shadow Python's dis while Capstone imports inspect.
_tool_directory = sys.path.pop(0)
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from elftools.elf.elffile import ELFFile
sys.path.insert(0, _tool_directory)

sys.path.insert(0, str(Path(__file__).resolve().parent / 'toolchain'))
import byteident

PREFIX = '_ZN21V92ConvolutionEncoder'
HELPER = PREFIX + '10inverseMapEPi'
PROCESS = PREFIX + '7processEPi'
TRANSMITTER = '_ZN14V92Transmitter7processEPhjPsRj'
ENCODER = tuple(PREFIX + suffix for suffix in (
    'C1Ev', 'C2Ev', 'D1Ev', 'D2Ev', '25makeStateTtransitionTableEv',
    '5resetEi', '10inverseMapEPi', '7processEPi'))
TAIL_CALLERS = (
    '_ZN18V90Phase4Modulator30recivedPartTwoSilenceRrnSUVtagEv',
    '_ZN16V92EchoCancellerC1EP13V92Parametersjj',
    '_ZN16V92EchoCancellerC2EP13V92Parametersjj')
CALLERS = set(ENCODER + (TRANSMITTER,) + TAIL_CALLERS)
TABLES = (PREFIX + '14cosetMapping4DE', PREFIX + '16subsetLabelTableE')
TARGETS = set(ENCODER + TABLES)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scan(path):
    """Return resolved references plus selected callers' outgoing transfers."""
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf.little_endian
        assert elf['e_machine'] == 'EM_386' and elf['e_type'] == 'ET_REL'
        table = elf.get_section_by_name('.symtab')
        symbols = list(table.iter_symbols())
        sections = list(elf.iter_sections())
        functions, starts, maximum_ends, records, locations = {}, {}, {}, {}, {}
        for symbol in symbols:
            index = symbol['st_shndx']
            if symbol.name in CALLERS | TARGETS:
                records[symbol.name] = dict(
                    section=sections[index].name if isinstance(index, int) else index,
                    offset=symbol['st_value'], size=symbol['st_size'],
                    bind=symbol['st_info']['bind'], type=symbol['st_info']['type'],
                    visibility=symbol['st_other']['visibility'])
            if not isinstance(index, int):
                continue
            if symbol.name in TARGETS and symbol['st_size']:
                locations.setdefault(index, []).append(symbol)
            if symbol['st_info']['type'] == 'STT_FUNC' and symbol['st_size']:
                functions.setdefault(index, []).append(symbol)
        for index, items in functions.items():
            items.sort(key=lambda s: s['st_value'])
            starts[index] = [s['st_value'] for s in items]
            maximum, ends = 0, []
            for symbol in items:
                maximum = max(maximum, symbol['st_value'] + symbol['st_size'])
                ends.append(maximum)
            maximum_ends[index] = ends

        def owners(index, offset):
            items = functions.get(index, [])
            position = bisect.bisect_right(starts.get(index, []), offset) - 1
            result = []
            while position >= 0:
                if maximum_ends[index][position] <= offset:
                    break
                symbol = items[position]
                if offset < symbol['st_value'] + symbol['st_size']:
                    result.append(symbol.name)
                position -= 1
            return sorted(result)

        # Decode from every sized function's symbol boundary, never from a
        # section start through padding or text data. Exact aliases share one
        # decode; all other overlaps and incomplete decodes are explicit.
        direct, direct_at, instruction_count = {}, {}, 0
        incomplete, overlaps, aliases = [], [], []
        decoder = Cs(CS_ARCH_X86, CS_MODE_32)
        for index, items in functions.items():
            section = sections[index]
            assert section['sh_flags'] & 4
            groups = {}
            for symbol in items:
                groups.setdefault((symbol['st_value'], symbol['st_size']), []).append(symbol.name)
            previous_end = 0
            for (start, length), names in sorted(groups.items()):
                if start < previous_end:
                    overlaps.append(dict(section=section.name, start=start, size=length, functions=names))
                previous_end = max(previous_end, start + length)
                if len(names) > 1:
                    aliases.append(dict(section=section.name, start=start, size=length, functions=sorted(names)))
                data = section.data()[start:start + length]
                decoded = list(decoder.disasm_lite(data, start))
                consumed = sum(item[1] for item in decoded)
                if consumed != length or len(data) != length:
                    incomplete.append(dict(section=section.name, start=start, size=length,
                                           decoded_bytes=consumed, available_bytes=len(data), functions=names))
                for address, size, mnemonic, operand in decoded:
                    instruction_count += 1
                    if not (mnemonic == 'call' or mnemonic.startswith('j')):
                        continue
                    try:
                        target = int(operand, 0) & 0xffffffff
                    except ValueError:  # Register or memory indirect transfer.
                        continue
                    record = dict(section=section.name, offset=address,
                                  instruction=mnemonic, functions=owners(index, address))
                    direct[(index, address)] = (size, target, record)
                    # E8/E9 rel32; near Jcc has a two-byte opcode.
                    if size == 5:
                        direct_at[(index, address + 1)] = record
                    elif size == 6 and mnemonic.startswith('j'):
                        direct_at[(index, address + 2)] = record

        references, outgoing, relocated_direct = [], [], set()
        relocation_count, unsupported = 0, []
        for section in sections:
            if section['sh_type'] != 'SHT_REL':
                continue
            target_index = section['sh_info']
            owner = sections[target_index]
            reloc_symbols = elf.get_section(section['sh_link'])
            data = owner.data()
            for number, relocation in enumerate(section.iter_relocations()):
                relocation_count += 1
                at, kind = relocation['r_offset'], relocation['r_info_type']
                symbol = reloc_symbols.get_symbol(relocation['r_info_sym'])
                addend = int.from_bytes(data[at:at + 4], 'little', signed=True)
                transfer = direct_at.get((target_index, at))
                if transfer:
                    relocated_direct.add((target_index, transfer['offset']))
                record = dict(section=owner.name, offset=at, kind=kind,
                              symbol=symbol.name, addend=addend,
                              functions=owners(target_index, at),
                              instruction=transfer['instruction'] if transfer else None)
                resolved = []
                index = symbol['st_shndx']
                if kind == 1 or (kind == 2 and transfer):
                    adjustment = 4 if kind == 2 else 0
                    relative = addend + adjustment
                    if symbol.name in TARGETS:
                        # Preserve any named address expression, including a
                        # nonzero addend into an undefined exported function.
                        resolved.append((symbol.name, relative))
                    if isinstance(index, int):
                        address = (symbol['st_value'] + relative) & 0xffffffff
                        for candidate in locations.get(index, []):
                            delta = address - candidate['st_value']
                            if 0 <= delta < candidate['st_size']:
                                resolved.append((candidate.name, delta))
                elif symbol.name in TARGETS:
                    unsupported.append(record)
                for name, delta in sorted(set(resolved)):
                    references.append(dict(record, target=name, interior_offset=delta,
                                           via='relocation'))
                if transfer and any(name in CALLERS for name in record['functions']):
                    outgoing.append(dict(record, target=symbol.name,
                                         target_addend=addend + 4, via='relocation'))
        for (index, at), (size, address, record) in direct.items():
            if (index, at) in relocated_direct:
                continue
            names = owners(index, address)
            for candidate in locations.get(index, []):
                delta = address - candidate['st_value']
                internal_jump = (record['instruction'].startswith('j') and
                                 candidate.name in record['functions'])
                if 0 <= delta < candidate['st_size'] and not internal_jump:
                    references.append(dict(record, target=candidate.name,
                                           interior_offset=delta, via='resolved-direct'))
            internal_jump = (record['instruction'].startswith('j') and
                             set(names) & set(record['functions']))
            if any(name in CALLERS for name in record['functions']) and not internal_jump:
                outgoing.append(dict(record, target=names,
                                     target_offset=address, via='resolved-direct'))
        return dict(path=str(path), sha256=sha(path), symbols=records,
                    denominator=dict(functions=sum(map(len, functions.values())),
                                     relocations=relocation_count,
                                     decoded_instructions=instruction_count),
                    references=references, outgoing=outgoing,
                    incomplete_function_decodes=incomplete, overlapping_functions=overlaps,
                    exact_alias_groups=aliases,
                    unsupported_target_relocations=unsupported)


def copied_elf_controls(blob, directory):
    """Change only relocation metadata/addends in copied, non-executed ELFs."""
    with blob.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        symbols = list(table.iter_symbols())
        helper_index = next(i for i, s in enumerate(symbols) if s.name == HELPER)
        helper = symbols[helper_index]
        text_index = helper['st_shndx']
        helper_value_field = table['sh_offset'] + helper_index * table['sh_entsize'] + 4
        helper_last_byte = (elf.get_section(text_index)['sh_offset'] +
                            helper['st_value'] + helper['st_size'] - 1)
        section_index = next(i for i, s in enumerate(symbols)
                             if s['st_info']['type'] == 'STT_SECTION' and
                             s['st_shndx'] == text_index)
        sites = {}
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            owner = elf.get_section(section['sh_info'])
            for i, rel in enumerate(section.iter_relocations()):
                name = symbols[rel['r_info_sym']].name
                if name in (PROCESS, HELPER, TABLES[0]):
                    sites.setdefault(name, (section['sh_offset'] + i * 8 + 4,
                                            owner['sh_offset'] + rel['r_offset']))
    result = {}
    for label, source, new_index, addend in (
            ('extra-independent-caller', PROCESS, helper_index, None),
            ('section-relative-equivalent', HELPER, section_index, helper['st_value'] - 4),
            ('non-call-address-reference', TABLES[0], helper_index, None)):
        data = bytearray(blob.read_bytes())
        info_at, addend_at = sites[source]
        old = struct.unpack_from('<I', data, info_at)[0]
        struct.pack_into('<I', data, info_at, (new_index << 8) | (old & 255))
        if addend is not None:
            struct.pack_into('<i', data, addend_at, addend)
        path = directory / (label + '.o')
        path.write_bytes(data)
        observed = scan(path)
        refs = [r for r in observed['references'] if r['target'] == HELPER]
        if label == 'extra-independent-caller':
            assert len(refs) == 2 and any(TRANSMITTER in r['functions'] for r in refs)
        elif label == 'section-relative-equivalent':
            assert len(refs) == 1 and refs[0]['symbol'] == ''
            assert refs[0]['interior_offset'] == 0
        else:
            assert len(refs) == 2 and any(r['kind'] == 1 and r['instruction'] is None for r in refs)
        result[label] = dict(sha256=sha(path), helper_references=refs, detected=True)
    for label in ('incomplete-function-decode', 'overlapping-function-extent'):
        data = bytearray(blob.read_bytes())
        if label == 'incomplete-function-decode':
            # A truncated 0F opcode stream cannot complete this function.
            data[helper_last_byte - 15:helper_last_byte + 1] = b'\x0f' * 16
        else:
            struct.pack_into('<I', data, helper_value_field, helper['st_value'] - 1)
        path = directory / (label + '.o')
        path.write_bytes(data)
        observed = scan(path)
        key = ('incomplete_function_decodes' if label == 'incomplete-function-decode'
               else 'overlapping_functions')
        assert any(HELPER in item['functions'] for item in observed[key])
        result[label] = dict(sha256=sha(path), detected=True, diagnostics=observed[key])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', required=True, type=Path)
    parser.add_argument('--reference', default=Path(byteident.BLOB), type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    manifest = json.loads((args.baseline_dir / 'manifest.json').read_text())
    assert manifest['revision'] == '6fe0b1e4203ffc8f2b3c0d67345de3613f4465ef'
    assert manifest['reference_sha256'] == sha(args.reference)
    assert 'DSPLIB_REPRODUCE_BUGS' in manifest['config']
    assert (args.baseline_dir / '.build-config').read_text() == manifest['config']
    assert len(manifest['objects']) == 300
    original = scan(args.reference)
    objects = []
    for name, expected in manifest['objects'].items():
        path = args.baseline_dir / name
        assert sha(path) == expected, name
        objects.append(scan(path))
    assert not original['unsupported_target_relocations']
    assert not any(o['unsupported_target_relocations'] for o in objects)
    for item in [original] + objects:
        assert not item['incomplete_function_decodes'], (item['path'], item['incomplete_function_decodes'])
        assert not item['overlapping_functions'], (item['path'], item['overlapping_functions'])
    native_helper = [r for r in original['references'] if r['target'] == HELPER]
    native_process = [r for r in original['references'] if r['target'] == PROCESS]
    assert len(native_helper) == 1 and native_helper[0]['functions'] == [PROCESS]
    assert len(native_process) == 1 and native_process[0]['functions'] == [TRANSMITTER]
    current_helper = [r for o in objects for r in o['references'] if r['target'] == HELPER]
    current_process = [r for o in objects for r in o['references'] if r['target'] == PROCESS]
    assert not current_helper
    assert len(current_process) == 1 and current_process[0]['functions'] == [TRANSMITTER]
    encoder = args.baseline_dir / 'src_pump_v90_V92ConvEncoder.cpp.o'
    transmitter = args.baseline_dir / 'src_pump_v90_V92Transmitter.cpp.o'
    comparisons = {}
    for name in ENCODER + (TRANSMITTER,):
        path = transmitter if name == TRANSMITTER else encoder
        comparisons[name] = dict(original_size=byteident.sizes(str(args.reference))[name],
                                 current_size=byteident.sizes(str(path))[name],
                                 verdict=byteident.verdict(*byteident.body(str(args.reference), name),
                                                          *byteident.body(str(path), name)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    controls = copied_elf_controls(args.reference, args.output.parent)
    metadata_checks = {}
    current_encoder = next(o for o in objects if Path(o['path']) == encoder)
    for name in ENCODER + TABLES:
        before, after = original['symbols'][name], current_encoder['symbols'][name]
        for key in ('section', 'bind', 'type', 'visibility'):
            assert before[key] == after[key], (name, key)
        metadata_checks[name] = {key: before[key] for key in ('section', 'bind', 'type', 'visibility')}
    demangled = subprocess.run(['c++filt', HELPER, PROCESS, TRANSMITTER],
                               check=True, capture_output=True, text=True).stdout.splitlines()
    import local_data_ownership
    import tumap
    ownership = local_data_ownership.build(args.reference)
    witnesses = [record for record in ownership['records'].values()
                 if record['function']['name'] in ENCODER]
    assert len(witnesses) == 8 and all(not r['witnesses'] for r in witnesses)
    tus, _, _ = tumap.read_tus(str(args.reference))
    encoder_files = [tu for tu in tus if tu['name'] == 'V92ConvEncoder.cpp']
    assert len(encoder_files) == 1 and not encoder_files[0]['anchors']
    with encoder.open('rb') as stream:
        current_data = ELFFile(stream).get_section_by_name('.data').data()
    with args.reference.open('rb') as stream:
        native_data = ELFFile(stream).get_section_by_name('.data').data()[0x6ac0:0x6c00]
    assert len(current_data) == 320 and current_data == native_data
    constraints = {}
    for caller in TAIL_CALLERS:
        def bag(collection):
            return Counter((edge['instruction'], edge['target'])
                           for item in collection for edge in item['outgoing']
                           if caller in edge['functions'])
        before, after = bag([original]), bag(objects)
        if caller == TAIL_CALLERS[0]:
            assert before == {('jmp', '_ZN18V90Phase4Modulator13recivedSUVtagEv'): 1}
            assert after == {('call', 'edprintf'): 1}
        else:
            assert before[('jmp', '_ZN16V92EchoCanceller5resetEv')] == 1
            assert after[('jmp', '_ZN9FloatARMA5resetEv')] == 1
            assert after[('call', 'edprintf')] - before[('call', 'edprintf')] == 2
        constraints[caller] = {
            side: [dict(instruction=instruction, target=target, count=count)
                   for (instruction, target), count in sorted(counts.items())]
            for side, counts in [('original', before), ('current', after)]}
    # Keep empty objects in the denominator without emitting 300 redundant rows.
    relevant = [o for o in objects if o['symbols'] or o['references'] or o['outgoing']]
    report = dict(revision=manifest['revision'], config=manifest['config'],
                  baseline_manifest_sha256=sha(args.baseline_dir / 'manifest.json'),
                  object_count=len(objects), original=original, current=relevant,
                  current_denominator={key: sum(o['denominator'][key] for o in objects)
                                       for key in original['denominator']},
                  function_comparisons=comparisons, controls=controls,
                  matching_metadata=metadata_checks,
                  mangled_signatures=dict(zip((HELPER, PROCESS, TRANSMITTER), demangled)),
                  toolchain_manifest_sha256=sha(args.baseline_dir / 'tc_manifest.txt'),
                  original_ownership=dict(records=witnesses, FILE_records=encoder_files),
                  independent_inline_constraints=constraints,
                  exact_data_block=dict(original_section='.data', original_offset=0x6ac0,
                                        current_offset=0, size=320,
                                        sha256=hashlib.sha256(current_data).hexdigest()),
                  limits=['No executable or compiler-profile change.',
                          'No return type or access control inferred from ordinary method mangling.',
                          'Indirect destinations without relocated addresses and external callers are unobservable.',
                          'GLOBAL DEFAULT and lack of LOCAL FILE anchors do not prove TU ownership.'])
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"300/300 objects hash-verified; original {original['denominator']}; current {report['current_denominator']}")
    print('inverseMap refs: original 1, current 0; process refs: original 1, current 1; controls 5/5 detected')
    print(f'Wrote {args.output}')


if __name__ == '__main__':
    main()
