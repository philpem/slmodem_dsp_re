#!/usr/bin/env python3
"""Inventory direct calls and unconditional inter-function jumps together.

Static target multiplicity is a review aid, not CFG/path equivalence. Internal
jumps are excluded; indirect transfers and ambiguous/interior destinations are
reported as unresolved. No compiler, assembler or source variants are run.
"""
import playbook_small_patterns  # Preload stdlib dis before ELF dependencies.
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import tempfile

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_IMM
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
WINDOW = '_Z12designWindowIfEv10WindowTypePT_j'
BOXCAR = '_Z6boxcarIfEvPT_j'
GENTOO = b'GCC: (GNU) 3.4.2  (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf.little_endian and elf['e_machine'] == 'EM_386'
        assert elf['e_type'] == 'ET_REL', 'section-relative relocatable input required'
        table = elf.get_section_by_name('.symtab')
        functions = [s for s in table.iter_symbols() if s['st_info']['type'] == 'STT_FUNC'
                     and s['st_size'] and isinstance(s['st_shndx'], int)]
        # The deliberately empty pow.S input has no compiler banner or body.
        # It contributes no transfer observations; every nonempty function
        # input must carry the recovered compiler's banner.
        comments = elf.get_section_by_name('.comment')
        if functions:
            assert comments and GENTOO in comments.data().split(b'\0'), ('require recovered Gentoo object', path)
        starts, relocs = defaultdict(set), {}
        for symbol in functions:
            starts[symbol['st_shndx'], symbol['st_value']].add(symbol.name)
        for section in elf.iter_sections():
            if section['sh_type'] != 'SHT_REL':
                continue
            symbols = elf.get_section(section['sh_link'])
            for relocation in section.iter_relocations():
                key = (section['sh_info'], relocation['r_offset'])
                assert key not in relocs, ('overlapping relocations', path, key)
                relocs[key] = (relocation['r_info_type'],
                               symbols.get_symbol(relocation['r_info_sym']))
        decoder = Cs(CS_ARCH_X86, CS_MODE_32)
        decoder.detail = True
        result, section_bytes = {}, {}
        for symbol in functions:
            assert symbol.name not in result, ('duplicate function name', path, symbol.name)
            section_index = symbol['st_shndx']
            if section_index not in section_bytes:
                section_bytes[section_index] = elf.get_section(section_index).data()
            raw = section_bytes[section_index]
            lo, hi = symbol['st_value'], symbol['st_value'] + symbol['st_size']
            code = raw[lo:hi]
            rows = list(decoder.disasm(code, lo))
            assert sum(i.size for i in rows) == len(code), ('incomplete decode', path, symbol.name)
            events, unresolved, internal = [], [], 0
            for instruction in rows:
                if instruction.mnemonic not in ('call', 'jmp'):
                    continue
                item = {'op': instruction.mnemonic, 'offset': instruction.address-lo}
                if len(instruction.operands) != 1 or instruction.operands[0].type != X86_OP_IMM:
                    unresolved.append(dict(item, refusal='indirect transfer; may be switch dispatch'))
                    continue
                relocation = relocs.get((symbol['st_shndx'], instruction.address+1))
                target = instruction.operands[0].imm
                if relocation:
                    kind, destination = relocation
                    addend = int.from_bytes(raw[instruction.address+1:instruction.address+5],
                                            'little', signed=True)
                    if instruction.size != 5 or kind != 2 or addend != -4:
                        unresolved.append(dict(item, refusal='unsupported relocation shape/addend'))
                        continue
                    defined_function = (destination['st_info']['type'] == 'STT_FUNC'
                                        and isinstance(destination['st_shndx'], int))
                    external = (destination['st_shndx'] == 'SHN_UNDEF'
                                and destination['st_info']['type'] in ('STT_FUNC', 'STT_NOTYPE'))
                    if not destination.name or not (defined_function or external):
                        unresolved.append(dict(item, refusal='relocation does not name function/external'))
                        continue
                    name, resolution = destination.name, 'R_386_PC32'
                elif instruction.mnemonic == 'jmp' and lo <= target < hi:
                    internal += 1
                    continue
                else:
                    names = starts.get((symbol['st_shndx'], target), set())
                    if len(names) != 1:
                        unresolved.append(dict(item, refusal='not one function start in caller section'))
                        continue
                    name, resolution = next(iter(names)), 'same-section unique function start'
                events.append(dict(item, target=name, resolution=resolution))
            result[symbol.name] = {'events': events, 'unresolved': unresolved,
                                   'internal_jumps_excluded': internal}
        return result


def target_bag(row):
    return Counter(event['target'] for event in row['events'])


def transfer_bag(row):
    return Counter((event['op'], event['target']) for event in row['events'])


def controls(reference, original):
    """Change an actual relocation target/opcode; exercise internal-loop refusal."""
    window = original[WINDOW]
    assert len(window['events']) == 5
    assert sum(e['op'] == 'jmp' and e['target'] == BOXCAR for e in window['events']) == 1
    assert target_bag(window)[BOXCAR] == 2
    with reference.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        symbol = next(s for s in table.iter_symbols() if s.name == WINDOW)
        section = elf.get_section(symbol['st_shndx'])
        site = next(e for e in window['events'] if e['op'] == 'jmp' and e['target'] == BOXCAR)
        position = section['sh_offset'] + symbol['st_value'] + site['offset']
        assert reference.read_bytes()[position] == 0xe9
    with tempfile.TemporaryDirectory(prefix='tail-transfer-controls-') as work:
        path = Path(work)/'changed.o'
        changed = bytearray(reference.read_bytes())
        # A call has the same rel32 operand but a different transfer boundary.
        changed[position] = 0xe8
        path.write_bytes(changed)
        call = inventory(path)[WINDOW]
        assert target_bag(call) == target_bag(window)
        assert transfer_bag(call) != transfer_bag(window)
        assert call['events'] != window['events']
        # Redirect to the target function's interior. The addend must be refused,
        # rather than silently credited as the same named boxcar invocation.
        changed = bytearray(reference.read_bytes())
        changed[position+1:position+5] = (-3).to_bytes(4, 'little', signed=True)
        path.write_bytes(changed)
        interior = inventory(path)[WINDOW]
        assert target_bag(interior)[BOXCAR] == 1
        assert any(r['refusal'] == 'unsupported relocation shape/addend' for r in interior['unresolved'])
    assert sum(r['internal_jumps_excluded'] for r in original.values()) > 0
    assert sum(len(r['unresolved']) for r in original.values()) > 0
    assert not original[BOXCAR]['events'] and original[BOXCAR]['internal_jumps_excluded'] > 0
    return {'separate_boxcar_tail_and_call': True, 'changed_opcode_detected': True,
            'interior_addend_refused': True, 'boxcar_loop_not_a_tail': True,
            'production_inputs_modified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    parser.add_argument('--census', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    census = json.loads(args.census.read_text())
    config = (args.objects/'.build-config').read_text()
    assert '-DDSPLIB_REPRODUCE_BUGS' in config
    manifest = [line.split() for line in (args.objects/'tc_manifest.txt').read_text().splitlines()]
    assert {p.name for p in args.objects.glob('*.o')} == {name for name, source in manifest}
    paths = [args.reference] + [args.objects/name for name, source in manifest]
    before = {str(p): sha(p) for p in paths}
    original = inventory(args.reference)
    calibration = controls(args.reference, original)
    counts, seen, nominations, copies, kind_nominations = Counter(), set(), [], [], []
    for name, source in manifest:
        retained = inventory(args.objects/name)
        counts['objects'] += 1
        counts['objects_without_function_bodies'] += int(not retained)
        counts['emitted_defining_copies'] += len(retained)
        for symbol in sorted(retained.keys() & original.keys()):
            seen.add(symbol)
            counts['shared_defining_copies'] += 1
            a, b = original[symbol], retained[symbol]
            for side, row in [('original', a), ('candidate', b)]:
                counts[side+'_unresolved_sites'] += len(row['unresolved'])
                counts[side+'_internal_jumps_excluded'] += row['internal_jumps_excluded']
            if not any(e['op'] == 'jmp' for row in (a, b) for e in row['events']):
                continue
            counts['copies_with_named_tail'] += 1
            for side, row in [('original', a), ('candidate', b)]:
                counts[side+'_named_tail_sites'] += sum(e['op'] == 'jmp' for e in row['events'])
            row = {'symbol': symbol, 'object': name, 'source': source, 'original': a,
                   'candidate': b, 'target_multiplicity_equal': target_bag(a) == target_bag(b),
                   'transfer_kind_multiplicity_equal': transfer_bag(a) == transfer_bag(b)}
            copies.append(row)
            if not row['target_multiplicity_equal']:
                nominations.append(row)
            if not row['transfer_kind_multiplicity_equal']:
                kind_nominations.append(row)
    assert len(seen) == census['compared'], 'shared-name denominator drift'
    assert set(census['exact_symbols']) <= seen
    assert all(sha(Path(p)) == value for p, value in before.items()), 'input changed during audit'
    assert (args.objects/'.build-config').read_text() == config
    report = {'config': config, 'input_sha256': before, 'census_sha256': sha(args.census),
              'controls': calibration, 'counts': dict(counts), 'shared_unique_names': len(seen),
              'missing_reference_names': sorted(set(original)-seen),
              'tail_copies': copies, 'target_multiplicity_nominations': nominations,
              'transfer_kind_multiplicity_nominations': kind_nominations,
              'limits': __doc__ + '\nConditional exits, indirect targets, aliases and interior '
                        'targets remain unproved. Multiplicity differences can be cloning or inlining; '
                        'equal bags do not establish arguments, order, paths or behavior.'}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print('tail audit:', dict(counts), '; shared names', len(seen), '/', census['compared'],
          '; target/kind-multiplicity nominations', len(nominations), '/', len(kind_nominations),
          '; 4 controls passed')
    for row in nominations:
        print(row['source'], row['symbol'])


if __name__ == '__main__':
    main()
