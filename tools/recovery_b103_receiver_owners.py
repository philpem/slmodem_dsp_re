#!/usr/bin/env python3
"""Replay Bell 103 receiver owner reload boundaries on complete Gentoo TUs."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
_paths = sys.path[:]
sys.path[:] = [p for p in sys.path if Path(p or '.').resolve() != ROOT / 'tools']
import dis
sys.path[:] = _paths
sys.path.insert(0, str(ROOT / 'tools'))
import experiment_toolchain as tc
sys.path.insert(0, str(ROOT / 'tools/toolchain'))
import byteident as b
from elftools.elf.elffile import ELFFile

REV = '64d53dd51'
SOURCES = ('B103prc', 'B103int')


def json_value(value):
    if isinstance(value, bytes):
        return {'bytes_hex': value.hex()}
    raise TypeError(type(value).__name__)


def pair_metadata(objects):
    sections, relocations, definitions, undefined = {}, [], {}, set()
    for name, path in objects.items():
        with path.open('rb') as stream:
            elf = ELFFile(stream)
            symbols = elf.get_section_by_name('.symtab')
            for symbol in symbols.iter_symbols():
                if symbol['st_info']['bind'] == 'STB_LOCAL':
                    continue
                if symbol['st_shndx'] == 'SHN_UNDEF':
                    undefined.add(symbol.name)
                else:
                    assert symbol.name not in definitions
                    definitions[symbol.name] = [symbol['st_info']['bind'],
                        symbol['st_info']['type'], symbol['st_other']['visibility']]
            for section in elf.iter_sections():
                if section['sh_type'] in ('SHT_REL', 'SHT_RELA'):
                    target = elf.get_section(section['sh_info'])
                    if target.name == '.text':
                        continue
                    for relocation in section.iter_relocations():
                        symbol = symbols.get_symbol(relocation['r_info_sym'])
                        offset = relocation['r_offset']
                        addend = int.from_bytes(target.data()[offset:offset + 4], 'little')
                        relocations.append([name, target.name, offset,
                                            relocation['r_info_type'], symbol.name, addend])
                elif section.name not in ('.text', '.symtab', '.strtab', '.shstrtab'):
                    sections[name + ':' + section.name] = [section['sh_type'],
                        section['sh_flags'], section['sh_size'], section['sh_addralign'],
                        hashlib.sha256(section.data()).hexdigest()]
    return {'sections': sections, 'nontext_relocations': sorted(relocations),
            'definitions': definitions, 'external_imports': sorted(undefined - definitions.keys())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--objects', type=Path, default=ROOT / 'build/b103-count-baseline')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = {name: subprocess.check_output(['git', 'show', REV + ':src/pump/b103/' + name + '.c'],
                                           cwd=ROOT, text=True) for name in SOURCES}
    start = source['B103prc'].index('short\nRxDetMarkB103(')
    end = source['B103prc'].index('\n}\n', start) + 2
    original = source['B103prc'][start:end]
    generated = {}
    for label, hdx, dsp in [('baseline', False, False),
                            ('hdx-direct', True, False),
                            ('dsp-direct', False, True),
                            ('both-direct', True, True)]:
        body = original
        for owner, direct in [('hdx', hdx), ('dsp', dsp)]:
            if direct:
                declaration = '\tstruct b103_' + owner + ' *' + owner + ' = fp->' + owner + ';\n'
                assert body.count(declaration) == 1
                body = body.replace(declaration, '').replace(owner + '->', 'fp->' + owner + '->')
        texts = dict(source)
        texts['B103prc'] = source['B103prc'][:start] + body + source['B103prc'][end:]
        generated[label] = texts
    assert len({json.dumps(value, sort_keys=True) for value in generated.values()}) == 4
    for name, text in generated['both-direct'].items():
        assert text == (ROOT / 'src/pump/b103' / (name + '.c')).read_text()
    config = (args.objects / '.build-config').read_text()
    image = next(line[6:] for line in config.splitlines() if line.startswith('image '))
    flags = shlex.split(next(line[6:] for line in config.splitlines() if line.startswith('flags ')))
    flags = ['-I/src/include' if f == '-Iinclude' else '/src/tools/toolchain/period_compat.h'
             if f == 'tools/toolchain/period_compat.h' else f for f in flags] + ['-da']
    report = {'revision': REV, 'config': config, 'cells': {},
              'limits': 'Four crossed hdx/dsp owner-reload cells, with no register controls. '
                        'Differential tests are separate.'}
    tc.print_identity(image, tc.GENTOO_COMPILER_PATH, True)
    baseline_objects = None
    for label, texts in generated.items():
        directory = out / label
        directory.mkdir(exist_ok=True)
        commands, objects = [], {}
        for name, text in texts.items():
            (directory / (name + '.c')).write_text(text)
            target = '/work/' + label
            command = tc.docker_prefix(image, ROOT, out, True) + ['/bin/sh', '-c',
                'cd ' + shlex.quote(target) + ' || exit 1; ' + tc.compile_shell(
                    tc.GENTOO_COMPILER_PATH, flags, target + '/' + name + '.o',
                    target + '/' + name + '.c')]
            with (directory / (name + '.compile.log')).open('w') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
            commands.append(command)
            objects[name] = directory / (name + '.o')
        if label == 'baseline':
            for name, obj in objects.items():
                assert obj.read_bytes() == (args.objects / ('src_pump_b103_' + name + '.c.o')).read_bytes()
            baseline_objects = objects
        owners = {n: obj for obj in objects.values() for n in b.sizes(str(obj))}
        baseline_owners = {n: obj for obj in baseline_objects.values() for n in b.sizes(str(obj))}
        assert len(owners) == 17 and owners.keys() == baseline_owners.keys()
        assert pair_metadata(objects) == pair_metadata(baseline_objects)
        rows = {n: {'size': len(b.body(str(obj), n)[0]),
                    'verdict': b.verdict(*b.body(b.BLOB, n), *b.body(str(obj), n)),
                    'relocations': b.body(str(obj), n)[1]}
                for n, obj in sorted(owners.items())}
        base_rows = report['cells'].get('baseline', {}).get('rows', rows)
        entry = {'commands': commands, 'shared_symbols': 17, 'rows': rows,
                 'changed_bodies': [n for n, obj in owners.items()
                     if b.body(str(obj), n) != b.body(str(baseline_owners[n]), n)],
                 'gains': [n for n in rows if rows[n]['verdict'][0] == 'EXACT'
                           and base_rows[n]['verdict'][0] != 'EXACT'],
                 'losses': [n for n in rows if rows[n]['verdict'][0] != 'EXACT'
                            and base_rows[n]['verdict'][0] == 'EXACT'],
                 'objects': {n: hashlib.sha256(o.read_bytes()).hexdigest() for n, o in objects.items()},
                 'source_hashes': {n: hashlib.sha256(t.encode()).hexdigest() for n, t in texts.items()},
                 'nontext_and_bindings_identical': True}
        report['cells'][label] = entry
        (out / 'results.json').write_text(json.dumps(report, indent=2, default=json_value) + '\n')
        print(label, '17/17 symbols; gains', entry['gains'], 'losses', entry['losses'], flush=True)


if __name__ == '__main__':
    main()
