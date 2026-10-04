#!/usr/bin/env python3
"""Audit full objects and first RTL differences for the value-carrier controls."""
import json
import re
from pathlib import Path
import playbook_small_patterns as d
import jumptable
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ADID = '_ZN25V90AutoDigitalImpDetector'
ORDINARY = ADID+'23updateLinMappMeanAndVarEss'
ALTERNATE = ADID+'26updateLinMappMeanAndVarAltEss'
DECODER = '_ZN27ParallelDifferentialDecoderIhE7processEPhS1_'
QC = ADID+'18setQcLinearMappingEv'
UREF = ADID+'10updateUrefEv'
STUDY = ADID+'16studyUrefHandlerEfj'

def inspect(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        names = {i: section.name for i, section in enumerate(elf.iter_sections())}
        records, positions = {}, {}
        for symbol in tab.iter_symbols():
            if not symbol.name:
                continue
            entry = symbol.entry
            section = names.get(entry['st_shndx'], entry['st_shndx'])
            records[symbol.name] = [entry['st_info']['type'], entry['st_info']['bind'], entry['st_other']['visibility'], section]
            if entry['st_info']['type'] == 'STT_FUNC':
                positions[symbol.name] = [entry['st_value'], entry['st_size']]
            else:
                records[symbol.name] += [entry['st_value'], entry['st_size']]
        allocated = {section.name: bytearray(section.data()) for section in elf.iter_sections()
                     if section['sh_flags'] & 2 and section['sh_type'] != 'SHT_NOBITS' and not section['sh_flags'] & 4}
        nobits = {section.name: section['sh_size'] for section in elf.iter_sections()
                  if section['sh_flags'] & 2 and section['sh_type'] == 'SHT_NOBITS'}
        relocs = []
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            owner = elf.get_section(section['sh_info'])
            if owner['sh_flags'] & 4:
                continue
            for rel in section.iter_relocations():
                target = tab.get_symbol(rel['r_info_sym'])
                offset = rel['r_offset']
                kind = {1: 'R_386_32', 2: 'R_386_PC32'}[rel['r_info_type']]
                raw = owner.data()[offset:offset+4]
                name = target.name or names[target['st_shndx']]
                identity = d.b.relocation_target(kind, name, raw, d.b.section_symbols(str(path)))
                if identity[0] == 'section' and identity[1] == '.text':
                    assert kind == 'R_386_32'
                    owners = [symbol for symbol in tab.iter_symbols()
                              if symbol['st_info']['type'] == 'STT_FUNC'
                              and names.get(symbol['st_shndx']) == '.text'
                              and symbol['st_value'] <= identity[2] < symbol['st_value']+symbol['st_size']]
                    assert len(owners) == 1, (path, identity, 'ambiguous code owner')
                    owner_symbol = owners[0]
                    start = owner_symbol['st_value']
                    instructions = jumptable.instructions(str(path), '.text', start, start+owner_symbol['st_size'])
                    assert identity[2] in {row[0] for row in instructions}, 'data destination inside instruction'
                    identity = ('audited-code-destination', owner_symbol.name, identity[2]-start)
                relocs.append([owner.name, offset, kind, identity])
                if owner.name in allocated:
                    assert offset+4 <= len(allocated[owner.name])
                    allocated[owner.name][offset:offset+4] = b'\0'*4
        return {'records': records, 'allocated': {name: data.hex() for name, data in allocated.items()}, 'nobits': nobits,
                'relocations': relocs, 'text_positions': positions}

def function_chunk(path, needle):
    chunks = [part for part in path.read_text().split(';; Function ') if needle in part.split('\n', 1)[0]]
    assert len(chunks) == 1, (path, needle, len(chunks))
    return chunks[0]

def refusal_controls():
    path = d.ROOT/'build/gcc3-value-carriers-adid-cross/V90AutoDigitalImpDetector/baseline/candidate.o'
    out = d.ROOT/'build/gcc3-value-carriers-controls'
    out.mkdir(exist_ok=True)
    raw = path.read_bytes()
    base = inspect(path)
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        tab = elf.get_section_by_name('.symtab')
        rodata = elf.get_section_by_name('.rodata')
        table = elf.get_section_by_name('.rel.rodata')
        starts = {row[0] for symbol in tab.iter_symbols()
                  if symbol['st_info']['type'] == 'STT_FUNC' and symbol['st_shndx'] == elf.get_section_index('.text')
                  for row in jumptable.instructions(str(path), '.text', symbol['st_value'], symbol['st_value']+symbol['st_size'])}
        for relocation in table.iter_relocations():
            offset = rodata['sh_offset']+relocation['r_offset']
            destination = int.from_bytes(raw[offset:offset+4], 'little')
            if destination+1 not in starts:
                break
        else:
            raise AssertionError('interior-destination control has no fixture')
        changed = bytearray(raw)
        changed[offset:offset+4] = (destination+1).to_bytes(4, 'little')
        bad = out/'inside-instruction.o'
        bad.write_bytes(changed)
        try:
            inspect(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('interior code destination accepted')
        changed = bytearray(raw)
        changed[elf.get_section_by_name('.rodata.cst4')['sh_offset']] ^= 1
        bad = out/'changed-constant.o'
        bad.write_bytes(changed)
        assert inspect(bad)['allocated'] != base['allocated'], 'changed constant masked'
    return {'interior_destination_refused': True, 'changed_constant_detected': True}

def main():
    reports, controls = [], []
    for package in ['adid', 'adid-cross', 'decoder', 'consumers', 'uref']:
        root = d.ROOT/'build'/('gcc3-value-carriers-'+package)
        result = json.loads((root/'results.json').read_text())
        for family, content in result['families'].items():
            cells = content['cells']
            assert cells['baseline']['baseline_reproduced']
            base = inspect(root/family/'baseline/candidate.o')
            for label, cell in cells.items():
                path = root/family/label/'candidate.o'
                observed = inspect(path)
                for key in ['records', 'allocated', 'nobits']:
                    assert observed[key] == base[key], (package, family, label, key)
                relocation_changes = []
                if observed['relocations'] != base['relocations']:
                    assert package == 'uref' and label.endswith('uref-1')
                    assert len(observed['relocations']) == len(base['relocations'])
                    for before, after in zip(base['relocations'], observed['relocations']):
                        assert before[:3] == after[:3]
                        if before != after:
                            assert before[3][:2] == after[3][:2] == ('audited-code-destination', STUDY)
                            relocation_changes.append([before, after])
                expected, expected_gains = [], []
                if label != 'baseline':
                    if package == 'adid': expected = [ALTERNATE]
                    elif package == 'adid-cross':
                        if label.startswith('ordinary-1'): expected.extend([ORDINARY, QC])
                        if label.endswith('alternate-1'): expected.append(ALTERNATE)
                    elif package == 'uref':
                        if label.startswith('pair-1'): expected.extend([ORDINARY, ALTERNATE, QC])
                        if label.endswith('uref-1'): expected.extend([UREF, STUDY])
                    elif family == 'V90SignBitsExtractor': expected = [DECODER]
                expected_gains = [name for name in expected if name not in [QC, UREF, STUDY]]
                assert sorted(cell.get('changed_bodies', [])) == sorted(expected), (package, family, label)
                assert sorted(cell.get('gains', [])) == sorted(expected_gains)
                assert not cell.get('losses', [])
                if not expected:
                    assert path.read_bytes() == (root/family/'baseline/candidate.o').read_bytes()
                reports.append({'package': package, 'family': family, 'label': label,
                                'body_verdicts': len(cell['verdicts']), 'changed': expected,
                                'data_metadata_identical': True, 'relocation_changes': relocation_changes,
                                'text_position_changes': {name: [base['text_positions'].get(name), position]
                                                          for name, position in observed['text_positions'].items()
                                                          if base['text_positions'].get(name) != position}})
    root = d.ROOT/'build/gcc3-value-carriers-adid-cross/V90AutoDigitalImpDetector'
    for label in ['baseline', 'ordinary-0-alternate-1', 'ordinary-1-alternate-0', 'ordinary-1-alternate-1']:
        path = root/label/'V90AutoDigitalImpDetector.cpp.01.rtl'
        for name, enabled in [('updateLinMappMeanAndVar', label.startswith('ordinary-1')),
                              ('setQcLinearMapping', label.startswith('ordinary-1')),
                              ('updateLinMappMeanAndVarAlt', label.endswith('alternate-1'))]:
            chunk = function_chunk(path, '::'+name+'(')
            assert chunk.count('(plus:DF') == int(enabled), (label, name, 'DF addition')
            assert chunk.count('(plus:SF') == int(not enabled), (label, name, 'SF addition')
            controls.append({'label': label, 'method': name, 'double_addition': enabled})
    root = d.ROOT/'build/gcc3-value-carriers-decoder/V90SignBitsExtractor'
    for label in ['baseline', 'typed-result']:
        for stage in ['01.rtl', '20.combine', '25.greg', '26.postreload', '35.mach']:
            chunk = function_chunk(root/label/('V90SignBitsExtractor.cpp.'+stage), 'ParallelDifferentialDecoder<T>::process(')
            xor = re.search(r'\(xor:QI[\s\S]*?\)\)\)', chunk)
            assert xor, (label, stage, 'QI XOR control did not fire')
            operand = xor.group(0)
            if stage == '01.rtl':
                assert ('[ decoded ]' in operand) == (label == 'typed-result')
            if stage == '20.combine':
                assert '(mem:QI' in operand
                assert (operand.index('(mem:QI') < operand.index('[ x ]')) == (label == 'baseline')
            if stage in ['26.postreload', '35.mach']:
                assert ('(mem:QI' in operand) == (label == 'typed-result')
            controls.append({'label': label, 'stage': stage, 'xor': operand})
    root = d.ROOT/'build/gcc3-value-carriers-uref/V90AutoDigitalImpDetector'
    for label in ['baseline', 'pair-0-uref-1', 'pair-1-uref-0', 'pair-1-uref-1']:
        chunk = function_chunk(root/label/'V90AutoDigitalImpDetector.cpp.01.rtl', '::updateUref(')
        enabled = label.endswith('uref-1')
        assert chunk.count('(plus:DF') == int(enabled)
        assert chunk.count('(plus:SF') == int(not enabled)
        controls.append({'label': label, 'method': 'updateUref', 'double_addition': enabled})
    report = {'driver_cells': len(reports), 'body_verdicts': sum(r['body_verdicts'] for r in reports),
              'full_object_audits': reports, 'rtl_controls': controls, 'refusal_controls': refusal_controls()}
    (d.ROOT/'build/gcc3-value-carriers-audit.json').write_text(json.dumps(report, indent=2)+'\n')
    print(len(reports), 'full-object audits;', report['body_verdicts'], 'body verdicts;', len(controls), 'RTL controls passed')

if __name__ == '__main__':
    main()
