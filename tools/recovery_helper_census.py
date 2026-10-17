#!/usr/bin/env python3
"""Inventory production-only names before classifying reconstruction apparatus.

Only external definitions are removal nominations. Local/weak names and aliases
are counted explicitly; absence from the reference never proves apparatus.
Source token hits include declarations and definitions, not established callers.
Relocation owners are the ELF function ranges, with aliases retained.
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
# tools/dis.py is a command, not the standard library module.
_script_path = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT/'tools']
import dis
sys.path[:] = _script_path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ACCESSORS = {
    'FPM_log10_table_derived', 'FPM_log10_table_entry', 'FPM_log10_table_size',
    'FPM_sqrt_table_entry', 'FPM_sqrt_table_size', 'FPM_div_table_entry',
    'FPM_phasor_cos_entry', 'FPM_phasor_sin_entry', 'FP_Pow_coefficient',
    'CID_MTD_coeff', 'RcFixed_UpFactor', 'RcFixed_DownFactor', 'RcFixed_State',
    'v34handshak_txblock',
}
REVIEWS = {name: 'fixture-only accessor/probe; removal needs owner/full-TU control'
           for name in ACCESSORS}
REVIEWS.update({
    'rc_banks': 'offline coefficient catalog; inspect emission/storage consumers before removal',
    'dsplib_encode_plain': 'optional runtime diagnostic mode; not a fixture-only accessor',
    'dsplib_v34_blob_preemp': 'runtime bug-reproduction selector; not a fixture-only accessor',
})


def inventory(path):
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symbols = list(elf.get_section_by_name('.symtab').iter_symbols())
        functions = [s for s in symbols if s['st_info']['type'] == 'STT_FUNC'
                     and isinstance(s['st_shndx'], int) and s['st_size']]
        definitions = []
        for symbol in symbols:
            if not symbol.name or symbol['st_shndx'] == 'SHN_UNDEF' or symbol['st_info']['type'] in ('STT_FILE', 'STT_SECTION'):
                continue
            section = elf.get_section(symbol['st_shndx']).name if isinstance(symbol['st_shndx'], int) else symbol['st_shndx']
            aliases = sorted(s.name for s in symbols if s.name != symbol.name
                             and s['st_shndx'] == symbol['st_shndx']
                             and s['st_value'] == symbol['st_value']
                             and s['st_info']['type'] == symbol['st_info']['type'])
            definitions.append({'name': symbol.name, 'type': symbol['st_info']['type'],
                                'binding': symbol['st_info']['bind'], 'size': symbol['st_size'],
                                'section': section, 'aliases': aliases})
        edges = []
        for section in elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue
            table = elf.get_section(section['sh_link'])
            source = elf.get_section(section['sh_info'])
            for reloc in section.iter_relocations():
                target = table.get_symbol(reloc['r_info_sym'])
                owners = sorted(s.name for s in functions if s['st_shndx'] == section['sh_info']
                                and s['st_value'] <= reloc['r_offset'] < s['st_value']+s['st_size'])
                edges.append({'target': target.name, 'owners': owners, 'section': source.name,
                              'offset': reloc['r_offset'], 'type': reloc['r_info_type']})
        return {'definitions': definitions, 'relocations': edges}


def census(reference, objects):
    original = inventory(reference)
    original_names = {s['name'] for s in original['definitions']}
    ours = {str(p): inventory(p) for p in objects}
    extra, local = [], []
    for path, obj in ours.items():
        for symbol in obj['definitions']:
            if symbol['name'] in original_names:
                continue
            row = dict(symbol, object=path)
            if symbol['binding'] == 'STB_LOCAL':
                local.append(row)
                continue
            row['review'] = REVIEWS.get(symbol['name'], 'UNRESOLVED')
            if symbol['binding'] == 'STB_WEAK' and symbol['name'].startswith('_ZN10GenericIIRIfdE'):
                row['review'] = 'retained template-generated helper/structor; absence does not prove apparatus'
            row['relocation_referrers'] = [dict(edge, object=p) for p, inv in ours.items()
                                         for edge in inv['relocations'] if edge['target'] == symbol['name']]
            extra.append(row)
    return {'reference_defined_names': len(original_names), 'objects': len(objects),
            'emitted_definitions': sum(len(o['definitions']) for o in ours.values()),
            'extra_external_definitions': extra, 'extra_local_definitions': local,
            'extra_external_by_type': dict(Counter(s['type'] for s in extra)),
            'extra_local_by_type': dict(Counter(s['type'] for s in local)),
            'unresolved_external': sum(s['review'] == 'UNRESOLVED' for s in extra),
            'local_scope_limit': 'name census only; compiler pools/local helper identities remain unclassified; no removal authority',
            'relocation_scope_limit': 'named ELF relocation referrers only; same-TU resolved direct calls and indirect runtime calls are not caller completeness'}


def self_test():
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp)
        for name, body in {'ref': '.globl shared\n.type shared,@function\nshared: ret\n.size shared,.-shared\n',
                           'ours': '.globl shared,probe\n.weak generated\n.type shared,@function\nshared: call probe\nret\n.size shared,.-shared\n.type probe,@function\nprobe: ret\n.size probe,.-probe\n.set generated,probe\n.local own_data\n.type own_data,@object\n.data\nown_data: .long 12\n.size own_data,4\n'}.items():
            source = folder/(name+'.s')
            source.write_text(body)
            subprocess.run(['as', '--32', str(source), '-o', str(folder/(name+'.o'))], check=True)
        report = census(folder/'ref.o', [folder/'ours.o'])
        rows = {s['name']: s for s in report['extra_external_definitions']}
        assert set(rows) == {'probe', 'generated'}
        assert rows['generated']['binding'] == 'STB_WEAK' and rows['generated']['aliases'] == ['probe']
        assert rows['probe']['relocation_referrers'][0]['owners'] == ['shared']
        assert report['unresolved_external'] == 2
        assert [s['name'] for s in report['extra_local_definitions']] == ['own_data']
    print('helper census controls: 5 passed, 0 failed (extra/shared, weak/alias, caller, unresolved, local data)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path)
    parser.add_argument('--reference', type=Path, default=ROOT/'ref/slmodemd/dsplibs.o')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    assert args.objects and args.output, '--objects and --output required'
    paths = sorted(args.objects.glob('*.o'))
    assert paths, 'empty object denominator'
    report = census(args.reference, paths)
    report['reference_sha256'] = hashlib.sha256(args.reference.read_bytes()).hexdigest()
    report['object_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    for row in report['extra_external_definitions']:
        name = row['name']
        hits = subprocess.run(['rg', '-l', '-w', '-F', name, 'src', 'include', 'test', 'tools',
                               '-g', '!re/**', '-g', '*.c', '-g', '*.cpp', '-g', '*.h', '-g', '*.py'],
                              cwd=ROOT, capture_output=True, text=True)
        assert hits.returncode in (0, 1), hits.stderr
        row['source_token_files_including_comments'] = hits.stdout.splitlines()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('helper census: %d objects, %d emitted definitions, %d extra external (%d unresolved), %d extra local (unclassified)'
          % (report['objects'], report['emitted_definitions'], len(report['extra_external_definitions']),
             report['unresolved_external'], len(report['extra_local_definitions'])))
    for row in report['extra_external_definitions']:
        print(row['binding'], row['type'], row['size'], row['name'], row['review'])


if __name__ == '__main__':
    main()
