#!/usr/bin/env python3
"""Account for extra math imports without mistaking them for missing behavior.

Read-only ELF census over a complete period object directory. Real ELF controls
distinguish symbol presence, call multiplicity and decoded call boundaries.
The six review labels describe this tree; unrecognized callers stay unresolved.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Do not let tools/dis.py shadow the standard library imported by dependencies.
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT/'tools']
import dis
sys.path.insert(0, str(ROOT/'tools/toolchain'))
import byteident as b
import jumptable
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

MATH = {'cos', 'fmodf', 'log10', 'pow', 'sin', 'sqrt'}
REVIEWS = {
    ('log10', 'FPM_log10_table_generate'): 'added offline table generator; no reference function',
    ('sqrt', 'FPM_sqrt_table_generate'): 'generated fallback after fsqrt in added offline table generator; no reference function',
    ('sin', 'beepgen_sample'): 'reference fsin; retained C period profile emits library call',
    ('cos', 'beepgen_sample'): 'reference fcos; retained C period profile emits library call',
    ('fmodf', 'MTK_phasor'): 'reference fprem; retained C period header/profile leaves external fmodf',
    ('pow', 'GetGain'): 'same reference calls; reference defines linked libm pow, empty pow.S omits body',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(path):
    """Keep every selected relocation, with unresolved owners/boundaries explicit."""
    result = {'undefined': [], 'defined': [], 'functions': [], 'relocations': []}
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        owners = defaultdict(list)
        for symbol in table.iter_symbols():
            if symbol.name and symbol['st_info']['bind'] != 'STB_LOCAL':
                result['undefined' if symbol['st_shndx'] == 'SHN_UNDEF' else 'defined'].append(symbol.name)
            if symbol['st_info']['type'] == 'STT_FUNC' and isinstance(symbol['st_shndx'], int):
                result['functions'].append(symbol.name)
                if symbol['st_size']:
                    owners[symbol['st_shndx']].append(symbol)
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            data_section = elf.get_section(section['sh_info'])
            data = data_section.data()
            symbols = elf.get_section(section['sh_link'])
            for reloc in section.iter_relocations():
                target = symbols.get_symbol(reloc['r_info_sym'])
                if target.name not in MATH:
                    continue
                offset = reloc['r_offset']
                candidates = [s for s in owners[section['sh_info']]
                              if s['st_value'] <= offset < s['st_value'] + s['st_size']]
                ranges = {(s['st_value'], s['st_size']) for s in candidates}
                addend = int.from_bytes(data[offset:offset+4], 'little', signed=True)
                edge = {'target': target.name, 'section': data_section.name, 'offset': offset,
                        'owners': sorted(s.name for s in candidates), 'type': reloc['r_info_type'],
                        'addend': addend, 'direct_control': False,
                        'file_operand_offset': data_section['sh_offset'] + offset}
                if len(ranges) == 1 and offset > 0 and reloc['r_info_type'] == 2 and addend == -4:
                    owner = candidates[0]
                    try:
                        decoded = {r[0]: r for r in jumptable.instructions(
                            str(path), data_section.name, owner['st_value'],
                            owner['st_value'] + owner['st_size'])}
                    except jumptable.Refused as error:
                        decoded = {}
                        edge['decode_refusal'] = str(error)
                    instruction = decoded.get(offset-1)
                    if instruction and len(instruction[1]) == 5 and instruction[2] in ('call', 'jmp'):
                        edge['direct_control'] = True
                        edge['instruction'] = instruction[2]
                result['relocations'].append(edge)
    for key in ('undefined', 'defined', 'functions'):
        result[key] = sorted(set(result[key]))
    return result


def controls(path, original):
    """Mutation of an actual rel32 target preserves imports, changes call counts."""
    raw = bytearray(path.read_bytes())
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            symbols = elf.get_section(section['sh_link'])
            cos = next((i for i, s in enumerate(symbols.iter_symbols()) if s.name == 'cos'), None)
            if cos is None:
                continue
            for index, reloc in enumerate(section.iter_relocations()):
                if symbols.get_symbol(reloc['r_info_sym']).name == 'sin':
                    position = section['sh_offset'] + index*section['sh_entsize'] + 4
                    raw[position:position+4] = ((cos << 8) | reloc['r_info_type']).to_bytes(4, 'little')
                    break
            else:
                continue
            break
        else:
            raise AssertionError('sin-to-cos control did not execute')
    before = Counter(e['target'] for e in original['relocations'])
    with tempfile.TemporaryDirectory() as temp:
        altered = Path(temp)/'redirected.o'
        altered.write_bytes(raw)
        redirected = inventory(altered)
        after = Counter(e['target'] for e in redirected['relocations'])
        assert redirected['undefined'] == original['undefined']
        assert after-before == Counter(cos=1) and before-after == Counter(sin=1)
        edge = next(e for e in original['relocations'] if e['target'] == 'sin')
        raw = bytearray(path.read_bytes())
        # MOV imm32 has the same five-byte extent as CALL rel32.
        raw[edge['file_operand_offset']-1] = 0xb8
        altered = Path(temp)/'malformed.o'
        altered.write_bytes(raw)
        malformed = inventory(altered)
        assert sum(e['direct_control'] for e in malformed['relocations']) == \
            sum(e['direct_control'] for e in original['relocations']) - 1
    assert inventory(path) == original
    return {'same_imports_different_call_target': True, 'noncontrol_opcode_rejected': True,
            'unchanged_inventory_negative': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    paths = sorted(args.objects.glob('*.o'))
    assert paths, 'empty object denominator'
    reference = inventory(args.reference)
    candidates = {p: inventory(p) for p in paths}
    definitions = set().union(*(set(r['defined']) for r in candidates.values()))
    imports = set().union(*(set(r['undefined']) for r in candidates.values())) - definitions
    extra = imports - set(reference['undefined'])
    rows = []
    args.output.mkdir(parents=True, exist_ok=True)
    for path, candidate in candidates.items():
        for name in sorted({n for edge in candidate['relocations'] for n in edge['owners']}):
            edges = [e for e in candidate['relocations'] if name in e['owners']]
            original = [e for e in reference['relocations'] if name in e['owners']]
            in_reference = name in reference['functions']
            instructions = {label: b.insns(str(p), name) for label, p in
                            ([('candidate', path), ('reference', args.reference)] if in_reference
                             else [('candidate', path)])}
            row = {'object': path.name, 'symbol': name, 'reference_function': in_reference,
                   'candidate_edges': edges, 'reference_edges': original,
                   'instruction_counts': {label: dict(Counter(i[0] for i in insns))
                                          for label, insns in instructions.items()},
                   'reviews': {e['target']: REVIEWS.get((e['target'], name), 'UNRESOLVED') for e in edges}}
            if in_reference:
                row['reference_verdict'] = b.verdict(*b.body(str(args.reference), name), *b.body(str(path), name))
            for label, p in [('candidate', path)] + ([('reference', args.reference)] if in_reference else []):
                dump = subprocess.check_output(['objdump', '-drw', '-j', '.text', '--disassemble='+name, str(p)], text=True)
                (args.output/(name+'.'+label+'.dis')).write_text(dump)
            rows.append(row)
    canary = next(p for p, r in candidates.items() if {'sin', 'cos'} <= {e['target'] for e in r['relocations']})
    reviewed_sources = ('src/dsp/fpm_log10.c', 'src/dsp/fpm_sqrt.c',
                        'src/service/Beepgen.c', 'src/service/PHASOR.c', 'src/core/pow.S')
    report = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'reference_sha256': sha(args.reference), 'object_sha256': {p.name: sha(p) for p in paths},
              'reviewed_source_sha256': {p: sha(ROOT/p) for p in reviewed_sources},
              'build_config': (args.objects/'.build-config').read_text(),
              'objects': len(paths), 'emitted_function_copies': sum(len(r['functions']) for r in candidates.values()),
              'reference_imports': reference['undefined'], 'candidate_imports': sorted(imports),
              'candidate_only_imports': sorted(extra),
              'selected_math_relocations': sum(len(r['relocations']) for r in candidates.values()),
              'reference_math_relocations': len(reference['relocations']),
              'decoded_direct_candidate_math_relocations': sum(e['direct_control']
                  for r in candidates.values() for e in r['relocations']),
              'reference_math_definitions': sorted(MATH & set(reference['defined'])),
              'reference_math_undefined': sorted(MATH & set(reference['undefined'])),
              'controls': controls(canary, candidates[canary]), 'rows': rows,
              'limits': 'Read-only census of supplied objects, not a rebuild or new differential test. '
              'Review labels require source/body evidence; calls alone prove no reachability or behavior defect. '
              'Undefined presence differs from call count and from deliberately omitted linked library bodies. '
              'No source/profile change, exact gain/loss or whole-object equality is claimed.'}
    (args.output/'census.json').write_text(json.dumps(report, indent=2)+'\n')
    print('%d objects; %d function copies; %d/%d candidate/reference math relocations; '
          '%d imports vs %d reference; extras %s; %d controls pass' %
          (report['objects'], report['emitted_function_copies'], report['selected_math_relocations'],
           report['reference_math_relocations'], len(imports), len(reference['undefined']),
           ','.join(sorted(extra)), len(report['controls'])))
    for row in rows:
        print(row['symbol'], row['reviews'])


if __name__ == '__main__':
    main()
