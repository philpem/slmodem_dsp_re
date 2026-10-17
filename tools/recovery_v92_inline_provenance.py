#!/usr/bin/env python3
"""Diagnose V92 helper frontend cost separately from its final native body.

Six complete-TU controls, all diagnostic. The residue array is a source
hypothesis, not a recovered declaration. No flags or source forms are retained.

  mkdir -p build
  python3 tools/recovery_v92_inline_provenance.py --domain scalar-array-cost \\
      --baseline-dir build/archived-v92-baseline
  python3 tools/recovery_v92_inline_provenance.py --parameter-proof
  python3 tools/recovery_v92_inline_provenance.py --audit
"""
import re
import json
import sys
import struct
import hashlib
import subprocess
from collections import Counter
from pathlib import Path
import playbook_small_patterns as d

d.REV = '4a6519cba'
d.OUT_NAME = 'v92-inline-provenance'
d.SOURCE_PATHS = ('src/pump/v90/V92ConvEncoder.cpp',)
d.DUMP_FLAGS = ('-da', '-v', '-save-temps', '-fdump-tree-original',
                '-fdump-tree-inlined', '-fdump-tree-optimized')


def array_residues(source):
    start, end, body = d.function(source, 'V92ConvolutionEncoder::inverseMap')
    for index in range(4):
        old = '\tint r%d = ' % index
        assert body.count(old) == 1
        body = body.replace(old, ('\tint residue[4];\n' if index == 0 else '') + '\tr%d = ' % index)
        body = re.sub(r'\br%d\b' % index, 'residue[%d]' % index, body)
    return source[:start] + body + source[end:]


def variants(path, source):
    array = array_residues(source)
    assert array != source
    return {'baseline': source,
            'scalar-no-auto': source,
            'array-o3': array,
            'array-no-auto': array,
            'scalar-no-unit': source,
            'scalar-o2-auto-web': source}


d.variants = variants
d.CELL_FLAGS = lambda path, label: {
    'scalar-no-auto': ('-fno-inline-functions',),
    'array-no-auto': ('-fno-inline-functions',),
    'scalar-no-unit': ('-fno-unit-at-a-time',),
    'scalar-o2-auto-web': ('-O2', '-finline-functions', '-fweb'),
}.get(label, ())


def frontend_cost(text, member):
    values = {int(value) for value in re.findall(
        re.escape('V92ConvolutionEncoder::' + member) + r'\([^\n]*?\): (\d+) insns', text)}
    if len(values) != 1:
        raise ValueError('missing or inconsistent frontend cost for ' + member)
    return values.pop()


def parameter_table(path):
    """Read Gentoo cc1plus's own 32-bit (name,value,help) default records."""
    from elftools.elf.elffile import ELFFile
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        assert elf.elfclass == 32 and elf.little_endian
        symbols = [symbol for symbol in elf.get_section_by_name('.symtab').iter_symbols()
                   if symbol.name == 'lang_independent_params']
        assert len(symbols) == 1
        symbol = symbols[0]
        section = elf.get_section(symbol['st_shndx'])
        assert section.name == '.rodata' and symbol['st_size'] % 12 == 0
        at = symbol['st_value'] - section['sh_addr']
        raw = section.data()[at:at + symbol['st_size']]

        def string(address):
            owners = [s for s in elf.iter_sections()
                      if s['sh_flags'] & 2 and s['sh_addr'] <= address < s['sh_addr'] + s['sh_size']]
            assert len(owners) == 1
            owner = owners[0]
            return owner.data()[address - owner['sh_addr']:].split(b'\0', 1)[0].decode('ascii')

        rows = {}
        for offset in range(0, len(raw), 12):
            name_address, default, help_address = struct.unpack_from('<IiI', raw, offset)
            if (name_address, default, help_address) == (0, 0, 0):
                assert offset == len(raw) - 12, 'parameter sentinel must terminate the table'
                continue
            name, help_text = string(name_address), string(help_address)
            assert re.fullmatch('[a-z][a-z0-9-]+', name) and help_text
            assert name not in rows
            rows[name] = dict(default=default, help=help_text,
                              record_address=symbol['st_value'] + offset,
                              record_hex=raw[offset:offset + 12].hex())
        return dict(program_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                    table_address=symbol['st_value'], table_bytes=symbol['st_size'],
                    table_file_offset=section['sh_offset'] + at,
                    records=rows)


def parameter_proof():
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    image = result['config'].splitlines()[0].split(' ', 1)[1]
    script = ('set -e; export PATH=' + d.tc.GENTOO_COMPILER_PATH + ':$PATH; '
              'parameter_cc1=$(g++ -print-prog-name=cc1plus); '
              'printf "%s\\n" "$parameter_cc1"; '
              'cp "$parameter_cc1" /work/installed-cc1plus; "$parameter_cc1" --version')
    command = d.tc.docker_prefix(image, d.ROOT, root, True) + ['/bin/sh', '-c', script]
    (root / 'parameter-proof-command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (root / 'parameter-proof.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    proof = parameter_table(root / 'installed-cc1plus')
    assert proof['records']['max-inline-insns-auto']['default'] == 100
    assert proof['records']['max-inline-insns-single']['default'] == 500
    (root / 'parameter-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print('Installed Gentoo compiler: %d default records; max-inline-insns-auto=100, single=500' %
          len(proof['records']))


def audit():
    from gcc3_value_carriers_audit import inspect
    import tumap
    from elftools.elf.elffile import ELFFile
    root = d.ROOT / 'build' / d.OUT_NAME
    result = json.loads((root / 'results.json').read_text())
    family = result['families']['V92ConvEncoder']
    cells = family['cells']
    folder = root / 'V92ConvEncoder'
    prefix = '_ZN21V92ConvolutionEncoder'
    helper, process, builder = [prefix + name for name in
                               ('10inverseMapEPi', '7processEPi', '25makeStateTtransitionTableEv')]
    expected = {
        'baseline': ([], 440, 503, 98),
        'scalar-no-auto': ([process], 440, 215, 98),
        'array-o3': ([helper, process], 456, 215, 106),
        'array-no-auto': ([helper, process], 456, 215, 106),
        'scalar-no-unit': ([], 440, 503, None),
        'scalar-o2-auto-web': ([builder, process], 440, 647, 98),
    }
    assert set(cells) == set(expected)
    assert cells['baseline']['baseline_reproduced']
    assert result['revision'] == d.REV
    assert 'DSPLIB_REPRODUCE_BUGS' in result['config']
    base = inspect(folder / 'baseline/candidate.o')
    report = {'cells': {}}
    subset, coset = prefix + '16subsetLabelTableE', prefix + '14cosetMapping4DE'
    proof = parameter_table(root / 'installed-cc1plus')
    assert proof['records']['max-inline-insns-auto']['default'] == 100
    report['compiler_defaults'] = proof
    # Inspect a changed default record only; never execute this compiler copy.
    default_control = bytearray((root / 'installed-cc1plus').read_bytes())
    auto = proof['records']['max-inline-insns-auto']
    at = proof['table_file_offset'] + auto['record_address'] - proof['table_address'] + 4
    default_control[at:at + 4] = struct.pack('<i', 101)
    wrong_default = root / 'changed-default-negative-cc1plus'
    wrong_default.write_bytes(default_control)
    assert parameter_table(wrong_default)['records']['max-inline-insns-auto']['default'] != 100
    call = Counter({('R_386_PC32', ('symbol', helper, 4294967292)): 1})
    inlined = Counter({('R_386_32', ('symbol', prefix + '16subsetLabelTableE', 0)): 2,
                       ('R_386_32', ('symbol', prefix + '14cosetMapping4DE', 0)): 1})
    for label, (changed, helper_size, process_size, cost) in expected.items():
        path = folder / label / 'candidate.o'
        observed = inspect(path)
        cell = cells[label]
        if label == 'scalar-no-unit':
            records = {name: value[:] for name, value in base['records'].items()}
            records[coset][-2] = 0
            records[subset][-2] = 256
            assert observed['records'] == records, (label, 'only two table offsets may change')
            data = bytes.fromhex(base['allocated']['.data'])
            allocated = dict(base['allocated'], **{'.data': (data[64:] + data[:64]).hex()})
            assert observed['allocated'] == allocated
            assert observed['text_positions'] == base['text_positions']
        else:
            for key in ('records', 'allocated'):
                assert observed[key] == base[key], (label, key)
        for key in ('nobits', 'relocations'):
            assert observed[key] == base[key], (label, key)
        assert len(cell['functions']) == len(cell['verdicts']) == 8
        assert set(cell.get('changed_bodies', [])) == set(changed)
        assert cell.get('gains', []) == []
        assert cell.get('losses', []) == ([helper] if label.startswith('array-') else [])
        assert observed['text_positions'][helper][1] == helper_size
        assert observed['text_positions'][process][1] == process_size
        assert observed['text_positions'][builder][1] == 1456
        helper_insns = d.b.insns(str(path), helper)
        frame_subtractions = [operands for mnemonic, operands in helper_insns
                              if mnemonic == 'sub' and operands.endswith(',%esp')]
        assert frame_subtractions == (['$0x1c,%esp'] if label.startswith('array-') else [])
        slots = {match.group(1) for mnemonic, operands in helper_insns if mnemonic == 'mov'
                 for match in [re.fullmatch(r'%[a-z]+,((?:0x[0-9a-f]+)?\(%esp\))', operands)]
                 if match}
        assert slots == ({'(%esp)', '0x4(%esp)', '0x8(%esp)'} if label.startswith('array-') else set())
        relocs = Counter(d.b.body(str(path), process)[1].values())
        is_call = label in ('scalar-no-auto', 'array-o3', 'array-no-auto')
        assert relocs == (call if is_call else inlined), label
        graph = folder / label / 'V92ConvEncoder.cpp.00.cgraph'
        if cost is None:
            assert not graph.exists(), 'no-unit control unexpectedly emits unit callgraph dump'
        else:
            text = graph.read_text()
            assert frontend_cost(text, 'inverseMap') == cost
            assert frontend_cost(text, 'makeStateTtransitionTable') == 350
            assert ('Inlined into int V92ConvolutionEncoder::process' in text) == (not is_call)
        report['cells'][label] = dict(helper_bytes=helper_size, process_bytes=process_size,
                                      frontend_cost=cost, changed_bodies=changed,
                                      helper_local_stack_bytes=28 if label.startswith('array-') else 0,
                                      helper_stack_store_slots=sorted(slots),
                                      gains=[], losses=cell.get('losses', []))
    # New cost extraction must refuse absent evidence, rather than return zero.
    text = (folder / 'array-o3/V92ConvEncoder.cpp.00.cgraph').read_text()
    try:
        frontend_cost(text.replace('inverseMap', 'missingMember'), 'inverseMap')
    except ValueError:
        pass
    else:
        raise AssertionError('missing-cost negative control did not fire')
    tus, _, _ = tumap.read_tus(d.b.BLOB)
    owner = [tu for tu in tus if tu['name'] == 'V92ConvEncoder.cpp']
    assert len(owner) == 1 and not owner[0]['anchors'] and not owner[0]['locals']
    symbols = tumap.elfinfo.read_symbols(d.b.BLOB)
    original = [symbol for symbol in symbols if symbol.name == helper]
    assert len(original) == 1 and original[0].bind == 'GLOBAL'
    report['ownership'] = dict(file=owner[0]['name'], file_sequence=owner[0]['seq'],
                               local_text_anchors=0, helper_binding=original[0].bind)
    def tables_and_relocations(path):
        with path.open('rb') as stream:
            elf = ELFFile(stream)
            table = elf.get_section_by_name('.symtab')
            tables = {}
            relocations = []
            for symbol in table.iter_symbols():
                if symbol.name in (subset, coset):
                    section = elf.get_section(symbol['st_shndx'])
                    at, size = symbol['st_value'], symbol['st_size']
                    tables[symbol.name] = dict(section=section.name, offset=at, size=size,
                                               bind=symbol['st_info']['bind'],
                                               type=symbol['st_info']['type'],
                                               payload=section.data()[at:at + size].hex())
            for section in elf.iter_sections():
                if section['sh_type'] != 'SHT_REL':
                    continue
                owner_section = elf.get_section(section['sh_info'])
                symbol_table = elf.get_section(section['sh_link'])
                for relocation in section.iter_relocations():
                    at = relocation['r_offset']
                    target = symbol_table.get_symbol(relocation['r_info_sym']).name
                    addend = int.from_bytes(owner_section.data()[at:at + 4], 'little', signed=True)
                    relocations.append([owner_section.name, at, relocation['r_info_type'], target, addend])
            return tables, relocations
    originals, _ = tables_and_relocations(Path(d.b.BLOB))
    current, base_relocations = tables_and_relocations(folder / 'baseline/candidate.o')
    no_unit, no_unit_relocations = tables_and_relocations(folder / 'scalar-no-unit/candidate.o')
    assert base_relocations == no_unit_relocations, 'raw relocation offsets/targets/addends changed'
    for name in (subset, coset):
        assert current[name]['payload'] == no_unit[name]['payload'] == originals[name]['payload']
        assert current[name]['size'] == originals[name]['size']
        assert current[name]['bind'] == no_unit[name]['bind'] == originals[name]['bind'] == 'STB_GLOBAL'
        assert current[name]['type'] == no_unit[name]['type'] == originals[name]['type'] == 'STT_OBJECT'
    report['tables'] = dict(original=originals, retained=current, no_unit=no_unit,
                            changed_raw_relocation_addends=[])
    report['missing_cost_negative_control'] = 'rejected'
    (root / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('V92 inline provenance: 6 cells / 48 body verdicts; 5 callgraph dumps '
          '(no-unit omits it); named data/bindings preserved; no-unit restores original table order; '
          '2 reported helper exact losses; missing-cost and changed-default negative controls fired')

if __name__ == '__main__':
    if sys.argv[1:] == ['--audit']:
        audit()
    elif sys.argv[1:] == ['--parameter-proof']:
        parameter_proof()
    else:
        d.main()
