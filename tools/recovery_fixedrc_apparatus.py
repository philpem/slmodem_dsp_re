#!/usr/bin/env python3
"""Bounded full-TU control for FixedRC fixture helpers/offline catalog removal."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import playbook_small_patterns as d
from recovery_generator_separation import separate, named_data
from recovery_fax_debug_audit import Object
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

HELPERS = ('RcFixed_UpFactor', 'RcFixed_DownFactor', 'RcFixed_State')
PATH = 'src/core/FixedRC.c'


def remove_apparatus(source):
    for name in HELPERS:
        source, _ = separate(source, name)
    start = source.index('const struct rc_bank rc_banks[')
    end = source.index('\n};', start) + len('\n};')
    return source[:start] + source[end:]


def allocated(path):
    """Positioned allocated relocations and masked bytes, including jump tables."""
    result = {}
    symbols = d.b.section_symbols(str(path))
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name('.symtab')
        functions = [s for s in table.iter_symbols() if s['st_info']['type'] == 'STT_FUNC' and s['st_size']]
        for index, section in enumerate(elf.iter_sections()):
            if not section['sh_flags'] & 2:
                continue
            raw = bytearray(section.data())
            rels = {}
            for relsec in elf.iter_sections():
                if not isinstance(relsec, RelocationSection) or relsec['sh_info'] != index:
                    continue
                relsyms = elf.get_section(relsec['sh_link'])
                for rel in relsec.iter_relocations():
                    offset = rel['r_offset']
                    symbol = relsyms.get_symbol(rel['r_info_sym'])
                    kind = {1: 'R_386_32', 2: 'R_386_PC32'}[rel['r_info_type']]
                    target = d.b.relocation_target(kind, symbol.name or elf.get_section(symbol['st_shndx']).name,
                                                   bytes(raw[offset:offset+4]), symbols)
                    if target[:2] == ('section', '.text'):
                        owners = [s for s in functions if s['st_shndx'] == elf.get_section_index('.text')
                                  and s['st_value'] <= target[2] < s['st_value']+s['st_size']]
                        assert len(owners) == 1, (path, offset, target)
                        target = ('function', owners[0].name, target[2]-owners[0]['st_value'])
                    rels[offset] = (kind, target)
                    raw[offset:offset+4] = b'\0'*4
            result[section.name] = (bytes(raw), rels)
    return result


def shift_target(target):
    return (target[0], target[1], target[2]-160) if target[:2] == ('section', '.rodata') else target


def generator_replay(root, out, revision, image, flags):
    """Keep every emitted offline coefficient/catalog byte and metadata record."""
    report = {}
    for label in ('baseline', 'candidate'):
        folder = out/'generator'/label
        folder.mkdir(parents=True, exist_ok=True)
        script = folder/'gen_rc_coeffs.py'
        contents = subprocess.check_output(['git', 'show', revision+':tools/gen_rc_coeffs.py'], cwd=root) if label == 'baseline' else (root/'tools/gen_rc_coeffs.py').read_bytes()
        script.write_bytes(contents)
        source = folder/'rc_coeffs.c'
        generation = [sys.executable, str(script), str(root/'ref/slmodemd/dsplibs.o'), '-o', str(source)]
        generated = subprocess.check_output(generation, cwd=root, text=True)
        compile_flags = ['-I/work/historical-headers']+flags if label == 'baseline' else flags
        virtual = '/work/generator/'+label
        command = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '-c',
            d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, compile_flags+['-v'], virtual+'/candidate.o', virtual+'/rc_coeffs.c')]
        with (folder/'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        obj = folder/'candidate.o'
        report[label] = {'generation_command': generation, 'generation_output': generated,
                         'command': command, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                         'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()}
    before, after = out/'generator/baseline/candidate.o', out/'generator/candidate/candidate.o'
    assert before.read_bytes() == after.read_bytes(), 'offline generator output object changed'
    # Independently check the generated banks against the reference ELF bytes.
    import gen_rc_coeffs as generator
    expected_banks = {}
    with (root/'ref/slmodemd/dsplibs.o').open('rb') as stream:
        raw = ELFFile(stream).get_section_by_name('.rodata').data()
        for mode, (address, taps) in generator.BANKS.items():
            expected_banks['rc_coeff_%05x' % address] = raw[address:address+taps*generator.UP[mode]*2].hex()
    named = named_data(after)
    assert set(named) == set(expected_banks) | {'rc_banks'}
    for name, value in expected_banks.items():
        assert named[name][2] == value, name
    default_folder = out/'generator-default'
    default_folder.mkdir(exist_ok=True)
    subprocess.run([sys.executable, str(root/'tools/gen_rc_coeffs.py'), str(root/'ref/slmodemd/dsplibs.o')], cwd=default_folder, check=True)
    assert (default_folder/'build/rc_coeffs.c').read_bytes() == (out/'generator/candidate/rc_coeffs.c').read_bytes()
    report.update({'raw_object_equal': True, 'reference_banks_checked': len(expected_banks),
                   'reference_coefficient_bytes_checked': sum(len(v)//2 for v in expected_banks.values()),
                   'catalog_modes_retained': 20, 'safe_default_output_checked': True})
    print('generator replay:', len(expected_banks), 'banks/20 modes, raw object identical; default output under build/', flush=True)
    return report


def audit(before, after):
    a, b = Object(before).canonical(), Object(after).canonical()
    assert set(b[0]) == set(a[0])-set(HELPERS)
    changed = [n for n in b[0] if a[0][n] != b[0][n]]
    relchanges = []
    for name in b[0]:
        old = {off: (kind, shift_target(target)) for off, (kind, target) in d.b.body(str(before), name)[1].items()}
        if old != d.b.body(str(after), name)[1]:
            relchanges.append(name)
    expected = {(n, 'STB_GLOBAL', 'STT_FUNC', 'STV_DEFAULT') for n in HELPERS}
    expected.add(('rc_banks', 'STB_GLOBAL', 'STT_OBJECT', 'STV_DEFAULT'))
    assert set(a[2])-set(b[2]) == expected and not set(b[2])-set(a[2])
    olddata, newdata = named_data(before), named_data(after)
    assert newdata == {n: value for n, value in olddata.items() if n != 'rc_banks'}
    oldalloc, newalloc = allocated(before), allocated(after)
    assert set(oldalloc) == set(newalloc)
    for section, (raw, rels) in oldalloc.items():
        if section == '.text':
            continue  # each surviving complete body and positioned target checked above
        skip = 160 if section == '.rodata' else 0
        assert newalloc[section][0] == raw[skip:], (section, 'payload')
        expected_rels = {at-skip: (kind, shift_target(target)) for at, (kind, target) in rels.items() if at >= skip}
        assert newalloc[section][1] == expected_rels, (section, 'positioned relocations')
    assert not changed and not relchanges, (changed, relchanges)
    return {'surviving_bodies': len(b[0]), 'changed_surviving_bodies': changed,
            'changed_positioned_relocations': relchanges, 'removed_bindings': sorted(expected),
            'retained_named_data': len(newdata), 'removed_catalog_bytes': 160,
            'removed_catalog_pointer_relocations': 18,
            'text_bytes_removed_including_alignment': len(oldalloc['.text'][0])-len(newalloc['.text'][0]),
            'all_remaining_allocated_data_equal_at_shifted_positions': True}


def controls(before, after, folder):
    """Show that a changed coefficient and changed positioned target are rejected."""
    folder.mkdir(exist_ok=True)
    raw = bytearray(after.read_bytes())
    with after.open('rb') as stream:
        elf = ELFFile(stream)
        symbol = next(s for s in elf.get_section_by_name('.symtab').iter_symbols()
                      if s.name == 'rc80to96filter')
        position = elf.get_section(symbol['st_shndx'])['sh_offset'] + symbol['st_value']
        relsec = elf.get_section_by_name('.rel.text')
        text = elf.get_section(relsec['sh_info'])
        table = elf.get_section(relsec['sh_link'])
        relocation = next(rel for rel in relsec.iter_relocations() if rel['r_info_type'] == 1
                          and table.get_symbol(rel['r_info_sym'])['st_shndx'] == elf.get_section_index('.data'))
        relocation_position = text['sh_offset'] + relocation['r_offset']
    data_bad = bytearray(raw)
    data_bad[position] ^= 1
    target_bad = bytearray(raw)
    old = int.from_bytes(target_bad[relocation_position:relocation_position+4], 'little')
    target_bad[relocation_position:relocation_position+4] = (old+4).to_bytes(4, 'little')
    for label, contents in [('coefficient-value', data_bad), ('positioned-target', target_bad)]:
        path = folder/(label+'.o')
        path.write_bytes(contents)
        try:
            audit(before, path)
        except AssertionError:
            continue
        raise AssertionError('audit failed to detect '+label)
    audit(before, after)
    print('FixedRC audit controls: 3 passed, 0 failed (coefficient, positioned target, unchanged)', flush=True)
    return {'changed_coefficient_rejected': True, 'changed_positioned_target_rejected': True,
            'unchanged_control_passed': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--revision', default='f51d73c9')
    parser.add_argument('--working', action='store_true')
    args = parser.parse_args()
    root = d.ROOT
    out = root/'build/fidelity13-fixedrc-apparatus'
    out.mkdir(parents=True, exist_ok=True)
    config = (args.baseline/'.build-config').read_text()
    image = config.splitlines()[0].split(' ', 1)[1]
    flags = shlex.split(next(x[6:] for x in config.splitlines() if x.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/'+f if f == 'tools/toolchain/period_compat.h' else f for f in flags]
    identity = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '-c',
        'export PATH='+d.tc.GENTOO_COMPILER_PATH+':$PATH; gcc --version; selected_as=$(gcc -print-prog-name=as); "$selected_as" --version']
    with (out/'identity.log').open('w') as log:
        subprocess.run(identity, stdout=log, stderr=subprocess.STDOUT, check=True)
    source = subprocess.check_output(['git', 'show', args.revision+':'+PATH], cwd=root, text=True)
    historical_header = subprocess.check_output(['git', 'show', args.revision+':include/dsplib/fixedrc.h'], cwd=root)
    historical = out/'historical-headers/dsplib/fixedrc.h'
    historical.parent.mkdir(parents=True, exist_ok=True)
    historical.write_bytes(historical_header)
    cells = [('baseline', source), ('apparatus-in-fixtures', remove_apparatus(source))]
    if args.working:
        cells.append(('working-source', (root/PATH).read_text()))
    result = {'revision': args.revision, 'config': config, 'identity_command': identity, 'cells': {},
              'historical_fixedrc_header_sha256': hashlib.sha256(historical_header).hexdigest()}
    headers = subprocess.check_output(['rg', '--files', 'include', 'src', '-g', '*.h', '-g', '!re/**'], cwd=root, text=True).splitlines()
    result['header_sha256'] = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in headers}
    before = None
    for label, contents in cells:
        folder = out/label
        folder.mkdir(parents=True, exist_ok=True)
        (folder/'FixedRC.c').write_text(contents)
        virtual = '/work/'+label
        compile_flags = flags if label == 'working-source' else ['-I/work/historical-headers']+flags
        shell = d.tc.compile_shell(d.tc.GENTOO_COMPILER_PATH, compile_flags+['-v'], virtual+'/candidate.o', virtual+'/FixedRC.c')
        command = d.tc.docker_prefix(image, root, out, True) + ['/bin/sh', '-c', shell]
        with (folder/'compile.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        obj = folder/'candidate.o'
        entry = {'command': command, 'source_sha256': hashlib.sha256(contents.encode()).hexdigest(),
                 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
                 'functions': sorted(d.b.sizes(str(obj))),
                 'verdicts': {n: d.b.verdict(*d.b.body(d.b.BLOB, n), *d.b.body(str(obj), n))
                              for n in sorted(set(d.b.sizes(str(obj))) & set(d.b.sizes(d.b.BLOB)))}}
        result['cells'][label] = entry
        if label == 'baseline':
            assert obj.read_bytes() == (args.baseline/'src_core_FixedRC.c.o').read_bytes(), 'raw baseline drift'
            entry['raw_baseline_reproduced'] = True
            before = obj
        else:
            entry.update(audit(before, obj))
            old = result['cells']['baseline']['verdicts']
            entry['exact_gains'] = sorted(n for n, v in entry['verdicts'].items() if v[0] == 'EXACT' and old[n][0] != 'EXACT')
            entry['exact_losses'] = sorted(n for n, v in old.items() if v[0] == 'EXACT' and entry['verdicts'][n][0] != 'EXACT')
            assert not entry['exact_gains'] and not entry['exact_losses']
            if label == 'working-source':
                assert obj.read_bytes() == (out/'apparatus-in-fixtures/candidate.o').read_bytes()
                entry['raw_removal_candidate_reproduced'] = True
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
        print(label, entry.get('surviving_bodies', len(entry['functions'])), 'bodies; raw/canonical controls passed', flush=True)
    if args.working:
        result['controls'] = controls(before, out/'working-source/candidate.o', out/'controls')
        result['generator_replay'] = generator_replay(root, out, args.revision, image, flags)
        (out/'results.json').write_text(json.dumps(result, indent=2)+'\n')
    assert all(hashlib.sha256((root/p).read_bytes()).hexdigest() == value for p, value in result['header_sha256'].items())


if __name__ == '__main__':
    main()
